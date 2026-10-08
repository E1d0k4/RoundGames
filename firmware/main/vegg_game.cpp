#include "vegg_game.h"

#include <math.h>
#include <stdio.h>

#include "nvs.h"
#include "vegg_sprites.h"

namespace {

enum VeggState {
    VEGG_RUNNING,
    VEGG_JUMPING,
    VEGG_GAME_OVER
};

static lv_obj_t *screen = NULL;
static lv_obj_t *runner = NULL;
static lv_obj_t *score_label = NULL;
static lv_obj_t *best_label = NULL;
static lv_obj_t *game_over_panel = NULL;
static lv_timer_t *timer = NULL;
static lv_obj_t *obstacle_obj[4] = {NULL, NULL, NULL, NULL};

struct Obstacle { bool active; bool branch; float x; };
static Obstacle obstacles[4];

static bool active = false;
static VeggState state = VEGG_RUNNING;
static float runner_x = 140.0f;
static float jump_h = 0.0f;
static float jump_v = 0.0f;
static float speed = 240.0f;
static float distance_run = 0.0f;
static float run_time = 0.0f;
static float next_gap = 260.0f;
static uint32_t score = 0;
static uint32_t best = 0;
static uint32_t last_tick = 0;
static uint32_t game_over_at = 0;
static uint32_t rng_state = 2463534242u;
static void (*exit_callback)(void) = NULL;

static const lv_color_t SKY = LV_COLOR_MAKE(150, 210, 235);
static const lv_color_t SKY2 = LV_COLOR_MAKE(205, 235, 220);
static const lv_color_t GROUND = LV_COLOR_MAKE(70, 150, 60);
static const lv_color_t GROUND_DARK = LV_COLOR_MAKE(45, 110, 55);
static const lv_color_t BUSH = LV_COLOR_MAKE(30, 110, 50);
static const lv_color_t BRANCH = LV_COLOR_MAKE(105, 66, 38);

static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static float frand(void)
{
    return (float)(rnd() & 0xFFFFu) / 65535.0f;
}

static void nvs_load_best(void)
{
    nvs_handle_t nvs;
    best = 0;
    if (nvs_open("vegg", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u32(nvs, "best", &best);
        nvs_close(nvs);
    }
}

static void nvs_save_best(void)
{
    nvs_handle_t nvs;
    if (nvs_open("vegg", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u32(nvs, "best", best);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

static void update_labels(void)
{
    if (!score_label) return;
    lv_label_set_text_fmt(score_label, "SCORE  %04lu", (unsigned long)score);
    lv_label_set_text_fmt(best_label, "BEST  %04lu", (unsigned long)best);
}

static void create_runner_image(void)
{
    if (!runner) return;
    lv_image_set_src(runner, NULL);
}

static const lv_image_dsc_t make_image_dsc(uint8_t frame)
{
    lv_image_dsc_t dsc = {};
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    dsc.header.w = VEGG_EEVEE_W[frame];
    dsc.header.h = VEGG_EEVEE_H[frame];
    dsc.header.stride = VEGG_EEVEE_W[frame] * 4;

    switch (frame) {
        case 0: dsc.data = VEGG_EEVEE_0; break;
        case 1: dsc.data = VEGG_EEVEE_1; break;
        default: dsc.data = VEGG_EEVEE_2; break;
    }
    dsc.data_size = dsc.header.stride * dsc.header.h;
    return dsc;
}

static void set_runner_frame(uint8_t frame)
{
    static lv_image_dsc_t dsc;
    dsc = make_image_dsc(frame);
    lv_image_set_src(runner, &dsc);
    lv_obj_set_size(runner, dsc.header.w * 3, dsc.header.h * 3);
    lv_obj_set_pos(runner,
                   (int)runner_x - (int)(dsc.header.w * 3 / 2),
                   304 - (int)jump_h - (int)(dsc.header.h * 3));
}

static void clear_obstacle(int i)
{
    if (obstacle_obj[i]) {
        lv_obj_del(obstacle_obj[i]);
        obstacle_obj[i] = NULL;
    }
}

static void draw_obstacle(int i)
{
    clear_obstacle(i);
    if (!screen) return;

    lv_obj_t *o = lv_obj_create(screen);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, obstacles[i].branch ? BRANCH : BUSH, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);

    if (obstacles[i].branch) {
        lv_obj_set_size(o, 92, 18);
        lv_obj_set_pos(o, (int)obstacles[i].x - 80, 242);
    } else {
        lv_obj_set_size(o, 42, 34);
        lv_obj_set_pos(o, (int)obstacles[i].x - 21, 280);
    }
    obstacle_obj[i] = o;
}

static void spawn_obstacle(void)
{
    int slot = -1;
    for (int i = 0; i < 4; ++i) {
        if (!obstacles[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return;

    obstacles[slot].active = true;
    obstacles[slot].branch = score >= 15 && frand() < (score >= 80 ? 0.5f : 0.3f);
    obstacles[slot].x = 520.0f;
    draw_obstacle(slot);

    float min_gap = speed * 0.48f + 55.0f;
    float random_gap = speed * (0.10f + frand() * 0.36f)
                     + (obstacles[slot].branch ? 75.0f : 55.0f);
    next_gap = fmaxf(min_gap, random_gap);
}

static bool collision(void)
{
    const float left = runner_x - 24.0f;
    const float right = runner_x + 24.0f;
    const float bottom = jump_h;
    const float top = jump_h + 54.0f;

    for (int i = 0; i < 4; ++i) {
        if (!obstacles[i].active) continue;

        if (!obstacles[i].branch) {
            if (right > obstacles[i].x - 18.0f &&
                left < obstacles[i].x + 18.0f &&
                bottom < 28.0f) {
                return true;
            }
        } else {
            if (right > obstacles[i].x - 88.0f &&
                left < obstacles[i].x + 8.0f &&
                top > 55.0f) {
                return true;
            }
        }
    }
    return false;
}

static void finish_run(void)
{
    state = VEGG_GAME_OVER;
    game_over_at = lv_tick_get();

    if (score > best) {
        best = score;
        nvs_save_best();
    }

    if (game_over_panel) {
        lv_obj_clear_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);
    }
    update_labels();
}

static void jump(void)
{
    if (state == VEGG_GAME_OVER) {
        if (lv_tick_get() - game_over_at > 700) {
            state = VEGG_RUNNING;
            score = 0;
            distance_run = 0;
            run_time = 0;
            jump_h = 0;
            jump_v = 0;
            next_gap = 240;
            for (int i = 0; i < 4; ++i) {
                obstacles[i].active = false;
                clear_obstacle(i);
            }
            if (game_over_panel) {
                lv_obj_add_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);
            }
            update_labels();
        }
        return;
    }

    if (state == VEGG_RUNNING) {
        jump_v = 820.0f;
        state = VEGG_JUMPING;
    } else if (state == VEGG_JUMPING && jump_v > 0) {
        jump_v = -1500.0f;
    }
}

static void tap_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!active) return;
    jump();
}

static void exit_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (exit_callback) exit_callback();
}

static void tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    if (!active) return;

    const uint32_t now = lv_tick_get();
    float dt = (now - last_tick) / 1000.0f;
    if (dt > 0.05f) dt = 0.05f;
    last_tick = now;

    if (state == VEGG_GAME_OVER) {
        if (now - game_over_at >= 6000) {
            if (exit_callback) exit_callback();
        }
        return;
    }

    run_time += dt;
    speed = fminf(240.0f + 12.0f * run_time, 700.0f);
    distance_run += speed * dt;
    score = (uint32_t)(distance_run / 10.0f);

    if (state == VEGG_JUMPING) {
        jump_v -= 2700.0f * dt;
        jump_h += jump_v * dt;
        if (jump_h <= 0) {
            jump_h = 0;
            jump_v = 0;
            state = VEGG_RUNNING;
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (!obstacles[i].active) continue;
        obstacles[i].x -= speed * dt;
        if (obstacles[i].x < -140) {
            obstacles[i].active = false;
            clear_obstacle(i);
        } else {
            draw_obstacle(i);
        }
    }

    next_gap -= speed * dt;
    if (next_gap <= 0) spawn_obstacle();

    if (collision()) finish_run();

    const uint8_t frame = jump_h > 0.5f
        ? 0
        : (uint8_t)((uint32_t)(run_time * 10.0f) % VEGG_EEVEE_FRAMES);

    set_runner_frame(frame);
    update_labels();
}

static void build_scene(void)
{
    lv_obj_set_style_bg_color(screen, SKY, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *horizon = lv_obj_create(screen);
    lv_obj_remove_style_all(horizon);
    lv_obj_set_style_bg_color(horizon, SKY2, 0);
    lv_obj_set_size(horizon, 466, 300);
    lv_obj_set_pos(horizon, 0, 0);
    lv_obj_clear_flag(horizon, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *planet = lv_obj_create(screen);
    lv_obj_remove_style_all(planet);
    lv_obj_set_style_bg_color(planet, GROUND, 0);
    lv_obj_set_style_radius(planet, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(planet, 700, 300);
    lv_obj_set_pos(planet, -117, 285);
    lv_obj_clear_flag(planet, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *grass = lv_obj_create(screen);
    lv_obj_remove_style_all(grass);
    lv_obj_set_style_bg_color(grass, GROUND_DARK, 0);
    lv_obj_set_style_radius(grass, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_size(grass, 650, 240);
    lv_obj_set_pos(grass, -92, 300);
    lv_obj_clear_flag(grass, LV_OBJ_FLAG_CLICKABLE);

    score_label = lv_label_create(screen);
    lv_obj_set_style_text_font(score_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(score_label, lv_color_white(), 0);
    lv_label_set_text(score_label, "SCORE  0000");
    lv_obj_align(score_label, LV_ALIGN_TOP_MID, 0, 58);

    best_label = lv_label_create(screen);
    lv_obj_set_style_text_font(best_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(best_label, lv_color_white(), 0);
    lv_label_set_text(best_label, "BEST  0000");
    lv_obj_align(best_label, LV_ALIGN_TOP_MID, 0, 88);

    lv_obj_t *back = lv_button_create(screen);
    lv_obj_set_size(back, 64, 46);
    lv_obj_align(back, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_t *back_label = lv_label_create(back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);
    lv_obj_add_event_cb(back, exit_cb, LV_EVENT_CLICKED, NULL);

    game_over_panel = lv_obj_create(screen);
    lv_obj_remove_style_all(game_over_panel);
    lv_obj_set_style_bg_color(game_over_panel, lv_color_hex(0x193021), 0);
    lv_obj_set_style_bg_opa(game_over_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(game_over_panel, 22, 0);
    lv_obj_set_style_border_width(game_over_panel, 2, 0);
    lv_obj_set_style_border_color(game_over_panel, lv_color_hex(0xFFBE5A), 0);
    lv_obj_set_size(game_over_panel, 320, 190);
    lv_obj_align(game_over_panel, LV_ALIGN_CENTER, 0, -35);
    lv_obj_add_flag(game_over_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *over = lv_label_create(game_over_panel);
    lv_label_set_text(over, "GAME OVER");
    lv_obj_set_style_text_font(over, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(over, lv_color_hex(0xFF963C), 0);
    lv_obj_align(over, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *hint = lv_label_create(game_over_panel);
    lv_label_set_text(hint, "Tap = retry");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_22, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -20);

    runner = lv_image_create(screen);
    lv_obj_add_flag(runner, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_clear_flag(runner, LV_OBJ_FLAG_CLICKABLE);
    create_runner_image();

    lv_obj_t *touch = lv_obj_create(screen);
    lv_obj_remove_style_all(touch);
    lv_obj_set_size(touch, 466, 370);
    lv_obj_set_pos(touch, 0, 90);
    lv_obj_set_style_bg_opa(touch, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(touch, tap_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(touch, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

} // namespace

void vegg_open(lv_obj_t *new_screen)
{
    if (!new_screen || active) return;

    screen = new_screen;
    nvs_load_best();
    rng_state ^= lv_tick_get();

    for (int i = 0; i < 4; ++i) {
        obstacles[i].active = false;
        obstacle_obj[i] = NULL;
    }

    runner_x = 140;
    jump_h = 0;
    jump_v = 0;
    speed = 240;
    distance_run = 0;
    run_time = 0;
    score = 0;
    next_gap = 260;
    state = VEGG_RUNNING;
    active = true;
    last_tick = lv_tick_get();

    build_scene();
    update_labels();
    timer = lv_timer_create(tick, 40, NULL);
}

void vegg_stop(void)
{
    if (timer) {
        lv_timer_del(timer);
        timer = NULL;
    }

    for (int i = 0; i < 4; ++i) {
        obstacle_obj[i] = NULL;
    }

    runner = NULL;
    score_label = NULL;
    best_label = NULL;
    game_over_panel = NULL;
    screen = NULL;
    active = false;
}

void vegg_set_exit_callback(void (*callback)(void))
{
    exit_callback = callback;
}

bool vegg_is_active(void)
{
    return active;
}

uint32_t vegg_best_score(void)
{
    nvs_load_best();
    return best;
}

void vegg_set_best_score(uint32_t value)
{
    best = value;
    nvs_save_best();
}
