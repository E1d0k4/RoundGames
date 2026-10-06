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

static void play_test_tone(void)
{
    if(sound_muted)return;
    if(!speaker_codec)speaker_codec=bsp_audio_codec_speaker_init();
    if(!speaker_codec)return;
    static int16_t tone[2400]; static bool ready=false;
    if(!ready){for(int i=0;i<2400;i++)tone[i]=(i%27<13)?6000:-6000;ready=true;}
    esp_codec_dev_sample_info_t fs={.sample_rate=24000,.channel=1,.bits_per_sample=16};
    if(esp_codec_dev_open(speaker_codec,&fs)!=ESP_OK)return;
    esp_codec_dev_set_out_vol(speaker_codec,current_volume);
    esp_codec_dev_write(speaker_codec,tone,sizeof(tone));
    esp_codec_dev_close(speaker_codec);
}

static void volume_test_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    play_test_tone();
}

static void build_volume_page(void)
{
    clear_screen();
    lv_obj_t *s=lv_scr_act();
    add_title(s,language ? "Ton" : "Sound");

    volume_value_label=lv_label_create(s);
    lv_label_set_text_fmt(volume_value_label,"%d%%",current_volume);
    lv_obj_align(volume_value_label,LV_ALIGN_CENTER,0,-65);

    volume_slider=lv_slider_create(s);
    lv_obj_set_width(volume_slider,300);
    lv_slider_set_range(volume_slider,0,100);
    lv_slider_set_value(volume_slider,current_volume,LV_ANIM_OFF);
    lv_obj_align(volume_slider,LV_ALIGN_CENTER,0,-5);
    lv_obj_add_event_cb(volume_slider,volume_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *m=lv_button_create(s); lv_obj_set_size(m,55,42);
    lv_obj_align(m,LV_ALIGN_CENTER,-180,-5);
    lv_obj_t *ml=lv_label_create(m); lv_label_set_text(ml,"-"); lv_obj_center(ml);
    lv_obj_add_event_cb(m,volume_minus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *p=lv_button_create(s); lv_obj_set_size(p,55,42);
    lv_obj_align(p,LV_ALIGN_CENTER,180,-5);
    lv_obj_t *pl=lv_label_create(p); lv_label_set_text(pl,"+"); lv_obj_center(pl);
    lv_obj_add_event_cb(p,volume_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *t=lv_button_create(s); lv_obj_set_size(t,170,50);
    lv_obj_align(t,LV_ALIGN_CENTER,0,70);
    lv_obj_t *tl=lv_label_create(t);
    lv_label_set_text(tl,language ? "Testton" : "Test tone"); lv_obj_center(tl);
    lv_obj_add_event_cb(t,volume_test_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *mu=lv_button_create(s); lv_obj_set_size(mu,170,50);
    lv_obj_align(mu,LV_ALIGN_CENTER,0,130);
    lv_obj_t *mul=lv_label_create(mu);
    lv_label_set_text(mul,sound_muted ? (language ? "Ton an":"Unmute") : (language ? "Stumm":"Mute"));
    lv_obj_center(mul);
    lv_obj_add_event_cb(mu,mute_cb,LV_EVENT_CLICKED,NULL);

    add_back_button(s,false);
}

static void build_language_page(void)
{
    clear_screen();
    lv_obj_t *s=lv_scr_act();
    add_title(s,language ? "Sprache" : "Language");

    lv_obj_t *de=lv_button_create(s); lv_obj_set_size(de,170,70);
    lv_obj_align(de,LV_ALIGN_CENTER,-95,0);
    lv_obj_t *dl=lv_label_create(de); lv_label_set_text(dl,language ? "✓ Deutsch":"Deutsch"); lv_obj_center(dl);
    lv_obj_add_event_cb(de,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)1);

    lv_obj_t *en=lv_button_create(s); lv_obj_set_size(en,170,70);
    lv_obj_align(en,LV_ALIGN_CENTER,95,0);
    lv_obj_t *el=lv_label_create(en); lv_label_set_text(el,language ? "English":"✓ English"); lv_obj_center(el);
    lv_obj_add_event_cb(en,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)0);

    add_back_button(s,false);
}

static void build_theme_page(void);
static void build_info_page(void)
{
    clear_screen();
    lv_obj_t *s=lv_scr_act();
    add_title(s,"RoundGames");

    lv_obj_t *info=lv_label_create(s);
    lv_label_set_text(info,
        "Firmware  Phase 3\n"
        "Board     ESP32-S3\n"
        "Display   AMOLED 1.75\"\n"
        "Size      466 x 466");
    lv_obj_set_style_text_font(info,&lv_font_montserrat_18,0);
    lv_obj_align(info,LV_ALIGN_CENTER,0,0);
    add_back_button(s,false);
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
    strftime(buf, sizeof(buf), "%H:%M", &tm_now);
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
