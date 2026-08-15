/*
 * MintVID - conformance check for mr_yuv420_dither8() (core/mr_yuv_dither.c).
 *
 * Compares the fused YUV420P -> 8-bit-indexed conversion against the real
 * three-stage composition it replaces - mr_yuv420_to_rgb24() (core/mr_yuv.c)
 * -> mr_scale_resize_rgb24() (core/mr_scale.c) -> mr_dither_rgb8()
 * (core/mr_dither.c), calling those actual functions directly (not a
 * reimplementation) - for the exact geometry mr_yuv420_dither8() is
 * documented to match: destination width == source width, height an exact
 * integer multiple of the vertical scale factor.
 */
#include "../core/mr_yuv.h"
#include "../core/mr_scale.h"
#include "../core/mr_dither.h"
#include "../core/mr_yuv_dither.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned state = 0x79647468U;
static uint8_t random_byte(void)
{
    state = state * 1664525U + 1013904223U;
    return (uint8_t)(state >> 24);
}

static int run_case(int width, int height, int vscale, int y_stride,
                    int u_stride, int v_stride, int extreme, int seed)
{
    int cw = (width + 1) / 2, ch = (height + 1) / 2;
    int dst_h = height / vscale;
    uint8_t *y = (uint8_t *)malloc((size_t)y_stride * height);
    uint8_t *u = (uint8_t *)malloc((size_t)u_stride * ch);
    uint8_t *v = (uint8_t *)malloc((size_t)v_stride * ch);
    uint8_t *rgb_full = (uint8_t *)malloc((size_t)width * height * 3);
    uint8_t *rgb_resized = (uint8_t *)malloc((size_t)width * dst_h * 3);
    uint8_t *ref_out = (uint8_t *)malloc((size_t)width * dst_h);
    uint8_t *got_out = (uint8_t *)malloc((size_t)width * dst_h);
    int yy, xx, y_base = seed & 7, fails = 0;
    (void)cw;

    state = 0x79647468U ^ (unsigned)seed;
    for (yy = 0; yy < height; yy++)
        for (xx = 0; xx < y_stride; xx++)
            y[(size_t)yy * y_stride + xx] = extreme ? ((xx ^ yy) & 1 ? 255 : 0)
                                                    : random_byte();
    for (yy = 0; yy < ch; yy++)
        for (xx = 0; xx < u_stride; xx++)
            u[(size_t)yy * u_stride + xx] = extreme ? ((xx ^ yy) & 1 ? 255 : 0)
                                                    : random_byte();
    for (yy = 0; yy < ch; yy++)
        for (xx = 0; xx < v_stride; xx++)
            v[(size_t)yy * v_stride + xx] = extreme ? ((xx ^ yy) & 1 ? 0 : 255)
                                                    : random_byte();

    /* Reference: the real three-stage pipeline. */
    mr_yuv420_to_rgb24(rgb_full, width * 3, y, y_stride, u, u_stride,
                       v, v_stride, width, height, NULL, NULL);
    if (vscale == 1) {
        memcpy(rgb_resized, rgb_full, (size_t)width * height * 3);
    } else {
        mr_scale_resize_rgb24(rgb_full, width, height, width * 3,
                              rgb_resized, width, dst_h, width * 3);
    }
    mr_dither_rgb8(rgb_resized, width, dst_h, width * 3, ref_out, width,
                  y_base);

    /* Under test: the direct fused path. */
    mr_yuv420_dither8(y, y_stride, u, u_stride, v, v_stride,
                      width, height, vscale, got_out, width, y_base);

    if (memcmp(ref_out, got_out, (size_t)width * dst_h) != 0) {
        int i;
        for (i = 0; i < width * dst_h; i++) {
            if (ref_out[i] != got_out[i]) {
                printf("FAIL w=%d h=%d vscale=%d y_stride=%d u_stride=%d "
                       "v_stride=%d extreme=%d seed=%d index=%d "
                       "ref=%u got=%u\n",
                       width, height, vscale, y_stride, u_stride, v_stride,
                       extreme, seed, i, ref_out[i], got_out[i]);
                fails = 1;
                break;
            }
        }
    }

    free(y); free(u); free(v);
    free(rgb_full); free(rgb_resized); free(ref_out); free(got_out);
    return fails;
}

int main(void)
{
    static const struct { int w, h, vscale; } geoms[] = {
        { 640, 360, 2 },   /* the motivating case: HIRES non-laced AGA fit */
        { 640, 180, 1 },   /* identity (no vertical scaling)                */
        { 320, 240, 1 },
        { 320, 240, 2 },
        { 320, 240, 4 },
        { 480, 360, 3 },
        { 16, 12, 2 },
        { 16, 8, 1 },
        { 18, 12, 2 },     /* odd width - exercises the tail-pixel path     */
        { 2, 2, 1 },
    };
    size_t n = sizeof geoms / sizeof geoms[0], i;
    int fails = 0, seed = 0;

    for (i = 0; i < n; i++) {
        int w = geoms[i].w, h = geoms[i].h, vs = geoms[i].vscale;
        /* Tight strides. */
        fails += run_case(w, h, vs, w, (w + 1) / 2, (w + 1) / 2, 0, seed++);
        fails += run_case(w, h, vs, w, (w + 1) / 2, (w + 1) / 2, 1, seed++);
        /* Padded strides, to catch a stride bug that a tight buffer would
         * hide. */
        fails += run_case(w, h, vs, w + 8, (w + 1) / 2 + 4,
                          (w + 1) / 2 + 4, 0, seed++);
        /* A handful of random y_base/seed repeats per geometry. */
        {
            int r;
            for (r = 0; r < 5; r++)
                fails += run_case(w, h, vs, w, (w + 1) / 2, (w + 1) / 2, 0,
                                  seed++);
        }
    }

    if (fails) {
        printf("YUV420->indexed direct-dither checks FAILED: %d "
               "mismatch(es)\n", fails);
        return 1;
    }
    printf("YUV420->indexed direct-dither checks passed\n");
    return 0;
}
