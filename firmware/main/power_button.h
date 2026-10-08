#pragma once

#include <stdbool.h>

typedef void (*power_button_callback_t)(void);

void power_button_init(void);
void power_button_set_callback(power_button_callback_t callback);
