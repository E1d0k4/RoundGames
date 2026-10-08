/*

 * Tic Tac Toe fuer Waveshare ESP32-S3-Touch-AMOLED-1.75 (466x466, rund)

 * ---------------------------------------------------------------------

 * - Modi:      Klassik (3 gewinnt, Unentschieden moeglich)

 *              Unendlich (max. 3 Zeichen pro Spieler, das aelteste verschwindet)

 * - Spieler:   1 Spieler (gegen KI, Leicht / Schwer) oder 2 Spieler

 * - Toolkit:   LVGL 8.3 / 8.4 (ESP-IDF, z.B. ueber esp_lvgl_port)

 *

 * Benoetigte sdkconfig-Optionen:

 *   CONFIG_LV_FONT_MONTSERRAT_20=y

 *   CONFIG_LV_FONT_MONTSERRAT_28=y

 *   CONFIG_LV_FONT_MONTSERRAT_40=y

 *

 * Einbindung: Display + Touch + LVGL wie im Waveshare-Beispiel initialisieren,

 * dann (mit LVGL-Lock) ttt_start() aufrufen.

 */

#include <stdlib.h>

#include <stdint.h>

#include <stdbool.h>

#include "tic_tac_toe.h"
#include "language.h"
#include "lvgl.h"

#include "esp_random.h"

/* ------------------------------------------------------------------ */

/*  Farben & Layout                                                    */

/* ------------------------------------------------------------------ */

#define COL_BG      lv_color_hex(0x0B0F1A)

#define COL_CELL    lv_color_hex(0x1B2236)

#define COL_CELL_P  lv_color_hex(0x2A3452)

#define COL_X       lv_color_hex(0x35E0FF)

#define COL_O       lv_color_hex(0xFF5C8A)

#define COL_ACCENT  lv_color_hex(0x7C5CFF)

#define COL_WIN     lv_color_hex(0x2EE59D)

#define COL_TEXT    lv_color_hex(0xE8ECF8)

#define COL_DIM     lv_color_hex(0x7F8AA8)

#define CELL_SIZE   78

#define CELL_GAP    8

#define GRID_LEFT   108

#define GRID_TOP    118

enum { MODE_CLASSIC = 0, MODE_INFINITE = 1 };

enum { LEVEL_EASY = 0, LEVEL_HARD = 1 };

/* ------------------------------------------------------------------ */

/*  Spiellogik                                                         */

/* ------------------------------------------------------------------ */

typedef struct {

int8_t cell[9];      /* 0 leer, 1 = X, 2 = O                       */

int8_t order[3][5];  /* Reihenfolge der Zuege je Spieler (1..2)    */

int8_t count[3];     /* Anzahl gesetzter Zeichen je Spieler        */

int8_t turn;         /* 1 = X, 2 = O                               */

} board_t;

static const int8_t LINES[8][3] = {

    {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}

};

/* Auswahl im Menue */

static int s_mode  = MODE_CLASSIC;

static int s_two   = 0;           /* 0 = 1 Spieler, 1 = 2 Spieler */

static int s_level = LEVEL_HARD;

/* Spielzustand */

static board_t s_b;

static bool    s_over;

static int     s_winner;          /* 0 = Unentschieden */

static int     s_line = -1;

static int     s_score[3];

static int     s_rounds;

/* UI-Objekte */

static lv_obj_t   *s_cell[9];

static lv_obj_t   *s_lbl[9];

static lv_obj_t   *s_status, *s_scorelbl, *s_level_row;

static lv_timer_t *s_ai_timer, *s_round_timer;
static lv_obj_t *s_screen;

static void menu_create(void);

static void game_create(void);

static int winner_line(const board_t *b, int p)

{

for (int l = 0; l < 8; l++) {

if (b->cell[LINES[l][0]] == p && b->cell[LINES[l][1]] == p &&

b->cell[LINES[l][2]] == p) return l;

    }

return -1;

}

static void apply_move(board_t *b, int idx, int mode)

{

int p = b->turn;

if (mode == MODE_INFINITE && b->count[p] == 3) {

int old = b->order[p][0];

b->cell[old] = 0;

b->order[p][0] = b->order[p][1];

b->order[p][1] = b->order[p][2];

b->count[p] = 2;

    }

b->cell[idx] = p;

b->order[p][b->count[p]++] = idx;

b->turn = 3 - p;

}

static bool board_full(const board_t *b)

{

for (int i = 0; i < 9; i++) if (!b->cell[i]) return false;

return true;

}

/* Negamax mit Alpha-Beta. Wert aus Sicht des Spielers, der am Zug ist. */

static int negamax(const board_t *b, int depth, int alpha, int beta, int mode)

{

int best = -100;

bool any = false;

for (int i = 0; i < 9; i++) {

if (b->cell[i]) continue;

any = true;

board_t n = *b;

int p = n.turn;

apply_move(&n, i, mode);

int s;

if (winner_line(&n, p) >= 0) s = 10 + depth;

else if (depth == 0)         s = 0;

else                         s = -negamax(&n, depth - 1, -beta, -alpha, mode);

if (s > best)  best = s;

if (best > alpha) alpha = best;

if (alpha >= beta) break;

    }

return any ? best : 0;

}

static int ai_pick(const board_t *b)

{

int moves[9], n = 0;

for (int i = 0; i < 9; i++) if (!b->cell[i]) moves[n++] = i;

if (!n) return -1;

if (s_level == LEVEL_EASY && (esp_random() % 100) < 50)

return moves[esp_random() % n];

int depth = (s_level == LEVEL_EASY) ? 2 : (s_mode == MODE_CLASSIC ? 9 : 6);

int best = -1000, cand[9], nc = 0;

for (int k = 0; k < n; k++) {

board_t nb = *b;

int p = nb.turn;

apply_move(&nb, moves[k], s_mode);

int s;

if (winner_line(&nb, p) >= 0) s = 10 + depth;

else if (depth <= 1)          s = 0;

else                          s = -negamax(&nb, depth - 1, -100, 100, s_mode);

if (s > best) { best = s; nc = 0; }

if (s == best) cand[nc++] = moves[k];

    }

return cand[esp_random() % nc];

}

/* ------------------------------------------------------------------ */

/*  UI-Helfer                                                          */

/* ------------------------------------------------------------------ */

static void kill_timers(void)

{

if (s_ai_timer)    { lv_timer_del(s_ai_timer);    s_ai_timer = NULL; }

if (s_round_timer) { lv_timer_del(s_round_timer); s_round_timer = NULL; }

}

static void prepare_screen(void)

{

kill_timers();

lv_obj_t *scr = s_screen;

lv_obj_clean(scr);

lv_obj_set_style_bg_color(scr, COL_BG, 0);

lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text, int w, int h,

lv_color_t col, lv_event_cb_t cb)

{

lv_obj_t *btn = lv_btn_create(parent);

lv_obj_set_size(btn, w, h);

lv_obj_set_style_bg_color(btn, col, 0);

lv_obj_set_style_radius(btn, h / 2, 0);

lv_obj_set_style_shadow_width(btn, 0, 0);

lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

lv_obj_t *l = lv_label_create(btn);

lv_label_set_text(l, text);

lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);

lv_obj_set_style_text_color(l, COL_TEXT, 0);

lv_obj_center(l);

return btn;

}

static void zoom_cb(void *obj, int32_t v)

{

lv_obj_set_style_transform_zoom((lv_obj_t *)obj, v, 0);

}

static void pop_anim(lv_obj_t *o)

{

lv_anim_t a;

lv_anim_init(&a);

lv_anim_set_var(&a, o);

lv_anim_set_exec_cb(&a, zoom_cb);

lv_anim_set_values(&a, 150, 256);

lv_anim_set_time(&a, 280);

lv_anim_set_path_cb(&a, lv_anim_path_overshoot);

lv_anim_start(&a);

}

/* ------------------------------------------------------------------ */

/*  Menue                                                              */

/* ------------------------------------------------------------------ */

static const char *MAP_MODE[]  = { "Klassik", "Unendlich", "" };

static const char *MAP_PLAYER[] = { "1 Spieler", "2 Spieler", "" };

static const char *MAP_LEVEL[] = { "Leicht", "Schwer", "" };

static void sel_cb(lv_event_t *e)

{

lv_obj_t *m = lv_event_get_target(e);

int *dst = (int *)lv_event_get_user_data(e);

*dst = (int)lv_btnmatrix_get_selected_btn(m);

if (dst == &s_two && s_level_row) {

if (s_two) lv_obj_add_flag(s_level_row, LV_OBJ_FLAG_HIDDEN);

else       lv_obj_clear_flag(s_level_row, LV_OBJ_FLAG_HIDDEN);

    }

}

static lv_obj_t *make_toggle(lv_obj_t *parent, const char **map, int y,

int selected, int *target)

{

lv_obj_t *m = lv_btnmatrix_create(parent);

lv_btnmatrix_set_map(m, map);

lv_btnmatrix_set_btn_ctrl_all(m, LV_BTNMATRIX_CTRL_CHECKABLE);

lv_btnmatrix_set_one_checked(m, true);

lv_btnmatrix_set_btn_ctrl(m, selected, LV_BTNMATRIX_CTRL_CHECKED);

lv_obj_set_size(m, 290, 56);

lv_obj_align(m, LV_ALIGN_TOP_MID, 0, y);

lv_obj_set_style_bg_opa(m, LV_OPA_TRANSP, LV_PART_MAIN);

lv_obj_set_style_border_width(m, 0, LV_PART_MAIN);

lv_obj_set_style_pad_all(m, 0, LV_PART_MAIN);

lv_obj_set_style_pad_column(m, 10, LV_PART_MAIN);

lv_obj_set_style_bg_color(m, COL_CELL, LV_PART_ITEMS);

lv_obj_set_style_radius(m, 18, LV_PART_ITEMS);

lv_obj_set_style_border_width(m, 0, LV_PART_ITEMS);

lv_obj_set_style_shadow_width(m, 0, LV_PART_ITEMS);

lv_obj_set_style_text_color(m, COL_DIM, LV_PART_ITEMS);

lv_obj_set_style_text_font(m, &lv_font_montserrat_20, LV_PART_ITEMS);

lv_obj_set_style_bg_color(m, COL_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);

lv_obj_set_style_text_color(m, COL_TEXT, LV_PART_ITEMS | LV_STATE_CHECKED);

lv_obj_add_event_cb(m, sel_cb, LV_EVENT_VALUE_CHANGED, target);

return m;

}

static void start_cb(lv_event_t *e)

{

    (void)e;

s_score[1] = s_score[2] = 0;

s_rounds = 0;

game_create();

}

static void menu_create(void)

{

prepare_screen();

lv_obj_t *scr = lv_scr_act();

lv_obj_t *title = lv_label_create(scr);

lv_label_set_recolor(title, true);

lv_label_set_text(title, "#35E0FF TIC#  #E8ECF8 TAC#  #FF5C8A TOE#");

lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 52);

make_toggle(scr, MAP_MODE,   108, s_mode, &s_mode);

make_toggle(scr, MAP_PLAYER, 176, s_two,  &s_two);

s_level_row = make_toggle(scr, MAP_LEVEL, 244, s_level, &s_level);

if (s_two) lv_obj_add_flag(s_level_row, LV_OBJ_FLAG_HIDDEN);

lv_obj_t *start = make_button(scr, LV_SYMBOL_PLAY "  Start", 210, 60, COL_ACCENT, start_cb);

lv_obj_align(start, LV_ALIGN_TOP_MID, 0, 322);

lv_obj_set_style_bg_grad_color(start, COL_X, 0);

lv_obj_set_style_bg_grad_dir(start, LV_GRAD_DIR_HOR, 0);

lv_obj_t *hint = lv_label_create(scr);

lv_label_set_text(hint, "Unendlich: nur 3 Zeichen pro Spieler");

lv_obj_set_style_text_color(hint, COL_DIM, 0);

lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);

lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 400);

}

/* ------------------------------------------------------------------ */

/*  Spielansicht                                                       */

/* ------------------------------------------------------------------ */

static void render(void)

{

int fade = -1;

if (!s_over && s_mode == MODE_INFINITE && s_b.count[s_b.turn] == 3)

fade = s_b.order[s_b.turn][0];   /* verschwindet beim naechsten Zug */

for (int i = 0; i < 9; i++) {

int c = s_b.cell[i];

lv_label_set_text(s_lbl[i], c == 1 ? "X" : c == 2 ? "O" : "");

lv_obj_set_style_text_color(s_lbl[i], c == 1 ? COL_X : COL_O, 0);

lv_obj_set_style_text_opa(s_lbl[i], i == fade ? LV_OPA_30 : LV_OPA_COVER, 0);

bool w = s_line >= 0 && (i == LINES[s_line][0] || i == LINES[s_line][1] ||

i == LINES[s_line][2]);

lv_obj_set_style_border_width(s_cell[i], w ? 4 : 0, 0);

    }

/* Status */

const char *txt;

lv_color_t col = COL_TEXT;

if (s_over) {

if (s_winner == 0)      { txt = "Unentschieden"; col = COL_DIM; }

else if (s_two)         { txt = s_winner == 1 ? "X gewinnt!" : "O gewinnt!"; col = COL_WIN; }

else                    { txt = s_winner == 1 ? "Du gewinnst!" : "KI gewinnt!"; col = COL_WIN; }

    } else {

if (s_two) { txt = s_b.turn == 1 ? "X ist dran" : "O ist dran"; }

else       { txt = s_b.turn == 1 ? "Du (X) bist dran" : "KI denkt..."; }

col = s_b.turn == 1 ? COL_X : COL_O;

    }

lv_label_set_text(s_status, txt);

lv_obj_set_style_text_color(s_status, col, 0);

lv_label_set_text_fmt(s_scorelbl, "X  %d  :  %d  O", s_score[1], s_score[2]);

}

static void ai_cb(lv_timer_t *t);

static void do_move(int idx);

static void schedule_ai(void)

{

if (!s_two && !s_over && s_b.turn == 2 && !s_ai_timer) {

s_ai_timer = lv_timer_create(ai_cb, 500, NULL);

lv_timer_set_repeat_count(s_ai_timer, 1);

    }

}

static void reset_board(void)

{

for (int i = 0; i < 9; i++) s_b.cell[i] = 0;

for (int p = 0; p < 3; p++) {

s_b.count[p] = 0;

for (int k = 0; k < 5; k++) s_b.order[p][k] = 0;

    }

s_b.turn = (s_rounds % 2 == 0) ? 1 : 2;   /* Anfaenger wechselt */

s_over = false;

s_winner = 0;

s_line = -1;

}

static void round_cb(lv_timer_t *t)

{

    (void)t;

s_round_timer = NULL;

s_rounds++;

reset_board();

render();

schedule_ai();

}

static void ai_cb(lv_timer_t *t)

{

    (void)t;

s_ai_timer = NULL;

if (s_over) return;

int m = ai_pick(&s_b);

if (m >= 0) do_move(m);

}

static void do_move(int idx)

{

int p = s_b.turn;

apply_move(&s_b, idx, s_mode);

int l = winner_line(&s_b, p);

if (l >= 0) {

s_over = true; s_winner = p; s_line = l; s_score[p]++;

    } else if (s_mode == MODE_CLASSIC && board_full(&s_b)) {

s_over = true; s_winner = 0;

    }

render();

pop_anim(s_cell[idx]);

if (s_over) {

s_round_timer = lv_timer_create(round_cb, 2800, NULL);

lv_timer_set_repeat_count(s_round_timer, 1);

    } else {

schedule_ai();

    }

}

static void cell_cb(lv_event_t *e)

{

int idx = (int)(intptr_t)lv_event_get_user_data(e);

if (s_over || s_ai_timer || s_b.cell[idx]) return;

if (!s_two && s_b.turn == 2) return;

do_move(idx);

}

static void home_btn_cb(lv_event_t *e)
{
    (void)e;
    kill_timers();
    menu_create();
}

static void restart_cb(lv_event_t *e)

{

    (void)e;

kill_timers();

s_score[1] = s_score[2] = 0;

s_rounds = 0;

reset_board();

render();

schedule_ai();

}

static void game_create(void)

{

prepare_screen();

lv_obj_t *scr = lv_scr_act();

s_status = lv_label_create(scr);

lv_obj_set_style_text_font(s_status, &lv_font_montserrat_22, 0);

lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 52);

s_scorelbl = lv_label_create(scr);

lv_obj_set_style_text_font(s_scorelbl, &lv_font_montserrat_20, 0);

lv_obj_set_style_text_color(s_scorelbl, COL_DIM, 0);

lv_obj_align(s_scorelbl, LV_ALIGN_TOP_MID, 0, 88);

for (int i = 0; i < 9; i++) {

int r = i / 3, c = i % 3;

lv_obj_t *b = lv_btn_create(scr);

lv_obj_set_size(b, CELL_SIZE, CELL_SIZE);

lv_obj_set_pos(b, GRID_LEFT + c * (CELL_SIZE + CELL_GAP),

GRID_TOP  + r * (CELL_SIZE + CELL_GAP));

lv_obj_set_style_bg_color(b, COL_CELL, 0);

lv_obj_set_style_bg_color(b, COL_CELL_P, LV_STATE_PRESSED);

lv_obj_set_style_radius(b, 22, 0);

lv_obj_set_style_shadow_width(b, 0, 0);

lv_obj_set_style_border_color(b, COL_WIN, 0);

lv_obj_set_style_border_width(b, 0, 0);

lv_obj_set_style_transform_pivot_x(b, CELL_SIZE / 2, 0);

lv_obj_set_style_transform_pivot_y(b, CELL_SIZE / 2, 0);

lv_obj_add_event_cb(b, cell_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

lv_obj_t *l = lv_label_create(b);

lv_obj_set_style_text_font(l, &lv_font_montserrat_48, 0);

lv_label_set_text(l, "");

lv_obj_center(l);

s_cell[i] = b;

s_lbl[i]  = l;

    }

lv_obj_t *bm = make_button(scr, LV_SYMBOL_HOME, 100, 42, COL_CELL, home_btn_cb);

lv_obj_align(bm, LV_ALIGN_TOP_MID, -55, 388);

lv_obj_t *br = make_button(scr, LV_SYMBOL_REFRESH, 100, 42, COL_ACCENT, restart_cb);

lv_obj_align(br, LV_ALIGN_TOP_MID, 55, 388);

reset_board();

render();

schedule_ai();

}

/* ------------------------------------------------------------------ */

/*  Einstiegspunkt                                                     */

/* ------------------------------------------------------------------ */

void tic_tac_toe_open(lv_obj_t *screen)
{
    s_screen = screen;
    menu_create();
}