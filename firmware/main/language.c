#include "language.h"

static int language = 0;

const char *language_tr(const char *en, const char *de)
{
    return language ? de : en;
}

int language_get(void)
{
    return language;
}

void language_set(int value)
{
    language = value ? 1 : 0;
}
