#include "input.h"

static bool launcher_active = false;
static int launcher_page = 0;
static int launcher_page_count = 1;

static input_action_cb_t bottom_action = NULL;
static input_action_cb_t left_action = NULL;
static input_action_cb_t right_action = NULL;
static bool gesture_registered = false;

static void input_gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    if (dir == LV_DIR_BOTTOM) {
        if (bottom_action) bottom_action();
        return;
    }

    if (!launcher_active) return;

    if (dir == LV_DIR_LEFT) {
        if (launcher_page < launcher_page_count - 1) launcher_page++;
        if (left_action) left_action();
    } else if (dir == LV_DIR_RIGHT) {
        if (launcher_page > 0) launcher_page--;
        if (right_action) right_action();
    }
}

void input_init(void)
{
    launcher_active = false;
    launcher_page = 0;
    launcher_page_count = 1;
    bottom_action = NULL;
    left_action = NULL;
    right_action = NULL;
    gesture_registered = false;
}

void input_register_screen(lv_obj_t *screen)
{
    if (!screen || gesture_registered) return;
    lv_obj_add_event_cb(screen, input_gesture_cb, LV_EVENT_GESTURE, NULL);
    gesture_registered = true;
}

void input_set_launcher_state(bool active, int page, int page_count)
{
    launcher_active = active;
    launcher_page = page;
    launcher_page_count = page_count > 0 ? page_count : 1;

    if (launcher_page < 0) launcher_page = 0;
    if (launcher_page >= launcher_page_count) launcher_page = launcher_page_count - 1;
}

int input_get_launcher_page(void)
{
    return launcher_page;
}

void input_set_actions(input_action_cb_t bottom, input_action_cb_t left, input_action_cb_t right)
{
    bottom_action = bottom;
    left_action = left;
    right_action = right;
}
