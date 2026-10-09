#include "orbit_breaker.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "esp_random.h"
#include "input.h"
#include "theme.h"

namespace {
constexpr int SCREEN_SIZE = 466;
constexpr int CX = 233;
constexpr int CY = 233;
constexpr int RINGS = 7;
constexpr int SEGMENTS = 24;
constexpr float INNER_R = 62.0f;
constexpr float RING_STEP = 20.0f;
constexpr float PADDLE_R = 220.0f;
constexpr float BALL_R = 6.0f;
constexpr float PI_F = 3.14159265358979323846f;
constexpr float TAU_F = 6.28318530717958647692f;

enum GameState { MENU, READY, PLAYING, PAUSED, LEVEL_DONE, GAME_OVER };
enum GameMode { CLASSIC, ENDLESS, TIME_ATTACK };

static lv_obj_t *screen = nullptr;
static lv_timer_t *timer = nullptr;
static lv_obj_t *brick_arc[RINGS][SEGMENTS] = {};
static lv_obj_t *ball_obj = nullptr;
static lv_obj_t *paddle_arc = nullptr;
static lv_obj_t *center_panel = nullptr;
static lv_obj_t *title_label = nullptr;
static lv_obj_t *score_label = nullptr;
static lv_obj_t *detail_label = nullptr;
static lv_obj_t *mode_buttons[3] = {nullptr, nullptr, nullptr};
static uint8_t bricks[RINGS][SEGMENTS] = {};
static GameState state = MENU;
static GameMode mode = CLASSIC;
static int level = 1;
static int lives = 3;
static int combo = 0;
static int destroyable = 0;
static int score = 0;
static int best_score[3] = {};
static float bx = 0.0f, by = 0.0f, vx = 0.0f, vy = 0.0f;
static float paddle_angle = PI_F * 0.5f;
static float time_left = 90.0f;
static float shift_timer = 15.0f;
static float state_timer = 0.0f;
static float wide_timer = 0.0f;
static float slow_timer = 0.0f;
static float fire_timer = 0.0f;

static lv_color_t ring_color(int ring)
{
    static const uint32_t colors[RINGS] = {
        0xFF3C3C, 0xFF8C28, 0xF0DC3C, 0x50DC5A,
        0x3CD2E6, 0x5078FF, 0xBE5AF0
    };
    return lv_color_hex(colors[ring]);
}

static float wrap_angle(float a)
{
    while (a < 0.0f) a += TAU_F;
    while (a >= TAU_F) a -= TAU_F;
    return a;
}

static float angle_diff(float a, float b)
{
    float d = a - b;
    while (d > PI_F) d -= TAU_F;
    while (d < -PI_F) d += TAU_F;
    return d;
}

static float radius_of(float x, float y) { return sqrtf(x * x + y * y); }
static float brick_inner(int ring) { return INNER_R + ring * RING_STEP + 1.5f; }
static float brick_outer(int ring) { return INNER_R + (ring + 1) * RING_STEP - 1.5f; }
static float paddle_half_width(void) { return wide_timer > 0.0f ? 0.42f : 0.27f; }
static float ball_speed(void) { return fminf(190.0f + 12.0f * level, 340.0f) * (slow_timer > 0.0f ? 0.7f : 1.0f); }
static int multiplier(void) { return 1 + (combo / 3 > 4 ? 4 : combo / 3); }

static void update_hud()
{
    if (!score_label || !detail_label) return;
    lv_label_set_text_fmt(score_label, "%d", score);
    if (state == MENU) {
        lv_label_set_text(detail_label, "ORBIT BREAKER");
    } else if (state == READY) {
        lv_label_set_text_fmt(detail_label, "LEVEL %d  -  TIPPE ZUM START", level);
    } else if (state == PAUSED) {
        lv_label_set_text(detail_label, "PAUSE  -  MITTE TIPpen");
    } else if (state == GAME_OVER) {
        lv_label_set_text_fmt(detail_label, "GAME OVER  |  BEST %d", best_score[mode]);
    } else if (state == LEVEL_DONE) {
        lv_label_set_text(detail_label, "LEVEL GESCHAFFT!");
    } else if (mode == TIME_ATTACK) {
        lv_label_set_text_fmt(detail_label, "ZEIT %ds  LEBEN %d", (int)ceilf(time_left), lives);
    } else {
        lv_label_set_text_fmt(detail_label, "LEVEL %d  LEBEN %d  x%d", level, lives, multiplier());
    }
}

static void set_arc_color(lv_obj_t *arc, lv_color_t color)
{
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
}

static void render_brick(int ring, int segment)
{
    lv_obj_t *arc = brick_arc[ring][segment];
    if (!arc) return;
    uint8_t hp = bricks[ring][segment];
    if (hp == 0) {
        lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_HIDDEN);
    if (hp == 255) set_arc_color(arc, lv_color_hex(0x85859B));
    else if (hp == 1) set_arc_color(arc, ring_color(ring));
    else if (hp == 2) set_arc_color(arc, lv_color_hex(0xB7B7C8));
    else set_arc_color(arc, lv_color_hex(0xFFFFFF));
}

static void render_paddle()
{
    if (!paddle_arc) return;
    float half = paddle_half_width();
    int start = (int)lroundf(wrap_angle(paddle_angle - half) * 180.0f / PI_F);
    int end = (int)lroundf(wrap_angle(paddle_angle + half) * 180.0f / PI_F);
    lv_arc_set_angles(paddle_arc, start, end);
    lv_obj_set_style_arc_color(paddle_arc,
        wide_timer > 0.0f ? lv_color_hex(0x6EFFA0) : lv_color_hex(0x46D2FF),
        LV_PART_INDICATOR);
}

static void render_ball()
{
    if (!ball_obj) return;
    lv_obj_set_pos(ball_obj, CX + (int)lroundf(bx) - (int)BALL_R,
                   CY + (int)lroundf(by) - (int)BALL_R);
    lv_obj_set_style_bg_color(ball_obj,
        fire_timer > 0.0f ? lv_color_hex(0xFF7820) : lv_color_hex(0xFFFFFF), 0);
}

static void render_all()
{
    for (int r = 0; r < RINGS; ++r)
        for (int s = 0; s < SEGMENTS; ++s)
            render_brick(r, s);
    render_paddle();
    render_ball();
    update_hud();
}

static uint8_t pattern_hp(int lv, int r, int s)
{
    switch (lv % 10) {
        case 0: return (r >= 3 && r <= 5) ? 1 : 0;
        case 1: return (r >= 2 && r <= 5 && ((r + s) & 1) == 0) ? 1 : 0;
        case 2: return (r >= 1 && r <= 5 && s % 6 != 0) ? 1 : 0;
        case 3: return (r >= 1 && r <= 5) ? ((r & 1) ? 1 : 2) : 0;
        case 4: return (s % 8 < 4) ? (r == 6 ? 2 : 1) : 0;
        case 5: if (r == 3 && s % 3 == 0) return 255; return (r >= 1 && r <= 5) ? 1 : 0;
        case 6: return ((s + r * 3) % 24 < 12) ? (r >= 4 ? 2 : 1) : 0;
        case 7: return r <= 5 ? (r >= 4 ? 2 : 1) : 0;
        case 8: if (r < 1) return 0; return ((r + s) % 2 == 0) ? 2 : (s % 4 == 1 ? 3 : 0);
        default: if (r == 3 && s % 8 == 0) return 255; if (r == 5 && s % 4 == 0) return 255; return (uint8_t)(1 + r % 3);
    }
}

static void recount()
{
    destroyable = 0;
    for (int r = 0; r < RINGS; ++r)
        for (int s = 0; s < SEGMENTS; ++s)
            if (bricks[r][s] != 0 && bricks[r][s] != 255) ++destroyable;
}

static void load_level()
{
    memset(bricks, 0, sizeof(bricks));
    if (mode == ENDLESS) {
        for (int r = 0; r < 3; ++r)
            for (int s = 0; s < SEGMENTS; ++s)
                bricks[r][s] = (esp_random() % 100 < 70) ? 1 : 0;
        shift_timer = fmaxf(6.0f, 15.0f - 0.8f * level);
    } else {
        for (int r = 0; r < RINGS; ++r)
            for (int s = 0; s < SEGMENTS; ++s)
                bricks[r][s] = pattern_hp(level - 1, r, s);
    }
    recount();
    wide_timer = slow_timer = fire_timer = 0.0f;
    render_all();
}

static void launch_ball()
{
    float a = paddle_angle + PI_F + ((int)(esp_random() % 31) - 15) / 100.0f;
    float sp = ball_speed();
    vx = cosf(a) * sp;
    vy = sinf(a) * sp;
    state = PLAYING;
    update_hud();
}

static void show_game_visuals(bool playing)
{
    if (center_panel) {
        if (playing) lv_obj_add_flag(center_panel, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(center_panel, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 3; ++i) {
        if (mode_buttons[i]) {
            if (playing) lv_obj_add_flag(mode_buttons[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_clear_flag(mode_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    for (int r = 0; r < RINGS; ++r) {
        for (int s = 0; s < SEGMENTS; ++s) {
            if (playing) render_brick(r, s);
            else if (brick_arc[r][s]) lv_obj_add_flag(brick_arc[r][s], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (ball_obj) {
        if (playing) lv_obj_clear_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ball_obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (paddle_arc) {
        if (playing) lv_obj_clear_flag(paddle_arc, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(paddle_arc, LV_OBJ_FLAG_HIDDEN);
    }
}

static void start_game(GameMode new_mode)
{
    mode = new_mode;
    score = 0;
    level = 1;
    lives = 3;
    combo = 0;
    time_left = 90.0f;
    paddle_angle = PI_F * 0.5f;
    load_level();
    show_game_visuals(true);
    float rr = PADDLE_R - BALL_R - 3.0f;
    bx = cosf(paddle_angle) * rr;
    by = sinf(paddle_angle) * rr;
    state = READY;
    state_timer = 1.2f;
    update_hud();
}

static void game_over()
{
    state = GAME_OVER;
    if (score > best_score[mode]) best_score[mode] = score;
    update_hud();
}

static void lose_ball()
{
    combo = 0;
    if (mode == TIME_ATTACK) {
        time_left -= 5.0f;
        if (time_left <= 0.0f) { game_over(); return; }
    } else {
        --lives;
        if (lives <= 0) { game_over(); return; }
    }
    wide_timer = slow_timer = fire_timer = 0.0f;
    state = READY;
    state_timer = 1.1f;
    float rr = PADDLE_R - BALL_R - 3.0f;
    bx = cosf(paddle_angle) * rr;
    by = sinf(paddle_angle) * rr;
    update_hud();
}

static void endless_shift()
{
    for (int s = 0; s < SEGMENTS; ++s) {
        if (bricks[RINGS - 1][s] != 0) { game_over(); return; }
    }
    for (int r = RINGS - 1; r > 0; --r)
        memcpy(bricks[r], bricks[r - 1], SEGMENTS);
    for (int s = 0; s < SEGMENTS; ++s)
        bricks[0][s] = (esp_random() % 100 < 65) ? ((esp_random() % 100 < 15) ? 2 : 1) : 0;
    bool any = false;
    for (int s = 0; s < SEGMENTS; ++s) if (bricks[0][s]) any = true;
    if (!any) bricks[0][esp_random() % SEGMENTS] = 1;
    ++level;
    recount();
    shift_timer = fmaxf(6.0f, 15.0f - 0.8f * level);
    render_all();
}

static int cell_at(float x, float y)
{
    float r = radius_of(x, y);
    int ring = (int)((r - INNER_R) / RING_STEP);
    if (ring < 0 || ring >= RINGS) return -1;
    float a = atan2f(y, x);
    if (a < 0) a += TAU_F;
    int seg = (int)(a / (TAU_F / SEGMENTS));
    if (seg >= SEGMENTS) seg = SEGMENTS - 1;
    return ring * SEGMENTS + seg;
}

static void hit_brick(int index)
{
    int r = index / SEGMENTS;
    int s = index % SEGMENTS;
    uint8_t hp = bricks[r][s];
    if (hp == 0) return;
    if (hp != 255) {
        ++combo;
        if (fire_timer > 0.0f || hp == 1) {
            bricks[r][s] = 0;
            --destroyable;
            score += 15 * multiplier();
        } else {
            bricks[r][s] = hp - 1;
            score += 5 * multiplier();
        }
        render_brick(r, s);
    }
    float rball = radius_of(bx, by);
    if (rball < 1.0f) rball = 1.0f;
    float nx = bx / rball, ny = by / rball;
    float dot = vx * nx + vy * ny;
    vx -= 2.0f * dot * nx;
    vy -= 2.0f * dot * ny;
    bx = nx * (INNER_R + (r + 0.5f) * RING_STEP - BALL_R - 1.0f);
    by = ny * (INNER_R + (r + 0.5f) * RING_STEP - BALL_R - 1.0f);
    update_hud();
}

static void update_game(float dt)
{
    if (state == READY) {
        state_timer -= dt;
        float rr = PADDLE_R - BALL_R - 3.0f;
        bx = cosf(paddle_angle) * rr;
        by = sinf(paddle_angle) * rr;
        if (state_timer <= 0.0f) launch_ball();
        render_ball();
        return;
    }
    if (state == LEVEL_DONE) {
        state_timer -= dt;
        if (state_timer <= 0.0f) {
            ++level;
            if (mode == TIME_ATTACK) time_left += 10.0f;
            load_level();
            state = READY;
            state_timer = 1.0f;
        }
        update_hud();
        return;
    }
    if (state != PLAYING) return;

    wide_timer = fmaxf(0.0f, wide_timer - dt);
    slow_timer = fmaxf(0.0f, slow_timer - dt);
    fire_timer = fmaxf(0.0f, fire_timer - dt);
    if (mode == TIME_ATTACK) {
        time_left -= dt;
        if (time_left <= 0.0f) { game_over(); return; }
    }
    if (mode == ENDLESS) {
        shift_timer -= dt;
        if (shift_timer <= 0.0f || destroyable == 0) {
            if (destroyable == 0) score += 100;
            endless_shift();
            if (state != PLAYING) return;
        }
    }

    float sp = radius_of(vx, vy);
    float target = ball_speed();
    if (sp > 0.1f) { vx = vx / sp * target; vy = vy / sp * target; }
    const int substeps = 3;
    float h = dt / substeps;
    for (int k = 0; k < substeps && state == PLAYING; ++k) {
        float ox = bx, oy = by;
        bx += vx * h;
        by += vy * h;
        float rr = radius_of(bx, by);
        int c = cell_at(bx, by);
        if (c >= 0 && bricks[c / SEGMENTS][c % SEGMENTS] != 0) {
            hit_brick(c);
        } else if (rr + BALL_R >= PADDLE_R - 5.0f) {
            float a = atan2f(by, bx);
            float d = angle_diff(a, paddle_angle);
            if (fabsf(d) <= paddle_half_width() + BALL_R / fmaxf(rr, 1.0f)) {
                float speed = radius_of(vx, vy);
                float deflect = fmaxf(-0.95f, fminf(0.95f, d / paddle_half_width() * 0.8f));
                float normal = atan2f(by, bx);
                float out = normal + PI_F + deflect;
                vx = cosf(out) * speed;
                vy = sinf(out) * speed;
                bx = cosf(normal) * (PADDLE_R - BALL_R - 4.0f);
                by = sinf(normal) * (PADDLE_R - BALL_R - 4.0f);
                combo = 0;
            } else if (rr > PADDLE_R + BALL_R + 8.0f) {
                lose_ball();
                return;
            }
        } else if (rr > PADDLE_R + BALL_R + 8.0f) {
            lose_ball();
            return;
        }
        (void)ox; (void)oy;
    }
    render_paddle();
    render_ball();
    update_hud();
    if (mode != ENDLESS && destroyable == 0) {
        score += 100 * level + (mode == TIME_ATTACK ? 0 : lives * 50);
        state = LEVEL_DONE;
        state_timer = 1.6f;
        update_hud();
    }
}

static void mode_button_cb(lv_event_t *e)
{
    GameMode m = (GameMode)(intptr_t)lv_event_get_user_data(e);
    start_game(m);
}

static void screen_pressing_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) != screen) return;
    if (state != PLAYING && state != READY && state != PAUSED) return;
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    float dx = (float)p.x - CX;
    float dy = (float)p.y - CY;
    if (sqrtf(dx * dx + dy * dy) > 48.0f) {
        paddle_angle = wrap_angle(atan2f(dy, dx));
        render_paddle();
    }
}

static void screen_clicked_cb(lv_event_t *e)
{
    if (lv_event_get_target(e) != screen) return;
    if (state == READY) launch_ball();
    else if (state == PLAYING) {
        lv_indev_t *indev = lv_indev_active();
        if (indev) {
            lv_point_t p;
            lv_indev_get_point(indev, &p);
            float dx = (float)p.x - CX, dy = (float)p.y - CY;
            if (sqrtf(dx * dx + dy * dy) < 48.0f) state = PAUSED;
        }
    } else if (state == PAUSED) state = PLAYING;
    else if (state == GAME_OVER || state == LEVEL_DONE) {
        state = MENU;
        update_hud();
    }
    if (state == MENU) {
        show_game_visuals(false);
    } else if (state == PLAYING || state == READY || state == PAUSED || state == GAME_OVER || state == LEVEL_DONE) {
        show_game_visuals(true);
        render_all();
    }
    update_hud();
}

static void create_mode_button(const char *label, int y, GameMode m)
{
    lv_obj_t *button = lv_button_create(screen);
    mode_buttons[(int)m] = button;
    lv_obj_set_size(button, 220, 52);
    lv_obj_align(button, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_bg_color(button, lv_color_hex(m == CLASSIC ? 0x244B75 : (m == ENDLESS ? 0x245E49 : 0x69406F)), LV_PART_MAIN);
    lv_obj_t *text = lv_label_create(button);
    lv_label_set_text(text, label);
    lv_obj_set_style_text_font(text, &lv_font_montserrat_20, 0);
    lv_obj_center(text);
    lv_obj_add_event_cb(button, mode_button_cb, LV_EVENT_CLICKED, (void *)(intptr_t)m);
    lv_obj_add_flag(button, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

static void build_ui(lv_obj_t *target)
{
    screen = target;
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    theme_apply(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x03050B), 0);

    title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "ORBIT BREAKER");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x50DFFF), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 8);

    for (int r = 0; r < RINGS; ++r) {
        float radius = INNER_R + (r + 0.5f) * RING_STEP;
        for (int s = 0; s < SEGMENTS; ++s) {
            lv_obj_t *arc = lv_arc_create(screen);
            int diameter = (int)lroundf(radius * 2.0f);
            lv_obj_set_size(arc, diameter, diameter);
            lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
            lv_arc_set_rotation(arc, 0);
            int start = (int)lroundf((float)s * 360.0f / SEGMENTS + 1.6f);
            int end = (int)lroundf((float)(s + 1) * 360.0f / SEGMENTS - 1.6f);
            lv_arc_set_bg_angles(arc, start, end);
            lv_arc_set_angles(arc, start, end);
            lv_obj_set_style_arc_width(arc, (int)RING_STEP - 3, LV_PART_INDICATOR);
            lv_obj_set_style_arc_width(arc, (int)RING_STEP - 3, LV_PART_MAIN);
            lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(arc, 0, 0);
            lv_obj_set_style_pad_all(arc, 0, 0);
            lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_set_style_border_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(arc, LV_OBJ_FLAG_HIDDEN);
            brick_arc[r][s] = arc;
        }
    }

    paddle_arc = lv_arc_create(screen);
    lv_obj_set_size(paddle_arc, (int)(PADDLE_R * 2), (int)(PADDLE_R * 2));
    lv_obj_align(paddle_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(paddle_arc, 0, 359);
    lv_arc_set_angles(paddle_arc, 60, 120);
    lv_obj_set_style_arc_width(paddle_arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(paddle_arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(paddle_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(paddle_arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(paddle_arc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(paddle_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(paddle_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_clear_flag(paddle_arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    ball_obj = lv_obj_create(screen);
    lv_obj_set_size(ball_obj, (int)(BALL_R * 2), (int)(BALL_R * 2));
    lv_obj_set_style_radius(ball_obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ball_obj, 0, 0);
    lv_obj_set_style_bg_color(ball_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(ball_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    center_panel = lv_obj_create(screen);
    lv_obj_set_size(center_panel, 180, 130);
    lv_obj_align(center_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(center_panel, lv_color_hex(0x050812), 0);
    lv_obj_set_style_bg_opa(center_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(center_panel, 0, 0);
    lv_obj_set_style_radius(center_panel, 18, 0);
    lv_obj_clear_flag(center_panel, LV_OBJ_FLAG_SCROLLABLE);

    score_label = lv_label_create(center_panel);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(score_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 8);
    detail_label = lv_label_create(center_panel);
    lv_obj_set_width(detail_label, 166);
    lv_obj_set_style_text_align(detail_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(detail_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(detail_label, lv_color_hex(0x9FAAC5), 0);
    lv_obj_align(detail_label, LV_ALIGN_TOP_MID, 0, 43);

    create_mode_button("KLASSIK", 160, CLASSIC);
    create_mode_button("ENDLOS", 220, ENDLESS);
    create_mode_button("ZEITANGRIFF", 280, TIME_ATTACK);

    lv_obj_add_event_cb(screen, screen_pressing_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(screen, screen_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

    state = MENU;
    show_game_visuals(false);
    update_hud();
}

static void tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    static uint32_t last_ms = 0;
    uint32_t now = lv_tick_get();
    float dt = last_ms == 0 ? 0.016f : (float)(now - last_ms) / 1000.0f;
    last_ms = now;
    if (dt > 0.05f) dt = 0.05f;
    if (state == PLAYING || state == READY || state == LEVEL_DONE) {
        update_game(dt);
    }
}

} // namespace

extern "C" void orbit_breaker_open(lv_obj_t *target)
{
    if (!target) return;
    orbit_breaker_stop();
    memset(brick_arc, 0, sizeof(brick_arc));
    build_ui(target);
    timer = lv_timer_create(tick, 16, nullptr);
}

extern "C" void orbit_breaker_stop(void)
{
    if (timer) {
        lv_timer_del(timer);
        timer = nullptr;
    }
    screen = nullptr;
    ball_obj = nullptr;
    paddle_arc = nullptr;
    center_panel = nullptr;
    title_label = nullptr;
    score_label = nullptr;
    detail_label = nullptr;
    memset(mode_buttons, 0, sizeof(mode_buttons));
    memset(brick_arc, 0, sizeof(brick_arc));
}
