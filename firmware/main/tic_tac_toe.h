#ifndef TIC_TAC_TOE_H
#define TIC_TAC_TOE_H

#include "lvgl.h"

void tic_tac_toe_open(lv_obj_t *screen);
void tic_tac_toe_prepare_settings(void);
void tic_tac_toe_resume_from_settings(void);
void tic_tac_toe_stop(void);

#endif
