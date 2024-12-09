#include "http_client.h"
#include "led_control.h"

#include "cJSON.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "mbedtls/base64.h"
#include "mbedtls/debug.h"

static const char *HTTP_CLIENT_TAG = "HTTP_CLIENT";

#define READER_ID CONFIG_ESP_READER_ID
#define SERVER_URL CONFIG_ESP_SERVER_URL
#define SERVER_TIMEOUT_MS 10000 // 10 seconds

extern const char server_cert_pem_start[] asm("_binary_server_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_server_cert_pem_end");

void base64_encode(const char *input, char *output) {
    size_t input_len = strlen(input);
    size_t output_len;
    mbedtls_base64_encode((unsigned char *)output, 128, &output_len, (const unsigned char *)input, input_len);
    output[output_len] = '\0';
}

void mbedtls_debug(void *ctx, int level, const char *file, int line, const char *str) {
    ((void) level);
    ESP_LOGI("mbedtls", "%s:%04d: %s", file, line, str);
}

void configure_mbedtls_logging(void) {
    esp_log_level_set("mbedtls", ESP_LOG_DEBUG);
    mbedtls_ssl_config ssl_conf;
    mbedtls_ssl_config_init(&ssl_conf);
    mbedtls_ssl_conf_dbg(&ssl_conf, mbedtls_debug, NULL);
}

void send_card_to_server(uint32_t cardNumber) {
    char post_data[256];
    snprintf(post_data, sizeof(post_data), "{\"cardID\":\"%08lx\", \"readerID\":\"%s\"}", cardNumber, READER_ID);

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
        .timeout_ms = SERVER_TIMEOUT_MS,
        .cert_pem = server_cert_pem_start, // Add the server certificate
        .keep_alive_enable = true,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Set HTTP POST method and payload
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_log_level_set("HTTP_CLIENT", ESP_LOG_VERBOSE);

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(HTTP_CLIENT_TAG, "HTTP POST Status: %d", status_code);
        // Response data processed directly in the event handler
    } else {
        ESP_LOGE(HTTP_CLIENT_TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char response_buffer[1024];
    static int buffer_pos = 0;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (buffer_pos + evt->data_len < sizeof(response_buffer) - 1) {
                    memcpy(response_buffer + buffer_pos, evt->data, evt->data_len);
                    buffer_pos += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_ON_FINISH");
            response_buffer[buffer_pos] = '\0';
            ESP_LOGI(HTTP_CLIENT_TAG, "Server Response: %s", response_buffer);

            // Parse the JSON response
            cJSON *json = cJSON_Parse(response_buffer);
            if (json == NULL) {
                ESP_LOGE(HTTP_CLIENT_TAG, "Failed to parse JSON response");
            } else {
                cJSON *message = cJSON_GetObjectItem(json, "message");
                cJSON *status = cJSON_GetObjectItem(json, "status");

                if (cJSON_IsString(message) && (message->valuestring != NULL)) {
                    ESP_LOGI(HTTP_CLIENT_TAG, "Message: %s", message->valuestring);
                }

                if (cJSON_IsString(status) && (status->valuestring != NULL)) {
                    ESP_LOGI(HTTP_CLIENT_TAG, "Status: %s", status->valuestring);
                    // Control LEDs based on status
                    control_leds(status->valuestring);
                }

                cJSON_Delete(json);
            }
            buffer_pos = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(HTTP_CLIENT_TAG, "HTTP_EVENT_REDIRECT");
            break;
    }

    return ESP_OK;
}
