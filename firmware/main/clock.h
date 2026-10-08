#pragma once

#include <stddef.h>
#include <time.h>

void clock_init(void);
time_t clock_now(void);
void clock_adjust_seconds(int delta);
void clock_format_hm(char *buffer, size_t buffer_size);
