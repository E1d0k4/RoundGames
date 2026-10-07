#include "settings.h"

#include <sys/time.h>
#include "nvs.h"

void settings_save(const settings_data_t *data)
{
    if (!data) return;

    nvs_handle_t nvs;
    if (nvs_open("settings", NVS_READWRITE, &nvs) != ESP_OK) return;

    nvs_set_i32(nvs, "brightness", data->brightness);
    nvs_set_i32(nvs, "volume", data->volume);
    nvs_set_i32(nvs, "dim_bright", data->dim_brightness);
    nvs_set_i32(nvs, "dim_time", data->dim_timeout);
    nvs_set_i32(nvs, "language", data->language);
    nvs_set_i32(nvs, "theme", data->theme);
    nvs_set_i32(nvs, "muted", data->muted);
    nvs_set_i32(nvs, "screensaver", data->screensaver_enabled);
    nvs_set_u32(nvs, "favorites", data->favorite_games);
    nvs_set_i64(nvs, "epoch", (int64_t)data->epoch);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void settings_load(settings_data_t *data)
{
    if (!data) return;

    nvs_handle_t nvs;
    if (nvs_open("settings", NVS_READONLY, &nvs) != ESP_OK) return;

    int32_t value;
    if (nvs_get_i32(nvs, "brightness", &value) == ESP_OK) data->brightness = value;
    if (nvs_get_i32(nvs, "volume", &value) == ESP_OK) data->volume = value;
    if (nvs_get_i32(nvs, "dim_bright", &value) == ESP_OK) data->dim_brightness = value;
    if (nvs_get_i32(nvs, "dim_time", &value) == ESP_OK) data->dim_timeout = value;
    if (nvs_get_i32(nvs, "language", &value) == ESP_OK) data->language = value;
    if (nvs_get_i32(nvs, "theme", &value) == ESP_OK) data->theme = value;
    if (nvs_get_i32(nvs, "muted", &value) == ESP_OK) data->muted = value;
    if (nvs_get_i32(nvs, "screensaver", &value) == ESP_OK) data->screensaver_enabled = value;

    uint32_t favorites;
    if (nvs_get_u32(nvs, "favorites", &favorites) == ESP_OK) data->favorite_games = favorites;

    int64_t epoch;
    if (nvs_get_i64(nvs, "epoch", &epoch) == ESP_OK && epoch > 1700000000) {
        data->epoch = (time_t)epoch;
        struct timeval tv = { .tv_sec = data->epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    nvs_close(nvs);
}
