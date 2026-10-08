#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "lvgl.h"

#include "display.h"
#include "input.h"
#include "launcher.h"
#include "audio.h"
#include "language.h"
#include "settings.h"
#include "clock.h"
#include "theme.h"
#include "screensaver.h"
#include "settings_ui.h"
#include "tic_tac_toe.h"

static const char *TAG = "roundgames";

static int current_brightness = 50;
static int current_volume = 70;
static int dim_brightness = 10;
static int dim_timeout = 30;

static bool sound_muted = false;
static uint32_t favorite_games = 1u;

static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_value_label = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *volume_value_label = NULL;
static lv_obj_t *dim_slider = NULL;
static lv_obj_t *dim_value_label = NULL;



static void build_launcher(void);
static void build_settings_menu(void);
static void build_brightness_page(void);
static void build_volume_page(void);
static void build_language_page(void);
static void build_clock_page(void);
static void build_theme_page(void);
static void theme_cb(lv_event_t *e);
static void build_info_page(void);
static void show_status(const char *title, const char *message);

static const char *tr(const char *en, const char *de) { return language_tr(en, de); }

static void save_settings(void)
{
    settings_data_t data = {
        .brightness = current_brightness,
        .volume = current_volume,
        .dim_brightness = dim_brightness,
        .dim_timeout = dim_timeout,
        .language = language_get(),
        .theme = theme_get(),
        .muted = sound_muted,
        .screensaver_enabled = screensaver_is_enabled(),
        .favorite_games = favorite_games
    };
    data.epoch = clock_now();
    settings_save(&data);
}

static void load_settings(void)
{
    settings_data_t data = {
        .brightness = current_brightness,
        .volume = current_volume,
        .dim_brightness = dim_brightness,
        .dim_timeout = dim_timeout,
        .language = language_get(),
        .theme = theme_get(),
        .muted = sound_muted,
        .screensaver_enabled = screensaver_is_enabled(),
        .favorite_games = favorite_games
    };
    settings_load(&data);

    current_brightness = data.brightness;
    current_volume = data.volume;
    dim_brightness = data.dim_brightness;
    dim_timeout = data.dim_timeout;
    language_set(data.language);
    theme_set(data.theme);
    sound_muted = data.muted;
    screensaver_set_enabled(data.screensaver_enabled);
    screensaver_set_normal_brightness(current_brightness);
    screensaver_set_dim_brightness(dim_brightness);
    screensaver_set_timeout(dim_timeout);
    favorite_games = data.favorite_games;

    if (theme_get() < 0 || theme_get() > 3) theme_set(0);
    if (current_brightness < 10) current_brightness = 10;
    if (current_brightness > 100) current_brightness = 100;
    if (current_volume < 0) current_volume = 0;
    if (current_volume > 100) current_volume = 100;
    if (dim_brightness < 5) dim_brightness = 5;
    if (dim_brightness > 50) dim_brightness = 50;
    if (dim_timeout < 10) dim_timeout = 10;
    if (dim_timeout > 600) dim_timeout = 600;
}

static void activity_reset(void)
{
    screensaver_activity_reset();
}



static void clear_screen(void)
{
    launcher_set_active(false);
    activity_reset();
    lv_obj_clean(lv_scr_act());
    brightness_slider = NULL;
    brightness_value_label = NULL;
    volume_slider = NULL;
    volume_value_label = NULL;
    dim_slider = NULL;
    dim_value_label = NULL;
    theme_apply(lv_scr_act());
    input_register_screen(lv_scr_act());
}

static void back_button_cb(lv_event_t *e) { LV_UNUSED(e); activity_reset(); build_settings_menu(); }
static void generic_back_launcher_cb(lv_event_t *e) { LV_UNUSED(e); input_set_game_state(false, NULL); screensaver_set_game_active(false); activity_reset(); build_launcher(); }

static void set_brightness(int value)
{
    if (value < 10) value = 10;
    if (value > 100) value = 100;
    current_brightness = value; display_set_brightness(value);
    screensaver_set_normal_brightness(value);
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
    audio_set_volume(current_volume, sound_muted);
    save_settings(); activity_reset();
}

static void volume_slider_cb(lv_event_t *e) { set_volume(lv_slider_get_value(lv_event_get_target(e))); }
static void volume_slider_release_cb(lv_event_t *e) { LV_UNUSED(e); audio_play_test_tone(current_volume, sound_muted); }
static void volume_minus_cb(lv_event_t *e) { LV_UNUSED(e); set_volume(current_volume - 10); audio_play_test_tone(current_volume, sound_muted); }
static void volume_plus_cb(lv_event_t *e) { LV_UNUSED(e); set_volume(current_volume + 10); audio_play_test_tone(current_volume, sound_muted); }

static void mute_cb(lv_event_t *e) { LV_UNUSED(e); sound_muted = !sound_muted; audio_set_volume(current_volume, sound_muted); save_settings(); build_volume_page(); }

static void set_dim_brightness(int value)
{
    if (value < 5) value = 5;
    if (value > 50) value = 50;
    dim_brightness = value;
    screensaver_set_dim_brightness(value);
    if (dim_slider) lv_slider_set_value(dim_slider, value, LV_ANIM_OFF);
    if (dim_value_label) lv_label_set_text_fmt(dim_value_label, "%d%%", value);
    save_settings(); activity_reset();
}
static void dim_slider_cb(lv_event_t *e) { set_dim_brightness(lv_slider_get_value(lv_event_get_target(e))); }
static void dim_minus_cb(lv_event_t *e) { LV_UNUSED(e); set_dim_brightness(dim_brightness - 5); }
static void dim_plus_cb(lv_event_t *e) { LV_UNUSED(e); set_dim_brightness(dim_brightness + 5); }
static void dim_time_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    dim_timeout += 10;
    if (dim_timeout > 120) dim_timeout = 10;
    screensaver_set_timeout(dim_timeout);
    save_settings();
    build_brightness_page();
}

static void language_cb(lv_event_t *e) { language_set((int)(intptr_t)lv_event_get_user_data(e)); save_settings(); build_language_page(); }
static void theme_cb(lv_event_t *e) { theme_set((int)(intptr_t)lv_event_get_user_data(e)); save_settings(); build_theme_page(); }
static void screensaver_toggle_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    screensaver_set_enabled(!screensaver_is_enabled());
    save_settings();
    build_brightness_page();
}

static void clock_adjust_cb(lv_event_t *e)
{
    int delta = (int)(intptr_t)lv_event_get_user_data(e);
    clock_adjust_seconds(delta);
    save_settings();
    build_clock_page();
}

static void settings_async_cb(void *user_data)
{
    LV_UNUSED(user_data);
    build_settings_menu();
}

static void launcher_async_cb(void *user_data)
{
    LV_UNUSED(user_data);
    build_launcher();
}

static void gesture_bottom_cb(void)
{
    /* Defer screen rebuild until the current gesture event has finished. */
    lv_async_call(settings_async_cb, NULL);
}

static void gesture_left_cb(void)
{
    lv_async_call(launcher_async_cb, NULL);
}

static void gesture_right_cb(void)
{
    lv_async_call(launcher_async_cb, NULL);
}

static void gesture_game_back_cb(void)
{
    lv_async_call(launcher_async_cb, NULL);
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
        lv_obj_t *button=settings_ui_button_create(screen,195,76,LV_ALIGN_TOP_LEFT,28+((i%2)*215),118+((i/2)*86),false);

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

    lv_obj_t *n=lv_label_create(s); lv_label_set_text(n,"Normal"); lv_obj_set_style_text_font(n,&lv_font_montserrat_18,0); lv_obj_align(n,LV_ALIGN_TOP_MID,0,112);
    brightness_value_label=lv_label_create(s); lv_label_set_text_fmt(brightness_value_label,"%d%%",current_brightness); lv_obj_set_style_text_font(brightness_value_label,&lv_font_montserrat_18,0); lv_obj_align_to(brightness_value_label,n,LV_ALIGN_OUT_RIGHT_MID,12,0);
    brightness_slider=lv_slider_create(s); lv_obj_set_width(brightness_slider,220); lv_slider_set_range(brightness_slider,10,100); lv_slider_set_value(brightness_slider,current_brightness,LV_ANIM_OFF); lv_obj_align(s,LV_ALIGN_TOP_MID,0,0); lv_obj_align(brightness_slider,LV_ALIGN_TOP_MID,0,154); lv_obj_add_event_cb(brightness_slider,brightness_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *bm=settings_ui_button_create(s,60,52,LV_ALIGN_TOP_LEFT,34,139,false);  lv_obj_t *bml=lv_label_create(bm); lv_label_set_text(bml,"-"); lv_obj_center(bml); lv_obj_add_event_cb(bm,brightness_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *bp=settings_ui_button_create(s,60,52,LV_ALIGN_TOP_RIGHT,-34,139,false);  lv_obj_t *bpl=lv_label_create(bp); lv_label_set_text(bpl,"+"); lv_obj_center(bpl); lv_obj_add_event_cb(bp,brightness_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *d=lv_label_create(s); lv_label_set_text(d,tr("Dim","Dimmen")); lv_obj_set_style_text_font(d,&lv_font_montserrat_18,0); lv_obj_align(d,LV_ALIGN_TOP_MID,0,194);
    dim_value_label=lv_label_create(s); lv_label_set_text_fmt(dim_value_label,"%d%%",dim_brightness); lv_obj_set_style_text_font(dim_value_label,&lv_font_montserrat_18,0); lv_obj_align_to(dim_value_label,d,LV_ALIGN_OUT_RIGHT_MID,12,0);
    dim_slider=lv_slider_create(s); lv_obj_set_width(dim_slider,220); lv_slider_set_range(dim_slider,5,50); lv_slider_set_value(dim_slider,dim_brightness,LV_ANIM_OFF); lv_obj_align(dim_slider,LV_ALIGN_TOP_MID,0,236); lv_obj_add_event_cb(dim_slider,dim_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *dm=settings_ui_button_create(s,60,52,LV_ALIGN_TOP_LEFT,34,221,false);  lv_obj_t *dml=lv_label_create(dm); lv_label_set_text(dml,"-"); lv_obj_center(dml); lv_obj_add_event_cb(dm,dim_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *dp=settings_ui_button_create(s,60,52,LV_ALIGN_TOP_RIGHT,-34,221,false);  lv_obj_t *dpl=lv_label_create(dp); lv_label_set_text(dpl,"+"); lv_obj_center(dpl); lv_obj_add_event_cb(dp,dim_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *ss=settings_ui_button_create(s,195,60,LV_ALIGN_TOP_LEFT,28,298,screensaver_is_enabled());
    lv_obj_t *ssl=lv_label_create(ss); lv_label_set_text_fmt(ssl,"%s: %s",tr("Screen","Bildschirm"),screensaver_is_enabled() ? "ON":"OFF"); lv_obj_center(ssl); lv_obj_add_event_cb(ss,screensaver_toggle_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *to=settings_ui_button_create(s,195,60,LV_ALIGN_TOP_RIGHT,-28,298,false);
    lv_obj_t *tol=lv_label_create(to); lv_label_set_text_fmt(tol,"%s: %ds",tr("After","Nach"),dim_timeout); lv_obj_center(tol); lv_obj_add_event_cb(to,dim_time_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void build_volume_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Sound","Ton"));
    volume_value_label=lv_label_create(s); lv_label_set_text_fmt(volume_value_label,"%d%%",current_volume); lv_obj_set_style_text_font(volume_value_label,&lv_font_montserrat_24,0); lv_obj_align(volume_value_label,LV_ALIGN_TOP_MID,0,141);
    volume_slider=lv_slider_create(s); lv_obj_set_width(volume_slider,260); lv_slider_set_range(volume_slider,0,100); lv_slider_set_value(volume_slider,current_volume,LV_ANIM_OFF); lv_obj_align(volume_slider,LV_ALIGN_TOP_MID,0,189); lv_obj_add_event_cb(volume_slider,volume_slider_cb,LV_EVENT_VALUE_CHANGED,NULL); lv_obj_add_event_cb(volume_slider,volume_slider_release_cb,LV_EVENT_RELEASED,NULL);

    lv_obj_t *m=settings_ui_button_create(s,65,52,LV_ALIGN_TOP_LEFT,38,174,false); lv_obj_t *ml=lv_label_create(m); lv_label_set_text(ml,"-"); lv_obj_center(ml); lv_obj_add_event_cb(m,volume_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *p=settings_ui_button_create(s,65,52,LV_ALIGN_TOP_RIGHT,-38,174,false); lv_obj_t *pl=lv_label_create(p); lv_label_set_text(pl,"+"); lv_obj_center(pl); lv_obj_add_event_cb(p,volume_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *mu=settings_ui_button_create(s,210,60,LV_ALIGN_TOP_MID,0,269,sound_muted); lv_obj_t *mul=lv_label_create(mu); lv_label_set_text(mul,sound_muted ? tr("Unmute","Ton an") : tr("Mute","Stumm")); lv_obj_center(mul); lv_obj_add_event_cb(mu,mute_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void build_language_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Language","Sprache"));
    lv_obj_t *de=settings_ui_button_create(s,180,76,LV_ALIGN_TOP_MID,-100,192,language_get()); lv_obj_t *dl=lv_label_create(de); lv_label_set_text(dl,"Deutsch"); lv_obj_center(dl); lv_obj_add_event_cb(de,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)1);
    lv_obj_t *en=settings_ui_button_create(s,180,76,LV_ALIGN_TOP_MID,100,192,!language_get()); lv_obj_t *el=lv_label_create(en); lv_label_set_text(el,"English"); lv_obj_center(el); lv_obj_add_event_cb(en,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)0);
    add_back_button(s,false);
}

static void build_clock_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Clock","Uhr"));
    char buf[8];
    clock_format_hm(buf, sizeof(buf));
    lv_obj_t *clock=lv_label_create(s); lv_label_set_text(clock,buf); lv_obj_set_style_text_font(clock,&lv_font_montserrat_24,0); lv_obj_align(clock,LV_ALIGN_TOP_MID,0,78);
    lv_obj_t *mh=settings_ui_button_create(s,180,62,LV_ALIGN_TOP_MID,-100,164,false); lv_obj_t *mhl=lv_label_create(mh); lv_label_set_text(mhl,tr("- 1 h","- 1 Std")); lv_obj_center(mhl); lv_obj_add_event_cb(mh,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-3600);
    lv_obj_t *ph=settings_ui_button_create(s,180,62,LV_ALIGN_TOP_MID,100,164,false); lv_obj_t *phl=lv_label_create(ph); lv_label_set_text(phl,tr("+ 1 h","+ 1 Std")); lv_obj_center(phl); lv_obj_add_event_cb(ph,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)3600);
    lv_obj_t *mm=settings_ui_button_create(s,180,62,LV_ALIGN_TOP_MID,-100,236,false); lv_obj_t *mml=lv_label_create(mm); lv_label_set_text(mml,tr("- 1 min","- 1 Min")); lv_obj_center(mml); lv_obj_add_event_cb(mm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-60);
    lv_obj_t *pm=settings_ui_button_create(s,180,62,LV_ALIGN_TOP_MID,100,236,false); lv_obj_t *pml=lv_label_create(pm); lv_label_set_text(pml,tr("+ 1 min","+ 1 Min")); lv_obj_center(pml); lv_obj_add_event_cb(pm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)60);
    add_back_button(s,false);
}

static void build_theme_page(void)
{
    clear_screen();
    lv_obj_t *s = lv_scr_act();
    add_title(s, tr("Theme", "Darstellung"));

    lv_obj_t *hint = lv_label_create(s);
    lv_label_set_text(hint, tr("Background", "Hintergrund"));
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 88);

    const char *names[] = {
        tr("Dark", "Dunkel"), tr("Light", "Hell"), "Aurora", "Pulse"
    };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *button = lv_button_create(s);
        lv_obj_set_size(button, 180, 70);
        lv_obj_align(button, LV_ALIGN_TOP_MID,
                     (i % 2) ? 95 : -95, 116 + (i / 2) * 76);
        style_option_button(button, theme_get() == i);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, names[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, theme_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)i);
    }

    lv_obj_t *note = lv_label_create(s);
    lv_label_set_text(note,
        tr("Aurora and Pulse are animated backgrounds for the whole interface.",
           "Aurora und Pulse sind animierte Hintergründe für die gesamte Oberfläche."));
    lv_obj_set_style_text_font(note, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(note, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(note, 420);
    lv_obj_align(note, LV_ALIGN_TOP_MID, 0, 278);

    add_back_button(s, false);
}

static void build_info_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,"RoundGames");
    lv_obj_t *info=lv_label_create(s);
    lv_label_set_text(info,"Firmware  Phase 3\nBoard     ESP32-S3\nDisplay   AMOLED 1.75\"\nSize      466 x 466");
    lv_obj_set_style_text_font(info,&lv_font_montserrat_18,0); lv_obj_align(info,LV_ALIGN_CENTER,0,0);
    add_back_button(s,false);
}


static void launcher_game_action(int game_index, const char *name)
{
    LV_UNUSED(name);
    activity_reset();
    input_set_game_state(true, gesture_game_back_cb);
    screensaver_set_game_active(true);

    if (game_index == 0) {
        clear_screen();
        tic_tac_toe_open(lv_scr_act());
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s\n%s",
                 name,
                 tr("Test placeholder", "Test-Platzhalter"));
        show_status(name, msg);
    }
}

static void build_launcher(void)
{
    clear_screen();
    launcher_set_active(true);
    launcher_build(lv_scr_act(), launcher_game_action);
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
    screensaver_init();
    input_init();
    input_set_actions(gesture_bottom_cb, gesture_left_cb, gesture_right_cb);
    input_set_activity_callback(activity_reset);
    load_settings();
    launcher_init(8);
    display_init();
    display_set_brightness(current_brightness);
    display_lock();
    build_launcher();

    audio_init();
    lv_timer_create(screensaver_tick, 100, NULL);
    lv_timer_create(theme_animation_tick,33,NULL);
    display_unlock();
    ESP_LOGI(TAG,"RoundGames Phase 3 UI ready");
}
