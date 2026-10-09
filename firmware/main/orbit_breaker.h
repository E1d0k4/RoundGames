#ifndef ORBIT_BREAKER_H
#define ORBIT_BREAKER_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void orbit_breaker_open(lv_obj_t *screen);
void orbit_breaker_stop(void);

#ifdef __cplusplus
}
#endif

#endif
