#ifndef VEGG_GAME_H
#define VEGG_GAME_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void vegg_open(lv_obj_t *screen);
void vegg_stop(void);
void vegg_set_exit_callback(void (*callback)(void));
bool vegg_is_active(void);
uint32_t vegg_best_score(void);
void vegg_set_best_score(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif
