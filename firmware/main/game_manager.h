#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "lvgl.h"

typedef enum {
    GAME_ID_NONE = -1,
    GAME_ID_TIC_TAC_TOE = 0,
    GAME_ID_SNAKE = 1
} game_id_t;

void game_manager_init(void);
bool game_manager_start(game_id_t game_id, lv_obj_t *screen);
void game_manager_stop(void);
bool game_manager_is_active(void);
game_id_t game_manager_current(void);

#endif
