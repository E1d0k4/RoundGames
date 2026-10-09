#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

typedef enum {
    GAME_ID_NONE = -1,
    GAME_ID_TIC_TAC_TOE = 0,
    GAME_ID_SNAKE = 1,
    GAME_ID_VEGG = 2,
    GAME_ID_ORBIT_BREAKER = 3
} game_id_t;

void game_manager_init(void);
void game_manager_set_exit_callback(void (*callback)(void));

size_t game_manager_game_count(void);
game_id_t game_manager_game_id_at(int index);
const char *game_manager_game_name_at(int index);
const char *game_manager_game_icon_at(int index);
uint32_t game_manager_game_color_at(int index);

bool game_manager_start_index(int index, lv_obj_t *screen);
bool game_manager_start(game_id_t game_id, lv_obj_t *screen);
void game_manager_stop(void);
bool game_manager_is_active(void);
game_id_t game_manager_current(void);

#endif
