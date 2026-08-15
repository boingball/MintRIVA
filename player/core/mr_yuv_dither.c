/*
 * MintVID - direct YUV420P -> 8-bit indexed conversion (see the header for
 * the geometry contract this relies on).
 *
 * This is a straight algebraic fusion of two existing, independently
 * validated pipelines - core/mr_yuv.c's mr_yuv420_to_rgb24() and
 * core/mr_dither.c's mr_dither_rgb8() - with mr_scale_resize_rgb24()'s
 * nearest-neighbour row/column selection folded in as a source-index
 * computation instead of an intermediate buffer pass. Deliberately does NOT
 * reuse those files' own lookup tables (both are file-static/private) -
 * this rebuilds equivalent tables locally from the same published formulas,
 * so tests/mr_yuv_dither_check.c can compare this function's output against
 * the real three-stage composition (mr_yuv420_to_rgb24 + mr_scale_resize_rgb24
 * + mr_dither_rgb8, called directly) as an independent check that neither
 * copy of the formulas drifted from the other.
 */
#include "mr_yuv_dither.h"

/* ---- YCbCr -> RGB tables (mirrors core/mr_yuv.c's build_tables()) ---- */
static int g_luma_x298[256];
static int g_e_x409[256];
static int g_d_xm100[256];
static int g_e_xm208[256];
static int g_d_x516[256];
static int g_yuv_tables_ready = 0;

static void build_yuv_tables(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        int d = i - 128, e = i - 128;
        g_luma_x298[i] = 298 * i;
        g_e_x409[i] = 409 * e;
        g_d_xm100[i] = -100 * d;
        g_e_xm208[i] = -208 * e;
        g_d_x516[i] = 516 * d;
    }
    g_yuv_tables_ready = 1;
}

static uint8_t clip8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

/* ---- RGB -> 8-bit dither tables (mirrors core/mr_dither.c's build_lut()) */
static const uint8_t bayer4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};
static uint8_t lut_r[16][256], lut_g[16][256], lut_b[16][256];
static int     dither_lut_ready = 0;

static void build_dither_lut(void)
{
    int t, v;
    for (t = 0; t < 16; t++)
        for (v = 0; v < 256; v++) {
            int val = v + (t - 8) * 51 / 16;
            int q;
            if (val < 0) val = 0; else if (val > 255) val = 255;
            q = (val * 5 + 127) / 255;
            lut_r[t][v] = (uint8_t)(q * 36);
            lut_g[t][v] = (uint8_t)(q * 6);
            lut_b[t][v] = (uint8_t)q;
        }
    dither_lut_ready = 1;
}

void mr_yuv420_dither8(const uint8_t *y_plane, int y_stride,
                       const uint8_t *u_plane, int u_stride,
                       const uint8_t *v_plane, int v_stride,
                       int width, int height, int vscale,
                       uint8_t *out, int out_stride, int y_base)
{
    int dst_h, oy;
    if (!y_plane || !u_plane || !v_plane || !out ||
        width <= 0 || height <= 0 || vscale <= 0)
        return;
    if (!g_yuv_tables_ready) build_yuv_tables();
    if (!dither_lut_ready) build_dither_lut();

    dst_h = height / vscale;
    for (oy = 0; oy < dst_h; oy++) {
        int src_row = vscale * oy + vscale / 2;
        int chroma_row = src_row >> 1;
        const uint8_t *src_y = y_plane + (size_t)src_row * y_stride;
        const uint8_t *src_u = u_plane + (size_t)chroma_row * u_stride;
        const uint8_t *src_v = v_plane + (size_t)chroma_row * v_stride;
        uint8_t *dr = out + (size_t)oy * out_stride;
        const uint8_t *threshold = bayer4[(y_base + oy) & 3];
        int x;

        for (x = 0; x + 1 < width; x += 2) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            int red_add = g_e_x409[vv] + 128;
            int green_add = g_d_xm100[uu] + g_e_xm208[vv] + 128;
            int blue_add = g_d_x516[uu] + 128;
            int luma0 = (int)src_y[x] - 16;
            int luma1 = (int)src_y[x + 1] - 16;
            int t0 = threshold[x & 3], t1 = threshold[(x + 1) & 3];
            int scaled_y0, scaled_y1, r, g, b;

            if (luma0 < 0) luma0 = 0;
            scaled_y0 = g_luma_x298[luma0];
            r = clip8((scaled_y0 + red_add) >> 8);
            g = clip8((scaled_y0 + green_add) >> 8);
            b = clip8((scaled_y0 + blue_add) >> 8);
            dr[x] = (uint8_t)(lut_r[t0][r] + lut_g[t0][g] + lut_b[t0][b]);

            if (luma1 < 0) luma1 = 0;
            scaled_y1 = g_luma_x298[luma1];
            r = clip8((scaled_y1 + red_add) >> 8);
            g = clip8((scaled_y1 + green_add) >> 8);
            b = clip8((scaled_y1 + blue_add) >> 8);
            dr[x + 1] = (uint8_t)(lut_r[t1][r] + lut_g[t1][g] + lut_b[t1][b]);
        }
        if (x < width) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            int red_add = g_e_x409[vv] + 128;
            int green_add = g_d_xm100[uu] + g_e_xm208[vv] + 128;
            int blue_add = g_d_x516[uu] + 128;
            int luma0 = (int)src_y[x] - 16;
            int t0 = threshold[x & 3];
            int scaled_y0, r, g, b;

            if (luma0 < 0) luma0 = 0;
            scaled_y0 = g_luma_x298[luma0];
            r = clip8((scaled_y0 + red_add) >> 8);
            g = clip8((scaled_y0 + green_add) >> 8);
            b = clip8((scaled_y0 + blue_add) >> 8);
            dr[x] = (uint8_t)(lut_r[t0][r] + lut_g[t0][g] + lut_b[t0][b]);
        }
    }
}
