#include "game_manager.h"

#include "input.h"
#include "screensaver.h"
#include "tic_tac_toe.h"
#include "snake.h"

static game_id_t current_game = GAME_ID_NONE;
static void (*exit_callback)(void) = NULL;

static void game_manager_exit_cb(void)
{
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
}

bool game_manager_start(game_id_t game_id, lv_obj_t *screen)
{
    if (!screen || current_game != GAME_ID_NONE) {
        return false;
    }

    bool started = false;

    switch (game_id) {
        case GAME_ID_TIC_TAC_TOE:
            input_set_game_gesture_callback(NULL);
            tic_tac_toe_open(screen);
            started = true;
            break;

        case GAME_ID_SNAKE:
            snake_open(screen);
            started = true;
            break;

        default:
            return false;
    }

    if (started) {
        current_game = game_id;
        input_set_game_state(true, game_manager_exit_cb);
        screensaver_set_game_active(true);
    }

    return started;
}

void game_manager_stop(void)
{
    if (current_game == GAME_ID_TIC_TAC_TOE) {
        tic_tac_toe_stop();
    } else if (current_game == GAME_ID_SNAKE) {
        snake_stop();
    }

    current_game = GAME_ID_NONE;
    input_set_game_state(false, NULL);
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
