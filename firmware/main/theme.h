#pragma once

#include "lvgl.h"

int theme_get(void);
void theme_set(int value);
void theme_apply(lv_obj_t *screen);
void theme_animation_tick(lv_timer_t *timer);
