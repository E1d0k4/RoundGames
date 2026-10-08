#include "theme.h"

#include <math.h>

static int theme = 0;
static lv_obj_t *theme_effects[6] = {0};
static int theme_phase = 0;

int theme_get(void) { return theme; }

void theme_set(int value)
{
    if (value < 0) value = 0;
    if (value > 3) value = 3;
    theme = value;
}

void theme_apply(lv_obj_t *screen)
{
    if (!screen) return;
    for (int i = 0; i < 6; i++) theme_effects[i] = NULL;
    if (theme == 0) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x101014), 0);
        lv_obj_set_style_text_color(screen, lv_color_hex(0xFFFFFF), 0);
        return;
    }
    if (theme == 1) {
        lv_obj_set_style_bg_color(screen, lv_color_hex(0xF2F2F2), 0);
        lv_obj_set_style_text_color(screen, lv_color_hex(0x101014), 0);
        return;
    }

    lv_obj_set_style_bg_color(screen, lv_color_hex(theme == 2 ? 0x07151A : 0x120914), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xFFFFFF), 0);
    const uint32_t aurora[] = {0x00BFA5,0x1976D2,0x7E57C2,0x26A69A,0x1565C0,0xAB47BC};
    const uint32_t pulse[] = {0x7C3AED,0xDB2777,0x2563EB,0x059669,0xF59E0B,0x06B6D4};
    const uint32_t *colors = theme == 2 ? aurora : pulse;

    for (int i = 0; i < 6; i++) {
        theme_effects[i] = lv_obj_create(screen);
        lv_obj_set_style_radius(theme_effects[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(theme_effects[i], lv_color_hex(colors[i]), 0);
        lv_obj_set_style_bg_opa(theme_effects[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(theme_effects[i], 0, 0);
        lv_obj_clear_flag(theme_effects[i], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_background(theme_effects[i]);
        if (theme == 2) {
            lv_obj_set_size(theme_effects[i], 190, 190);
            lv_obj_set_pos(theme_effects[i], -15 + i * 70, 35 + (i % 3) * 105);
            lv_obj_set_style_opa(theme_effects[i], 95, 0);
        } else {
            lv_obj_set_size(theme_effects[i], 85, 85);
            lv_obj_set_pos(theme_effects[i], 15 + i * 82, 55 + (i % 3) * 145);
            lv_obj_set_style_opa(theme_effects[i], 130, 0);
        }
    }
}

void theme_animation_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    if (theme != 2 && theme != 3) return;
    theme_phase = (theme_phase + 1) % 360;

    if (theme == 2) {
        for (int i = 0; i < 6; i++) {
            if (!theme_effects[i]) continue;
            int x = -15 + i * 70 + (int)(sin((theme_phase + i * 60) * 0.0174533) * 55.0);
            int y = 35 + (i % 3) * 105 + (int)(cos((theme_phase + i * 75) * 0.0174533) * 45.0);
            lv_obj_set_pos(theme_effects[i], x, y);
            lv_obj_set_style_opa(theme_effects[i], (lv_opa_t)(75 + (int)((sin((theme_phase + i * 50) * 0.0174533) + 1.0) * 25.0)), 0);
        }
    } else {
        for (int i = 0; i < 6; i++) {
            if (!theme_effects[i]) continue;
            int size = 65 + (int)((sin((theme_phase + i * 60) * 0.0174533) + 1.0) * 30.0);
            int x = 10 + i * 82 + (int)(sin((theme_phase + i * 45) * 0.0174533) * 18.0);
            int y = 50 + (i % 3) * 145 + (int)(cos((theme_phase + i * 55) * 0.0174533) * 20.0);
            lv_obj_set_size(theme_effects[i], size, size);
            lv_obj_set_pos(theme_effects[i], x, y);
            lv_obj_set_style_opa(theme_effects[i], (lv_opa_t)(90 + (int)((sin((theme_phase + i * 60) * 0.0174533) + 1.0) * 45.0)), 0);
        }
    }
}
