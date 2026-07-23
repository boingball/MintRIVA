/*
 * MintRIVA - MPEG-1/MPEG-2 video decoder adapter.
 *
 * libmpeg2 supplies the Main Profile bitstream decoder, reference pictures and
 * display reordering. The adapter consumes one elementary-stream PES payload
 * per call and converts the displayed YUV420 frame to RGB24.
 */
#include "mr_mpeg2.h"

#include "mpeg2.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    mpeg2dec_t        *decoder;
    const mpeg2_info_t *info;
    uint8_t           *rgb;
    uint8_t           *queued_rgb;
    int                queued;
    int                flushing;
    int                flush_done;
} mpeg2_state;

static int clip8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static mr_status emit_rgb(mr_decoder *dec, uint8_t *rgb)
{
    mpeg2_state *s = (mpeg2_state *)dec->priv;
    const mpeg2_sequence_t *seq = s->info->sequence;
    uint8_t *const *planes = s->info->display_fbuf->buf;
    int width = dec->width, height = dec->height;
    int y_stride, uv_stride, x, y;

    if (!seq || !planes[0] || !planes[1] || !planes[2] || !rgb)
        return MR_EFORMAT;
    if ((int)seq->picture_width < width) width = (int)seq->picture_width;
    if ((int)seq->picture_height < height) height = (int)seq->picture_height;
    y_stride = (int)seq->width;
    uv_stride = (int)seq->chroma_width;

    for (y = 0; y < height; y++) {
        const uint8_t *yr = planes[0] + (size_t)y * y_stride;
        const uint8_t *ur = planes[1] + (size_t)(y >> 1) * uv_stride;
        const uint8_t *vr = planes[2] + (size_t)(y >> 1) * uv_stride;
        uint8_t *dst = rgb + (size_t)y * dec->width * 3u;
        for (x = 0; x < width; x++) {
            int c = (int)yr[x] - 16;
            int d = (int)ur[x >> 1] - 128;
            int e = (int)vr[x >> 1] - 128;
            if (c < 0) c = 0;
            dst[x * 3 + 0] =
                (uint8_t)clip8((298 * c + 409 * e + 128) >> 8);
            dst[x * 3 + 1] =
                (uint8_t)clip8((298 * c - 100 * d - 208 * e + 128) >> 8);
            dst[x * 3 + 2] =
                (uint8_t)clip8((298 * c + 516 * d + 128) >> 8);
        }
    }

    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = dec->height;
    return MR_OK;
}

static mr_status pump(mr_decoder *dec, uint8_t *data, uint32_t len)
{
    mpeg2_state *s = (mpeg2_state *)dec->priv;
    mr_status result = MR_EAGAIN;
    unsigned outputs = 0;
    unsigned guard = 0;

    if (s->queued)
        return MR_EFORMAT;
    mpeg2_buffer(s->decoder, data, data + len);
    while (guard++ < 100000u) {
        mpeg2_state_t state = mpeg2_parse(s->decoder);
        if (state == STATE_BUFFER)
            break;
        if (state == STATE_INVALID)
            return MR_EFORMAT;
        /* INVALID_END still carries libmpeg2's final display picture when a
         * transport stream ends without an explicit sequence-end code. */
        if ((state == STATE_SLICE || state == STATE_END ||
            state == STATE_INVALID_END) &&
            s->info->display_fbuf) {
            uint8_t *dst;
            if (outputs == 0)
                dst = s->rgb;
            else if (outputs == 1)
                dst = s->queued_rgb;
            else
                return MR_EFORMAT;
            result = emit_rgb(dec, dst);
            if (result != MR_OK) return result;
            outputs++;
        }
    }
    if (outputs > 1)
        s->queued = 1;
    return guard >= 100000u ? MR_EFORMAT : result;
}

static void mpeg2_close_decoder(mr_decoder *dec)
{
    mpeg2_state *s = dec ? (mpeg2_state *)dec->priv : NULL;
    if (!s) return;
    if (s->decoder) mpeg2_close(s->decoder);
    free(s->rgb);
    free(s->queued_rgb);
    free(s);
    dec->priv = NULL;
    dec->frame.data = NULL;
}

static mr_status mpeg2_open_decoder(mr_decoder *dec)
{
    mpeg2_state *s;
    size_t pixels;
    if ((size_t)dec->width > SIZE_MAX / (size_t)dec->height)
        return MR_ENOMEM;
    pixels = (size_t)dec->width * (size_t)dec->height;
    if (pixels > SIZE_MAX / 3u) return MR_ENOMEM;

    s = (mpeg2_state *)calloc(1, sizeof *s);
    if (!s) return MR_ENOMEM;
    dec->priv = s;
    s->decoder = mpeg2_init();
    if (!s->decoder) {
        mpeg2_close_decoder(dec);
        return MR_ENOMEM;
    }
    s->info = mpeg2_info(s->decoder);
    s->rgb = (uint8_t *)malloc(pixels * 3u);
    s->queued_rgb = (uint8_t *)malloc(pixels * 3u);
    if (!s->rgb || !s->queued_rgb) {
        mpeg2_close_decoder(dec);
        return MR_ENOMEM;
    }

    dec->frame.width = dec->width;
    dec->frame.height = dec->height;
    dec->frame.fmt = MR_PIX_RGB24;
    dec->frame.stride = dec->width * 3;
    dec->frame.data = s->rgb;
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = 0;
    return MR_OK;
}

static mr_status mpeg2_decode_packet(mr_decoder *dec,
                                     const uint8_t *data, uint32_t len)
{
    mpeg2_state *s = (mpeg2_state *)dec->priv;
    if (!s || !data || !len) return MR_EFORMAT;
    s->flushing = 0;
    s->flush_done = 0;
    return pump(dec, (uint8_t *)data, len);
}

static mr_status mpeg2_flush_decoder(mr_decoder *dec)
{
    static uint8_t sequence_end[4] = { 0x00, 0x00, 0x01, 0xb7 };
    mpeg2_state *s = (mpeg2_state *)dec->priv;
    mr_status st;
    if (!s || s->flush_done) return MR_EAGAIN;
    if (s->queued) {
        uint8_t *tmp = s->rgb;
        s->rgb = s->queued_rgb;
        s->queued_rgb = tmp;
        s->queued = 0;
        dec->frame.data = s->rgb;
        dec->frame.dirty_y0 = 0;
        dec->frame.dirty_y1 = dec->height;
        return MR_OK;
    }
    if (!s->flushing) {
        s->flushing = 1;
        st = pump(dec, sequence_end, sizeof sequence_end);
        if (st == MR_OK) return st;
    }
    s->flush_done = 1;
    return MR_EAGAIN;
}

const mr_codec mr_codec_mpeg2 = {
    "MPEG-1/2 Video (libmpeg2)",
    {
        MR_FOURCC('m','p','g','2'),
        MR_FOURCC('M','P','G','2'),
        MR_FOURCC('m','p','g','1'),
        0, 0, 0, 0, 0
    },
    mpeg2_open_decoder,
    mpeg2_decode_packet,
    mpeg2_close_decoder,
    mpeg2_flush_decoder
};
