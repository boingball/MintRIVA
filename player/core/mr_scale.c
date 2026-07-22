/*
 * MintRIVA - 2x nearest-neighbour upscale.
 */
#include "mr_scale.h"

void mr_scale2x_rgb24(const uint8_t *src, int w, int h, int src_stride,
                      uint8_t *dst, int dst_stride)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const uint8_t *sr = src + (size_t)y * src_stride;
        uint8_t       *d0 = dst + (size_t)(y * 2)     * dst_stride;
        uint8_t       *d1 = dst + (size_t)(y * 2 + 1) * dst_stride;
        for (x = 0; x < w; x++) {
            uint8_t r = sr[x * 3 + 0], g = sr[x * 3 + 1], b = sr[x * 3 + 2];
            int o = x * 6;
            d0[o+0]=r; d0[o+1]=g; d0[o+2]=b; d0[o+3]=r; d0[o+4]=g; d0[o+5]=b;
            d1[o+0]=r; d1[o+1]=g; d1[o+2]=b; d1[o+3]=r; d1[o+4]=g; d1[o+5]=b;
        }
    }
}

void mr_scale2x_u8(const uint8_t *src, int w, int h, int src_stride,
                   uint8_t *dst, int dst_stride)
{
    int x, y;
    for (y = 0; y < h; y++) {
        const uint8_t *sr = src + (size_t)y * src_stride;
        uint8_t       *d0 = dst + (size_t)(y * 2)     * dst_stride;
        uint8_t       *d1 = dst + (size_t)(y * 2 + 1) * dst_stride;
        for (x = 0; x < w; x++) {
            uint8_t v = sr[x];
            d0[x * 2] = v; d0[x * 2 + 1] = v;
            d1[x * 2] = v; d1[x * 2 + 1] = v;
        }
    }
}

void mr_scale_down_rgb24(const uint8_t *src, int w, int h, int src_stride,
                         uint8_t *dst, int dst_stride, int factor)
{
    int dw = w / factor, dh = h / factor;
    int area = factor * factor;
    int ox, oy, ix, iy;
    for (oy = 0; oy < dh; oy++) {
        uint8_t *dr = dst + (size_t)oy * dst_stride;
        for (ox = 0; ox < dw; ox++) {
            unsigned r = 0, g = 0, b = 0;
            for (iy = 0; iy < factor; iy++) {
                const uint8_t *sr = src + (size_t)(oy * factor + iy) * src_stride
                                        + (size_t)(ox * factor) * 3;
                for (ix = 0; ix < factor; ix++) {
                    r += sr[0]; g += sr[1]; b += sr[2];
                    sr += 3;
                }
            }
            dr[ox * 3 + 0] = (uint8_t)(r / area);
            dr[ox * 3 + 1] = (uint8_t)(g / area);
            dr[ox * 3 + 2] = (uint8_t)(b / area);
        }
    }
}
