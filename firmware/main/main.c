#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "lvgl.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "roundgames";

static void build_launcher(void);

static void clear_screen(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
}

static void back_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    build_launcher();
}

static void show_status(const char *title, const char *message)
{
    clear_screen();

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_26, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 70);

    lv_obj_t *message_label = lv_label_create(screen);
    lv_label_set_text(message_label, message);
    lv_obj_set_style_text_font(message_label, &lv_font_montserrat_18, 0);
    lv_obj_set_width(message_label, 380);
    lv_obj_set_style_text_align(message_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(message_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 180, 64);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -55);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    lv_obj_add_event_cb(back, back_button_cb, LV_EVENT_CLICKED, NULL);
}

static void launcher_button_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    if (name == NULL) {
        return;
    }

    if (strcmp(name, "Tic-Tac-Toe") == 0) {
        show_status("Tic-Tac-Toe", "Game module will be added next.");
    } else {
        show_status("Settings", "System settings UI will be added next.");
    }
}

static void build_launcher(void)
{
    clear_screen();

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "RoundGames");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 55);

    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Phase 1 - display + touch test");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 95);

    lv_obj_t *game = lv_button_create(screen);
    lv_obj_set_size(game, 300, 78);
    lv_obj_align(game, LV_ALIGN_CENTER, 0, -35);

    lv_obj_t *game_label = lv_label_create(game);
    lv_label_set_text(game_label, "Tic-Tac-Toe");
    lv_obj_set_style_text_font(game_label, &lv_font_montserrat_22, 0);
    lv_obj_center(game_label);
    lv_obj_add_event_cb(game, launcher_button_cb, LV_EVENT_CLICKED, (void *)"Tic-Tac-Toe");

    lv_obj_t *settings = lv_button_create(screen);
    lv_obj_set_size(settings, 300, 78);
    lv_obj_align(settings, LV_ALIGN_CENTER, 0, 65);

    lv_obj_t *settings_label = lv_label_create(settings);
    lv_label_set_text(settings_label, "Settings");
    lv_obj_set_style_text_font(settings_label, &lv_font_montserrat_22, 0);
    lv_obj_center(settings_label);
    lv_obj_add_event_cb(settings, launcher_button_cb, LV_EVENT_CLICKED, (void *)"Settings");

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Touch a button to test the touchscreen.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -35);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RoundGames");

    bsp_display_start();

    bsp_display_lock(-1);
    build_launcher();
    bsp_display_unlock();

    ESP_LOGI(TAG, "RoundGames Phase 1 UI ready");
}
