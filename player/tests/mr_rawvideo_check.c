#include "../core/mr_rawvideo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failed;
#define CHECK(v) do { if (!(v)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #v); failed++; \
} } while (0)

static mr_status open_decoder(mr_decoder *d, int width, int height,
                              const uint8_t *config, uint32_t config_len)
{
    memset(d, 0, sizeof *d);
    d->codec = &mr_codec_rawvideo;
    d->width = width; d->height = height;
    d->config = config; d->config_len = config_len;
    return d->codec->open(d);
}

static void expect_pixel(const mr_decoder *d, int x, int y,
                         int r, int g, int b)
{
    const uint8_t *p = d->frame.data + (size_t)y * d->frame.stride + x * 3;
    CHECK(p[0] == r); CHECK(p[1] == g); CHECK(p[2] == b);
}

static void check_pairs(void)
{
    static const struct {
        uint8_t cb, y, cr;
        uint8_t r, g, b;
    } cases[] = {
        { 128, 128, 128, 130, 130, 130 }, /* neutral grey */
        { 128,  16, 128,   0,   0,   0 }, /* studio black */
        { 128, 235, 128, 255, 255, 255 }, /* studio white */
        { 128,  81, 240, 255,   0,  76 }, /* red-biased */
        { 240,  41, 128,  29,   0, 255 }  /* blue-biased */
    };
    unsigned i;
    for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        uint8_t packet[4] = { cases[i].cb, cases[i].y,
                              cases[i].cr, cases[i].y };
        mr_decoder d;
        CHECK(open_decoder(&d, 2, 1, NULL, 0) == MR_OK);
        CHECK(d.codec->decode(&d, packet, sizeof packet) == MR_OK);
        expect_pixel(&d, 0, 0, cases[i].r, cases[i].g, cases[i].b);
        expect_pixel(&d, 1, 0, cases[i].r, cases[i].g, cases[i].b);
        d.codec->close(&d);
    }
}

static void check_stride_odd_and_short(void)
{
    uint8_t stride[4] = { 12, 0, 0, 0 };
    uint8_t guarded[2 + 24];
    mr_decoder d;
    memset(guarded, 0xa5, sizeof guarded);
    /* Two padded, top-down rows; width=3 consumes two complete UYVY pairs. */
    { uint8_t rows[24] = {
        128,16,128,235, 128,128,128,77, 9,9,9,9,
        128,235,128,16, 128,16,128,88, 8,8,8,8
      };
      memcpy(guarded + 1, rows, sizeof rows);
    }
    CHECK(open_decoder(&d, 3, 2, stride, sizeof stride) == MR_OK);
    CHECK(d.codec->decode(&d, guarded + 1, 23) == MR_EFORMAT);
    CHECK(d.codec->decode(&d, guarded + 1, 24) == MR_OK);
    CHECK(guarded[0] == 0xa5 && guarded[25] == 0xa5);
    expect_pixel(&d, 0, 0, 0, 0, 0);
    expect_pixel(&d, 1, 0, 255, 255, 255);
    expect_pixel(&d, 2, 0, 130, 130, 130);
    expect_pixel(&d, 0, 1, 255, 255, 255);
    expect_pixel(&d, 1, 1, 0, 0, 0);
    expect_pixel(&d, 2, 1, 0, 0, 0);
    d.codec->close(&d);
}

static void check_sample_strides(void)
{
    uint8_t tight_stride[4] = { 0x80, 0x02, 0, 0 }; /* 640 */
    uint8_t padded_stride[4] = { 0x96, 0x02, 0, 0 }; /* 662 */
    uint8_t invalid_stride[4] = { 0x7e, 0x02, 0, 0 }; /* 638 */
    uint8_t tight[640];
    uint8_t *padded;
    mr_decoder d;
    unsigned i;

    CHECK(mr_rawvideo_is_uyvy422(MR_FOURCC('2','v','u','y')));
    CHECK(mr_rawvideo_is_uyvy422(MR_FOURCC('2','V','u','y')));
    CHECK(mr_rawvideo_is_uyvy422(MR_FOURCC('U','Y','V','Y')));
    CHECK(!mr_rawvideo_is_uyvy422(MR_FOURCC('Y','U','Y','2')));
    CHECK(mr_rawvideo_uyvy422_stride(320, 136, 90112) == 662);
    CHECK(662u * 136u == 90032u);
    CHECK(90112u - 662u * 136u == 80u);
    CHECK(mr_rawvideo_uyvy422_stride(320, 1, 638) == 0);

    for (i = 0; i < sizeof tight; i += 4) {
        tight[i] = 128; tight[i + 1] = 16;
        tight[i + 2] = 128; tight[i + 3] = 235;
    }
    CHECK(open_decoder(&d, 320, 1, tight_stride, 4) == MR_OK);
    CHECK(d.codec->decode(&d, tight, sizeof tight) == MR_OK);
    expect_pixel(&d, 0, 0, 0, 0, 0);
    expect_pixel(&d, 319, 0, 255, 255, 255);
    d.codec->close(&d);

    padded = (uint8_t *)malloc(90112);
    CHECK(padded != NULL);
    if (padded) {
        int y, frame;
        memset(padded, 0xcc, 90112);
        for (y = 0; y < 136; y++) {
            uint8_t *row = padded + (size_t)y * 662;
            for (i = 0; i < 640; i += 4) {
                row[i] = 128; row[i + 1] = y & 1 ? 235 : 16;
                row[i + 2] = 128; row[i + 3] = y & 1 ? 16 : 235;
            }
        }
        CHECK(open_decoder(&d, 320, 136, padded_stride, 4) == MR_OK);
        CHECK(d.codec->decode(&d, padded, 90031) == MR_EFORMAT);
        for (frame = 0; frame < 165; frame++)
            CHECK(d.codec->decode(&d, padded, 90112) == MR_OK);
        expect_pixel(&d, 0, 0, 0, 0, 0);
        expect_pixel(&d, 319, 0, 255, 255, 255);
        expect_pixel(&d, 0, 1, 255, 255, 255);
        expect_pixel(&d, 319, 1, 0, 0, 0);
        expect_pixel(&d, 0, 135, 255, 255, 255);
        d.codec->close(&d);
        free(padded);
    }

    CHECK(open_decoder(&d, 320, 1, invalid_stride, 4) == MR_EFORMAT);
}

int main(void)
{
    check_pairs();
    check_stride_odd_and_short();
    check_sample_strides();
    if (failed) return 1;
    puts("raw UYVY422 decoder checks passed");
    return 0;
}
