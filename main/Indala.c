#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

#define GPIO_DATA0 4
#define GPIO_DATA1 5

volatile uint32_t card_data = 0;
volatile int bit_count = 0;

void IRAM_ATTR handle_wiegand_data0(void* arg) {
    card_data <<= 1;
    bit_count++;
}

void IRAM_ATTR handle_wiegand_data1(void* arg) {
    card_data <<= 1;
    card_data |= 1;
    bit_count++;
}

void read_wiegand_task(void *arg) {
    while (true) {
        if (bit_count > 0) {
            ESP_LOGI("IndalaReader", "Bits: %d, Data: %lu", bit_count, card_data);
        }
        
        if (bit_count == 26) { // Assuming 26-bit Wiegand format
            ESP_LOGI("IndalaReader", "Card Data: %lu", card_data);
            card_data = 0; // Reset for next read
            bit_count = 0;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    // Initialize GPIOs
    gpio_reset_pin(GPIO_DATA0);
    gpio_reset_pin(GPIO_DATA1);
    gpio_set_direction(GPIO_DATA0, GPIO_MODE_INPUT);
    gpio_set_direction(GPIO_DATA1, GPIO_MODE_INPUT);

    // Enable internal pull-ups
    gpio_pullup_en(GPIO_DATA0);
    gpio_pullup_en(GPIO_DATA1);

    // Install GPIO ISR service
    gpio_install_isr_service(0); // Use 0 to indicate no flags

    // Install GPIO ISR handlers
    gpio_set_intr_type(GPIO_DATA0, GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(GPIO_DATA1, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(GPIO_DATA0, handle_wiegand_data0, (void*) GPIO_DATA0);
    gpio_isr_handler_add(GPIO_DATA1, handle_wiegand_data1, (void*) GPIO_DATA1);

    // Create a task with an increased stack size
    xTaskCreate(read_wiegand_task, "read_wiegand_task", 2048, NULL, 10, NULL);
}
