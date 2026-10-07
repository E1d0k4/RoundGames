#include "clock.h"

#include <sys/time.h>

time_t clock_now(void)
{
    time_t now;
    time(&now);
    return now;
}

void clock_adjust_seconds(int delta)
{
    struct timeval tv = {
        .tv_sec = clock_now() + delta,
        .tv_usec = 0
    };
    settimeofday(&tv, NULL);
}

void clock_format_hm(char *buffer, size_t buffer_size)
{
    if (!buffer || buffer_size == 0) return;

    time_t now = clock_now();
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buffer, buffer_size, "%H:%M", &tm_now);
}
