#include "led_control.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define DOOR_LOCK_SET 18
#define DOOR_LOCK_UNSET 19
#define DOOR_SENSOR_PIN 20
#define GREEN_LED_PIN 21
#define RED_LED_PIN 22

static const char *DOOR_SENSOR_TAG = "DOOR SENSOR";

static void lock_door()
{
    gpio_set_level(RED_LED_PIN, 1);
    gpio_set_level(GREEN_LED_PIN, 0);
    gpio_set_level(DOOR_LOCK_UNSET, 0);
    gpio_set_level(DOOR_LOCK_SET, 1);
}

static void unlock_door()
{
    gpio_set_level(RED_LED_PIN, 0);
    gpio_set_level(GREEN_LED_PIN, 1);
    gpio_set_level(DOOR_LOCK_SET, 0);
    gpio_set_level(DOOR_LOCK_UNSET, 1);
}

void init_leds(void)
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RED_LED_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << GREEN_LED_PIN);
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << DOOR_LOCK_SET);
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << DOOR_LOCK_UNSET);
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << DOOR_SENSOR_PIN);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    lock_door();
}

void control_leds(const bool accessGranted)
{
    if(accessGranted)
    {
        unlock_door();
        const int greenLedOnTimeMS = 3000;
        vTaskDelay(pdMS_TO_TICKS(greenLedOnTimeMS));

        const int doorCloseWaitTimeMS = 1000;
        while(1)
        {
            int door_state = gpio_get_level(DOOR_SENSOR_PIN);
            if(door_state == 0)
            {
                ESP_LOGI(DOOR_SENSOR_TAG, "Door Closed.");
                break;
            }
            else
            {
                ESP_LOGI(DOOR_SENSOR_TAG, "Door Open.");
                vTaskDelay(pdMS_TO_TICKS(doorCloseWaitTimeMS));
            }
        }
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

    lock_door();
}

void fail_to_read_card()
{
    const int timesToBlink = 2;
    const int blinkDelayMS = 500;
    int index;
    for(index = 0; index < timesToBlink; ++index)
    {
        gpio_set_level(RED_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
        gpio_set_level(RED_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
    }
}

void fail_to_connect_wifi()
{
    const int timesToBlink = 12;
    const int blinkDelayMS = 500;
    int index;
    for(index = 0; index < timesToBlink; ++index)
    {
        gpio_set_level(RED_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
        gpio_set_level(RED_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
    }
}

void connected_to_wifi()
{
    const int timesToBlink = 3;
    const int blinkDelayMS = 300;
    int index;
    for(index = 0; index < timesToBlink; ++index)
    {
        gpio_set_level(GREEN_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
        gpio_set_level(GREEN_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(blinkDelayMS));
    }
}
