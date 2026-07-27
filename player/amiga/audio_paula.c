/*
 * MintRIVA - Paula (audio.device) PCM output backend.
 *
 * One allocated Paula channel, mono, signed 8-bit, double-buffered CMD_WRITE.
 * Source PCM (8/16-bit, mono/stereo) is downmixed/converted to signed 8-bit
 * mono and pushed into a ring FIFO; audio_service() reaps completed writes and
 * resubmits from the FIFO, so the player just has to call it often.
 *
 * Teardown follows MintAMP's hard-won rule: an in-flight CMD_WRITE must be
 * AbortIO'd and WaitIO'd (reaped) before the chip buffer it points at is freed
 * or the device is closed, so Paula never DMAs freed memory.
 */
#include "mr_audio.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>   /* BeginIO() prototype (amiga.lib helper) */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define PAL_CLOCK  3546895UL   /* Paula colour clock (PAL)                  */
#define MIN_PERIOD 124         /* Paula hardware minimum period             */
#define NBUF       2           /* double buffer                            */

struct mr_audio {
    struct MsgPort *port;
    struct IOAudio *io[NBUF];
    signed char    *chip[NBUF];
    int             bufsz;         /* samples per chip buffer               */
    int             busy[NBUF];
    int             nsub[NBUF];    /* samples submitted in that buffer      */
    int             opened;        /* audio.device open (io[0])             */

    unsigned        period;
    unsigned        rate;
    int             src_channels;
    int             src_bits;

    signed char    *fifo;          /* ring of converted 8-bit mono samples  */
    unsigned        fifo_size;
    unsigned        head, tail, count;

    unsigned long   played;        /* sample-frames actually played         */
    clock_t         last_service;
    clock_t         longest_service_gap;
    unsigned long   underruns;
    unsigned long   minimum_buffered_ms;
    int             had_buffered_audio;
};

/* ---- ring FIFO ---------------------------------------------------------- */

static void fifo_push(mr_audio *a, signed char s)
{
    if (a->count >= a->fifo_size) return;      /* full: drop (shouldn't hit) */
    a->fifo[a->head] = s;
    a->head = (a->head + 1) % a->fifo_size;
    a->count++;
}

static int fifo_pop_into(mr_audio *a, signed char *dst, int n)
{
    int i = 0;
    while (i < n && a->count) {
        dst[i++] = a->fifo[a->tail];
        a->tail = (a->tail + 1) % a->fifo_size;
        a->count--;
    }
    return i;
}

/* ---- lifecycle ---------------------------------------------------------- */

mr_audio *audio_open(unsigned rate, int channels, int bits)
{
    static UBYTE anychan[4] = { 1, 2, 4, 8 };  /* let the device pick one   */
    mr_audio *a;
    int i;

    if (rate == 0 || (bits != 8 && bits != 16) || channels < 1)
        return NULL;

    a = (mr_audio *)calloc(1, sizeof *a);
    if (!a) return NULL;
    a->rate = rate;
    a->src_channels = channels;
    a->src_bits = bits;
    a->period = (unsigned)(PAL_CLOCK / rate);
    a->minimum_buffered_ms = ULONG_MAX;
    if (a->period < MIN_PERIOD) a->period = MIN_PERIOD;

    a->port = CreateMsgPort();
    if (!a->port) { audio_close(a); return NULL; }

    a->io[0] = (struct IOAudio *)CreateIORequest(a->port, sizeof(struct IOAudio));
    if (!a->io[0]) { audio_close(a); return NULL; }
    a->io[0]->ioa_Request.io_Message.mn_Node.ln_Pri = ADALLOC_MAXPREC;
    a->io[0]->ioa_Data   = anychan;
    a->io[0]->ioa_Length = sizeof anychan;
    if (OpenDevice((CONST_STRPTR)"audio.device", 0,
                   (struct IORequest *)a->io[0], 0) != 0) {
        audio_close(a); return NULL;
    }
    a->opened = 1;

    /* Second request shares the opened unit/channel; copy but preserve its own
     * message node so Exec's port lists stay intact. */
    a->io[1] = (struct IOAudio *)CreateIORequest(a->port, sizeof(struct IOAudio));
    if (!a->io[1]) { audio_close(a); return NULL; }
    {
        struct Message keep = a->io[1]->ioa_Request.io_Message;
        memcpy(a->io[1], a->io[0], sizeof(struct IOAudio));
        a->io[1]->ioa_Request.io_Message = keep;
        a->io[1]->ioa_Request.io_Message.mn_ReplyPort = a->port;
    }

    a->bufsz = (int)(rate / 20);               /* ~50 ms per buffer         */
    if (a->bufsz < 256) a->bufsz = 256;
    for (i = 0; i < NBUF; i++) {
        a->chip[i] = (signed char *)AllocMem((ULONG)a->bufsz,
                                             MEMF_CHIP | MEMF_CLEAR);
        if (!a->chip[i]) { audio_close(a); return NULL; }
    }

    /* Several seconds of cushion: coarsely-interleaved files deliver audio in
     * multi-second chunks (e.g. CDR-Dinner's ~2.3 s), with video-only runs
     * between them. A one-second FIFO dropped the tail of each burst and then
     * starved during the video run - audible as chunky audio and stuttery
     * pacing. Buffer ~4 s so a burst fits and the video run does not starve.
     * Sync is unaffected: pacing follows samples actually played, not queued. */
    a->fifo_size = rate * 4;                    /* ~4 s cushion              */
    if (a->fifo_size < 8192) a->fifo_size = 8192;
    a->fifo = (signed char *)malloc(a->fifo_size);
    if (!a->fifo) { audio_close(a); return NULL; }

    return a;
}

void audio_close(mr_audio *a)
{
    int i;
    if (!a) return;

    /* Reap any in-flight write before touching its chip buffer / the device. */
    for (i = 0; i < NBUF; i++) {
        if (a->busy[i] && a->io[i]) {
            AbortIO((struct IORequest *)a->io[i]);
            WaitIO((struct IORequest *)a->io[i]);
            a->busy[i] = 0;
        }
    }
    if (a->opened && a->io[0])
        CloseDevice((struct IORequest *)a->io[0]);
    for (i = 0; i < NBUF; i++)
        if (a->io[i]) DeleteIORequest((struct IORequest *)a->io[i]);
    if (a->port) DeleteMsgPort(a->port);
    for (i = 0; i < NBUF; i++)
        if (a->chip[i]) FreeMem(a->chip[i], (ULONG)a->bufsz);
    free(a->fifo);
    free(a);
}

/* ---- streaming ---------------------------------------------------------- */

void audio_write(mr_audio *a, const unsigned char *pcm, unsigned bytes)
{
    int bps, ch, framebytes;
    unsigned n, k;

    if (!a || !pcm) return;
    bps = a->src_bits / 8;
    ch  = a->src_channels;
    framebytes = bps * ch;
    if (framebytes <= 0) return;
    n = bytes / (unsigned)framebytes;

    for (k = 0; k < n; k++) {
        const unsigned char *p = pcm + (size_t)k * framebytes;
        int s;
        if (a->src_bits == 16) {
            int l = (int)(short)(p[0] | (p[1] << 8));      /* LE, signed     */
            if (ch >= 2) {
                int r = (int)(short)(p[2] | (p[3] << 8));
                s = (l + r) / 2;
            } else s = l;
            s >>= 8;                                        /* 16 -> 8 bit    */
        } else {
            int l = (int)audio_pcm_u8_to_s16(p[0]);
            if (ch >= 2) {
                int r = (int)audio_pcm_u8_to_s16(p[1]);
                s = (l + r) / 2;
            } else s = l;
            s >>= 8;
        }
        fifo_push(a, (signed char)s);
    }
}

void audio_write_s16(mr_audio *a, const short *pcm,
                     unsigned frames, int channels)
{
    unsigned k;
    if (!a || !pcm || channels < 1) return;
    for (k = 0; k < frames; k++) {
        int s = pcm[(size_t)k * channels];
        if (channels >= 2)
            s = (s + pcm[(size_t)k * channels + 1]) / 2;
        fifo_push(a, (signed char)(s >> 8));
    }
}

void audio_service(mr_audio *a)
{
    int i;
    clock_t service_now;
    unsigned long buffered;
    if (!a) return;
    service_now = clock();
    if (a->last_service && service_now - a->last_service > a->longest_service_gap)
        a->longest_service_gap = service_now - a->last_service;
    a->last_service = service_now;

    /* Reap finished writes. */
    for (i = 0; i < NBUF; i++) {
        if (a->busy[i] && CheckIO((struct IORequest *)a->io[i])) {
            WaitIO((struct IORequest *)a->io[i]);
            a->busy[i] = 0;
            a->played += (unsigned long)a->nsub[i];
        }
    }
    /* Submit idle buffers from the FIFO. */
    for (i = 0; i < NBUF; i++) {
        if (!a->busy[i] && a->count > 0) {
            int n = fifo_pop_into(a, a->chip[i], a->bufsz);
            if (n <= 0) continue;
            a->io[i]->ioa_Request.io_Command = CMD_WRITE;
            a->io[i]->ioa_Request.io_Flags   = ADIOF_PERVOL;
            a->io[i]->ioa_Data   = (UBYTE *)a->chip[i];
            a->io[i]->ioa_Length = (ULONG)n;
            a->io[i]->ioa_Period = a->period;
            a->io[i]->ioa_Volume = 64;
            a->io[i]->ioa_Cycles = 1;
            BeginIO((struct IORequest *)a->io[i]);
            a->busy[i] = 1;
            a->nsub[i] = n;
        }
    }
    buffered = audio_buffered_ms(a);
    if ((buffered || a->had_buffered_audio) &&
        buffered < a->minimum_buffered_ms)
        a->minimum_buffered_ms = buffered;
    if (buffered) a->had_buffered_audio = 1;
    else if (a->had_buffered_audio) {
        a->underruns++;
        a->had_buffered_audio = 0;
    }
}

unsigned long audio_elapsed_ms(mr_audio *a)
{
    if (!a || !a->rate) return 0;
    return (a->played * 1000UL) / a->rate;
}

unsigned long audio_buffered_ms(mr_audio *a)
{
    unsigned long samples;
    int i;
    if (!a || !a->rate) return 0;
    samples = a->count;
    for (i = 0; i < NBUF; i++)
        if (a->busy[i]) samples += (unsigned long)a->nsub[i];
    return samples * 1000UL / a->rate;
}

void audio_diagnostics(mr_audio *a, mr_audio_diagnostics *diag)
{
    if (!diag) return;
    memset(diag, 0, sizeof *diag);
    if (!a) return;
    diag->underruns = a->underruns;
    diag->minimum_buffered_ms = a->minimum_buffered_ms == ULONG_MAX
                              ? 0 : a->minimum_buffered_ms;
    diag->longest_service_gap_ms =
        (unsigned long)(a->longest_service_gap * 1000 / CLOCKS_PER_SEC);
}

int audio_starved(mr_audio *a)
{
    if (!a) return 1;
    return (!a->busy[0] && !a->busy[1] && a->count == 0);
}
