#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_codec_dev.h"
#include "lvgl.h"

#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "roundgames";

static int current_brightness = 50;
static int current_volume = 70; // audio test
static int dim_brightness = 10;
static int dim_timeout = 30;
static int language = 0;       // 0 = English, 1 = German
static int theme = 0;          // 0 = dark, 1 = light
static bool sound_muted = false;
static bool screensaver_enabled = true;
static bool screensaver_active = false;

static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_value_label = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *volume_value_label = NULL;
static lv_obj_t *dim_slider = NULL;
static lv_obj_t *dim_value_label = NULL;
static lv_obj_t *screen_saver = NULL;
static lv_timer_t *screensaver_timer = NULL;
static int inactivity_seconds = 0;
static bool gesture_registered = false;

static esp_codec_dev_handle_t speaker_codec = NULL;
static int16_t tone_buffer[2880];
static bool tone_ready = false;

static void screensaver_wake_cb(lv_event_t *e);

static void build_launcher(void);
static void build_settings_menu(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Einstellungen" : "Settings");

    static const char *symbols[] = {
        LV_SYMBOL_CHARGE, LV_SYMBOL_VOLUME_MAX,
        LV_SYMBOL_KEYBOARD, LV_SYMBOL_REFRESH,
        LV_SYMBOL_EYE_OPEN, LV_SYMBOL_FILE
    };
    static const char *names[] = {
        "Brightness", "Sound", "Language",
        "Clock", "Theme", "Info"
    };

    for (int i = 0; i < 6; i++) {
        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 66);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT,
                     38 + ((i % 2) * 205),
                     68 + ((i / 2) * 76));
        lv_obj_t *icon = lv_label_create(button);
        lv_label_set_text(icon, symbols[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_center(icon);
        lv_obj_add_event_cb(button, settings_menu_cb, LV_EVENT_CLICKED, (void *)names[i]);
    }

    add_back_button(screen, true);
}

static void build_brightness_page(void)
{
    clear_screen();
    lv_obj_t *s = lv_scr_act();
    add_title(s, language ? "Helligkeit" : "Brightness");

    lv_obj_t *n = lv_label_create(s);
    lv_label_set_text(n, "Normal");
    lv_obj_align(n, LV_ALIGN_TOP_LEFT, 42, 70);
    brightness_value_label = lv_label_create(s);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", current_brightness);
    lv_obj_align(brightness_value_label, LV_ALIGN_TOP_RIGHT, -42, 70);

    brightness_slider = lv_slider_create(s);
    lv_obj_set_width(brightness_slider, 300);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_align(brightness_slider, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *bm=lv_button_create(s); lv_obj_set_size(bm,55,42);
    lv_obj_align(bm,LV_ALIGN_TOP_LEFT,28,90);
    lv_obj_t *bml=lv_label_create(bm); lv_label_set_text(bml,"-"); lv_obj_center(bml);
    lv_obj_add_event_cb(bm,brightness_minus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *bp=lv_button_create(s); lv_obj_set_size(bp,55,42);
    lv_obj_align(bp,LV_ALIGN_TOP_RIGHT,-28,90);
    lv_obj_t *bpl=lv_label_create(bp); lv_label_set_text(bpl,"+"); lv_obj_center(bpl);
    lv_obj_add_event_cb(bp,brightness_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *d=lv_label_create(s);
    lv_label_set_text(d,language ? "Dimmen" : "Dim");
    lv_obj_align(d,LV_ALIGN_TOP_LEFT,42,145);
    dim_value_label=lv_label_create(s);
    lv_label_set_text_fmt(dim_value_label,"%d%%",dim_brightness);
    lv_obj_align(dim_value_label,LV_ALIGN_TOP_RIGHT,-42,145);

    dim_slider=lv_slider_create(s); lv_obj_set_width(dim_slider,300);
    lv_slider_set_range(dim_slider,5,50);
    lv_slider_set_value(dim_slider,dim_brightness,LV_ANIM_OFF);
    lv_obj_align(dim_slider,LV_ALIGN_TOP_MID,0,175);
    lv_obj_add_event_cb(dim_slider,dim_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *dm=lv_button_create(s); lv_obj_set_size(dm,55,42);
    lv_obj_align(dm,LV_ALIGN_TOP_LEFT,28,165);
    lv_obj_t *dml=lv_label_create(dm); lv_label_set_text(dml,"-"); lv_obj_center(dml);
    lv_obj_add_event_cb(dm,dim_minus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *dp=lv_button_create(s); lv_obj_set_size(dp,55,42);
    lv_obj_align(dp,LV_ALIGN_TOP_RIGHT,-28,165);
    lv_obj_t *dpl=lv_label_create(dp); lv_label_set_text(dpl,"+"); lv_obj_center(dpl);
    lv_obj_add_event_cb(dp,dim_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *ss=lv_button_create(s); lv_obj_set_size(ss,175,52);
    lv_obj_align(ss,LV_ALIGN_TOP_LEFT,48,225);
    lv_obj_t *ssl=lv_label_create(ss);
    lv_label_set_text_fmt(ssl,"%s: %s",language ? "Bildschirm" : "Screen",
                          screensaver_enabled ? "ON" : "OFF");
    lv_obj_center(ssl);
    lv_obj_add_event_cb(ss,screensaver_toggle_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *to=lv_button_create(s); lv_obj_set_size(to,175,52);
    lv_obj_align(to,LV_ALIGN_TOP_RIGHT,-48,225);
    lv_obj_t *tol=lv_label_create(to);
    lv_label_set_text_fmt(tol,"%s: %ds",language ? "Nach" : "After",dim_timeout);
    lv_obj_center(tol);
    lv_obj_add_event_cb(to,dim_time_cb,LV_EVENT_CLICKED,NULL);

    add_back_button(s,false);
}

static void build_volume_page(void);
static void build_language_page(void);
static void build_clock_page(void);
static void build_theme_page(void);
static void build_info_page(void);
static void show_status(const char *title, const char *message);

static void save_settings(void)
{
    nvs_handle_t nvs;
    if (nvs_open("settings", NVS_READWRITE, &nvs) != ESP_OK) return;

    nvs_set_i32(nvs, "brightness", current_brightness);
    nvs_set_i32(nvs, "volume", current_volume);
    nvs_set_i32(nvs, "dim_bright", dim_brightness);
    nvs_set_i32(nvs, "dim_time", dim_timeout);
    nvs_set_i32(nvs, "language", language);
    nvs_set_i32(nvs, "theme", theme);
    nvs_set_i32(nvs, "muted", sound_muted);
    nvs_set_i32(nvs, "screensaver", screensaver_enabled);

    time_t now;
    time(&now);
    nvs_set_i64(nvs, "epoch", (int64_t)now);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static void load_settings(void)
{
    nvs_handle_t nvs;
    if (nvs_open("settings", NVS_READONLY, &nvs) != ESP_OK) return;

    int32_t value;
    if (nvs_get_i32(nvs, "brightness", &value) == ESP_OK) current_brightness = value;
    if (nvs_get_i32(nvs, "volume", &value) == ESP_OK) current_volume = value;
    if (nvs_get_i32(nvs, "dim_bright", &value) == ESP_OK) dim_brightness = value;
    if (nvs_get_i32(nvs, "dim_time", &value) == ESP_OK) dim_timeout = value;
    if (nvs_get_i32(nvs, "language", &value) == ESP_OK) language = value;
    if (nvs_get_i32(nvs, "theme", &value) == ESP_OK) theme = value;
    if (nvs_get_i32(nvs, "muted", &value) == ESP_OK) sound_muted = value;
    if (nvs_get_i32(nvs, "screensaver", &value) == ESP_OK) screensaver_enabled = value;

    int64_t epoch;
    if (nvs_get_i64(nvs, "epoch", &epoch) == ESP_OK && epoch > 1700000000) {
        struct timeval tv = { .tv_sec = (time_t)epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    nvs_close(nvs);

    if (current_brightness < 10) current_brightness = 10;
    if (current_brightness > 100) current_brightness = 100;
    if (current_volume < 0) current_volume = 0;
    if (current_volume > 100) current_volume = 100;
    if (dim_brightness < 5) dim_brightness = 5;
    if (dim_brightness > 50) dim_brightness = 50;
    if (dim_timeout < 10) dim_timeout = 10;
    if (dim_timeout > 600) dim_timeout = 600;
}

static void apply_theme(lv_obj_t *screen)
{
    if (theme == 0) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x101014), 0);
        lv_obj_set_style_text_color(screen, lv_color_hex(0xFFFFFF), 0);
    } else {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F2F2), 0);
        lv_obj_set_style_text_color(screen, lv_color_hex(0x101014), 0);
    }
}

static void activity_reset(void)
{
    inactivity_seconds = 0;
    if (screensaver_active) {
        screensaver_active = false;
        if (screen_saver) {
            lv_obj_del(screen_saver);
            screen_saver = NULL;
        }
        bsp_display_brightness_set(current_brightness);
    }
}

static void settings_gesture_cb(lv_event_t *e);

static void clear_screen(void)
{
    activity_reset();
    lv_obj_clean(lv_scr_act());
    brightness_slider = NULL;
    brightness_value_label = NULL;
    volume_slider = NULL;
    volume_value_label = NULL;
    dim_slider = NULL;
    dim_value_label = NULL;
    apply_theme(lv_scr_act());
    if (!gesture_registered) {
        lv_obj_add_event_cb(lv_scr_act(), settings_gesture_cb, LV_EVENT_GESTURE, NULL);
        gesture_registered = true;
    }
}

static void back_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_reset();
    build_settings_menu();
}

static void generic_back_launcher_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_reset();
    build_launcher();
}

static void set_brightness(int value)
{
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    current_brightness = value;
    bsp_display_brightness_set(value);
    if (brightness_slider) lv_slider_set_value(brightness_slider, value, LV_ANIM_OFF);
    if (brightness_value_label) lv_label_set_text_fmt(brightness_value_label, "%d%%", value);
    save_settings();
    activity_reset();
}

static void brightness_slider_cb(lv_event_t *e)
{
    set_brightness(lv_slider_get_value(lv_event_get_target(e)));
}

static void brightness_minus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_brightness(current_brightness - 10);
}

static void brightness_plus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_brightness(current_brightness + 10);
}

static void audio_apply_volume(void) { if (speaker_codec) esp_codec_dev_set_out_vol(speaker_codec, sound_muted ? 0 : current_volume); }

static void set_volume(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    current_volume = value;
    if (volume_slider) lv_slider_set_value(volume_slider, value, LV_ANIM_OFF);
    if (volume_value_label) lv_label_set_text_fmt(volume_value_label, "%d%%", value);
    // Audio hardware hookup belongs to the audio service; the setting is already persistent.
    save_settings();
    activity_reset();
}

static void volume_slider_cb(lv_event_t *e)
{
    set_volume(lv_slider_get_value(lv_event_get_target(e)));
}

static void volume_minus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_volume(current_volume - 10);
}

static void volume_plus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_volume(current_volume + 10);
}

static void mute_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    sound_muted = !sound_muted;
    save_settings();
    build_volume_page();
}

static void set_dim_brightness(int value)
{
    if (value < 5) value = 5;
    if (value > 50) value = 50;
    dim_brightness = value;
    if (dim_slider) lv_slider_set_value(dim_slider, value, LV_ANIM_OFF);
    if (dim_value_label) lv_label_set_text_fmt(dim_value_label, "%d%%", value);
    save_settings();
    activity_reset();
}

static void dim_slider_cb(lv_event_t *e)
{
    set_dim_brightness(lv_slider_get_value(lv_event_get_target(e)));
}

static void dim_minus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_dim_brightness(dim_brightness - 5);
}

static void dim_plus_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    set_dim_brightness(dim_brightness + 5);
}

static void dim_time_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    dim_timeout += 10;
    if (dim_timeout > 120) dim_timeout = 10;
    save_settings();
    build_dimming_page();
}

static void language_cb(lv_event_t *e)
{
    language = (int)(intptr_t)lv_event_get_user_data(e);
    save_settings();
    build_language_page();
}

static void theme_cb(lv_event_t *e)
{
    theme = (int)(intptr_t)lv_event_get_user_data(e);
    save_settings();
    build_theme_page();
}

static void screensaver_toggle_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    screensaver_enabled = !screensaver_enabled;
    save_settings();
    build_screensaver_page();
}

static void clock_adjust_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    time_t now;
    time(&now);
    now += delta;
    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    save_settings();
    build_clock_page();
}

static void settings_gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM) {
        lv_indev_wait_release(lv_indev_active());
        build_settings_menu();
    }
}

static void settings_menu_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    if (!name) return;

    if (!strcmp(name, "Brightness")) build_brightness_page();
    else if (!strcmp(name, "Sound")) build_volume_page();
    else if (!strcmp(name, "Dimming")) build_dimming_page();
    else if (!strcmp(name, "Language")) build_language_page();
    else if (!strcmp(name, "Clock")) build_clock_page();
    else if (!strcmp(name, "Screensaver")) build_screensaver_page();
    else if (!strcmp(name, "Theme")) build_theme_page();
    else if (!strcmp(name, "Info")) build_info_page();
}

static void add_back_button(lv_obj_t *screen, bool to_launcher)
{
    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 90, 44);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *icon = lv_label_create(back);
    lv_label_set_text(icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_center(icon);
    lv_obj_add_event_cb(back, to_launcher ? generic_back_launcher_cb : back_button_cb,
                        LV_EVENT_CLICKED, NULL);
}

static void add_title(lv_obj_t *screen, const char *text)
{
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, text);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
}

static void build_settings_menu(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Einstellungen" : "Settings");

    static const char *symbols[] = {
        LV_SYMBOL_IMAGE, LV_SYMBOL_VOLUME_MAX,
        LV_SYMBOL_EYE_CLOSE, LV_SYMBOL_SETTINGS,
        LV_SYMBOL_SETTINGS, LV_SYMBOL_IMAGE,
        LV_SYMBOL_EDIT, LV_SYMBOL_WIFI
    };
    static const char *names[] = {
        "Brightness", "Sound", "Dimming", "Language",
        "Clock", "Screensaver", "Theme", "Info"
    };

    for (int i = 0; i < 8; i++) {
        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 58);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT,
                     38 + ((i % 2) * 205),
                     68 + ((i / 2) * 62));
        lv_obj_t *icon = lv_label_create(button);
        lv_label_set_text(icon, symbols[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_center(icon);
        lv_obj_add_event_cb(button, settings_menu_cb, LV_EVENT_CLICKED, (void *)names[i]);
    }

    add_back_button(screen, true);
}

static void build_brightness_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Helligkeit" : "Brightness");

    brightness_value_label = lv_label_create(screen);
    lv_label_set_text_fmt(brightness_value_label, "%d%%", current_brightness);
    lv_obj_set_style_text_font(brightness_value_label, &lv_font_montserrat_24, 0);
    lv_obj_align(brightness_value_label, LV_ALIGN_CENTER, 0, -55);

    brightness_slider = lv_slider_create(screen);
    lv_obj_set_width(brightness_slider, 330);
    lv_slider_set_range(brightness_slider, 10, 100);
    lv_slider_set_value(brightness_slider, current_brightness, LV_ANIM_OFF);
    lv_obj_align(brightness_slider, LV_ALIGN_CENTER, 0, 5);
    lv_obj_add_event_cb(brightness_slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *minus = lv_button_create(screen);
    lv_obj_set_size(minus, 90, 64);
    lv_obj_align(minus, LV_ALIGN_CENTER, -120, 85);
    lv_obj_t *ml = lv_label_create(minus); lv_label_set_text(ml, "-"); lv_obj_center(ml);
    lv_obj_add_event_cb(minus, brightness_minus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = lv_button_create(screen);
    lv_obj_set_size(plus, 90, 64);
    lv_obj_align(plus, LV_ALIGN_CENTER, 120, 85);
    lv_obj_t *pl = lv_label_create(plus); lv_label_set_text(pl, "+"); lv_obj_center(pl);
    lv_obj_add_event_cb(plus, brightness_plus_cb, LV_EVENT_CLICKED, NULL);

    add_back_button(screen, false);
}

static void build_volume_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Lautstärke" : "Sound");

    volume_value_label = lv_label_create(screen);
    lv_label_set_text_fmt(volume_value_label, "%d%%", current_volume);
    lv_obj_set_style_text_font(volume_value_label, &lv_font_montserrat_24, 0);
    lv_obj_align(volume_value_label, LV_ALIGN_CENTER, 0, -55);

    volume_slider = lv_slider_create(screen);
    lv_obj_set_width(volume_slider, 330);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, current_volume, LV_ANIM_OFF);
    lv_obj_align(volume_slider, LV_ALIGN_CENTER, 0, 5);
    lv_obj_add_event_cb(volume_slider, volume_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *minus = lv_button_create(screen); lv_obj_set_size(minus, 90, 64);
    lv_obj_align(minus, LV_ALIGN_CENTER, -120, 85);
    lv_obj_t *ml = lv_label_create(minus); lv_label_set_text(ml, "-"); lv_obj_center(ml);
    lv_obj_add_event_cb(minus, volume_minus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = lv_button_create(screen); lv_obj_set_size(plus, 90, 64);
    lv_obj_align(plus, LV_ALIGN_CENTER, 120, 85);
    lv_obj_t *pl = lv_label_create(plus); lv_label_set_text(pl, "+"); lv_obj_center(pl);
    lv_obj_add_event_cb(plus, volume_plus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *mute = lv_button_create(screen); lv_obj_set_size(mute, 160, 48);
    lv_obj_align(mute, LV_ALIGN_CENTER, 0, 150);
    lv_obj_t *mt = lv_label_create(mute);
    lv_label_set_text(mt, sound_muted ? "Unmute" : "Mute");
    lv_obj_center(mt);
    lv_obj_add_event_cb(mute, mute_cb, LV_EVENT_CLICKED, NULL);

    add_back_button(screen, false);
}

static void build_dimming_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Dimmung" : "Dimming");

    dim_value_label = lv_label_create(screen);
    lv_label_set_text_fmt(dim_value_label, "%d%%", dim_brightness);
    lv_obj_set_style_text_font(dim_value_label, &lv_font_montserrat_24, 0);
    lv_obj_align(dim_value_label, LV_ALIGN_CENTER, 0, -65);

    dim_slider = lv_slider_create(screen);
    lv_obj_set_width(dim_slider, 330);
    lv_slider_set_range(dim_slider, 5, 50);
    lv_slider_set_value(dim_slider, dim_brightness, LV_ANIM_OFF);
    lv_obj_align(dim_slider, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(dim_slider, dim_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *minus = lv_button_create(screen); lv_obj_set_size(minus, 80, 55);
    lv_obj_align(minus, LV_ALIGN_CENTER, -115, 70);
    lv_obj_t *ml = lv_label_create(minus); lv_label_set_text(ml, "-"); lv_obj_center(ml);
    lv_obj_add_event_cb(minus, dim_minus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *plus = lv_button_create(screen); lv_obj_set_size(plus, 80, 55);
    lv_obj_align(plus, LV_ALIGN_CENTER, 115, 70);
    lv_obj_t *pl = lv_label_create(plus); lv_label_set_text(pl, "+"); lv_obj_center(pl);
    lv_obj_add_event_cb(plus, dim_plus_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *time = lv_button_create(screen); lv_obj_set_size(time, 240, 48);
    lv_obj_align(time, LV_ALIGN_CENTER, 0, 135);
    lv_obj_t *tl = lv_label_create(time);
    lv_label_set_text_fmt(tl, language ? "Zeit: %ds" : "Timeout: %ds", dim_timeout);
    lv_obj_center(tl);
    lv_obj_add_event_cb(time, dim_time_cb, LV_EVENT_CLICKED, NULL);

    add_back_button(screen, false);
}

static void build_language_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, "Language / Sprache");

    lv_obj_t *de = lv_button_create(screen); lv_obj_set_size(de, 180, 70);
    lv_obj_align(de, LV_ALIGN_CENTER, -100, 0);
    lv_obj_t *dt = lv_label_create(de); lv_label_set_text(dt, "Deutsch"); lv_obj_center(dt);
    lv_obj_add_event_cb(de, language_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    lv_obj_t *en = lv_button_create(screen); lv_obj_set_size(en, 180, 70);
    lv_obj_align(en, LV_ALIGN_CENTER, 100, 0);
    lv_obj_t *et = lv_label_create(en); lv_label_set_text(et, "English"); lv_obj_center(et);
    lv_obj_add_event_cb(en, language_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);

    add_back_button(screen, false);
}

static void build_clock_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Uhr & Datum" : "Clock & Date");

    time_t now; time(&now);
    struct tm tm_now; localtime_r(&now, &tm_now);

    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S\\n%d.%m.%Y", &tm_now);

    lv_obj_t *clock = lv_label_create(screen);
    lv_label_set_text(clock, buf);
    lv_obj_set_style_text_font(clock, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(clock, LV_ALIGN_CENTER, 0, -45);

    lv_obj_t *minus = lv_button_create(screen); lv_obj_set_size(minus, 150, 55);
    lv_obj_align(minus, LV_ALIGN_CENTER, -90, 65);
    lv_obj_t *ml = lv_label_create(minus); lv_label_set_text(ml, "- 1 h"); lv_obj_center(ml);
    lv_obj_add_event_cb(minus, clock_adjust_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-3600);

    lv_obj_t *plus = lv_button_create(screen); lv_obj_set_size(plus, 150, 55);
    lv_obj_align(plus, LV_ALIGN_CENTER, 90, 65);
    lv_obj_t *pl = lv_label_create(plus); lv_label_set_text(pl, "+ 1 h"); lv_obj_center(pl);
    lv_obj_add_event_cb(plus, clock_adjust_cb, LV_EVENT_CLICKED, (void *)(intptr_t)3600);

    lv_obj_t *day = lv_button_create(screen); lv_obj_set_size(day, 240, 48);
    lv_obj_align(day, LV_ALIGN_CENTER, 0, 130);
    lv_obj_t *dl = lv_label_create(day); lv_label_set_text(dl, "+ 1 day"); lv_obj_center(dl);
    lv_obj_add_event_cb(day, clock_adjust_cb, LV_EVENT_CLICKED, (void *)(intptr_t)86400);

    add_back_button(screen, false);
}

static void build_screensaver_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Screensaver" : "Screensaver");

    lv_obj_t *state = lv_button_create(screen);
    lv_obj_set_size(state, 260, 70);
    lv_obj_align(state, LV_ALIGN_CENTER, 0, -15);
    lv_obj_t *sl = lv_label_create(state);
    lv_label_set_text(sl, screensaver_enabled ? "ON" : "OFF");
    lv_obj_center(sl);
    lv_obj_add_event_cb(state, screensaver_toggle_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text_fmt(info, language ? "Uhr nach %ds" : "Clock after %ds", dim_timeout);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 55);

    add_back_button(screen, false);
}

static void build_theme_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, language ? "Theme" : "Theme");

    lv_obj_t *dark = lv_button_create(screen); lv_obj_set_size(dark, 170, 70);
    lv_obj_align(dark, LV_ALIGN_CENTER, -95, 0);
    lv_obj_t *dt = lv_label_create(dark); lv_label_set_text(dt, "Dark"); lv_obj_center(dt);
    lv_obj_add_event_cb(dark, theme_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);

    lv_obj_t *light = lv_button_create(screen); lv_obj_set_size(light, 170, 70);
    lv_obj_align(light, LV_ALIGN_CENTER, 95, 0);
    lv_obj_t *lt = lv_label_create(light); lv_label_set_text(lt, "Light"); lv_obj_center(lt);
    lv_obj_add_event_cb(light, theme_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);

    add_back_button(screen, false);
}

static void build_info_page(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, "RoundGames");

    lv_obj_t *info = lv_label_create(screen);
    lv_label_set_text(info,
        "Firmware: Phase 3\\n"
        "Board: ESP32-S3 Touch AMOLED 1.75\\n"
        "Display: 466 x 466\\n"
        "Settings: NVS persistent");
    lv_obj_set_style_text_font(info, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_align(info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(info, LV_ALIGN_CENTER, 0, 0);

    add_back_button(screen, false);
}

static void screensaver_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!screensaver_enabled || screensaver_active) return;

    inactivity_seconds++;
    if (inactivity_seconds < dim_timeout) return;

    screensaver_active = true;
    bsp_display_brightness_set(dim_brightness);

    lv_obj_t *screen = lv_scr_act();
    screen_saver = lv_obj_create(screen);
    lv_obj_set_size(screen_saver, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(screen_saver, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(screen_saver, 0, 0);
    lv_obj_clear_flag(screen_saver, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen_saver, screensaver_wake_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clock = lv_label_create(screen_saver);
    time_t now; time(&now);
    struct tm tm_now; localtime_r(&now, &tm_now);
    char buf[32];
    strftime(buf, sizeof(buf), "%H:%M\\n%d.%m.", &tm_now);
    lv_label_set_text(clock, buf);
    lv_obj_set_style_text_color(clock, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(clock, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(clock);
}

static void screensaver_wake_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    activity_reset();
}

static void launcher_button_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    activity_reset();
    if (name && !strcmp(name, "Tic-Tac-Toe")) {
        show_status("Tic-Tac-Toe", "Game module will be added next.");
    }
}

static void build_launcher(void)
{
    clear_screen();

    lv_obj_t *screen = lv_scr_act();
    add_title(screen, "RoundGames");

    lv_obj_t *subtitle = lv_label_create(screen);
    lv_label_set_text(subtitle, "Phase 3 - system settings");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_16, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_MID, 0, 70);

    lv_obj_t *game = lv_button_create(screen);
    lv_obj_set_size(game, 300, 78);
    lv_obj_align(game, LV_ALIGN_CENTER, 0, -35);

    lv_obj_t *game_label = lv_label_create(game);
    lv_label_set_text(game_label, "Tic-Tac-Toe");
    lv_obj_set_style_text_font(game_label, &lv_font_montserrat_22, 0);
    lv_obj_center(game_label);
    lv_obj_add_event_cb(game, launcher_button_cb, LV_EVENT_CLICKED, (void *)"Tic-Tac-Toe");

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, "Swipe down for settings");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -35);

}

static void show_status(const char *title, const char *message)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, title);

    lv_obj_t *msg = lv_label_create(screen);
    lv_label_set_text(msg, message);
    lv_obj_set_width(msg, 380);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -10);

    add_back_button(screen, true);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting RoundGames Phase 3");

    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    load_settings();
    bsp_display_start();
    bsp_display_brightness_set(current_brightness);

    bsp_display_lock(-1);
    build_launcher();
    screensaver_timer = lv_timer_create(screensaver_tick, 1000, NULL);
    bsp_display_unlock();

    ESP_LOGI(TAG, "RoundGames Phase 3 UI ready");
}
