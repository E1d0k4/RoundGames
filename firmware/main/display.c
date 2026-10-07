#include "display.h"

#include "bsp/esp-bsp.h"

void display_init(void)
{
    bsp_display_start();
}

void display_set_brightness(int value)
{
    bsp_display_brightness_set(value);
}

void display_lock(void)
{
    bsp_display_lock(-1);
}

void display_unlock(void)
{
    bsp_display_unlock();
}
