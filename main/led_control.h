#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#pragma once

#include <stdbool.h>

void init_leds(void);
void control_leds(const bool accessGranted);
void fail_to_read_card();
void fail_to_connect_wifi();
void connected_to_wifi();

#endif // LED_CONTROL_H
