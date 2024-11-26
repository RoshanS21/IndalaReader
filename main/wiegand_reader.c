#include "wiegand_reader.h"
#include "http_client.h"
#include "led_control.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

// Wiegand Configuration
#define D0_PIN GPIO_NUM_4
#define D1_PIN GPIO_NUM_5
#define MAX_BITS 32
#define WIEGAND_TIMEOUT 50000

static esp_timer_handle_t wiegand_timer;
static QueueHandle_t gpio_evt_queue = NULL;
static const char *WIEGAND_TAG = "WIEGAND_READER";

typedef struct {
    uint8_t bitCount;
    uint8_t data[MAX_BITS / 8];
} wiegand_t;

volatile wiegand_t wiegandData;

void IRAM_ATTR d0_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void IRAM_ATTR d1_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void print_wiegand_data(const volatile wiegand_t* data) {
    char buffer[MAX_BITS + 1];
    for (int i = 0; i < data->bitCount; ++i) {
        buffer[i] = ((data->data[i / 8] >> (7 - (i % 8))) & 1) ? '1' : '0';
    }
    buffer[data->bitCount] = '\0';
    ESP_LOGI(WIEGAND_TAG, "Wiegand data: %s", buffer);
}

static void wiegand_timeout_cb(void* arg) {
    if (wiegandData.bitCount > 0) {
        ESP_LOGE(WIEGAND_TAG, "Wiegand timeout, received %d bits (incomplete data)", wiegandData.bitCount);
        print_wiegand_data(&wiegandData);
        wiegandData.bitCount = 0;
    }
}

void process_wiegand_data(void) {
    ESP_LOGI(WIEGAND_TAG, "Processing %d bits", wiegandData.bitCount);
    if (wiegandData.bitCount == 32) {
        // Extract Facility Code and Card Number
        uint8_t facilityCode = (wiegandData.data[0] << 1) | (wiegandData.data[1] >> 7);
        uint32_t cardNumber = ((wiegandData.data[1] & 0x7F) << 25) |
                              (wiegandData.data[2] << 17) |
                              (wiegandData.data[3] << 9) |
                              (wiegandData.data[4] << 1);

        ESP_LOGI(WIEGAND_TAG, "Facility Code: %d", facilityCode);
        ESP_LOGI(WIEGAND_TAG, "Card Number: %lu", (unsigned long)cardNumber);
        ESP_LOGI(WIEGAND_TAG, "Card Number(Hex): %08lx", (unsigned long)cardNumber);
        ESP_LOGI(WIEGAND_TAG, " ");

        // Send card number to the server
        send_card_to_server(cardNumber);
    } else {
        ESP_LOGE(WIEGAND_TAG, "Invalid Wiegand bit length");
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
                process_wiegand_data();
            }
        }
    }
}

void init_wiegand_reader(void)
{
    ESP_LOGI(WIEGAND_TAG, "Initializing Wiegand Reader...");
    wiegandData.bitCount = 0;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << D0_PIN) | (1ULL << D1_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(D0_PIN, d0_isr_handler, (void*) D0_PIN);
    gpio_isr_handler_add(D1_PIN, d1_isr_handler, (void*) D1_PIN);

    const esp_timer_create_args_t timer_args = {
        .callback = &wiegand_timeout_cb,
        .name = "wiegand_timer"
    };
    esp_timer_create(&timer_args, &wiegand_timer);

    xTaskCreate(gpio_task_example, "gpio_task_example", 8192, NULL, 10, NULL);

    ESP_LOGI(WIEGAND_TAG, "Wiegand Reader Initialized on Pins %d (D0) and %d (D1)", D0_PIN, D1_PIN);
}