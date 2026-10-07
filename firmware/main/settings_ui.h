#pragma once

#include <stdbool.h>
#include "lvgl.h"

lv_obj_t *settings_ui_button_create(lv_obj_t *parent, int width, int height, lv_align_t align, int x, int y, bool selected);
void settings_ui_center_label(lv_obj_t *button, const char *text, const lv_font_t *font);
