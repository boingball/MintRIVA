/*
 * MintVID - RGB24 -> 8-bit palette conversion with 4x4 ordered dithering.
 */
#include "mr_dither.h"

#if defined(MR_M68K_ASM)
/* core/mr_dither_m68k.S - hand-written m68k version of mr_dither_rgb8
 * below, unrolled 4 pixels at a time to match the Bayer pattern's period -
 * see that file's header comment. __asm__ binds to the bare .globl name
 * (m68k-amigaos-gcc otherwise decorates C symbols but not hand-written asm
 * labels), matching every other hand-asm entry point in this project. */
void mr_dither_rgb8_m68k(const uint8_t *rgb, int w, int h, int rgb_stride,
                         uint8_t *out, int out_stride, int y_base,
                         const uint8_t *lut_r, const uint8_t *lut_g,
                         const uint8_t *lut_b)
    __asm__("mr_dither_rgb8_m68k");
#endif

/* Normalised 4x4 Bayer matrix, values 0..15. */
static const uint8_t bayer4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

void mr_dither_palette(uint8_t *pal)
{
    int r, g, b, i = 0;
    for (r = 0; r < 6; r++)
        for (g = 0; g < 6; g++)
            for (b = 0; b < 6; b++) {
                pal[i * 3 + 0] = (uint8_t)(r * 255 / 5);
                pal[i * 3 + 1] = (uint8_t)(g * 255 / 5);
                pal[i * 3 + 2] = (uint8_t)(b * 255 / 5);
                i++;
            }
    for (; i < 256; i++)
        pal[i * 3 + 0] = pal[i * 3 + 1] = pal[i * 3 + 2] = 0;
}

/* Per-(threshold,value) tables giving each channel's pre-weighted cube
 * contribution, so the hot loop is three lookups + two adds - no per-pixel
 * multiply or divide (both are very slow on a 68030). Built once; the values
 * are identical to the old quant6()*weight arithmetic. */
static uint8_t lut_r[16][256], lut_g[16][256], lut_b[16][256];
static int     lut_ready = 0;

static void build_lut(void)
{
    int t, v;
    for (t = 0; t < 16; t++)
        for (v = 0; v < 256; v++) {
            int val = v + (t - 8) * 51 / 16;
            int q;
            if (val < 0) val = 0; else if (val > 255) val = 255;
            q = (val * 5 + 127) / 255;              /* 0..5                 */
            lut_r[t][v] = (uint8_t)(q * 36);
            lut_g[t][v] = (uint8_t)(q * 6);
            lut_b[t][v] = (uint8_t)q;
        }
    lut_ready = 1;
}

void mr_dither_rgb8(const uint8_t *rgb, int w, int h, int rgb_stride,
                    uint8_t *out, int out_stride, int y_base)
{
    int x, y;
    if (!lut_ready) build_lut();
#if defined(MR_M68K_ASM)
    mr_dither_rgb8_m68k(rgb, w, h, rgb_stride, out, out_stride, y_base,
                        &lut_r[0][0], &lut_g[0][0], &lut_b[0][0]);
    return;
#endif
    for (y = 0; y < h; y++) {
        const uint8_t *sr = rgb + (size_t)y * rgb_stride;
        uint8_t       *dr = out + (size_t)y * out_stride;
        const uint8_t *br = bayer4[(y_base + y) & 3];
        for (x = 0; x < w; x++) {
            const uint8_t *p = sr + x * 3;
            int t = br[x & 3];
            dr[x] = (uint8_t)(lut_r[t][p[0]] + lut_g[t][p[1]] + lut_b[t][p[2]]);
        }
    }
}
