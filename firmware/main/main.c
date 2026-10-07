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
static int current_volume = 70;
static int dim_brightness = 10;
static int dim_timeout = 30;
static int language = 0;
static int theme = 0;
static bool sound_muted = false;
static bool screensaver_enabled = true;
static bool screensaver_active = false;
static bool launcher_active = true;
static int launcher_page = 0;
static uint32_t favorite_games = 1u;

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

static void screensaver_wake_cb(lv_event_t *e);
static void build_launcher(void);
static void build_settings_menu(void);
static void build_brightness_page(void);
static void build_volume_page(void);
static void build_language_page(void);
static void build_clock_page(void);
static void build_theme_page(void);
static void build_info_page(void);
static void show_status(const char *title, const char *message);

static const char *tr(const char *en, const char *de) { return language ? de : en; }

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
    nvs_set_u32(nvs, "favorites", favorite_games);
    time_t now; time(&now);
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
    uint32_t favorites;
    if (nvs_get_u32(nvs, "favorites", &favorites) == ESP_OK) favorite_games = favorites;
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
    favorite_games |= 1u;
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
static void build_games_menu(void);

static void clear_screen(void)
{
    launcher_active = false;
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

static void back_button_cb(lv_event_t *e) { LV_UNUSED(e); activity_reset(); build_settings_menu(); }
static void generic_back_launcher_cb(lv_event_t *e) { LV_UNUSED(e); activity_reset(); build_launcher(); }

static void set_brightness(int value)
{
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    current_brightness = value; bsp_display_brightness_set(value);
    if (brightness_slider) lv_slider_set_value(brightness_slider, value, LV_ANIM_OFF);
    if (brightness_value_label) lv_label_set_text_fmt(brightness_value_label, "%d%%", value);
    save_settings(); activity_reset();
}
static void brightness_slider_cb(lv_event_t *e) { set_brightness(lv_slider_get_value(lv_event_get_target(e))); }
static void brightness_minus_cb(lv_event_t *e) { LV_UNUSED(e); set_brightness(current_brightness - 10); }
static void brightness_plus_cb(lv_event_t *e) { LV_UNUSED(e); set_brightness(current_brightness + 10); }

static void set_volume(int value)
{
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    current_volume = value;
    if (volume_slider) lv_slider_set_value(volume_slider, value, LV_ANIM_OFF);
    if (volume_value_label) lv_label_set_text_fmt(volume_value_label, "%d%%", value);
    if (speaker_codec) esp_codec_dev_set_out_vol(speaker_codec, sound_muted ? 0 : current_volume);
    save_settings(); activity_reset();
}
static void volume_slider_cb(lv_event_t *e) { set_volume(lv_slider_get_value(lv_event_get_target(e))); }
static void volume_minus_cb(lv_event_t *e) { LV_UNUSED(e); set_volume(current_volume - 10); }
static void volume_plus_cb(lv_event_t *e) { LV_UNUSED(e); set_volume(current_volume + 10); }

static void mute_cb(lv_event_t *e) { LV_UNUSED(e); sound_muted = !sound_muted; if (speaker_codec) esp_codec_dev_set_out_vol(speaker_codec, sound_muted ? 0 : current_volume); save_settings(); build_volume_page(); }

static void set_dim_brightness(int value)
{
    if (value < 5) value = 5;
    if (value > 50) value = 50;
    dim_brightness = value;
    if (dim_slider) lv_slider_set_value(dim_slider, value, LV_ANIM_OFF);
    if (dim_value_label) lv_label_set_text_fmt(dim_value_label, "%d%%", value);
    save_settings(); activity_reset();
}
static void dim_slider_cb(lv_event_t *e) { set_dim_brightness(lv_slider_get_value(lv_event_get_target(e))); }
static void dim_minus_cb(lv_event_t *e) { LV_UNUSED(e); set_dim_brightness(dim_brightness - 5); }
static void dim_plus_cb(lv_event_t *e) { LV_UNUSED(e); set_dim_brightness(dim_brightness + 5); }
static void dim_time_cb(lv_event_t *e) { LV_UNUSED(e); dim_timeout += 10; if (dim_timeout > 120) dim_timeout = 10; save_settings(); build_brightness_page(); }

static void language_cb(lv_event_t *e) { language = (int)(intptr_t)lv_event_get_user_data(e); save_settings(); build_language_page(); }
static void theme_cb(lv_event_t *e) { theme = (int)(intptr_t)lv_event_get_user_data(e); save_settings(); build_theme_page(); }
static void screensaver_toggle_cb(lv_event_t *e) { LV_UNUSED(e); screensaver_enabled = !screensaver_enabled; save_settings(); build_brightness_page(); }

static void clock_adjust_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    time_t now; time(&now); now += delta;
    struct timeval tv = { .tv_sec = now, .tv_usec = 0 }; settimeofday(&tv, NULL);
    save_settings(); build_clock_page();
}

static void settings_gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    lv_indev_wait_release(lv_indev_active());

    if (launcher_active) {
        if (dir == LV_DIR_BOTTOM) {
            build_settings_menu();
        } else if (dir == LV_DIR_TOP) {
            build_games_menu();
        } else if (dir == LV_DIR_LEFT) {
            launcher_page++;
            build_launcher();
        } else if (dir == LV_DIR_RIGHT) {
            if (launcher_page > 0) launcher_page--;
            build_launcher();
        }
    } else if (dir == LV_DIR_BOTTOM) {
        build_settings_menu();
    }
}

static void settings_menu_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    if (!name) return;
    if (!strcmp(name, "Brightness")) build_brightness_page();
    else if (!strcmp(name, "Sound")) build_volume_page();
    else if (!strcmp(name, "Language")) build_language_page();
    else if (!strcmp(name, "Clock")) build_clock_page();
    else if (!strcmp(name, "Theme")) build_theme_page();
    else if (!strcmp(name, "Info")) build_info_page();
}

static void add_back_button(lv_obj_t *screen, bool to_launcher)
{
    lv_obj_t *back = lv_button_create(screen); lv_obj_set_size(back, 90, 44); lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *icon = lv_label_create(back); lv_label_set_text(icon, LV_SYMBOL_LEFT); lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0); lv_obj_center(icon);
    lv_obj_add_event_cb(back, to_launcher ? generic_back_launcher_cb : back_button_cb, LV_EVENT_CLICKED, NULL);
}

static void add_title(lv_obj_t *screen, const char *text)
{
    lv_obj_t *title = lv_label_create(screen); lv_label_set_text(title, text); lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0); lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 35);
}

static void style_option_button(lv_obj_t *button, bool selected)
{
    lv_obj_set_style_bg_color(button, selected ? lv_color_hex(0x20A050) : lv_color_hex(0x30343B), LV_PART_MAIN);
}

static void add_language_icon(lv_obj_t *button)
{
    lv_obj_t *a1 = lv_label_create(button);
    lv_label_set_text(a1, "A");
    lv_obj_set_style_text_font(a1, &lv_font_montserrat_20, 0);
    lv_obj_align(a1, LV_ALIGN_CENTER, -38, 0);

    lv_obj_t *arrow1 = lv_label_create(button);
    lv_label_set_text(arrow1, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow1, &lv_font_montserrat_18, 0);
    lv_obj_align(arrow1, LV_ALIGN_CENTER, -12, 0);

    lv_obj_t *arrow2 = lv_label_create(button);
    lv_label_set_text(arrow2, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow2, &lv_font_montserrat_18, 0);
    lv_obj_align(arrow2, LV_ALIGN_CENTER, 12, 0);

    lv_obj_t *a2 = lv_label_create(button);
    lv_label_set_text(a2, "A");
    lv_obj_set_style_text_font(a2, &lv_font_montserrat_20, 0);
    lv_obj_align(a2, LV_ALIGN_CENTER, 38, 0);
}

static void build_settings_menu(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, tr("Settings", "Einstellungen"));

    static const char *symbols[] = {
        LV_SYMBOL_SETTINGS, LV_SYMBOL_VOLUME_MAX,
        "A>A", LV_SYMBOL_REFRESH,
        LV_SYMBOL_IMAGE, "i"
    };
    static const char *names[] = {
        "Brightness", "Sound", "Language",
        "Clock", "Theme", "Info"
    };

    for (int i=0;i<6;i++) {
        lv_obj_t *button=lv_button_create(screen);
        lv_obj_set_size(button,185,66);
        lv_obj_align(button,LV_ALIGN_TOP_LEFT,38+((i%2)*205),68+((i/2)*76));
        style_option_button(button, false);

        if (i == 2) {
            add_language_icon(button);
        } else {
            lv_obj_t *icon=lv_label_create(button);
            lv_label_set_text(icon,symbols[i]);
            lv_obj_set_style_text_font(icon,&lv_font_montserrat_24,0);
            lv_obj_center(icon);
        }
        lv_obj_add_event_cb(button,settings_menu_cb,LV_EVENT_CLICKED,(void*)names[i]);
    }
    add_back_button(screen,true);
}

static void build_brightness_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Brightness","Helligkeit"));

    lv_obj_t *n=lv_label_create(s); lv_label_set_text(n,"Normal"); lv_obj_align(n,LV_ALIGN_TOP_LEFT,42,70);
    brightness_value_label=lv_label_create(s); lv_label_set_text_fmt(brightness_value_label,"%d%%",current_brightness); lv_obj_align(brightness_value_label,LV_ALIGN_TOP_RIGHT,-42,70);
    brightness_slider=lv_slider_create(s); lv_obj_set_width(brightness_slider,300); lv_slider_set_range(brightness_slider,10,100); lv_slider_set_value(brightness_slider,current_brightness,LV_ANIM_OFF); lv_obj_align(brightness_slider,LV_ALIGN_TOP_MID,0,112); lv_obj_add_event_cb(brightness_slider,brightness_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *bm=lv_button_create(s); lv_obj_set_size(bm,55,42); lv_obj_align(bm,LV_ALIGN_TOP_LEFT,28,102); style_option_button(bm,false); lv_obj_t *bml=lv_label_create(bm); lv_label_set_text(bml,"-"); lv_obj_center(bml); lv_obj_add_event_cb(bm,brightness_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *bp=lv_button_create(s); lv_obj_set_size(bp,55,42); lv_obj_align(bp,LV_ALIGN_TOP_RIGHT,-28,102); style_option_button(bp,false); lv_obj_t *bpl=lv_label_create(bp); lv_label_set_text(bpl,"+"); lv_obj_center(bpl); lv_obj_add_event_cb(bp,brightness_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *d=lv_label_create(s); lv_label_set_text(d,tr("Dim","Dimmen")); lv_obj_align(d,LV_ALIGN_TOP_LEFT,42,172);
    dim_value_label=lv_label_create(s); lv_label_set_text_fmt(dim_value_label,"%d%%",dim_brightness); lv_obj_align(dim_value_label,LV_ALIGN_TOP_RIGHT,-42,172);
    dim_slider=lv_slider_create(s); lv_obj_set_width(dim_slider,300); lv_slider_set_range(dim_slider,5,50); lv_slider_set_value(dim_slider,dim_brightness,LV_ANIM_OFF); lv_obj_align(dim_slider,LV_ALIGN_TOP_MID,0,194); lv_obj_add_event_cb(dim_slider,dim_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *dm=lv_button_create(s); lv_obj_set_size(dm,55,42); lv_obj_align(dm,LV_ALIGN_TOP_LEFT,28,184); style_option_button(dm,false); lv_obj_t *dml=lv_label_create(dm); lv_label_set_text(dml,"-"); lv_obj_center(dml); lv_obj_add_event_cb(dm,dim_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *dp=lv_button_create(s); lv_obj_set_size(dp,55,42); lv_obj_align(dp,LV_ALIGN_TOP_RIGHT,-28,184); style_option_button(dp,false); lv_obj_t *dpl=lv_label_create(dp); lv_label_set_text(dpl,"+"); lv_obj_center(dpl); lv_obj_add_event_cb(dp,dim_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *ss=lv_button_create(s); lv_obj_set_size(ss,175,52); lv_obj_align(ss,LV_ALIGN_TOP_LEFT,48,260); style_option_button(ss,screensaver_enabled);
    lv_obj_t *ssl=lv_label_create(ss); lv_label_set_text_fmt(ssl,"%s: %s",tr("Screen","Bildschirm"),screensaver_enabled ? "ON":"OFF"); lv_obj_center(ssl); lv_obj_add_event_cb(ss,screensaver_toggle_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *to=lv_button_create(s); lv_obj_set_size(to,175,52); lv_obj_align(to,LV_ALIGN_TOP_RIGHT,-48,260); style_option_button(to,false);
    lv_obj_t *tol=lv_label_create(to); lv_label_set_text_fmt(tol,"%s: %ds",tr("After","Nach"),dim_timeout); lv_obj_center(tol); lv_obj_add_event_cb(to,dim_time_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void play_test_tone(void)
{
    if (sound_muted || current_volume <= 0) return;

    if (!speaker_codec) {
        speaker_codec = bsp_audio_codec_speaker_init();
        if (!speaker_codec) {
            ESP_LOGE(TAG, "Speaker codec init failed");
            return;
        }
    }

    esp_codec_dev_set_out_vol(speaker_codec, current_volume);

    static int16_t tone[22050];
    static bool ready = false;
    if (!ready) {
        for (int i = 0; i < 22050; i++) {
            int16_t sample = ((i % 25) < 12) ? 9000 : -9000;
            tone[i] = sample;
        }
        ready = true;
    }

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = 22050,
        .channel = 1,
        .bits_per_sample = 16
    };

    if (esp_codec_dev_open(speaker_codec, &fs) != ESP_OK) {
        ESP_LOGE(TAG, "Speaker open failed");
        return;
    }

    esp_err_t write_ret = esp_codec_dev_write(speaker_codec, tone, sizeof(tone));
    if (write_ret != ESP_OK) {
        ESP_LOGE(TAG, "Speaker write failed: %s", esp_err_to_name(write_ret));
    }
    esp_codec_dev_close(speaker_codec);
}

static void volume_test_cb(lv_event_t *e) { LV_UNUSED(e); play_test_tone(); }

static void build_volume_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Sound","Ton"));
    volume_value_label=lv_label_create(s); lv_label_set_text_fmt(volume_value_label,"%d%%",current_volume); lv_obj_set_style_text_font(volume_value_label,&lv_font_montserrat_24,0); lv_obj_align(volume_value_label,LV_ALIGN_CENTER,0,-65);
    volume_slider=lv_slider_create(s); lv_obj_set_width(volume_slider,300); lv_slider_set_range(volume_slider,0,100); lv_slider_set_value(volume_slider,current_volume,LV_ANIM_OFF); lv_obj_align(volume_slider,LV_ALIGN_CENTER,0,-5); lv_obj_add_event_cb(volume_slider,volume_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *m=lv_button_create(s); lv_obj_set_size(m,55,42); lv_obj_align(m,LV_ALIGN_CENTER,-180,-5); style_option_button(m,false); lv_obj_t *ml=lv_label_create(m); lv_label_set_text(ml,"-"); lv_obj_center(ml); lv_obj_add_event_cb(m,volume_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *p=lv_button_create(s); lv_obj_set_size(p,55,42); lv_obj_align(p,LV_ALIGN_CENTER,180,-5); style_option_button(p,false); lv_obj_t *pl=lv_label_create(p); lv_label_set_text(pl,"+"); lv_obj_center(pl); lv_obj_add_event_cb(p,volume_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *t=lv_button_create(s); lv_obj_set_size(t,170,50); lv_obj_align(t,LV_ALIGN_CENTER,0,70); style_option_button(t,false); lv_obj_t *tl=lv_label_create(t); lv_label_set_text(tl,tr("Test tone","Testton")); lv_obj_center(tl); lv_obj_add_event_cb(t,volume_test_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *mu=lv_button_create(s); lv_obj_set_size(mu,170,50); lv_obj_align(mu,LV_ALIGN_CENTER,0,130); style_option_button(mu,sound_muted); lv_obj_t *mul=lv_label_create(mu); lv_label_set_text(mul,sound_muted ? tr("Unmute","Ton an") : tr("Mute","Stumm")); lv_obj_center(mul); lv_obj_add_event_cb(mu,mute_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void build_language_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Language","Sprache"));
    lv_obj_t *de=lv_button_create(s); lv_obj_set_size(de,170,70); style_option_button(de,language); lv_obj_align(de,LV_ALIGN_CENTER,-95,0); lv_obj_t *dl=lv_label_create(de); lv_label_set_text(dl,"Deutsch"); lv_obj_center(dl); lv_obj_add_event_cb(de,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)1);
    lv_obj_t *en=lv_button_create(s); lv_obj_set_size(en,170,70); style_option_button(en,!language); lv_obj_align(en,LV_ALIGN_CENTER,95,0); lv_obj_t *el=lv_label_create(en); lv_label_set_text(el,"English"); lv_obj_center(el); lv_obj_add_event_cb(en,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)0);
    add_back_button(s,false);
}

static void build_clock_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Clock","Uhr"));
    time_t now; time(&now); struct tm tm_now; localtime_r(&now,&tm_now); char buf[8]; strftime(buf,sizeof(buf),"%H:%M",&tm_now);
    lv_obj_t *clock=lv_label_create(s); lv_label_set_text(clock,buf); lv_obj_set_style_text_font(clock,&lv_font_montserrat_24,0); lv_obj_align(clock,LV_ALIGN_TOP_MID,0,75);
    lv_obj_t *mh=lv_button_create(s); lv_obj_set_size(mh,170,55); lv_obj_align(mh,LV_ALIGN_TOP_MID,-95,145); style_option_button(mh,false); lv_obj_t *mhl=lv_label_create(mh); lv_label_set_text(mhl,tr("- 1 h","- 1 Std")); lv_obj_center(mhl); lv_obj_add_event_cb(mh,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-3600);
    lv_obj_t *ph=lv_button_create(s); lv_obj_set_size(ph,170,55); lv_obj_align(ph,LV_ALIGN_TOP_MID,95,145); style_option_button(ph,false); lv_obj_t *phl=lv_label_create(ph); lv_label_set_text(phl,tr("+ 1 h","+ 1 Std")); lv_obj_center(phl); lv_obj_add_event_cb(ph,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)3600);
    lv_obj_t *mm=lv_button_create(s); lv_obj_set_size(mm,170,55); lv_obj_align(mm,LV_ALIGN_TOP_MID,-95,215); style_option_button(mm,false); lv_obj_t *mml=lv_label_create(mm); lv_label_set_text(mml,tr("- 1 min","- 1 Min")); lv_obj_center(mml); lv_obj_add_event_cb(mm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-60);
    lv_obj_t *pm=lv_button_create(s); lv_obj_set_size(pm,170,55); lv_obj_align(pm,LV_ALIGN_TOP_MID,95,215); style_option_button(pm,false); lv_obj_t *pml=lv_label_create(pm); lv_label_set_text(pml,tr("+ 1 min","+ 1 Min")); lv_obj_center(pml); lv_obj_add_event_cb(pm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)60);
    add_back_button(s,false);
}

static void build_theme_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Theme","Darstellung"));
    lv_obj_t *dark=lv_button_create(s); lv_obj_set_size(dark,170,70); style_option_button(dark,theme==0); lv_obj_align(dark,LV_ALIGN_CENTER,-95,0); lv_obj_t *dt=lv_label_create(dark); lv_label_set_text(dt,"Dark"); lv_obj_center(dt); lv_obj_add_event_cb(dark,theme_cb,LV_EVENT_CLICKED,(void*)(intptr_t)0);
    lv_obj_t *light=lv_button_create(s); lv_obj_set_size(light,170,70); style_option_button(light,theme==1); lv_obj_align(light,LV_ALIGN_CENTER,95,0); lv_obj_t *lt=lv_label_create(light); lv_label_set_text(lt,"Light"); lv_obj_center(lt); lv_obj_add_event_cb(light,theme_cb,LV_EVENT_CLICKED,(void*)(intptr_t)1);
    add_back_button(s,false);
}

static void build_info_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,"RoundGames");
    lv_obj_t *info=lv_label_create(s);
    lv_label_set_text(info,"Firmware  Phase 3\nBoard     ESP32-S3\nDisplay   AMOLED 1.75\"\nSize      466 x 466");
    lv_obj_set_style_text_font(info,&lv_font_montserrat_18,0); lv_obj_align(info,LV_ALIGN_CENTER,0,0);
    add_back_button(s,false);
}

static void screensaver_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (!screensaver_enabled) return;

    if (screensaver_active) {
        lv_obj_t *clock = screen_saver ? lv_obj_get_child(screen_saver, 0) : NULL;
        if (clock) {
            time_t now; time(&now);
            struct tm tm_now; localtime_r(&now, &tm_now);
            char buf[32]; strftime(buf, sizeof(buf), "%H:%M", &tm_now);
            lv_label_set_text(clock, buf);
        }
        return;
    }

    inactivity_seconds++;
    if (inactivity_seconds < dim_timeout) return;
    screensaver_active = true; bsp_display_brightness_set(dim_brightness);
    lv_obj_t *screen = lv_scr_act();
    screen_saver = lv_obj_create(screen);
    lv_obj_set_size(screen_saver, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(screen_saver, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(screen_saver, 0, 0);
    lv_obj_clear_flag(screen_saver, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen_saver, screensaver_wake_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *clock = lv_label_create(screen_saver);
    time_t now; time(&now); struct tm tm_now; localtime_r(&now, &tm_now);
    char buf[32]; strftime(buf, sizeof(buf), "%H:%M", &tm_now);
    lv_label_set_text(clock, buf); lv_obj_set_style_text_color(clock, lv_color_hex(0xFFFFFF), 0); lv_obj_set_style_text_font(clock, &lv_font_montserrat_24, 0); lv_obj_set_style_text_align(clock, LV_TEXT_ALIGN_CENTER, 0); lv_obj_center(clock);
}

static void screensaver_wake_cb(lv_event_t *e) { LV_UNUSED(e); activity_reset(); }

typedef struct {
    const char *id;
    const char *name;
} game_entry_t;

static const game_entry_t games[] = {
    { "Tic-Tac-Toe", "Tic-Tac-Toe" }
};

#define GAME_COUNT ((int)(sizeof(games) / sizeof(games[0])))
#define GAMES_PER_PAGE 4

static int favorite_game_count(void)
{
    int count = 0;
    for (int i = 0; i < GAME_COUNT; i++) {
        if (favorite_games & (1u << i)) count++;
    }
    return count;
}

static int favorite_game_at(int favorite_index)
{
    int seen = 0;
    for (int i = 0; i < GAME_COUNT; i++) {
        if (favorite_games & (1u << i)) {
            if (seen == favorite_index) return i;
            seen++;
        }
    }
    return -1;
}

static void launcher_button_cb(lv_event_t *e)
{
    int game_index = (int)(intptr_t)lv_event_get_user_data(e);
    activity_reset();
    if (game_index >= 0 && game_index < GAME_COUNT &&
        !strcmp(games[game_index].id, "Tic-Tac-Toe")) {
        show_status("Tic-Tac-Toe",
                    tr("Game module will be added next.",
                       "Spielmodul kommt als Nächstes."));
    }
}

static void favorite_toggle_cb(lv_event_t *e)
{
    int game_index = (int)(intptr_t)lv_event_get_user_data(e);
    if (game_index < 0 || game_index >= GAME_COUNT) return;

    favorite_games ^= (1u << game_index);

    if (favorite_games == 0) {
        favorite_games |= (1u << game_index);
    }

    launcher_page = 0;
    save_settings();
    build_games_menu();
}

static void build_games_menu(void)
{
    clear_screen();
    lv_obj_t *screen = lv_scr_act();
    add_title(screen, tr("Games", "Spiele"));

    for (int i = 0; i < GAME_COUNT; i++) {
        int row = i / 2;
        int col = i % 2;
        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 66);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 38 + col * 205, 75 + row * 82);
        style_option_button(button, (favorite_games & (1u << i)) != 0);

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, games[i].name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 8, 0);

        lv_obj_add_event_cb(button, favorite_toggle_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint,
        tr("Tap a game to favorite it",
           "Tippe auf ein Spiel, um es zu favorisieren"));
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -55);

    add_back_button(screen, true);
}

static void build_launcher(void)
{
    clear_screen();
    launcher_active = true;

    lv_obj_t *screen = lv_scr_act();

    int favorite_count = favorite_game_count();
    int page_count = (favorite_count + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
    if (page_count < 1) page_count = 1;
    if (launcher_page >= page_count) launcher_page = page_count - 1;

    int start = launcher_page * GAMES_PER_PAGE;

    for (int slot = 0; slot < GAMES_PER_PAGE; slot++) {
        int favorite_index = start + slot;
        int game_index = favorite_game_at(favorite_index);
        int col = slot % 2;
        int row = slot / 2;

        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 120);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 38 + col * 205,
                     82 + row * 145);

        if (game_index >= 0) {
            style_option_button(button, false);
            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, games[game_index].name);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_obj_center(label);
            lv_obj_add_event_cb(button, launcher_button_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)game_index);
        } else {
            style_option_button(button, false);
            lv_obj_add_state(button, LV_STATE_DISABLED);
        }
    }

    if (page_count > 1) {
        for (int i = 0; i < page_count; i++) {
            lv_obj_t *dot = lv_obj_create(screen);
            lv_obj_set_size(dot, i == launcher_page ? 10 : 7, i == launcher_page ? 10 : 7);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(dot, i == launcher_page ? lv_color_hex(0x20A050) : lv_color_hex(0x60656D), 0);
            lv_obj_set_style_border_width(dot, 0, 0);
            lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (i - (page_count - 1) / 2) * 18, -18);
        }
    } else {
        lv_obj_t *dot = lv_obj_create(screen);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x20A050), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, 0, -18);
    }
}

static void show_status(const char *title,const char *message)
{
    clear_screen(); lv_obj_t *screen=lv_scr_act(); add_title(screen,title);
    lv_obj_t *msg=lv_label_create(screen); lv_label_set_text(msg,message); lv_obj_set_width(msg,380); lv_obj_set_style_text_align(msg,LV_TEXT_ALIGN_CENTER,0); lv_obj_align(msg,LV_ALIGN_CENTER,0,-10);
    add_back_button(screen,true);
}

void app_main(void)
{
    ESP_LOGI(TAG,"Starting RoundGames Phase 3");
    esp_err_t nvs_ret=nvs_flash_init();
    if(nvs_ret==ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret==ESP_ERR_NVS_NEW_VERSION_FOUND){nvs_flash_erase();nvs_flash_init();}
    load_settings();
    bsp_display_start();
    bsp_display_brightness_set(current_brightness);
    bsp_display_lock(-1);
    build_launcher();
    screensaver_timer=lv_timer_create(screensaver_tick,1000,NULL);
    bsp_display_unlock();
    ESP_LOGI(TAG,"RoundGames Phase 3 UI ready");
}
