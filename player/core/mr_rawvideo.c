/* Codec-neutral packed raw-video conversion. */
#include "mr_rawvideo.h"
#include <stdlib.h>

typedef struct {
    int width, height, dst_stride;
    uint32_t src_stride, minimum_row;
    uint8_t *frame;
} rawvideo_ctx;

int mr_rawvideo_is_uyvy422(uint32_t fourcc)
{
    return fourcc == MR_FOURCC('2','v','u','y') ||
           fourcc == MR_FOURCC('2','V','u','y') ||
           fourcc == MR_FOURCC('U','Y','V','Y');
}

static uint32_t uyvy422_minimum_stride(int width)
{
    uint32_t pairs;
    if (width <= 0) return 0;
    pairs = ((uint32_t)width + 1u) / 2u;
    return pairs <= UINT32_MAX / 4u ? pairs * 4u : 0;
}

uint32_t mr_rawvideo_uyvy422_stride(int width, int height,
                                    uint32_t packet_size)
{
    uint32_t minimum, stride;
    if (height <= 0) return 0;
    minimum = uyvy422_minimum_stride(width);
    if (!minimum) return 0;
    stride = (packet_size / (uint32_t)height) & ~1u;
    if (stride < minimum || stride > packet_size / (uint32_t)height)
        return 0;
    return stride;
}

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
    uint32_t minimum;
    if (!c) return MR_ENOMEM;
    minimum = uyvy422_minimum_stride(dec->width);
    if (!minimum) { free(c); return MR_EFORMAT; }
    c->width = dec->width;
    c->height = dec->height;
    c->dst_stride = dec->width * 3;
    /* MOV must provide the stored rowBytes.  Silently assuming packed rows
     * when container setup is lost produces plausible but badly sheared
     * pictures, so fail opening instead. */
    if (!dec->config || dec->config_len < 4) { free(c); return MR_EFORMAT; }
    c->src_stride = mr_rl32(dec->config);
    if (c->src_stride < minimum) { free(c); return MR_EFORMAT; }
    c->minimum_row = minimum;
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
    uint32_t last_row;
    int y, x;
    if (!data || c->height <= 0 || c->src_stride < c->minimum_row)
        return MR_EFORMAT;
    /* Only a visible packed row is required at the final row start.  Stored
     * padding after that row may be shorter than src_stride, and unrelated
     * packet trailing bytes are allowed. */
    if ((uint32_t)(c->height - 1) >
        (UINT32_MAX - c->minimum_row) / c->src_stride)
        return MR_EFORMAT;
    last_row = (uint32_t)(c->height - 1) * c->src_stride + c->minimum_row;
    if (len < last_row) return MR_EFORMAT;
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

uint32_t mr_rawvideo_source_stride(const mr_decoder *dec)
{
    const rawvideo_ctx *c;
    if (!dec || dec->codec != &mr_codec_rawvideo || !dec->priv) return 0;
    c = (const rawvideo_ctx *)dec->priv;
    return c->src_stride;
}

static void rawvideo_close(mr_decoder *dec)
{
    rawvideo_ctx *c = (rawvideo_ctx *)dec->priv;
    if (c) { free(c->frame); free(c); }
    dec->priv = NULL;
}

const mr_codec mr_codec_rawvideo = {
    "raw UYVY422 (2vuy)",
    { MR_FOURCC('2','v','u','y'), MR_FOURCC('2','V','u','y'),
      MR_FOURCC('U','Y','V','Y'), 0 },
    rawvideo_open, rawvideo_decode, rawvideo_close, NULL
};
