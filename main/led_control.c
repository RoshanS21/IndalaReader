#include "led_control.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define GREEN_LED_PIN 21
#define RED_LED_PIN 22

void init_leds(void) {
    // Configure Red LED Pin
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RED_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // Configure Green LED Pin
    io_conf.pin_bit_mask = (1ULL << GREEN_LED_PIN);
    gpio_config(&io_conf);

    // Door locked on Start
    gpio_set_level(RED_LED_PIN, 1);
}

void control_leds(const char* status)
{
    if(strcmp(status, "success") == 0)
    {
        // Unlock door
        gpio_set_level(RED_LED_PIN, 0);
        gpio_set_level(GREEN_LED_PIN, 1);
        const int greenLedOnTimeMS = 3000;
        vTaskDelay(pdMS_TO_TICKS(greenLedOnTimeMS));
    }
    else
    {
        // Blink red LED
        const int timesToBlink = 3;
        const int blinkDelayMS = 300;
        int index;
        for(index = 0; index < timesToBlink; ++index)
        {
            gpio_set_level(RED_LED_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
            gpio_set_level(RED_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
        }
    }

    gpio_set_level(RED_LED_PIN, 1);
    gpio_set_level(GREEN_LED_PIN, 0);
}