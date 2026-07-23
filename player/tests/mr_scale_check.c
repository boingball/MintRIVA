#include "../core/mr_scale.h"

#include <stdio.h>
#include <string.h>

static int check_fit(int w, int h, int max_w, int max_h,
                     int expected_w, int expected_h)
{
    int dw, dh;
    mr_scale_fit_rect(w, h, max_w, max_h, &dw, &dh);
    if (dw != expected_w || dh != expected_h) {
        fprintf(stderr, "fit %dx%d in %dx%d: got %dx%d, expected %dx%d\n",
                w, h, max_w, max_h, dw, dh, expected_w, expected_h);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const uint8_t src[4 * 3] = {
        10, 11, 12,  20, 21, 22,  30, 31, 32,  40, 41, 42
    };
    uint8_t dst[2 * 3];
    int failed = 0;

    failed |= check_fit(854, 480, 320, 256, 320, 180);
    failed |= check_fit(854, 480, 640, 512, 640, 360);
    failed |= check_fit(320, 180, 320, 256, 320, 180);
    failed |= check_fit(240, 320, 320, 256, 192, 256);

    /* Centre sampling when reducing four pixels to two selects 1 and 3. */
    mr_scale_resize_rgb24(src, 4, 1, 4 * 3, dst, 2, 1, 2 * 3);
    if (memcmp(dst, src + 3, 3) || memcmp(dst + 3, src + 9, 3)) {
        fprintf(stderr, "nearest-neighbour centre sampling failed\n");
        failed = 1;
    }

    if (!failed) puts("scale checks passed");
    return failed;
}
