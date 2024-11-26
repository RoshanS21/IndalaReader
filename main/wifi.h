#ifndef WIFI_H
#define WIFI_H

#pragma once

#include "esp_event.h"
#include <stdint.h>

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(void);

#endif // WIFI_H
