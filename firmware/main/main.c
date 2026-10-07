#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "lvgl.h"

#include "display.h"
#include "audio.h"
#include "language.h"
#include "settings.h"
#include "clock.h"
#include "theme.h"
#include "screensaver.h"

static const char *TAG = "roundgames";

static int current_brightness = 50;
static int current_volume = 70;
static int dim_brightness = 10;
static int dim_timeout = 30;

static bool sound_muted = false;
static bool launcher_active = true;
static int launcher_page = 0;
static uint32_t favorite_games = 1u;

static lv_obj_t *brightness_slider = NULL;
static lv_obj_t *brightness_value_label = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *volume_value_label = NULL;
static lv_obj_t *dim_slider = NULL;
static lv_obj_t *dim_value_label = NULL;
static lv_timer_t *theme_animation_timer = NULL;


static bool gesture_registered = false;

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

#define GAMES_PER_PAGE 4
#define GAME_COUNT 8
#define LAUNCHER_PAGE_COUNT ((GAME_COUNT + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE)

static void settings_gesture_cb(lv_event_t *e);

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
    theme_apply(lv_scr_act());
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

static void settings_gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    if (launcher_active) {
        if (dir == LV_DIR_BOTTOM) {
            build_settings_menu();
        } else if (dir == LV_DIR_LEFT) {
            if (launcher_page < LAUNCHER_PAGE_COUNT - 1) launcher_page++;
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
        lv_obj_align(button,LV_ALIGN_TOP_LEFT,38+((i%2)*205),123+((i/2)*76));
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

    lv_obj_t *n=lv_label_create(s); lv_label_set_text(n,"Normal"); lv_obj_set_style_text_font(n,&lv_font_montserrat_18,0); lv_obj_align(n,LV_ALIGN_TOP_MID,0,112);
    brightness_value_label=lv_label_create(s); lv_label_set_text_fmt(brightness_value_label,"%d%%",current_brightness); lv_obj_set_style_text_font(brightness_value_label,&lv_font_montserrat_18,0); lv_obj_align_to(brightness_value_label,n,LV_ALIGN_OUT_RIGHT_MID,12,0);
    brightness_slider=lv_slider_create(s); lv_obj_set_width(brightness_slider,220); lv_slider_set_range(brightness_slider,10,100); lv_slider_set_value(brightness_slider,current_brightness,LV_ANIM_OFF); lv_obj_align(s,LV_ALIGN_TOP_MID,0,0); lv_obj_align(brightness_slider,LV_ALIGN_TOP_MID,0,154); lv_obj_add_event_cb(brightness_slider,brightness_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);

    lv_obj_t *bm=lv_button_create(s); lv_obj_set_size(bm,48,42); lv_obj_align(bm,LV_ALIGN_TOP_LEFT,42,144); style_option_button(bm,false); lv_obj_t *bml=lv_label_create(bm); lv_label_set_text(bml,"-"); lv_obj_center(bml); lv_obj_add_event_cb(bm,brightness_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *bp=lv_button_create(s); lv_obj_set_size(bp,48,42); lv_obj_align(bp,LV_ALIGN_TOP_RIGHT,-42,144); style_option_button(bp,false); lv_obj_t *bpl=lv_label_create(bp); lv_label_set_text(bpl,"+"); lv_obj_center(bpl); lv_obj_add_event_cb(bp,brightness_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *d=lv_label_create(s); lv_label_set_text(d,tr("Dim","Dimmen")); lv_obj_set_style_text_font(d,&lv_font_montserrat_18,0); lv_obj_align(d,LV_ALIGN_TOP_MID,0,194);
    dim_value_label=lv_label_create(s); lv_label_set_text_fmt(dim_value_label,"%d%%",dim_brightness); lv_obj_set_style_text_font(dim_value_label,&lv_font_montserrat_18,0); lv_obj_align_to(dim_value_label,d,LV_ALIGN_OUT_RIGHT_MID,12,0);
    dim_slider=lv_slider_create(s); lv_obj_set_width(dim_slider,220); lv_slider_set_range(dim_slider,5,50); lv_slider_set_value(dim_slider,dim_brightness,LV_ANIM_OFF); lv_obj_align(dim_slider,LV_ALIGN_TOP_MID,0,236); lv_obj_add_event_cb(dim_slider,dim_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *dm=lv_button_create(s); lv_obj_set_size(dm,48,42); lv_obj_align(dm,LV_ALIGN_TOP_LEFT,42,226); style_option_button(dm,false); lv_obj_t *dml=lv_label_create(dm); lv_label_set_text(dml,"-"); lv_obj_center(dml); lv_obj_add_event_cb(dm,dim_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *dp=lv_button_create(s); lv_obj_set_size(dp,48,42); lv_obj_align(dp,LV_ALIGN_TOP_RIGHT,-42,226); style_option_button(dp,false); lv_obj_t *dpl=lv_label_create(dp); lv_label_set_text(dpl,"+"); lv_obj_center(dpl); lv_obj_add_event_cb(dp,dim_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *ss=lv_button_create(s); lv_obj_set_size(ss,175,52); lv_obj_align(ss,LV_ALIGN_TOP_LEFT,48,302); style_option_button(ss,screensaver_is_enabled());
    lv_obj_t *ssl=lv_label_create(ss); lv_label_set_text_fmt(ssl,"%s: %s",tr("Screen","Bildschirm"),screensaver_is_enabled() ? "ON":"OFF"); lv_obj_center(ssl); lv_obj_add_event_cb(ss,screensaver_toggle_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *to=lv_button_create(s); lv_obj_set_size(to,175,52); lv_obj_align(to,LV_ALIGN_TOP_RIGHT,-48,302); style_option_button(to,false);
    lv_obj_t *tol=lv_label_create(to); lv_label_set_text_fmt(tol,"%s: %ds",tr("After","Nach"),dim_timeout); lv_obj_center(tol); lv_obj_add_event_cb(to,dim_time_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void build_volume_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Sound","Ton"));
    volume_value_label=lv_label_create(s); lv_label_set_text_fmt(volume_value_label,"%d%%",current_volume); lv_obj_set_style_text_font(volume_value_label,&lv_font_montserrat_24,0); lv_obj_align(volume_value_label,LV_ALIGN_TOP_MID,0,141);
    volume_slider=lv_slider_create(s); lv_obj_set_width(volume_slider,260); lv_slider_set_range(volume_slider,0,100); lv_slider_set_value(volume_slider,current_volume,LV_ANIM_OFF); lv_obj_align(volume_slider,LV_ALIGN_TOP_MID,0,189); lv_obj_add_event_cb(volume_slider,volume_slider_cb,LV_EVENT_VALUE_CHANGED,NULL); lv_obj_add_event_cb(volume_slider,volume_slider_release_cb,LV_EVENT_RELEASED,NULL);

    lv_obj_t *m=lv_button_create(s); lv_obj_set_size(m,55,42); lv_obj_align(m,LV_ALIGN_TOP_LEFT,48,179); style_option_button(m,false); lv_obj_t *ml=lv_label_create(m); lv_label_set_text(ml,"-"); lv_obj_center(ml); lv_obj_add_event_cb(m,volume_minus_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t *p=lv_button_create(s); lv_obj_set_size(p,55,42); lv_obj_align(p,LV_ALIGN_TOP_RIGHT,-48,179); style_option_button(p,false); lv_obj_t *pl=lv_label_create(p); lv_label_set_text(pl,"+"); lv_obj_center(pl); lv_obj_add_event_cb(p,volume_plus_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t *mu=lv_button_create(s); lv_obj_set_size(mu,190,52); lv_obj_align(mu,LV_ALIGN_TOP_MID,0,273); style_option_button(mu,sound_muted); lv_obj_t *mul=lv_label_create(mu); lv_label_set_text(mul,sound_muted ? tr("Unmute","Ton an") : tr("Mute","Stumm")); lv_obj_center(mul); lv_obj_add_event_cb(mu,mute_cb,LV_EVENT_CLICKED,NULL);
    add_back_button(s,false);
}

static void build_language_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Language","Sprache"));
    lv_obj_t *de=lv_button_create(s); lv_obj_set_size(de,170,70); style_option_button(de,language_get()); lv_obj_align(de,LV_ALIGN_TOP_MID,-95,198); lv_obj_t *dl=lv_label_create(de); lv_label_set_text(dl,"Deutsch"); lv_obj_center(dl); lv_obj_add_event_cb(de,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)1);
    lv_obj_t *en=lv_button_create(s); lv_obj_set_size(en,170,70); style_option_button(en,!language_get()); lv_obj_align(en,LV_ALIGN_TOP_MID,95,198); lv_obj_t *el=lv_label_create(en); lv_label_set_text(el,"English"); lv_obj_center(el); lv_obj_add_event_cb(en,language_cb,LV_EVENT_CLICKED,(void*)(intptr_t)0);
    add_back_button(s,false);
}

static void build_clock_page(void)
{
    clear_screen(); lv_obj_t *s=lv_scr_act(); add_title(s,tr("Clock","Uhr"));
    char buf[8];
    clock_format_hm(buf, sizeof(buf));
    lv_obj_t *clock=lv_label_create(s); lv_label_set_text(clock,buf); lv_obj_set_style_text_font(clock,&lv_font_montserrat_24,0); lv_obj_align(clock,LV_ALIGN_TOP_MID,0,78);
    lv_obj_t *mh=lv_button_create(s); lv_obj_set_size(mh,170,55); lv_obj_align(mh,LV_ALIGN_TOP_MID,-95,169); style_option_button(mh,false); lv_obj_t *mhl=lv_label_create(mh); lv_label_set_text(mhl,tr("- 1 h","- 1 Std")); lv_obj_center(mhl); lv_obj_add_event_cb(mh,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-3600);
    lv_obj_t *ph=lv_button_create(s); lv_obj_set_size(ph,170,55); lv_obj_align(ph,LV_ALIGN_TOP_MID,95,169); style_option_button(ph,false); lv_obj_t *phl=lv_label_create(ph); lv_label_set_text(phl,tr("+ 1 h","+ 1 Std")); lv_obj_center(phl); lv_obj_add_event_cb(ph,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)3600);
    lv_obj_t *mm=lv_button_create(s); lv_obj_set_size(mm,170,55); lv_obj_align(mm,LV_ALIGN_TOP_MID,-95,239); style_option_button(mm,false); lv_obj_t *mml=lv_label_create(mm); lv_label_set_text(mml,tr("- 1 min","- 1 Min")); lv_obj_center(mml); lv_obj_add_event_cb(mm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)-60);
    lv_obj_t *pm=lv_button_create(s); lv_obj_set_size(pm,170,55); lv_obj_align(pm,LV_ALIGN_TOP_MID,95,239); style_option_button(pm,false); lv_obj_t *pml=lv_label_create(pm); lv_label_set_text(pml,tr("+ 1 min","+ 1 Min")); lv_obj_center(pml); lv_obj_add_event_cb(pm,clock_adjust_cb,LV_EVENT_CLICKED,(void*)(intptr_t)60);
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
        lv_obj_set_size(button, 170, 62);
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


typedef struct {
    const char *id;
    const char *name;
} game_entry_t;

static const game_entry_t games[] = {
    { "Tic-Tac-Toe", "Tic-Tac-Toe" },
    { "Test-App-2", "App 2" },
    { "Test-App-3", "App 3" },
    { "Test-App-4", "App 4" },
    { "Test-App-5", "App 5" },
    { "Test-App-6", "App 6" },
    { "Test-App-7", "App 7" },
    { "Test-App-8", "App 8" }
};


static void launcher_button_cb(lv_event_t *e)
{
    int game_index = (int)(intptr_t)lv_event_get_user_data(e);
    activity_reset();
    if (game_index == 0) {
        show_status("Tic-Tac-Toe",
                    tr("Game module will be added next.",
                       "Spielmodul kommt als Nächstes."));
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "%s\n%s",
                 games[game_index].name,
                 tr("Test placeholder", "Test-Platzhalter"));
        show_status(games[game_index].name, msg);
    }
}

static void build_launcher(void)
{
    clear_screen();
    launcher_active = true;

    lv_obj_t *screen = lv_scr_act();
    int start = launcher_page * GAMES_PER_PAGE;

    for (int slot = 0; slot < GAMES_PER_PAGE; slot++) {
        int game_index = start + slot;
        int col = slot % 2;
        int row = slot / 2;

        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 120);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 38 + col * 205,
                     82 + row * 145);
        style_option_button(button, false);

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, games[game_index].name);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_center(label);
        lv_obj_add_event_cb(button, launcher_button_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)game_index);
        /* Let launcher swipes reach the screen instead of being handled by the app button. */
        lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }

    for (int i = 0; i < LAUNCHER_PAGE_COUNT; i++) {
        lv_obj_t *dot = lv_obj_create(screen);
        lv_obj_set_size(dot, i == launcher_page ? 10 : 7,
                        i == launcher_page ? 10 : 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot,
                                  i == launcher_page ? lv_color_hex(0x20A050)
                                                     : lv_color_hex(0x60656D), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID,
                     (i - (LAUNCHER_PAGE_COUNT - 1) / 2) * 18, -18);
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
    screensaver_init();
    load_settings();
    display_init();
    display_set_brightness(current_brightness);
    display_lock();
    build_launcher();

    audio_init();
    lv_timer_create(screensaver_tick, 100, NULL);
    theme_animation_timer=lv_timer_create(theme_animation_tick,33,NULL);
    display_unlock();
    ESP_LOGI(TAG,"RoundGames Phase 3 UI ready");
}
