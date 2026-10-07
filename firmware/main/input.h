#pragma once

#include <stdbool.h>
#include "lvgl.h"

typedef void (*input_action_cb_t)(void);

void input_init(void);
void input_register_screen(lv_obj_t *screen);
void input_set_launcher_state(bool active, int page, int page_count);
int input_get_launcher_page(void);
void input_set_actions(input_action_cb_t bottom, input_action_cb_t left, input_action_cb_t right);
void input_set_game_state(bool active, input_action_cb_t up);
void input_set_activity_callback(input_action_cb_t callback);
