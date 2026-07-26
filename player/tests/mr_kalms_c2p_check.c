/* Amiga-side differential check for the vendored Kalms 68030 converter. */
#include "../core/mr_c2p.h"
#include "../vendor/kalms-c2p/normal/c2p1x1_8_c5_030.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long random_state = 0x4b616c6dUL;

static unsigned char random_byte(void)
{
    random_state = random_state * 1103515245UL + 12345UL;
    return (unsigned char)(random_state >> 16);
}

static int check_case(int width, int height)
{
    int bpr = width / 8, bplsize = bpr * height;
    size_t total = (size_t)bplsize * 8;
    unsigned char *chunky = (unsigned char *)malloc((size_t)width * height);
    unsigned char *kalms = (unsigned char *)malloc(total);
    unsigned char *reference = (unsigned char *)malloc(total);
    unsigned char *planes[8];
    int i, p, ok = chunky && kalms && reference;

    if (ok) {
        for (i = 0; i < width * height; i++) chunky[i] = random_byte();
        memset(kalms, 0, total);
        memset(reference, 0, total);
        for (p = 0; p < 8; p++) planes[p] = reference + (size_t)p * bplsize;
        mr_c2p8(chunky, width, height, width, 8, planes, bpr, 0, 0);
        c2p1x1_8_c5_030_smcinit(width, height, 0, bplsize);
        c2p1x1_8_c5_030(chunky, kalms);
        ok = memcmp(kalms, reference, total) == 0;
        if (!ok) fprintf(stderr, "Kalms mismatch at %dx%d\n", width, height);
    }
    free(reference);
    free(kalms);
    free(chunky);
    return ok;
}

int main(void)
{
    static const int widths[] = { 32, 64, 96, 160, 320, 640 };
    static const int heights[] = { 2, 7, 19 };
    unsigned int w, h;
    for (w = 0; w < sizeof widths / sizeof widths[0]; w++)
        for (h = 0; h < sizeof heights / sizeof heights[0]; h++)
            if (!check_case(widths[w], heights[h])) return 1;
    puts("Kalms C2P checks passed");
    return 0;
}
