#include "power_button.h"

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "esp_io_expander_tca9554.h"

#define POWER_BUTTON_PIN IO_EXPANDER_PIN_NUM_4
#define POWER_BUTTON_POLL_MS 30
#define POWER_BUTTON_SHORT_MAX_MS 1000

static esp_io_expander_handle_t io_expander = NULL;
static power_button_callback_t button_callback = NULL;
static bool previous_pressed = false;
static uint32_t press_start_ms = 0;

static void power_button_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!io_expander) return;

    uint32_t levels = 0;
    if (esp_io_expander_get_level(io_expander, POWER_BUTTON_PIN, &levels) != ESP_OK) {
        return;
    }

    bool pressed = (levels & POWER_BUTTON_PIN) != 0;
    uint32_t now = lv_tick_get();

    if (pressed && !previous_pressed) {
        press_start_ms = now;
    } else if (!pressed && previous_pressed) {
        uint32_t duration = now - press_start_ms;

        /* Only a short press is handled by RoundGames.
         * Long presses remain available to the board's AXP2101 power logic. */
        if (duration < POWER_BUTTON_SHORT_MAX_MS && button_callback) {
            button_callback();
        }
    }

    previous_pressed = pressed;
}

void power_button_init(void)
{
    io_expander = bsp_io_expander_init();
    if (!io_expander) return;

    esp_io_expander_set_dir(io_expander, POWER_BUTTON_PIN, IO_EXPANDER_INPUT);

    uint32_t levels = 0;
    if (esp_io_expander_get_level(io_expander, POWER_BUTTON_PIN, &levels) == ESP_OK) {
        previous_pressed = (levels & POWER_BUTTON_PIN) != 0;
    }

    lv_timer_create(power_button_tick, POWER_BUTTON_POLL_MS, NULL);
}

void power_button_set_callback(power_button_callback_t callback)
{
    button_callback = callback;
}
