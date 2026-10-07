#include "audio.h"

#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "roundgames_audio";

static esp_codec_dev_handle_t speaker_codec = NULL;
static TaskHandle_t tone_task_handle = NULL;
static volatile int tone_request_volume = -1;
static TickType_t last_tone_tick = 0;

static int volume_to_codec(int value)
{
    if (value <= 0) return 0;
    return 15 + (int)(sqrtf((float)value / 100.0f) * 85.0f);
}

static void play_test_tone_blocking(int requested_volume)
{
    if (requested_volume <= 0) return;

    if (!speaker_codec) {
        speaker_codec = bsp_audio_codec_speaker_init();
        if (!speaker_codec) {
            ESP_LOGE(TAG, "Speaker codec init failed");
            return;
        }

        esp_codec_dev_sample_info_t fs = {
            .sample_rate = 24000,
            .channel = 2,
            .bits_per_sample = 16
        };

        esp_err_t open_ret = esp_codec_dev_open(speaker_codec, &fs);
        if (open_ret != ESP_OK) {
            ESP_LOGE(TAG, "Speaker open failed: %s", esp_err_to_name(open_ret));
            speaker_codec = NULL;
            return;
        }

        ESP_LOGI(TAG, "Speaker codec opened: 24000 Hz, stereo, 16-bit");
    }

    esp_err_t vol_ret = esp_codec_dev_set_out_vol(
        speaker_codec, volume_to_codec(requested_volume));
    if (vol_ret != ESP_OK) {
        ESP_LOGE(TAG, "Speaker volume failed: %s", esp_err_to_name(vol_ret));
        return;
    }

    static int16_t tone[2880 * 2];
    static bool ready = false;
    if (!ready) {
        for (int i = 0; i < 2880; i++) {
            float t = (float)i / 24000.0f;
            float envelope = 1.0f;
            if (t < 0.008f) envelope = t / 0.008f;
            if (t > 0.085f) envelope = (0.120f - t) / 0.035f;
            if (envelope < 0.0f) envelope = 0.0f;

            float sample = sinf(2.0f * 3.14159265f * 1000.0f * t);
            int16_t value = (int16_t)(sample * envelope * 10000.0f);
            tone[i * 2] = value;
            tone[i * 2 + 1] = value;
        }
        ready = true;
    }

    esp_err_t write_ret = esp_codec_dev_write(speaker_codec, tone, sizeof(tone));
    if (write_ret != ESP_OK) {
        ESP_LOGE(TAG, "Speaker write failed: %s", esp_err_to_name(write_ret));
    }
}

static void tone_task(void *arg)
{
    (void)arg;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        int requested_volume = tone_request_volume;
        tone_request_volume = -1;
        TickType_t now = xTaskGetTickCount();

        if (requested_volume > 0 &&
            (last_tone_tick == 0 ||
             now - last_tone_tick >= pdMS_TO_TICKS(140))) {
            last_tone_tick = now;
            play_test_tone_blocking(requested_volume);
        }
    }
}

void audio_init(void)
{
    if (!speaker_codec) {
        speaker_codec = bsp_audio_codec_speaker_init();
        if (speaker_codec) {
            esp_codec_dev_sample_info_t fs = {
                .sample_rate = 24000,
                .channel = 2,
                .bits_per_sample = 16
            };

            esp_err_t open_ret = esp_codec_dev_open(speaker_codec, &fs);
            if (open_ret == ESP_OK) {
                ESP_LOGI(TAG, "Speaker ready");
            } else {
                ESP_LOGE(TAG, "Speaker open failed during startup: %s",
                         esp_err_to_name(open_ret));
                speaker_codec = NULL;
            }
        } else {
            ESP_LOGE(TAG, "Speaker codec init failed during startup");
        }
    }

    if (!tone_task_handle) {
        xTaskCreate(tone_task, "tone_task", 4096, NULL, 4, &tone_task_handle);
    }
}

void audio_set_volume(int volume, bool muted)
{
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    if (!speaker_codec) return;

    esp_err_t ret = esp_codec_dev_set_out_vol(
        speaker_codec, muted ? 0 : volume_to_codec(volume));

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Speaker volume failed: %s", esp_err_to_name(ret));
    }
}

void audio_play_test_tone(int volume, bool muted)
{
    if (muted || volume <= 0 || !tone_task_handle) return;

    tone_request_volume = volume;
    xTaskNotifyGive(tone_task_handle);
}
