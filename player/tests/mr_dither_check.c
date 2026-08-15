/*
 * Bit-exactness check for mr_dither_rgb8() (core/mr_dither.c) - on the host
 * build this exercises the portable C fallback; on a real m68k build
 * (tests/run_m68k_check.sh) mr_dither_rgb8() itself dispatches to the hand
 * asm mr_dither_rgb8_m68k() (core/mr_dither_m68k.S), so the same comparison
 * exercises the real asm there.
 *
 * ref_dither_rgb8() below is an independent re-derivation straight from the
 * threshold-offset/6x6x6-cube-quantisation formula in build_lut()
 * (core/mr_dither.c), computed fresh per pixel rather than via a shared
 * precomputed LUT - so a bug in either the LUT build or the asm's LUT
 * indexing would not be masked by testing against itself.
 */
#include "../core/mr_dither.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static uint32_t xrand(uint32_t *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7fffu;
}

static const uint8_t ref_bayer4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 },
};

static uint8_t ref_quant6(int val)
{
    int q;
    if (val < 0) val = 0; else if (val > 255) val = 255;
    q = (val * 5 + 127) / 255;              /* 0..5 */
    return (uint8_t)q;
}

static void ref_dither_rgb8(const uint8_t *rgb, int w, int h, int rgb_stride,
                            uint8_t *out, int out_stride, int y_base)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const uint8_t *sr = rgb + (size_t)y * rgb_stride;
        uint8_t       *dr = out + (size_t)y * out_stride;
        const uint8_t *br = ref_bayer4[(y_base + y) & 3];
        for (x = 0; x < w; x++) {
            const uint8_t *p = sr + x * 3;
            int t = br[x & 3];
            int offset = (t - 8) * 51 / 16;
            uint8_t qr = ref_quant6(p[0] + offset);
            uint8_t qg = ref_quant6(p[1] + offset);
            uint8_t qb = ref_quant6(p[2] + offset);
            dr[x] = (uint8_t)(qr * 36 + qg * 6 + qb);
        }
    }
}

static void check_size(int w, int h, uint32_t *seed, int extreme)
{
    int pad_w = w + 5, pad_h = h + 3; /* mismatched rgb_stride/out_stride */
    int rgb_stride = pad_w * 3 + 7, out_stride = pad_w + 11;
    unsigned char *rgb = (unsigned char *)malloc((size_t)rgb_stride * pad_h);
    uint8_t *out_asm = (uint8_t *)malloc((size_t)out_stride * pad_h);
    uint8_t *out_ref = (uint8_t *)malloc((size_t)out_stride * pad_h);
    int y_base, row, col;

    if (!rgb || !out_asm || !out_ref) {
        fprintf(stderr, "mr_dither_check: allocation failed\n");
        exit(1);
    }
    for (row = 0; row < rgb_stride * pad_h; row++)
        rgb[row] = extreme ? (uint8_t)((xrand(seed) & 1) ? 255 : 0)
                           : (uint8_t)(xrand(seed) & 0xff);

    for (y_base = 0; y_base < 5; y_base++) {
        memset(out_asm, 0xAA, (size_t)out_stride * pad_h);
        memset(out_ref, 0xAA, (size_t)out_stride * pad_h);
        mr_dither_rgb8(rgb, w, h, rgb_stride, out_asm, out_stride, y_base);
        ref_dither_rgb8(rgb, w, h, rgb_stride, out_ref, out_stride, y_base);
        for (row = 0; row < h; row++)
            for (col = 0; col < w; col++) {
                int idx = row * out_stride + col;
                if (out_asm[idx] != out_ref[idx]) {
                    if (g_failures < 20)
                        fprintf(stderr,
                                "FAIL dither: w=%d h=%d y_base=%d row=%d "
                                "col=%d expected=%d got=%d\n",
                                w, h, y_base, row, col, out_ref[idx],
                                out_asm[idx]);
                    g_failures++;
                }
            }
        /* Padding past the visible width/height must be left untouched. */
        for (row = 0; row < pad_h; row++)
            for (col = 0; col < out_stride; col++) {
                if (row < h && col < w) continue;
                int idx = row * out_stride + col;
                if (out_asm[idx] != 0xAA) {
                    if (g_failures < 20)
                        fprintf(stderr,
                                "FAIL dither padding: w=%d h=%d y_base=%d "
                                "row=%d col=%d wrote=%d\n",
                                w, h, y_base, row, col, out_asm[idx]);
                    g_failures++;
                }
            }
    }
    free(rgb); free(out_asm); free(out_ref);
}

int main(void)
{
    static const int widths[] = { 1, 2, 3, 4, 5, 7, 8, 15, 16, 17, 63, 64,
                                  65, 160, 320, 640 };
    static const int heights[] = { 1, 2, 3, 4, 5, 11, 96, 180, 360 };
    uint32_t seed = 3;
    unsigned wi, hi;
    for (wi = 0; wi < sizeof widths / sizeof widths[0]; wi++)
        for (hi = 0; hi < sizeof heights / sizeof heights[0]; hi++) {
            check_size(widths[wi], heights[hi], &seed, 0);
            check_size(widths[wi], heights[hi], &seed, 1);
        }

    if (g_failures) {
        fprintf(stderr, "mr_dither_check: %d mismatches\n", g_failures);
        return 1;
    }
    printf("dither checks passed\n");
    return 0;
}
