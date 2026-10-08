#include "screensaver.h"

#include "bsp/esp-bsp.h"
#include "clock.h"
#include "theme.h"

static bool enabled = true;
static bool game_active = false;
static bool active = false;
static int normal_brightness = 50;
static int dim_brightness = 10;
static int timeout_seconds = 30;
static int inactivity_seconds = 0;
static int idle_ticks = 0;

static lv_obj_t *screen_saver = NULL;
static lv_obj_t *screensaver_clock = NULL;

void screensaver_init(void)
{
    enabled = true;
    active = false;
    game_active = false;
    inactivity_seconds = 0;
    idle_ticks = 0;
    screen_saver = NULL;
    screensaver_clock = NULL;
}

void screensaver_set_enabled(bool value)
{
    enabled = value;
    if (!enabled && active) {
        screensaver_activity_reset();
    }
}

void screensaver_set_normal_brightness(int value)
{
    normal_brightness = value;
}

void screensaver_set_dim_brightness(int value)
{
    dim_brightness = value;
}

void screensaver_set_timeout(int seconds)
{
    timeout_seconds = seconds;
}

void screensaver_set_game_active(bool active)
{
    game_active = active;
    if (active) {
        screensaver_activity_reset();
    }
}

void screensaver_activity_reset(void)
{
    inactivity_seconds = 0;
    idle_ticks = 0;

    if (active) {
        active = false;
        if (screen_saver) {
            lv_obj_del(screen_saver);
            screen_saver = NULL;
        }
        screensaver_clock = NULL;
        bsp_display_brightness_set(normal_brightness);
        theme_apply(lv_scr_act());
    }
}

bool screensaver_is_active(void)
{
    return active;
}

bool screensaver_is_enabled(void)
{
    return enabled;
}

static void screensaver_wake_cb(lv_event_t *event)
{
    LV_UNUSED(event);
    screensaver_activity_reset();
}

void screensaver_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!enabled || game_active) return;

    if (active) {
        if (screensaver_clock) {
            char buf[32];
            clock_format_hm(buf, sizeof(buf));
            lv_label_set_text(screensaver_clock, buf);
        }
        return;
    }

    idle_ticks++;
    if (idle_ticks < 10) return;
    idle_ticks = 0;
    inactivity_seconds++;
    if (inactivity_seconds < timeout_seconds) return;

    active = true;
    bsp_display_brightness_set(dim_brightness);

    lv_obj_t *screen = lv_scr_act();
    screen_saver = lv_obj_create(screen);
    lv_obj_set_size(screen_saver, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(screen_saver,
                              lv_color_hex(theme_get() == 2 ? 0x07151A :
                                           theme_get() == 3 ? 0x120914 : 0x000000), 0);
    lv_obj_set_style_bg_opa(screen_saver, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen_saver, 0, 0);
    lv_obj_clear_flag(screen_saver, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen_saver, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(screen_saver, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen_saver, screensaver_wake_cb, LV_EVENT_CLICKED, NULL);

    theme_apply(screen_saver);

    screensaver_clock = lv_label_create(screen_saver);
    char buf[32];
    clock_format_hm(buf, sizeof(buf));
    lv_label_set_text(screensaver_clock, buf);
    lv_obj_clear_flag(screensaver_clock, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(screensaver_clock, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(screensaver_clock, &lv_font_montserrat_48, 0);
    lv_obj_set_style_transform_scale_x(screensaver_clock, 512, 0);
    lv_obj_set_style_transform_scale_y(screensaver_clock, 512, 0);
    lv_obj_set_style_text_align(screensaver_clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(screensaver_clock);
}
