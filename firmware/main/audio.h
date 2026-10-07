#pragma once

#include <stdbool.h>

void audio_init(void);
void audio_set_volume(int volume, bool muted);
void audio_play_test_tone(int volume, bool muted);
