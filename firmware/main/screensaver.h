#pragma once

#include <stdbool.h>
#include "lvgl.h"

void screensaver_init(void);
void screensaver_set_enabled(bool enabled);
void screensaver_set_normal_brightness(int value);
void screensaver_set_dim_brightness(int value);
void screensaver_set_timeout(int seconds);
void screensaver_activity_reset(void);
bool screensaver_is_active(void);
bool screensaver_is_enabled(void);
void screensaver_tick(lv_timer_t *timer);
