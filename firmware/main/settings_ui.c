#include "settings_ui.h"

static void style_button(lv_obj_t *button, bool selected)
{
    lv_obj_set_style_bg_color(button,
        selected ? lv_color_hex(0x20A050) : lv_color_hex(0x30343B),
        LV_PART_MAIN);
}

lv_obj_t *settings_ui_button_create(lv_obj_t *parent, int width, int height,
                                     lv_align_t align, int x, int y, bool selected)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_align(button, align, x, y);
    style_button(button, selected);
    return button;
}

void settings_ui_center_label(lv_obj_t *button, const char *text, const lv_font_t *font)
{
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    if (font) lv_obj_set_style_text_font(label, font, 0);
    lv_obj_center(label);
}
