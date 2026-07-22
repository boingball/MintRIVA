/*
 * MintRIVA - MPEG-1 source, wrapping pl_mpeg.
 *
 * This translation unit carries pl_mpeg's implementation (PL_MPEG_IMPLEMENTATION),
 * so it is the one place its (float-using MP2) code is compiled.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

#include "mr_mpeg1.h"

struct mr_mpeg1 {
    plm_t   *plm;
    uint8_t *fb;                 /* persistent RGB24 output                 */
    int      w, h;
    int      decim;              /* audio decimation (1 or 2) for Paula     */
    unsigned rate_eff;           /* effective audio rate after decimation   */
};

int mr_mpeg1_probe(const uint8_t *buf, size_t len)
{
    /* MPEG program stream pack header: 00 00 01 BA */
    return len >= 4 && buf[0] == 0x00 && buf[1] == 0x00 &&
           buf[2] == 0x01 && buf[3] == 0xBA;
}

mr_mpeg1 *mr_mpeg1_open(const uint8_t *buf, size_t len)
{
    mr_mpeg1 *m = (mr_mpeg1 *)calloc(1, sizeof *m);
    if (!m) return NULL;
    /* free_when_done = 0: the caller keeps ownership of buf. */
    m->plm = plm_create_with_memory((uint8_t *)buf, len, 0);
    if (!m->plm) { free(m); return NULL; }
    plm_set_loop(m->plm, 0);
    plm_set_audio_enabled(m->plm, plm_get_num_audio_streams(m->plm) > 0);
    m->w = plm_get_width(m->plm);
    m->h = plm_get_height(m->plm);
    if (m->w <= 0 || m->h <= 0) { plm_destroy(m->plm); free(m); return NULL; }
    m->fb = (uint8_t *)calloc((size_t)m->w * m->h * 3, 1);
    if (!m->fb) { plm_destroy(m->plm); free(m); return NULL; }
    { /* Paula tops out near ~28 kHz; halve higher rates (44.1/48) to fit. */
        int raw = (plm_get_num_audio_streams(m->plm) > 0)
                ? plm_get_samplerate(m->plm) : 0;
        m->decim = (raw > 28000) ? 2 : 1;
        m->rate_eff = (unsigned)(raw / m->decim);
    }
    return m;
}

int mr_mpeg1_width(mr_mpeg1 *m)  { return m ? m->w : 0; }
int mr_mpeg1_height(mr_mpeg1 *m) { return m ? m->h : 0; }
double mr_mpeg1_framerate(mr_mpeg1 *m) { return m ? plm_get_framerate(m->plm) : 0; }

unsigned mr_mpeg1_samplerate(mr_mpeg1 *m)
{
    return m ? m->rate_eff : 0;
}

int mr_mpeg1_next(mr_mpeg1 *m, mr_frame *out, double *pts)
{
    plm_frame_t *fr;
    if (!m) return 0;
    fr = plm_decode_video(m->plm);
    if (!fr) return 0;
    plm_frame_to_rgb(fr, m->fb, m->w * 3);
    out->width  = m->w;
    out->height = m->h;
    out->fmt    = MR_PIX_RGB24;
    out->stride = m->w * 3;
    out->data   = m->fb;
    out->dirty_y0 = 0;                          /* inter frames, but simplest */
    out->dirty_y1 = m->h;                       /* is a full repaint          */
    if (pts) *pts = fr->time;
    return 1;
}

int mr_mpeg1_audio(mr_mpeg1 *m, unsigned char *dst)
{
    plm_samples_t *s;
    unsigned j, out = 0;
    int decim;
    if (!m) return 0;
    s = plm_decode_audio(m->plm);
    if (!s) return 0;
    decim = m->decim;
    /* Take every `decim`-th stereo sample, emit little-endian signed-16 so it
     * is correct on the big-endian 68k regardless of host byte order. */
    for (j = 0; j < s->count; j += decim) {
        int ch;
        for (ch = 0; ch < 2; ch++) {
            float f = s->interleaved[j * 2 + ch];
            int v = (int)(f * 32767.0f);
            if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
            *dst++ = (unsigned char)(v & 0xff);
            *dst++ = (unsigned char)((v >> 8) & 0xff);
        }
        out++;
    }
    return (int)out;
}

void mr_mpeg1_rewind(mr_mpeg1 *m) { if (m) plm_rewind(m->plm); }

void mr_mpeg1_close(mr_mpeg1 *m)
{
    if (!m) return;
    if (m->plm) plm_destroy(m->plm);
    free(m->fb);
    free(m);
}
