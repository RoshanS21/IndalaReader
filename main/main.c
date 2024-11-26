#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_tls.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "mbedtls/base64.h"
#include "mbedtls/debug.h"
#include "nvs_flash.h"
#include <string.h>

#define SERVER_URL "https://5186-2603-8081-1410-5a02-3104-d336-b66a-5247.ngrok-free.app/api/card_reader/validate_card"
#define SERVER_TIMEOUT_MS 10000 // 10 seconds

extern const char server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_server_cert_pem_end");

// WiFi Configuration
#define WIFI_SSID      "MySpectrumWiFi80-2G"
#define WIFI_PASS      "livelyroad189"
#define MAX_RETRY      5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Wiegand Configuration
#define D0_PIN GPIO_NUM_4
#define D1_PIN GPIO_NUM_5
#define MAX_BITS 32
#define WIEGAND_TIMEOUT 50000
#define GREEN_LED_PIN 21
#define RED_LED_PIN 22

static EventGroupHandle_t wifi_event_group;
static const char *WIFI_TAG = "WIFI";
static const char *WIEGAND_TAG = "WIEGAND_READER";
// static const char *READER_ID = "Indala_Test_1";

static int retry_num = 0;
typedef struct {
    uint8_t bitCount;
    uint8_t data[MAX_BITS / 8];
} wiegand_t;

volatile wiegand_t wiegandData;

static QueueHandle_t gpio_evt_queue = NULL;
static esp_timer_handle_t wiegand_timer;

// Function Prototypes
void wifi_init_sta(void);
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void print_wiegand_data(const volatile wiegand_t* data);
static void process_wiegand_data(void);
void app_main(void);
// Declare the event handler function
esp_err_t _http_event_handler(esp_http_client_event_t *evt);


#include "esp_log.h"
#include "esp_tls.h"
#include "mbedtls/debug.h"

// Function to initialize LEDs
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
}

// Functio to control LEDs based on server response
void control_leds(const char* status)
{
    if(strcmp(status, "success") == 0)
    {
        // Turn OFF red LED and turn ON green LED
        gpio_set_level(RED_LED_PIN, 0);
        gpio_set_level(GREEN_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(5000)); // Keep green LED ON for 5 seconds
    }

    gpio_set_level(RED_LED_PIN, 1);
    gpio_set_level(GREEN_LED_PIN, 0);
}

// Debug function for mbedtls
void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);
    ESP_LOGI("mbedtls", "%s:%04d: %s", file, line, str);
}

// Function to configure the logging level for mbedtls
void configure_mbedtls_logging(void) {
    esp_log_level_set("mbedtls", ESP_LOG_DEBUG);
    mbedtls_ssl_config ssl_conf;
    mbedtls_ssl_config_init(&ssl_conf);
    mbedtls_ssl_conf_dbg(&ssl_conf, mbedtls_debug, NULL);
}


// Function to set a custom DNS server
void set_custom_dns(void) {
    ip_addr_t dnsserver;
    IP_ADDR4(&dnsserver, 8, 8, 8, 8); // Google's DNS server
    dns_setserver(0, &dnsserver);
    ESP_LOGI(WIFI_TAG, "Custom DNS server set: 8.8.8.8");
}

void base64_encode(const char *input, char *output) {
    size_t input_len = strlen(input);
    size_t output_len;
    mbedtls_base64_encode((unsigned char *)output, 128, &output_len, (const unsigned char *)input, input_len);
    output[output_len] = '\0'; // Null-terminate the output
}

// Function to send card number to server and parse response
static void send_card_to_server(uint32_t cardNumber) {
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "{\"cardID\":\"%08lx\", \"readerID\":\"Indala1\"}", cardNumber);

    // Combine username and password and encode in Base64
    char username[] = "cardReader";
    char password[] = "readerPass";
    char auth_str[128];
    snprintf(auth_str, sizeof(auth_str), "%s:%s", username, password);
    char auth_base64[128];
    base64_encode(auth_str, auth_base64);

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Basic %s", auth_base64);

    esp_http_client_config_t config = {
        .url = SERVER_URL,
        .timeout_ms = SERVER_TIMEOUT_MS, // Set timeout
        .cert_pem = server_cert_pem_start, // Add the server certificate
        .keep_alive_enable = true, // Ensure keep-alive is enabled
        .event_handler = _http_event_handler, // Event handler for detailed logging
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set HTTP POST method and payload
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    // Enable HTTP logging
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(WIEGAND_TAG, "HTTP POST Status: %d", status_code);

        // Process response data directly in the event handler
    } else {
        ESP_LOGE(WIEGAND_TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Define the event handler function
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char response_buffer[1024];
    static int buffer_pos = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (buffer_pos + evt->data_len < sizeof(response_buffer) - 1) {
                    memcpy(response_buffer + buffer_pos, evt->data, evt->data_len);
                    buffer_pos += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_ON_FINISH");
            response_buffer[buffer_pos] = '\0';
            ESP_LOGI(WIEGAND_TAG, "Server Response: %s", response_buffer);

            // Parse the JSON response
            cJSON *json = cJSON_Parse(response_buffer);
            if (json == NULL) {
                ESP_LOGE(WIEGAND_TAG, "Failed to parse JSON response");
            } else {
                cJSON *message = cJSON_GetObjectItem(json, "message");
                cJSON *status = cJSON_GetObjectItem(json, "status");

                if (cJSON_IsString(message) && (message->valuestring != NULL)) {
                    ESP_LOGI(WIEGAND_TAG, "Message: %s", message->valuestring);
                }

                if (cJSON_IsString(status) && (status->valuestring != NULL)) {
                    ESP_LOGI(WIEGAND_TAG, "Status: %s", status->valuestring);
                    // Control LEDs based on status
                    control_leds(status->valuestring);
                }

                cJSON_Delete(json);
            }
            buffer_pos = 0; // Reset buffer position for next request
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(WIEGAND_TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}


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
        ESP_LOGE(WIEGAND_TAG, "Wiegand timeout, received %d bits (incomplete data)", wiegandData.bitCount);
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
    ESP_LOGI(WIEGAND_TAG, "Wiegand data: %s", buffer);
}

static void process_wiegand_data() {
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

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (retry_num < MAX_RETRY) {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(WIFI_TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG, "Failed to connect");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        set_custom_dns(); // Set custom DNS after getting IP
    }
}

void wifi_init_sta(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void) {
    // Initialize LEDs
    init_leds();
    gpio_set_level(RED_LED_PIN, 1);

    // Configure mbedtls logging
    configure_mbedtls_logging();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(WIFI_TAG, "Initializing WiFi...");
    wifi_init_sta();

    // Delay to ensure Wi-Fi connection is established
    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected to WiFi");
    } else {
        ESP_LOGE(WIFI_TAG, "Failed to connect to WiFi");
        return;
    }

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
