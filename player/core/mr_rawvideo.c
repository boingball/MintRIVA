/* Codec-neutral packed raw-video conversion. */
#include "mr_rawvideo.h"
#include <stdlib.h>

typedef struct {
    int width, height, dst_stride;
    uint32_t src_stride;
    uint8_t *frame;
} rawvideo_ctx;

static uint8_t clamp8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

/* QuickTime 2vuy/UYVY uses studio-range BT.601 Y'CbCr: Y=16..235 and
 * Cb/Cr=16..240.  Values outside that nominal range are clamped only after
 * integer RGB reconstruction, matching the conventional CCIR 601 expansion. */
static void yuv601(uint8_t y, int cb, int cr, uint8_t *rgb)
{
    int c = (int)y - 16;
    int d = cb - 128;
    int e = cr - 128;
    if (c < 0) c = 0;
    rgb[0] = clamp8((298 * c + 409 * e + 128) >> 8);
    rgb[1] = clamp8((298 * c - 100 * d - 208 * e + 128) >> 8);
    rgb[2] = clamp8((298 * c + 516 * d + 128) >> 8);
}

static mr_status rawvideo_open(mr_decoder *dec)
{
    rawvideo_ctx *c = (rawvideo_ctx *)calloc(1, sizeof *c);
    size_t bytes;
    uint32_t minimum = (uint32_t)((dec->width + 1) / 2) * 4u;
    if (!c) return MR_ENOMEM;
    c->width = dec->width;
    c->height = dec->height;
    c->dst_stride = dec->width * 3;
    c->src_stride = dec->config_len >= 4 ? mr_rl32(dec->config) : minimum;
    if (c->src_stride < minimum) { free(c); return MR_EFORMAT; }
    bytes = (size_t)c->dst_stride * c->height;
    c->frame = (uint8_t *)malloc(bytes);
    if (!c->frame) { free(c); return MR_ENOMEM; }
    dec->priv = c;
    dec->frame.width = c->width; dec->frame.height = c->height;
    dec->frame.stride = c->dst_stride; dec->frame.fmt = MR_PIX_RGB24;
    dec->frame.data = c->frame;
    return MR_OK;
}

static mr_status rawvideo_decode(mr_decoder *dec, const uint8_t *data,
                                 uint32_t len)
{
    rawvideo_ctx *c = (rawvideo_ctx *)dec->priv;
    uint32_t needed;
    int y, x;
    if (!data || c->height <= 0 ||
        c->src_stride > UINT32_MAX / (uint32_t)c->height)
        return MR_EFORMAT;
    needed = c->src_stride * (uint32_t)c->height;
    if (len < needed) return MR_EFORMAT;
    for (y = 0; y < c->height; y++) {
        const uint8_t *src = data + (size_t)y * c->src_stride;
        uint8_t *dst = c->frame + (size_t)y * c->dst_stride;
        for (x = 0; x < c->width; x += 2, src += 4) {
            int cb = src[0], cr = src[2];
            yuv601(src[1], cb, cr, dst + x * 3);
            if (x + 1 < c->width)
                yuv601(src[3], cb, cr, dst + (x + 1) * 3);
        }
    }
    dec->frame.dirty_y0 = 0; dec->frame.dirty_y1 = c->height;
    return MR_OK;
}

static void rawvideo_close(mr_decoder *dec)
{
    rawvideo_ctx *c = (rawvideo_ctx *)dec->priv;
    if (c) { free(c->frame); free(c); }
    dec->priv = NULL;
}

const mr_codec mr_codec_rawvideo = {
    "raw UYVY422 (2vuy)",
    { MR_FOURCC('2','v','u','y'), MR_FOURCC('U','Y','V','Y'), 0 },
    rawvideo_open, rawvideo_decode, rawvideo_close, NULL
};
