#include "tic_tac_toe.h"

#include "audio.h"
#include "language.h"

#include <stdbool.h>
#include <stdint.h>

#define BOARD_SIZE 3

static lv_obj_t *board_buttons[BOARD_SIZE][BOARD_SIZE];
static lv_obj_t *status_label;
static lv_obj_t *action_panel;
static lv_obj_t *neon_lines[4];
static lv_timer_t *neon_timer;
static uint32_t neon_phase;
static tic_tac_toe_back_cb_t back_callback;

static char board[BOARD_SIZE][BOARD_SIZE];
static bool player_turn;
static bool game_over;

static const char *tr(const char *en, const char *de)
{
    return language_tr(en, de);
}

static void board_reset(void)
{
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            board[row][col] = 0;
        }
    }
    player_turn = true;
    game_over = false;
}

static bool board_has_winner(char mark)
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i][0] == mark && board[i][1] == mark && board[i][2] == mark) return true;
        if (board[0][i] == mark && board[1][i] == mark && board[2][i] == mark) return true;
    }

    if (board[0][0] == mark && board[1][1] == mark && board[2][2] == mark) return true;
    if (board[0][2] == mark && board[1][1] == mark && board[2][0] == mark) return true;

    return false;
}

static bool board_full(void)
{
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            if (board[row][col] == 0) return false;
        }
    }
    return true;
}

static lv_obj_t *add_neon_line(lv_obj_t *screen, const lv_point_precise_t *points, uint32_t count)
{
    lv_obj_t *line = lv_line_create(screen);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_line_width(line, 3, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_set_style_line_opa(line, LV_OPA_100, 0);
    lv_obj_set_style_shadow_color(line, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_shadow_opa(line, LV_OPA_80, 0);
    lv_obj_set_style_shadow_width(line, 12, 0);
    return line;
}

static void neon_animation_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    neon_phase += 7;

    for (int i = 0; i < 4; i++) {
        if (!neon_lines[i]) continue;
        uint32_t phase = neon_phase + (uint32_t)(i * 70);
        int wobble = (phase % 240 < 120) ? 2 : -2;
        int pulse = (phase % 180 < 90) ? 100 : 75;
        lv_obj_set_style_translate_x(neon_lines[i], (i < 2) ? wobble : 0, 0);
        lv_obj_set_style_translate_y(neon_lines[i], (i >= 2) ? wobble : 0, 0);
        lv_obj_set_style_shadow_width(neon_lines[i], (pulse > 90) ? 16 : 10, 0);
        lv_obj_set_style_shadow_opa(neon_lines[i], pulse, 0);
    }
}

static void neon_cleanup_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (neon_timer) {
        lv_timer_del(neon_timer);
        neon_timer = NULL;
    }
    for (int i = 0; i < 4; i++) neon_lines[i] = NULL;
}

static void update_cell(int row, int col)
{
    char text[2] = { board[row][col], '\0' };
    lv_obj_t *label = lv_obj_get_child(board_buttons[row][col], 0);

    if (label) {
        lv_label_set_text(label, text);
        if (board[row][col] == 'X') {
            lv_obj_set_style_text_color(label, lv_color_hex(0x20A8FF), 0);
            lv_obj_set_style_text_shadow_color(label, lv_color_hex(0x20A8FF), 0);
            lv_obj_set_style_text_shadow_opa(label, LV_OPA_90, 0);
            lv_obj_set_style_text_shadow_width(label, 18, 0);
        } else if (board[row][col] == 'O') {
            lv_obj_set_style_text_color(label, lv_color_hex(0xFF3030), 0);
            lv_obj_set_style_text_shadow_color(label, lv_color_hex(0xFF3030), 0);
            lv_obj_set_style_text_shadow_opa(label, LV_OPA_90, 0);
            lv_obj_set_style_text_shadow_width(label, 18, 0);
        } else {
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_text_shadow_opa(label, LV_OPA_TRANSP, 0);
        }
    }
}

static void set_status(const char *en, const char *de)
{
    if (status_label) {
        lv_label_set_text(status_label, tr(en, de));
    }
}

static void new_game_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    board_reset();

    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            update_cell(row, col);
        }
    }

    set_status("Your turn: X", "Du bist dran: X");
    audio_play_test_tone(30, false);
}

static void cell_cb(lv_event_t *e)
{
    if (game_over || !player_turn) return;

    int index = (int)(intptr_t)lv_event_get_user_data(e);
    int row = index / BOARD_SIZE;
    int col = index % BOARD_SIZE;

    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) return;
    if (board[row][col] != 0) return;

    board[row][col] = 'X';
    update_cell(row, col);
    audio_play_test_tone(25, false);

    if (board_has_winner('X')) {
        game_over = true;
        set_status("You win!", "Du gewinnst!");
        audio_play_test_tone(60, false);
        return;
    }

    if (board_full()) {
        game_over = true;
        set_status("Draw!", "Unentschieden!");
        return;
    }

    player_turn = false;
    set_status("Your turn is over", "Dein Zug ist fertig");

    /* Simple deterministic AI: take the first free field. */
    for (int r = 0; r < BOARD_SIZE && !player_turn; r++) {
        for (int c = 0; c < BOARD_SIZE && !player_turn; c++) {
            if (board[r][c] == 0) {
                board[r][c] = 'O';
                update_cell(r, c);
                player_turn = true;
            }
        }
    }

    if (board_has_winner('O')) {
        game_over = true;
        set_status("Computer wins!", "Computer gewinnt!");
        audio_play_test_tone(45, false);
        return;
    }

    if (board_full()) {
        game_over = true;
        set_status("Draw!", "Unentschieden!");
        return;
    }

    set_status("Your turn: X", "Du bist dran: X");
}

static void hide_action_panel(void)
{
    if (action_panel) {
        lv_obj_add_flag(action_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

static void back_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    hide_action_panel();
    if (back_callback) back_callback();
}

static void action_panel_new_game_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    hide_action_panel();
    new_game_cb(NULL);
}

static void game_gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    if (lv_indev_get_gesture_dir(indev) == LV_DIR_TOP && action_panel) {
        lv_obj_clear_flag(action_panel, LV_OBJ_FLAG_HIDDEN);
    }
}

void tic_tac_toe_open(lv_obj_t *screen, tic_tac_toe_back_cb_t back_cb)
{
    if (!screen) return;

    back_callback = back_cb;
    board_reset();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Tic-Tac-Toe");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, tr("Your turn: X", "Du bist dran: X"));
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_18, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 70);

    const int size = 88;
    const int gap = 8;
    const int start_x = -140;
    const int start_y = 72;
    const int cell = 88;
    const int step = size + gap;

    /* Transparent touch targets: the board itself is now a neon-green lightning cage. */
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            lv_obj_t *button = lv_button_create(screen);
            lv_obj_set_size(button, size, size);
            lv_obj_align(button, LV_ALIGN_TOP_MID,
                         start_x + col * step,
                         start_y + row * step);
            lv_obj_set_style_bg_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);

            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, "");
            lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_center(label);

            board_buttons[row][col] = button;
            lv_obj_add_event_cb(button, cell_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(row * BOARD_SIZE + col));
        }
    }

    /*
     * Jagged green neon lines form the 3x3 cage instead of nine solid fields.
     * The small zig-zags intentionally make the grid look like energized lightning.
     */
    const int left = 40;
    const int top = start_y;
    const int right = left + 3 * cell + 2 * gap;
    const int bottom = top + 3 * cell + 2 * gap;

    static lv_point_precise_t v1[] = {{left + cell + gap / 2, top - 4},
                                       {left + cell + gap / 2 + 4, top + 18},
                                       {left + cell + gap / 2 - 3, top + 39},
                                       {left + cell + gap / 2 + 5, top + 60},
                                       {left + cell + gap / 2 - 2, top + 82},
                                       {left + cell + gap / 2 + 3, bottom + 4}};
    static lv_point_precise_t v2[] = {{left + 2 * cell + gap + gap / 2, top - 4},
                                       {left + 2 * cell + gap + gap / 2 - 4, top + 22},
                                       {left + 2 * cell + gap + gap / 2 + 3, top + 44},
                                       {left + 2 * cell + gap + gap / 2 - 5, top + 66},
                                       {left + 2 * cell + gap + gap / 2 + 2, top + 88},
                                       {left + 2 * cell + gap + gap / 2 - 3, bottom + 4}};
    static lv_point_precise_t h1[] = {{left - 4, top + cell + gap / 2},
                                       {left + 24, top + cell + gap / 2 - 4},
                                       {left + 52, top + cell + gap / 2 + 3},
                                       {left + 80, top + cell + gap / 2 - 3},
                                       {left + 108, top + cell + gap / 2 + 4},
                                       {left + 136, top + cell + gap / 2 - 2},
                                       {left + 164, top + cell + gap / 2 + 3},
                                       {left + 192, top + cell + gap / 2 - 3},
                                       {left + 220, top + cell + gap / 2 + 3},
                                       {left + 260, top + cell + gap / 2 - 2},
                                       {right + 4, top + cell + gap / 2}};
    static lv_point_precise_t h2[] = {{left - 4, top + 2 * cell + gap + gap / 2},
                                       {left + 24, top + 2 * cell + gap + gap / 2 + 4},
                                       {left + 52, top + 2 * cell + gap + gap / 2 - 3},
                                       {left + 80, top + 2 * cell + gap + gap / 2 + 3},
                                       {left + 108, top + 2 * cell + gap + gap / 2 - 4},
                                       {left + 136, top + 2 * cell + gap + gap / 2 + 2},
                                       {left + 164, top + 2 * cell + gap + gap / 2 - 3},
                                       {left + 192, top + 2 * cell + gap + gap / 2 + 3},
                                       {left + 220, top + 2 * cell + gap + gap / 2 - 3},
                                       {left + 260, top + 2 * cell + gap + gap / 2 + 2},
                                       {right + 4, top + 2 * cell + gap + gap / 2}};

    neon_lines[0] = add_neon_line(screen, v1, sizeof(v1) / sizeof(v1[0]));
    neon_lines[1] = add_neon_line(screen, v2, sizeof(v2) / sizeof(v2[0]));
    neon_lines[2] = add_neon_line(screen, h1, sizeof(h1) / sizeof(h1[0]));
    neon_lines[3] = add_neon_line(screen, h2, sizeof(h2) / sizeof(h2[0]));
    neon_timer = lv_timer_create(neon_animation_tick, 40, NULL);

    /*
     * The action buttons stay hidden to give the board maximum space.
     * Swipe from bottom to top to reveal them.
     */
    action_panel = lv_obj_create(screen);
    lv_obj_set_size(action_panel, 320, 62);
    lv_obj_align(action_panel, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_opa(action_panel, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_pad_all(action_panel, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(action_panel, 0, LV_PART_MAIN);
    lv_obj_add_flag(action_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(action_panel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    lv_obj_t *new_game = lv_button_create(action_panel);
    lv_obj_set_size(new_game, 190, 50);
    lv_obj_align(new_game, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(new_game, lv_color_hex(0x20A050), LV_PART_MAIN);
    lv_obj_t *new_label = lv_label_create(new_game);
    lv_label_set_text(new_label, tr("New game", "Neues Spiel"));
    lv_obj_center(new_label);
    lv_obj_add_event_cb(new_game, action_panel_new_game_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back = lv_button_create(action_panel);
    lv_obj_set_size(back, 105, 50);
    lv_obj_align(back, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_20, 0);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(screen, game_gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(screen, neon_cleanup_cb, LV_EVENT_DELETE, NULL);
}
