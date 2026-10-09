#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef void (*launcher_game_cb_t)(int game_index, const char *name);

void launcher_init(void);
void launcher_build(lv_obj_t *screen, launcher_game_cb_t game_cb);
void launcher_set_active(bool active);
