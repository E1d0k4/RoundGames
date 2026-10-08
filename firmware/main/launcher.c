#include "launcher.h"

#include "input.h"

#include <stdint.h>

#define GAMES_PER_PAGE 4

static int game_count = 0;
static launcher_game_cb_t game_action = NULL;

static const char *game_names[] = {
    "Tic-Tac-Toe",
    "Snake",
    "Vegg",
    "App 4",
    "App 5",
    "App 6",
    "App 7",
    "App 8"
};

static void launcher_button_cb(lv_event_t *e)
{
    int game_index = (int)(intptr_t)lv_event_get_user_data(e);
    if (game_action && game_index >= 0 && game_index < game_count) {
        game_action(game_index, game_names[game_index]);
    }
}

void launcher_init(int count)
{
    game_count = count;
    if (game_count < 0) game_count = 0;
    if (game_count > (int)(sizeof(game_names) / sizeof(game_names[0]))) {
        game_count = sizeof(game_names) / sizeof(game_names[0]);
    }
    game_action = NULL;
}

void launcher_set_active(bool active)
{
    int page_count = (game_count + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
    input_set_launcher_state(active, input_get_launcher_page(), page_count > 0 ? page_count : 1);
}

void launcher_build(lv_obj_t *screen, launcher_game_cb_t game_cb)
{
    if (!screen) return;

    game_action = game_cb;
    int page_count = (game_count + GAMES_PER_PAGE - 1) / GAMES_PER_PAGE;
    if (page_count < 1) page_count = 1;

    launcher_set_active(true);
    int page = input_get_launcher_page();
    int start = page * GAMES_PER_PAGE;

    for (int slot = 0; slot < GAMES_PER_PAGE; slot++) {
        int game_index = start + slot;
        if (game_index >= game_count) break;

        int col = slot % 2;
        int row = slot / 2;

        lv_obj_t *button = lv_button_create(screen);
        lv_obj_set_size(button, 185, 120);
        lv_obj_align(button, LV_ALIGN_TOP_LEFT, 38 + col * 205, 82 + row * 145);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x30343B), LV_PART_MAIN);

        lv_obj_t *icon = lv_label_create(button);
        if (game_index == 0) {
            lv_label_set_text(icon, "X   O");
            lv_obj_set_style_text_color(icon, lv_color_hex(0x35E0FF), 0);
        } else if (game_index == 1) {
            lv_label_set_text(icon, "S");
            lv_obj_set_style_text_color(icon, lv_color_hex(0x00C850), 0);
        } else if (game_index == 2) {
            lv_label_set_text(icon, "E");
            lv_obj_set_style_text_color(icon, lv_color_hex(0xFF9F43), 0);
        } else {
            lv_label_set_text(icon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(icon, lv_color_hex(0x7C5CFF), 0);
        }
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 10);

        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, game_names[game_index]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
        lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -12);
        lv_obj_add_event_cb(button, launcher_button_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)game_index);
        lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
    }

    for (int i = 0; i < page_count; i++) {
        bool selected = i == page;
        lv_obj_t *dot = lv_obj_create(screen);
        lv_obj_set_size(dot, selected ? 10 : 7, selected ? 10 : 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, selected ? lv_color_hex(0x20A050) : lv_color_hex(0x60656D), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(dot, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID,
                     (i - (page_count - 1) / 2) * 18, -18);
    }
}
