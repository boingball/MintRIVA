#include "../core/mr_yuv.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t reference_clip(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

static void reference_convert(uint8_t *dst, int dst_stride,
                              const uint8_t *yp, int ys,
                              const uint8_t *up, int us,
                              const uint8_t *vp, int vs,
                              int width, int height)
{
    int y;
    for (y = 0; y < height; y++) {
        const uint8_t *yr = yp + y * ys;
        const uint8_t *ur = up + (y >> 1) * us;
        const uint8_t *vr = vp + (y >> 1) * vs;
        uint8_t *out = dst + y * dst_stride;
        int x;
        for (x = 0; x < width; x++) {
            int c = (int)yr[x] - 16;
            int d = (int)ur[x >> 1] - 128;
            int e = (int)vr[x >> 1] - 128;
            if (c < 0) c = 0;
            out[x * 3 + 0] = reference_clip((298 * c + 409 * e + 128) >> 8);
            out[x * 3 + 1] = reference_clip(
                (298 * c - 100 * d - 208 * e + 128) >> 8);
            out[x * 3 + 2] = reference_clip((298 * c + 516 * d + 128) >> 8);
        }
    }
}

static unsigned next_value(unsigned *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state;
}

static int run_case(int width, int height, unsigned seed)
{
    int ys = width + 3, cs = (width + 1) / 2 + 2, ds = width * 3 + 5;
    size_t yn = (size_t)ys * height;
    size_t cn = (size_t)cs * ((height + 1) / 2);
    size_t dn = (size_t)ds * height;
    uint8_t *yp = (uint8_t *)malloc(yn), *up = (uint8_t *)malloc(cn);
    uint8_t *vp = (uint8_t *)malloc(cn), *expected = (uint8_t *)malloc(dn);
    uint8_t *actual = (uint8_t *)malloc(dn);
    size_t i;
    int ok;
    if (!yp || !up || !vp || !expected || !actual) return 0;
    for (i = 0; i < yn; i++) yp[i] = (uint8_t)(next_value(&seed) >> 24);
    for (i = 0; i < cn; i++) up[i] = (uint8_t)(next_value(&seed) >> 24);
    for (i = 0; i < cn; i++) vp[i] = (uint8_t)(next_value(&seed) >> 24);
    memset(expected, 0xa5, dn);
    memset(actual, 0xa5, dn);
    reference_convert(expected, ds, yp, ys, up, cs, vp, cs, width, height);
    mr_yuv420_to_rgb24(actual, ds, yp, ys, up, cs, vp, cs, width, height,
                       NULL, NULL);
    ok = memcmp(expected, actual, dn) == 0;
    if (!ok) fprintf(stderr, "YUV mismatch at %dx%d\n", width, height);
    free(yp); free(up); free(vp); free(expected); free(actual);
    return ok;
}

static void count_service(void *opaque)
{
    (*(int *)opaque)++;
}

int main(void)
{
    static const int widths[] = { 1, 2, 3, 7, 16, 17, 31 };
    static const int heights[] = { 1, 2, 3, 5, 16, 17, 33 };
    unsigned i, j;
    uint8_t y[33], u[17], v[17], rgb[33 * 3];
    int services = 0;
    for (i = 0; i < sizeof widths / sizeof widths[0]; i++)
        for (j = 0; j < sizeof heights / sizeof heights[0]; j++)
            if (!run_case(widths[i], heights[j], 0x4d525956U + i * 31 + j))
                return 1;
    memset(y, 16, sizeof y); memset(u, 128, sizeof u); memset(v, 128, sizeof v);
    mr_yuv420_to_rgb24(rgb, 3, y, 1, u, 1, v, 1, 1, 33,
                       count_service, &services);
    if (services != 2) {
        fprintf(stderr, "service count %d, expected 2\n", services);
        return 1;
    }
    puts("YUV420 paired-pixel conversion: byte-exact");
    return 0;
}
