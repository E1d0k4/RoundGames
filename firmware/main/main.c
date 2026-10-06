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
static void build_brightness_page(void);
static void build_settings_menu(void)
{
    clear_screen();
    brightness_slider = NULL;
    brightness_value_label = NULL;

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    static const char *symbols[] = {
        LV_SYMBOL_IMAGE, LV_SYMBOL_VOLUME_MAX,
        LV_SYMBOL_EYE_CLOSE, LV_SYMBOL_SETTINGS,
        LV_SYMBOL_SETTINGS, LV_SYMBOL_IMAGE,
        LV_SYMBOL_EDIT, LV_SYMBOL_WIFI
    };

    static const char *labels[] = {
        "Brightness", "Sound",
        "Dimming", "Language",
        "Clock", "Screensaver",
        "Theme", "Info"
    };

    static const char *names[] = {
        "Brightness", "Sound",
        "Dimming", "Language",
        "Clock & Date", "Screensaver",
        "Theme", "Information"
    };

    for (int i = 0; i < 8; i++) {
        int column = i % 2;
        int row = i / 2;

        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 58);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT,
                     38 + (column * 205),
                     68 + (row * 62));

        lv_obj_t *icon = lv_label_create(button);
        lv_label_set_text(icon, symbols[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 18, 0);

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, labels[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
        lv_obj_set_width(label, 125);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(label, LV_ALIGN_RIGHT_MID, -8, 0);

        lv_obj_add_event_cb(button, settings_menu_cb, LV_EVENT_CLICKED,
                            (void *)names[i]);
    }

    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 180, 42);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -10);

    lv_obj_t *back_icon = lv_label_create(back);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_18, 0);
    lv_obj_align(back_icon, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_16, 0);
    lv_obj_align(back_label, LV_ALIGN_CENTER, 10, 0);

    lv_obj_add_event_cb(back, back_button_cb, LV_EVENT_CLICKED, NULL);
}

static void build_brightness_page(void)
{
    clear_screen();
    brightness_slider = NULL;
    brightness_value_label = NULL;

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Brightness");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 55);

    brightness_value_label = lv_label_create(screen);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", current_brightness);
    lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_24, 0);
    lv_obj_align(brightness_value_label, LV_ALIGN_CENTER, 0, -55);

    brightness_slider = lv_slider_create(screen);
    lv_obj_t *slider = brightness_slider;
    lv_obj_set_width(slider, 330);
    lv_slider_set_range(slider, 10, 100);
    lv_slider_set_value(slider, current_brightness, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 5);
    lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *minus = lv_button_create(screen);
    lv_obj_set_size(minus, 90, 64);
    lv_obj_align(minus, LV_ALIGN_CENTER, -120, 85);
    lv_obj_t *minus_label = lv_label_create(minus);
    lv_label_set_text(minus_label, "-");
    lv_obj_set_style_text_font(minus_label, &lv_font_montserrat_24, 0);
    lv_obj_center(minus_label);
    lv_obj_add_event_cb(minus, brightness_minus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = lv_button_create(screen);
    lv_obj_set_size(plus, 90, 64);
    lv_obj_align(plus, LV_ALIGN_CENTER, 120, 85);
    lv_obj_t *plus_label = lv_label_create(plus);
    lv_label_set_text(plus_label, "+");
    lv_obj_set_style_text_font(plus_label, &lv_font_montserrat_24, 0);
    lv_obj_center(plus_label);
    lv_obj_add_event_cb(plus, brightness_plus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 180, 60);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, back_button_cb, LV_EVENT_CLICKED, NULL);
}

static void show_status(const char *title, const char *message)
{
    clear_screen();

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title_label = lv_label_create(screen);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
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
        build_settings_menu();
    }
}

static void build_launcher(void)
{
    clear_screen();

    lv_obj_t *screen = lv_scr_act();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "RoundGames");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
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

    // Start at a comfortable brightness instead of full brightness.
    bsp_display_brightness_set(current_brightness);

    bsp_display_lock(-1);
    build_launcher();
    bsp_display_unlock();

    ESP_LOGI(TAG, "RoundGames Phase 1 UI ready");
}
