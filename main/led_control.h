#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#pragma once

#include <stdbool.h>

void init_leds(void);
void control_leds(const bool accessGranted);

#endif // LED_CONTROL_H
