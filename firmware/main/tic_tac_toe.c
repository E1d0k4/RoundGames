#include "tic_tac_toe.h"

#include "audio.h"
#include "language.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_random.h"

#define BOARD_SIZE 3

typedef enum {
    TTT_MODE_CLASSIC = 0,
    TTT_MODE_ENDLESS,
    TTT_MODE_TWO_PLAYER,
    TTT_MODE_TWO_PLAYER_ENDLESS
} ttt_mode_t;

static lv_obj_t *screen;
static lv_obj_t *board_buttons[BOARD_SIZE][BOARD_SIZE];
static lv_obj_t *status_label;
static lv_obj_t *score_label;
static lv_obj_t *mode_label;
static lv_obj_t *menu_panel;
static lv_obj_t *board_frame;
static lv_timer_t *neon_timer;
static uint32_t neon_phase;

static tic_tac_toe_back_cb_t back_callback;
static char board[BOARD_SIZE][BOARD_SIZE];
static bool game_over;
static char current_player;
static ttt_mode_t mode = TTT_MODE_CLASSIC;
static int player_score;
static int opponent_score;

static const char *tr(const char *en, const char *de)
{
    return language_tr(en, de);
}

static const char *mode_name_en(ttt_mode_t value)
{
    switch (value) {
        case TTT_MODE_ENDLESS: return "Endless";
        case TTT_MODE_TWO_PLAYER: return "2 Players";
        case TTT_MODE_TWO_PLAYER_ENDLESS: return "2P Endless";
        default: return "Classic";
    }
}

static const char *mode_name_de(ttt_mode_t value)
{
    switch (value) {
        case TTT_MODE_ENDLESS: return "Unendlich";
        case TTT_MODE_TWO_PLAYER: return "2 Spieler";
        case TTT_MODE_TWO_PLAYER_ENDLESS: return "2P Unendlich";
        default: return "Klassisch";
    }
}

static void board_reset(void)
{
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) board[row][col] = 0;
    }
    current_player = 'X';
    game_over = false;
}

static bool board_has_winner(char mark)
{
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i][0] == mark && board[i][1] == mark && board[i][2] == mark) return true;
        if (board[0][i] == mark && board[1][i] == mark && board[2][i] == mark) return true;
    }
    return (board[0][0] == mark && board[1][1] == mark && board[2][2] == mark) ||
           (board[0][2] == mark && board[1][1] == mark && board[2][0] == mark);
}

static bool board_full(void)
{
    for (int row = 0; row < BOARD_SIZE; row++)
        for (int col = 0; col < BOARD_SIZE; col++)
            if (board[row][col] == 0) return false;
    return true;
}


static int minimax(char turn, int depth)
{
    if (board_has_winner('O')) return 10 - depth;
    if (board_has_winner('X')) return depth - 10;
    if (board_full()) return 0;

    int best = (turn == 'O') ? -100 : 100;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] != 0) continue;
            board[r][c] = turn;
            int value = minimax(turn == 'O' ? 'X' : 'O', depth + 1);
            board[r][c] = 0;
            if (turn == 'O') {
                if (value > best) best = value;
            } else if (value < best) {
                best = value;
            }
        }
    }
    return best;
}

static void ai_move(void)
{
    int best_score = -100;
    int best_moves[9];
    int best_count = 0;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] != 0) continue;
            board[r][c] = 'O';
            int value = minimax('X', 0);
            board[r][c] = 0;
            if (value > best_score) {
                best_score = value;
                best_count = 0;
                best_moves[best_count++] = r * BOARD_SIZE + c;
            } else if (value == best_score) {
                best_moves[best_count++] = r * BOARD_SIZE + c;
            }
        }
    }

    if (best_count > 0) {
        int pick = best_moves[esp_random() % best_count];
        board[pick / BOARD_SIZE][pick % BOARD_SIZE] = 'O';
    }
}

static void update_cell(int row, int col)
{
    lv_obj_t *label = lv_obj_get_child(board_buttons[row][col], 0);
    if (!label) return;

    char text[2] = { board[row][col] ? board[row][col] : ' ', '\0' };
    lv_label_set_text(label, text);

    if (board[row][col] == 'X') {
        lv_obj_set_style_text_color(label, lv_color_hex(0x20A8FF), 0);
        lv_obj_set_style_shadow_color(label, lv_color_hex(0x20A8FF), 0);
        lv_obj_set_style_shadow_opa(label, LV_OPA_90, 0);
        lv_obj_set_style_shadow_width(label, 20, 0);
    } else if (board[row][col] == 'O') {
        lv_obj_set_style_text_color(label, lv_color_hex(0xFF3045), 0);
        lv_obj_set_style_shadow_color(label, lv_color_hex(0xFF3045), 0);
        lv_obj_set_style_shadow_opa(label, LV_OPA_90, 0);
        lv_obj_set_style_shadow_width(label, 20, 0);
    } else {
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_shadow_opa(label, LV_OPA_TRANSP, 0);
    }
}

static void update_all_cells(void)
{
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int c = 0; c < BOARD_SIZE; c++)
            update_cell(r, c);
}

static void update_header(void)
{
    if (mode_label) lv_label_set_text_fmt(mode_label, "%s", tr(mode_name_en(mode), mode_name_de(mode)));
    if (score_label) {
        if (mode == TTT_MODE_CLASSIC || mode == TTT_MODE_ENDLESS)
            lv_label_set_text_fmt(score_label, "X  %d   :   %d  O", player_score, opponent_score);
        else
            lv_label_set_text_fmt(score_label, "X  %d   :   %d  O", player_score, opponent_score);
    }
}

static void set_status(const char *en, const char *de)
{
    if (status_label) lv_label_set_text(status_label, tr(en, de));
}

static void start_round(void)
{
    board_reset();
    update_all_cells();
    update_header();
    set_status(tr("Your turn • X", "Du bist dran • X"), tr("Du bist dran • X", "Du bist dran • X"));
}

static void new_game(void)
{
    player_score = 0;
    opponent_score = 0;
    start_round();
    audio_play_test_tone(30, false);
}

static void finish_round(char winner)
{
    game_over = true;

    if (winner == 'X') {
        player_score++;
        set_status("YOU WIN", "DU GEWINNST");
        audio_play_test_tone(60, false);
    } else if (winner == 'O') {
        opponent_score++;
        set_status(mode == TTT_MODE_TWO_PLAYER || mode == TTT_MODE_TWO_PLAYER_ENDLESS ?
                   "PLAYER O WINS" : "COMPUTER WINS",
                   mode == TTT_MODE_TWO_PLAYER || mode == TTT_MODE_TWO_PLAYER_ENDLESS ?
                   "SPIELER O GEWINNT" : "COMPUTER GEWINNT");
        audio_play_test_tone(45, false);
    } else {
        set_status("DRAW", "UNENTSCHIEDEN");
    }
    update_header();

    if (mode == TTT_MODE_ENDLESS || mode == TTT_MODE_TWO_PLAYER_ENDLESS) {
        /* Keep the score and make the next round one tap away. */
    }
}

static void cell_cb(lv_event_t *e)
{
    if (game_over) return;

    int index = (int)(intptr_t)lv_event_get_user_data(e);
    int row = index / BOARD_SIZE;
    int col = index % BOARD_SIZE;
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE || board[row][col] != 0) return;

    if ((mode == TTT_MODE_CLASSIC || mode == TTT_MODE_ENDLESS) && current_player != 'X') return;

    board[row][col] = current_player;
    update_cell(row, col);
    audio_play_test_tone(25, false);

    if (board_has_winner(current_player)) {
        finish_round(current_player);
        return;
    }
    if (board_full()) {
        finish_round(0);
        return;
    }

    if (mode == TTT_MODE_CLASSIC || mode == TTT_MODE_ENDLESS) {
        current_player = 'O';
        set_status("Computer thinking...", "Computer denkt...");
        ai_move();
        update_all_cells();

        if (board_has_winner('O')) {
            finish_round('O');
            return;
        }
        if (board_full()) {
            finish_round(0);
            return;
        }
        current_player = 'X';
        set_status("Your turn • X", "Du bist dran • X");
    } else {
        current_player = (current_player == 'X') ? 'O' : 'X';
        if (current_player == 'X') set_status("Player X", "Spieler X");
        else set_status("Player O", "Spieler O");
    }
}

static void next_round_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    start_round();
}

static void close_menu(void)
{
    if (menu_panel) lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
}

static void menu_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (menu_panel) lv_obj_clear_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
}

static void back_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    close_menu();
    if (back_callback) back_callback();
}

static void new_game_button_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    close_menu();
    new_game();
}

static void mode_select_cb(lv_event_t *e)
{
    int selected = (int)(intptr_t)lv_event_get_user_data(e);
    mode = (ttt_mode_t)selected;
    close_menu();
    new_game();
}

static void neon_animation_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    neon_phase += 5;
    if (!board_frame) return;

    int pulse = (neon_phase % 180 < 90) ? 88 : 62;
    lv_obj_set_style_shadow_opa(board_frame, pulse, 0);
    lv_obj_set_style_shadow_width(board_frame, (pulse > 80) ? 18 : 12, 0);
}

static void cleanup_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (neon_timer) {
        lv_timer_del(neon_timer);
        neon_timer = NULL;
    }
    board_frame = NULL;
    menu_panel = NULL;
    status_label = NULL;
    score_label = NULL;
    mode_label = NULL;
}

static lv_obj_t *make_action_button(lv_obj_t *parent, const char *text, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x101B28), LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(0x39FF66), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(button, lv_color_hex(0x39FF66), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(button, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 8, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_18, 0);
    lv_obj_center(label);
    return button;
}

static void build_menu(lv_obj_t *parent)
{
    menu_panel = lv_obj_create(parent);
    lv_obj_set_size(menu_panel, 300, 340);
    lv_obj_center(menu_panel);
    lv_obj_set_style_bg_color(menu_panel, lv_color_hex(0x071018), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(menu_panel, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_border_color(menu_panel, lv_color_hex(0x39FF66), LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(menu_panel, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_panel, 14, LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(menu_panel);
    lv_label_set_text(title, tr("GAME MODE", "SPIELMODUS"));
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x39FF66), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    const char *labels_en[] = {"Classic", "Endless", "2 Players", "2P Endless"};
    const char *labels_de[] = {"Klassisch", "Unendlich", "2 Spieler", "2P Unendlich"};

    for (int i = 0; i < 4; i++) {
        lv_obj_t *button = make_action_button(menu_panel,
            tr(labels_en[i], labels_de[i]), 250, 48);
        lv_obj_align(button, LV_ALIGN_TOP_MID, 0, 45 + i * 57);
        lv_obj_add_event_cb(button, mode_select_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *close = make_action_button(menu_panel, LV_SYMBOL_CLOSE, 44, 38);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(close, menu_button_cb, LV_EVENT_CLICKED, NULL);
}

void tic_tac_toe_open(lv_obj_t *target_screen, tic_tac_toe_back_cb_t back_cb)
{
    if (!target_screen) return;

    screen = target_screen;
    back_callback = back_cb;
    player_score = 0;
    opponent_score = 0;
    mode = TTT_MODE_CLASSIC;
    board_reset();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "TIC TAC TOE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    mode_label = lv_label_create(screen);
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0x39FF66), 0);
    lv_obj_align(mode_label, LV_ALIGN_TOP_MID, 0, 47);

    score_label = lv_label_create(screen);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(score_label, lv_color_hex(0xB8C7D9), 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 68);

    status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 92);

    lv_obj_t *back = make_action_button(screen, LV_SYMBOL_LEFT, 40, 40);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 8, 10);
    lv_obj_add_event_cb(back, back_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *menu = make_action_button(screen, LV_SYMBOL_LIST, 40, 40);
    lv_obj_align(menu, LV_ALIGN_TOP_RIGHT, -8, 10);
    lv_obj_add_event_cb(menu, menu_button_cb, LV_EVENT_CLICKED, NULL);

    /* The board is a real centered object. Touch targets and neon cage share its coordinate system. */
    board_frame = lv_obj_create(screen);
    lv_obj_set_size(board_frame, 292, 292);
    lv_obj_align(board_frame, LV_ALIGN_CENTER, 0, 28);
    lv_obj_set_style_bg_opa(board_frame, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(board_frame, lv_color_hex(0x39FF66), LV_PART_MAIN);
    lv_obj_set_style_border_width(board_frame, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(board_frame, 22, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(board_frame, lv_color_hex(0x39FF66), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(board_frame, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(board_frame, 14, LV_PART_MAIN);
    lv_obj_clear_flag(board_frame, LV_OBJ_FLAG_SCROLLABLE);

    const int cell = 84;
    const int gap = 10;
    const int origin = 10;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            lv_obj_t *button = lv_button_create(board_frame);
            lv_obj_set_size(button, cell, cell);
            lv_obj_set_pos(button, origin + c * (cell + gap), origin + r * (cell + gap));
            lv_obj_set_style_bg_color(button, lv_color_hex(0x08121C), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(button, LV_OPA_70, LV_PART_MAIN);
            lv_obj_set_style_border_color(button, lv_color_hex(0x1C3142), LV_PART_MAIN);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
            lv_obj_set_style_radius(button, 14, LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);

            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, " ");
            lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
            lv_obj_center(label);

            board_buttons[r][c] = button;
            lv_obj_add_event_cb(button, cell_cb, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(r * BOARD_SIZE + c));
        }
    }

    /* Four subtle lightning rails, inside the centered frame. */
    lv_obj_t *rail1 = lv_obj_create(board_frame);
    lv_obj_set_size(rail1, 2, 250);
    lv_obj_set_pos(rail1, 96, 20);
    lv_obj_set_style_bg_color(rail1, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_shadow_color(rail1, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_shadow_width(rail1, 10, 0);
    lv_obj_set_style_shadow_opa(rail1, LV_OPA_70, 0);
    lv_obj_t *rail2 = lv_obj_create(board_frame);
    lv_obj_set_size(rail2, 2, 250);
    lv_obj_set_pos(rail2, 194, 20);
    lv_obj_set_style_bg_color(rail2, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_shadow_color(rail2, lv_color_hex(0x39FF66), 0);
    lv_obj_set_style_shadow_width(rail2, 10, 0);
    lv_obj_set_style_shadow_opa(rail2, LV_OPA_70, 0);
    lv_obj_move_to_index(rail1, 0);
    lv_obj_move_to_index(rail2, 0);

    lv_obj_t *new_game = make_action_button(screen, LV_SYMBOL_REFRESH, 42, 42);
    lv_obj_align(new_game, LV_ALIGN_BOTTOM_RIGHT, -8, -10);
    lv_obj_add_event_cb(new_game, new_game_button_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *round = make_action_button(screen, LV_SYMBOL_PLAY, 42, 42);
    lv_obj_align(round, LV_ALIGN_BOTTOM_LEFT, 8, -10);
    lv_obj_add_event_cb(round, next_round_cb, LV_EVENT_CLICKED, NULL);

    build_menu(screen);
    lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);

    update_header();
    set_status("Your turn • X", "Du bist dran • X");

    neon_timer = lv_timer_create(neon_animation_tick, 45, NULL);
    lv_obj_add_event_cb(screen, cleanup_cb, LV_EVENT_DELETE, NULL);
}
