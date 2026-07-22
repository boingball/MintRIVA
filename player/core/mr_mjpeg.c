/*
 * MintRIVA - Motion-JPEG decoder (each frame is a baseline JPEG).
 *
 * Thin adapter over picojpeg (public domain, Rich Geldreich; the copy here came
 * from MintAMP). Feeds one frame's JPEG bytes through picojpeg's callback
 * reader, then assembles the MCU blocks into the RGB24 framebuffer the rest of
 * MintRIVA expects. MJPEG is intra-only, so every frame is a full repaint.
 *
 * Second codec behind mr_codec.h - the demux/display/audio layers are unchanged,
 * which is the whole point of the plugin design.
 */
#include "mr_codec.h"
#include "picojpeg.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int            w, h;
    uint8_t       *fb;          /* persistent RGB24 output                  */
    const uint8_t *src;         /* current frame's JPEG bytes               */
    uint32_t       len, pos;
} mjpeg_ctx;

/* picojpeg pulls the source through this callback. */
static unsigned char need_bytes(unsigned char *pBuf, unsigned char buf_size,
                                unsigned char *pRead, void *cb)
{
    mjpeg_ctx *c = (mjpeg_ctx *)cb;
    unsigned n = c->len - c->pos;
    if (n > buf_size) n = buf_size;
    if (n) memcpy(pBuf, c->src + c->pos, n);
    c->pos += n;
    *pRead = (unsigned char)n;
    return 0;
}

static mr_status mjpeg_open(mr_decoder *dec)
{
    mjpeg_ctx *c = (mjpeg_ctx *)calloc(1, sizeof *c);
    if (!c) return MR_ENOMEM;
    c->w = dec->width; c->h = dec->height;
    c->fb = (uint8_t *)calloc((size_t)c->w * c->h * 3, 1);
    if (!c->fb) { free(c); return MR_ENOMEM; }
    dec->priv = c;
    dec->frame.width  = c->w;
    dec->frame.height = c->h;
    dec->frame.fmt    = MR_PIX_RGB24;
    dec->frame.stride = c->w * 3;
    dec->frame.data   = c->fb;
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = c->h;
    return MR_OK;
}

static mr_status mjpeg_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    mjpeg_ctx *c = (mjpeg_ctx *)dec->priv;
    pjpeg_image_info_t info;
    int stride = c->w * 3;
    int mcu_x = 0, mcu_y = 0, gray;

    c->src = data; c->len = len; c->pos = 0;
    if (pjpeg_decode_init(&info, need_bytes, c, 0) != 0) {
        pjpeg_decode_free();
        return MR_EFORMAT;
    }
    gray = (info.m_comps == 1);

    for (;;) {
        unsigned char st = pjpeg_decode_mcu();
        if (st) {
            pjpeg_decode_free();
            return (st == PJPG_NO_MORE_BLOCKS) ? MR_OK : MR_EFORMAT;
        }
        {
            /* Assemble this MCU: it is stored as consecutive 8x8 blocks in
             * raster (row-of-blocks) order within the R/G/B MCU buffers. */
            int px0 = mcu_x * info.m_MCUWidth;
            int py0 = mcu_y * info.m_MCUHeight;
            const uint8_t *sr = info.m_pMCUBufR;
            const uint8_t *sg = info.m_pMCUBufG;
            const uint8_t *sb = info.m_pMCUBufB;
            int by, bx, yy, xx;
            for (by = 0; by < info.m_MCUHeight; by += 8) {
                for (bx = 0; bx < info.m_MCUWidth; bx += 8) {
                    for (yy = 0; yy < 8; yy++) {
                        int oy = py0 + by + yy;
                        for (xx = 0; xx < 8; xx++) {
                            int ox = px0 + bx + xx;
                            uint8_t R = *sr++, G, B;
                            if (gray) { G = B = R; } else { G = *sg++; B = *sb++; }
                            if (ox < c->w && oy < c->h) {
                                uint8_t *d = c->fb + (size_t)oy * stride + ox * 3;
                                d[0] = R; d[1] = G; d[2] = B;
                            }
                        }
                    }
                }
            }
        }
        if (++mcu_x == info.m_MCUSPerRow) { mcu_x = 0; mcu_y++; }
    }
}

static void mjpeg_close(mr_decoder *dec)
{
    mjpeg_ctx *c = (mjpeg_ctx *)dec->priv;
    if (!c) return;
    pjpeg_decode_free();
    free(c->fb);
    free(c);
    dec->priv = NULL;
}

const mr_codec mr_codec_mjpeg = {
    "mjpeg",
    { MR_FOURCC('M','J','P','G'), MR_FOURCC('m','j','p','g'),
      MR_FOURCC('j','p','e','g'), MR_FOURCC('J','P','E','G') },
    mjpeg_open,
    mjpeg_decode,
    mjpeg_close,
};
