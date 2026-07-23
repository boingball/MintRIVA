#include "compat.h"

size_t mr_libavc_strnlen(const char *text, size_t maximum)
{
    size_t n = 0;
    while (n < maximum && text[n]) n++;
    return n;
}
