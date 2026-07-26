#include "../core/mr_c2p.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPLANES 8

static uint32_t random_state = 0x6d725632U;

static uint8_t next_random_byte(void)
{
    random_state = random_state * 1664525U + 1013904223U;
    return (uint8_t)(random_state >> 24);
}

static int check_case(int width, int height, int dirty_y0, int dirty_y1)
{
    int stride = width + 13, x0byte = 4, dst_y0 = 3 + dirty_y0;
    int bpr = x0byte + width / 8 + 7, dst_rows = height + 8;
    size_t plane_size = (size_t)bpr * dst_rows;
    uint8_t *chunky = (uint8_t *)malloc((size_t)stride * height);
    uint8_t *portable[NPLANES] = { 0 }, *riva[NPLANES] = { 0 };
    int plane, i, ok = chunky != NULL;

    if (ok)
        for (i = 0; i < stride * height; i++) chunky[i] = next_random_byte();
    for (plane = 0; ok && plane < NPLANES; plane++) {
        portable[plane] = (uint8_t *)malloc(plane_size);
        riva[plane] = (uint8_t *)malloc(plane_size);
        if (!portable[plane] || !riva[plane]) { ok = 0; break; }
        memset(portable[plane], 0xa5, plane_size);
        memset(riva[plane], 0xa5, plane_size);
    }
    if (ok) {
        const uint8_t *dirty = chunky + (size_t)dirty_y0 * stride;
        int rows = dirty_y1 - dirty_y0;
        mr_c2p8(dirty, width, rows, stride, NPLANES, portable, bpr,
                x0byte, dst_y0);
        mr_c2p8_riva32(dirty, width, rows, stride, NPLANES, riva, bpr,
                       x0byte, dst_y0);
        for (plane = 0; plane < NPLANES; plane++) {
            if (memcmp(portable[plane], riva[plane], plane_size)) {
                fprintf(stderr, "C2P mismatch: width=%d dirty=%d..%d plane=%d\n",
                        width, dirty_y0, dirty_y1, plane);
                ok = 0;
            }
        }
    }
    for (plane = 0; plane < NPLANES; plane++) {
        free(portable[plane]);
        free(riva[plane]);
    }
    free(chunky);
    return ok;
}

int main(void)
{
    int iteration, width;
    for (iteration = 0; iteration < 200; iteration++) {
        for (width = 32; width <= 160; width += 32) {
            if (!check_case(width, 11, 0, 11) ||
                !check_case(width, 11, 2, 9) ||
                !check_case(width, 11, 7, 8))
                return 1;
        }
    }
    puts("C2P checks passed");
    return 0;
}
