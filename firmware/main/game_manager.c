#include "game_manager.h"

#include <stddef.h>

#include "input.h"
#include "screensaver.h"
#include "tic_tac_toe.h"
#include "snake.h"
#include "vegg_game.h"
#include "orbit_breaker.h"

typedef struct {
    game_id_t id;
    const char *name;
    const char *icon;
    uint32_t color;
    void (*open)(lv_obj_t *screen);
    void (*stop)(void);
} game_definition_t;

/*
 * This is the single source of truth for registered games. The launcher reads
 * its labels and artwork metadata from here, and lifecycle dispatch uses the
 * same entries so the launcher cannot expose an unregistered game by mistake.
 */
static const game_definition_t games[] = {
    { GAME_ID_TIC_TAC_TOE, "Tic-Tac-Toe", "X   O", 0x35E0FF,
      tic_tac_toe_open, tic_tac_toe_stop },
    { GAME_ID_SNAKE, "Snake", "S", 0x00C850,
      snake_open, snake_stop },
    { GAME_ID_VEGG, "Vegg", "E", 0xFF9F43,
      vegg_open, vegg_stop },
    { GAME_ID_ORBIT_BREAKER, "Orbit Breaker", "O", 0x50DFFF,
      orbit_breaker_open, orbit_breaker_stop }
};

#define GAME_COUNT (sizeof(games) / sizeof(games[0]))

static game_id_t current_game = GAME_ID_NONE;
static void (*exit_callback)(void) = NULL;

static const game_definition_t *game_by_id(game_id_t game_id)
{
    for (size_t i = 0; i < GAME_COUNT; ++i) {
        if (games[i].id == game_id) {
            return &games[i];
        }
    }

    return NULL;
}

static const game_definition_t *game_by_index(int index)
{
    if (index < 0 || (size_t)index >= GAME_COUNT) {
        return NULL;
    }

    return &games[index];
}

static void game_manager_exit_cb(void)
{
    if (current_game == GAME_ID_NONE) {
        return;
    }

    game_manager_stop();
    if (exit_callback) {
        exit_callback();
    }
}

void game_manager_set_exit_callback(void (*callback)(void))
{
    exit_callback = callback;
}

void game_manager_init(void)
{
    current_game = GAME_ID_NONE;
    vegg_set_exit_callback(game_manager_exit_cb);
}

size_t game_manager_game_count(void)
{
    return GAME_COUNT;
}

game_id_t game_manager_game_id_at(int index)
{
    const game_definition_t *game = game_by_index(index);
    return game ? game->id : GAME_ID_NONE;
}

const char *game_manager_game_name_at(int index)
{
    const game_definition_t *game = game_by_index(index);
    return game ? game->name : NULL;
}

const char *game_manager_game_icon_at(int index)
{
    const game_definition_t *game = game_by_index(index);
    return game ? game->icon : NULL;
}

uint32_t game_manager_game_color_at(int index)
{
    const game_definition_t *game = game_by_index(index);
    return game ? game->color : 0xFFFFFF;
}

bool game_manager_start_index(int index, lv_obj_t *screen)
{
    game_id_t game_id = game_manager_game_id_at(index);
    if (game_id == GAME_ID_NONE) {
        return false;
    }

    return game_manager_start(game_id, screen);
}

bool game_manager_start(game_id_t game_id, lv_obj_t *screen)
{
    const game_definition_t *game = game_by_id(game_id);
    if (!screen || !game || current_game != GAME_ID_NONE) {
        return false;
    }

    /* A game must never inherit a gesture handler left by the previous game. */
    input_set_game_gesture_callback(NULL);
    game->open(screen);

    current_game = game_id;
    input_set_game_state(true, game_manager_exit_cb);
    screensaver_set_game_active(true);
    return true;
}

void game_manager_stop(void)
{
    game_id_t stopped_game = current_game;

    /* Clear state first so a cleanup callback cannot stop the same game twice. */
    current_game = GAME_ID_NONE;

    const game_definition_t *game = game_by_id(stopped_game);
    if (game) {
        game->stop();
    }

    input_set_game_state(false, NULL);
    input_set_game_gesture_callback(NULL);
    screensaver_set_game_active(false);
}

bool game_manager_is_active(void)
{
    return current_game != GAME_ID_NONE;
}

game_id_t game_manager_current(void)
{
    return current_game;
}
