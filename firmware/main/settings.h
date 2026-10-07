#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct {
    int brightness;
    int volume;
    int dim_brightness;
    int dim_timeout;
    int language;
    int theme;
    bool muted;
    bool screensaver_enabled;
    uint32_t favorite_games;
    time_t epoch;
} settings_data_t;

void settings_load(settings_data_t *data);
void settings_save(const settings_data_t *data);
