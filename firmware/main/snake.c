#include "snake.h"

#include <stdint.h>
#include <stdbool.h>

#include "lvgl.h"
#include "esp_random.h"
#include "input.h"
#include "theme.h"

#define GRID 20
#define CELL 20
#define BOARD 400
#define BOARD_X 33
#define BOARD_Y 50
#define MAX_LEN (GRID * GRID)

typedef struct { int8_t x, y; } point_t;
typedef enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT } dir_t;

static point_t body[MAX_LEN], food;
static int len, score;
static dir_t dir, next_dir;
static bool dead;
static lv_obj_t *screen, *field, *food_obj, *parts[MAX_LEN], *score_label, *status_label;
static lv_timer_t *timer;

static bool occupied(int x, int y, int skip_tail)
{
    for (int i = 0; i < len - skip_tail; i++)
        if (body[i].x == x && body[i].y == y) return true;
    return false;
}

static void place_food(void)
{
    if (len >= MAX_LEN) return;
    do {
        food.x = (int8_t)(esp_random() % GRID);
        food.y = (int8_t)(esp_random() % GRID);
    } while (occupied(food.x, food.y, 0));
}

static void set_dir(dir_t d)
{
    if ((d == DIR_UP && dir == DIR_DOWN) || (d == DIR_DOWN && dir == DIR_UP) ||
        (d == DIR_LEFT && dir == DIR_RIGHT) || (d == DIR_RIGHT && dir == DIR_LEFT))
        return;
    next_dir = d;
}

static void gesture_cb(lv_dir_t d)
{
    if (dead) return;
    if (d == LV_DIR_TOP) set_dir(DIR_UP);
    else if (d == LV_DIR_RIGHT) set_dir(DIR_RIGHT);
    else if (d == LV_DIR_BOTTOM) set_dir(DIR_DOWN);
    else if (d == LV_DIR_LEFT) set_dir(DIR_LEFT);
}

static void render(void)
{
    for (int i = 0; i < MAX_LEN; i++) {
        if (i >= len) {
            lv_obj_add_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(parts[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(parts[i], body[i].x * CELL + 1, body[i].y * CELL + 1);
        lv_obj_set_size(parts[i], CELL - 2, CELL - 2);
        lv_obj_set_style_bg_color(parts[i],
            dead ? lv_color_hex(0xFF5A28) :
            (i == 0 ? lv_color_hex(0x9CFF78) : lv_color_hex(0x00C850)), 0);
        lv_obj_set_style_radius(parts[i], i == 0 ? 7 : 4, 0);
    }

    if (dead) lv_obj_add_flag(food_obj, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_clear_flag(food_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(food_obj, food.x * CELL + 3, food.y * CELL + 3);
    }

    lv_label_set_text_fmt(score_label, "%d", score);
    lv_label_set_text(status_label, dead ? "GAME OVER" : "SNAKE");
    lv_obj_set_style_text_color(status_label,
        dead ? lv_color_hex(0xFF5A28) : lv_color_hex(0xFFFFFF), 0);
}

static void reset_game(void)
{
    len = 3;
    score = 0;
    dead = false;
    dir = next_dir = DIR_RIGHT;
    for (int i = 0; i < len; i++) {
        body[i].x = (int8_t)(GRID / 2 - i);
        body[i].y = GRID / 2;
    }
    place_food();
    if (timer) {
        lv_timer_set_period(timer, 180);
        lv_timer_reset(timer);
    }
}

static void step(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (dead) return;

    dir = next_dir;
    point_t head = body[0];
    if (dir == DIR_UP) head.y--;
    else if (dir == DIR_RIGHT) head.x++;
    else if (dir == DIR_DOWN) head.y++;
    else head.x--;

    bool eat = head.x == food.x && head.y == food.y;
    if (head.x < 0 || head.x >= GRID || head.y < 0 || head.y >= GRID ||
        occupied(head.x, head.y, eat ? 0 : 1)) {
        dead = true;
        render();
        return;
    }

    if (eat && len < MAX_LEN) len++;
    for (int i = len - 1; i > 0; i--) body[i] = body[i - 1];
    body[0] = head;

    if (eat) {
        score++;
        place_food();
        int period = 180 - score * 4;
        if (period < 70) period = 70;
        lv_timer_set_period(timer, (uint32_t)period);
    }
    render();
}

static void tap_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (dead) {
        reset_game();
        render();
    }
}

static void prepare_screen(void)
{
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    theme_apply(screen);
}

void snake_open(lv_obj_t *target)
{
    if (!target) return;
    screen = target;
    prepare_screen();

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "SNAKE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 7);

    score_label = lv_label_create(screen);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_22, 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 7);

    status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *border = lv_obj_create(screen);
    lv_obj_set_size(border, BOARD + 8, BOARD + 8);
    lv_obj_set_pos(border, BOARD_X - 4, BOARD_Y - 4);
    lv_obj_set_style_bg_color(border, lv_color_hex(0x28303D), 0);
    lv_obj_set_style_border_width(border, 0, 0);
    lv_obj_set_style_radius(border, 18, 0);
    lv_obj_clear_flag(border, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(border, LV_OBJ_FLAG_GESTURE_BUBBLE);

    field = lv_obj_create(screen);
    lv_obj_set_size(field, BOARD, BOARD);
    lv_obj_set_pos(field, BOARD_X, BOARD_Y);
    lv_obj_set_style_bg_color(field, lv_color_hex(0x080D13), 0);
    lv_obj_set_style_border_width(field, 0, 0);
    lv_obj_set_style_radius(field, 14, 0);
    lv_obj_clear_flag(field, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(field, LV_OBJ_FLAG_GESTURE_BUBBLE);

    food_obj = lv_obj_create(field);
    lv_obj_set_size(food_obj, CELL - 6, CELL - 6);
    lv_obj_set_style_bg_color(food_obj, lv_color_hex(0xFF3C4F), 0);
    lv_obj_set_style_border_width(food_obj, 0, 0);
    lv_obj_set_style_radius(food_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(food_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(food_obj, LV_OBJ_FLAG_GESTURE_BUBBLE);

    for (int i = 0; i < MAX_LEN; i++) {
        parts[i] = lv_obj_create(field);
        lv_obj_set_style_border_width(parts[i], 0, 0);
        lv_obj_clear_flag(parts[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(parts[i], LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, tap_cb, LV_EVENT_CLICKED, NULL);

    input_set_game_gesture_callback(gesture_cb);
    reset_game();
    render();
    timer = lv_timer_create(step, 180, NULL);
}
