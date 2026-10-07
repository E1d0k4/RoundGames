#pragma once

#include "lvgl.h"

typedef void (*tic_tac_toe_back_cb_t)(void);

void tic_tac_toe_open(lv_obj_t *screen, tic_tac_toe_back_cb_t back_cb);
