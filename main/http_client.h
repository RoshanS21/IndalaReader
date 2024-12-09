#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#pragma once

#include "esp_http_client.h"

void send_card_to_server(uint32_t cardNumber);
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
void configure_mbedtls_logging(void);

#endif // HTTP_CLIENT_H
