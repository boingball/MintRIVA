/*
 * MintRIVA - portable integer YUV420 to RGB24 conversion.
 *
 * Each U/V sample belongs to two neighbouring luma pixels. Calculating its
 * red/green/blue contribution once per pair removes four multiplications per
 * pixel pair while remaining byte-identical to the original scalar formula.
 */
#include "mr_yuv.h"

static uint8_t clip8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

static void emit_pixel(uint8_t *dst, int luma,
                       int red_add, int green_add, int blue_add)
{
    int scaled_y;
    if (luma < 0) luma = 0;
    scaled_y = 298 * luma;
    dst[0] = clip8((scaled_y + red_add) >> 8);
    dst[1] = clip8((scaled_y + green_add) >> 8);
    dst[2] = clip8((scaled_y + blue_add) >> 8);
}

void mr_yuv420_to_rgb24(uint8_t *dst, int dst_stride,
                        const uint8_t *y_plane, int y_stride,
                        const uint8_t *u_plane, int u_stride,
                        const uint8_t *v_plane, int v_stride,
                        int width, int height,
                        mr_yuv_service_fn service, void *service_opaque)
{
    int row;
    if (!dst || !y_plane || !u_plane || !v_plane ||
        width <= 0 || height <= 0)
        return;

    for (row = 0; row < height; row++) {
        const uint8_t *src_y = y_plane + row * y_stride;
        const uint8_t *src_u = u_plane + (row >> 1) * u_stride;
        const uint8_t *src_v = v_plane + (row >> 1) * v_stride;
        uint8_t *out = dst + row * dst_stride;
        int x;

        for (x = 0; x + 1 < width; x += 2) {
            int d = (int)src_u[x >> 1] - 128;
            int e = (int)src_v[x >> 1] - 128;
            int red_add = 409 * e + 128;
            int green_add = -100 * d - 208 * e + 128;
            int blue_add = 516 * d + 128;
            emit_pixel(out + x * 3, (int)src_y[x] - 16,
                       red_add, green_add, blue_add);
            emit_pixel(out + (x + 1) * 3, (int)src_y[x + 1] - 16,
                       red_add, green_add, blue_add);
        }
        if (x < width) {
            int d = (int)src_u[x >> 1] - 128;
            int e = (int)src_v[x >> 1] - 128;
            emit_pixel(out + x * 3, (int)src_y[x] - 16,
                       409 * e + 128, -100 * d - 208 * e + 128,
                       516 * d + 128);
        }
        if (service && (row & 15) == 15) service(service_opaque);
    }
}
