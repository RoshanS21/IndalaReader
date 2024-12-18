#include "http_client.h"
#include "led_control.h"
#include "wiegand_reader.h"
#include "wifi.h"

#include "nvs_flash.h"

void app_main(void) {
    init_leds();
    configure_mbedtls_logging();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    init_wiegand_reader();
}
