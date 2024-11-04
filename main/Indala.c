#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"

#define D0_PIN GPIO_NUM_4
#define D1_PIN GPIO_NUM_5

#define MAX_BITS 32 // max number of bits to capture
#define WIEGAND_TIMEOUT 50000 // time to wait for another bit (in microseconds)

static const char *TAG = "WIEGAND_READER";

typedef struct {
    uint8_t bitCount;
    uint8_t data[MAX_BITS / 8];
} wiegand_t;

volatile wiegand_t wiegandData;
static QueueHandle_t gpio_evt_queue = NULL;
static esp_timer_handle_t wiegand_timer;

static void print_wiegand_data(const volatile wiegand_t* data);

void IRAM_ATTR d0_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void IRAM_ATTR d1_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void wiegand_timeout_cb(void* arg) {
    if (wiegandData.bitCount > 0) {
        ESP_LOGE(TAG, "Wiegand timeout, received %d bits (incomplete data)", wiegandData.bitCount);
        print_wiegand_data(&wiegandData);
        wiegandData.bitCount = 0;
    }
}

static void print_wiegand_data(const volatile wiegand_t* data) {
    char buffer[MAX_BITS + 1];
    for (int i = 0; i < data->bitCount; ++i) {
        buffer[i] = ((data->data[i / 8] >> (7 - (i % 8))) & 1) ? '1' : '0';
    }
    buffer[data->bitCount] = '\0';
    ESP_LOGI(TAG, "Wiegand data: %s", buffer);
}

static void process_wiegand_data() {
    ESP_LOGI(TAG, "Received %d bits", wiegandData.bitCount);
    if (wiegandData.bitCount == 32) {
        ESP_LOGI(TAG, "Valid card data received");
        print_wiegand_data(&wiegandData);

        // Extract facility code and card number
        uint8_t facilityCode = (wiegandData.data[0] << 1) | (wiegandData.data[1] >> 7);
        uint32_t cardNumber = ((wiegandData.data[1] & 0x7F) << 25) |
                              (wiegandData.data[2] << 17) |
                              (wiegandData.data[3] << 9) |
                              (wiegandData.data[4] << 1);

        ESP_LOGI(TAG, "Facility Code: %d", facilityCode);
        ESP_LOGI(TAG, "Card Number: %08lx", (unsigned long)cardNumber);  // Use %08lx for unsigned long in hex
        ESP_LOGI(TAG, " ");
    } else {
        ESP_LOGE(TAG, "Invalid Wiegand bit length");
    }
    wiegandData.bitCount = 0;
}

static void gpio_task_example(void* arg) {
    uint32_t io_num;
    while (true) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            esp_timer_stop(wiegand_timer);
            esp_timer_start_once(wiegand_timer, WIEGAND_TIMEOUT);
            if (wiegandData.bitCount < MAX_BITS) {
                wiegandData.data[wiegandData.bitCount / 8] <<= 1;
                if (io_num == D1_PIN) {
                    wiegandData.data[wiegandData.bitCount / 8] |= 1;
                }
                wiegandData.bitCount++;
            }
            if (wiegandData.bitCount >= MAX_BITS) {
                print_wiegand_data(&wiegandData);
                process_wiegand_data();
                vTaskDelay(pdMS_TO_TICKS(1000)); // Add delay to avoid continuous logging
            }
        }
    }
}

void app_main(void) {
    wiegandData.bitCount = 0;

    // Configure D0 and D1 pins
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Use negative edge for precise data capture
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << D0_PIN) | (1ULL << D1_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Create a queue to handle GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Install GPIO ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(D0_PIN, d0_isr_handler, (void*) D0_PIN);
    gpio_isr_handler_add(D1_PIN, d1_isr_handler, (void*) D1_PIN);

    // Create a timer for Wiegand timeout
    const esp_timer_create_args_t timer_args = {
        .callback = &wiegand_timeout_cb,
        .name = "wiegand_timer"
    };
    esp_timer_create(&timer_args, &wiegand_timer);

    // Start the GPIO task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    ESP_LOGI(TAG, "Wiegand reader initialized on pins %d (D0) and %d (D1)", D0_PIN, D1_PIN);
}
