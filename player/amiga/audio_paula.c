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
#include <devices/timer.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>   /* BeginIO() prototype (amiga.lib helper) */
#include <proto/timer.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define PAL_CLOCK  3546895UL   /* Paula colour clock (PAL)                  */
#define MIN_PERIOD 124         /* Paula hardware minimum period             */
#define NBUF       2           /* double buffer                            */
#define PAULA_REQUEST_MS 100    /* measured validation value; tune here     */

typedef struct audio_request_timeline {
    uint64_t sequence;
    unsigned submitted_samples;
    uint64_t scheduled_start_us, scheduled_end_us;
    enum mr_audio_request_state state;
} audio_request_timeline;

extern struct Device *TimerBase;

struct mr_audio {
    struct MsgPort *port;
    struct IOAudio *io[NBUF];
    signed char    *chip[NBUF];
    int             bufsz;         /* samples per chip buffer               */
    int             busy[NBUF];
    int             nsub[NBUF];    /* samples submitted in that buffer      */
    audio_request_timeline request[NBUF];
    int             opened;        /* audio.device open (io[0])             */

    unsigned        period;
    unsigned        rate;
    unsigned        output_rate;
    int             src_channels;
    int             src_bits;

    signed char    *fifo;          /* ring of converted 8-bit mono samples  */
    unsigned        fifo_size;
    unsigned        head, tail, count;

    uint64_t        completed_samples;
    uint64_t        next_sequence, timeline_end_us, last_reported_clock_us;
    uint64_t        clock_largest_step_us;
    clock_t         last_service;
    clock_t         longest_service_gap;
    unsigned long   hardware_starvations;
    unsigned long   minimum_buffered_ms;
    unsigned long   minimum_active_ms;
    int             had_buffered_audio;
    int             running;
    int             ever_started;
    int             hardware_starved;
    clock_t         no_active_since;
    clock_t         longest_no_active;
};

static uint64_t audio_now_us(void)
{
    struct EClockVal value;
    ULONG frequency = TimerBase ? ReadEClock(&value) : 0;
    uint64_t ticks = ((uint64_t)value.ev_hi << 32) | value.ev_lo;
    return frequency ? ticks * 1000000ULL / frequency :
           (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC;
}

static int request_active(mr_audio *a, int i)
{
    return a->busy[i] && !CheckIO((struct IORequest *)a->io[i]);
}

static int active_request_count(mr_audio *a)
{
    int i, n = 0;
    for (i = 0; i < NBUF; i++) if (request_active(a, i)) n++;
    return n;
}

static int oldest_request(mr_audio *a)
{
    int i, oldest = -1;
    for (i = 0; i < NBUF; i++) {
        if (a->request[i].state == AUDIO_REQ_IDLE) continue;
        if (oldest < 0 || a->request[i].sequence < a->request[oldest].sequence)
            oldest = i;
    }
    return oldest;
}

static uint64_t request_duration_us(mr_audio *a, unsigned samples)
{
    return (uint64_t)samples * 1000000ULL / a->output_rate;
}

static unsigned request_estimated_played(mr_audio *a, int i, uint64_t now)
{
    audio_request_timeline *r = &a->request[i];
    uint64_t elapsed, duration;
    if (r->state == AUDIO_REQ_COMPLETE || now >= r->scheduled_end_us)
        return r->submitted_samples;
    if (now <= r->scheduled_start_us) return 0;
    elapsed = now - r->scheduled_start_us;
    duration = r->scheduled_end_us - r->scheduled_start_us;
    return duration ? (unsigned)(elapsed * r->submitted_samples / duration) : 0;
}

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
    a->minimum_active_ms = ULONG_MAX;
    if (a->period < MIN_PERIOD) a->period = MIN_PERIOD;
    a->output_rate = (unsigned)(PAL_CLOCK / a->period);

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

    /* libavc is a synchronous API and has no safe slice-yield hook. Keep two
     * genuinely submitted requests, each long enough to span the observed
     * 40-54 ms core decode calls plus scheduler overhead. */
    a->bufsz = (int)(a->output_rate * PAULA_REQUEST_MS / 1000);
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
    uint64_t now;
    unsigned long buffered;
    int active_at_entry;
    if (!a) return;
    now = audio_now_us();
    service_now = clock();
    if (a->last_service && service_now - a->last_service > a->longest_service_gap)
        a->longest_service_gap = service_now - a->last_service;
    a->last_service = service_now;
    active_at_entry = active_request_count(a);
    if (a->running && active_at_entry == 0) {
        if (!a->no_active_since) a->no_active_since = service_now;
    } else a->no_active_since = 0;

    /* Reap finished writes. */
    for (i = 0; i < NBUF; i++) {
        if (a->busy[i] && CheckIO((struct IORequest *)a->io[i])) {
            WaitIO((struct IORequest *)a->io[i]);
            a->busy[i] = 0;
            a->request[i].state = AUDIO_REQ_COMPLETE;
        }
    }
    /* Commit completed requests strictly in submission order. A later array
     * slot completing first must never advance the master clock past an older
     * outstanding request. */
    for (;;) {
        int oldest = oldest_request(a);
        if (oldest < 0 || a->request[oldest].state != AUDIO_REQ_COMPLETE) break;
        a->completed_samples += a->request[oldest].submitted_samples;
        memset(&a->request[oldest], 0, sizeof a->request[oldest]);
    }
    {
        int oldest = oldest_request(a);
        if (oldest >= 0) a->request[oldest].state = AUDIO_REQ_PLAYING;
    }
    /* Submit idle buffers from the FIFO. */
    for (i = 0; i < NBUF; i++) {
        if (!a->busy[i] && a->request[i].state == AUDIO_REQ_IDLE && a->count > 0) {
            int had_outstanding = oldest_request(a) >= 0;
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
            a->request[i].sequence = a->next_sequence++;
            a->request[i].submitted_samples = (unsigned)n;
            a->request[i].scheduled_start_us = now > a->timeline_end_us
                                               ? now : a->timeline_end_us;
            a->request[i].scheduled_end_us =
                a->request[i].scheduled_start_us +
                request_duration_us(a, (unsigned)n);
            a->timeline_end_us = a->request[i].scheduled_end_us;
            a->request[i].state = had_outstanding
                                ? AUDIO_REQ_QUEUED : AUDIO_REQ_PLAYING;
        }
    }
    buffered = audio_buffered_ms(a);
    if ((buffered || a->had_buffered_audio) &&
        buffered < a->minimum_buffered_ms)
        a->minimum_buffered_ms = buffered;
    if (buffered) a->had_buffered_audio = 1;
    else if (a->had_buffered_audio) {
        a->had_buffered_audio = 0;
    }
    {
        int active = active_request_count(a);
        int timeline_active = 0;
        unsigned long active_samples = 0;
        for (i = 0; i < NBUF; i++)
            if (request_active(a, i)) active_samples += (unsigned long)a->nsub[i];
        for (i = 0; i < NBUF; i++)
            if (a->request[i].state == AUDIO_REQ_PLAYING ||
                a->request[i].state == AUDIO_REQ_QUEUED) timeline_active++;
        if (a->running) {
            unsigned long active_ms = active_samples * 1000UL / a->rate;
            if (active_ms < a->minimum_active_ms)
                a->minimum_active_ms = active_ms;
        }
        if (a->running && timeline_active == 0) {
            clock_t gap;
            if (!a->hardware_starved) {
                a->hardware_starvations++;
                a->hardware_starved = 1;
            }
            if (!a->no_active_since) a->no_active_since = service_now;
            gap = service_now - a->no_active_since;
            if (gap > a->longest_no_active) a->longest_no_active = gap;
        } else {
            a->hardware_starved = 0;
            a->no_active_since = 0;
        }
    }
}

uint64_t audio_elapsed_us(mr_audio *a)
{
    uint64_t samples, clock_us, now, step;
    int oldest, used = 0;
    if (!a || !a->output_rate) return 0;
    now = audio_now_us();
    samples = a->completed_samples;
    oldest = oldest_request(a);
    while (oldest >= 0 && used++ < NBUF) {
        unsigned played = request_estimated_played(a, oldest, now);
        samples += played;
        if (played < a->request[oldest].submitted_samples) break;
        {
            uint64_t seq = a->request[oldest].sequence;
            int i, next = -1;
            for (i = 0; i < NBUF; i++)
                if (a->request[i].state != AUDIO_REQ_IDLE &&
                    a->request[i].sequence > seq &&
                    (next < 0 || a->request[i].sequence <
                               a->request[next].sequence)) next = i;
            oldest = next;
        }
    }
    clock_us = samples * 1000000ULL / a->output_rate;
    if (clock_us < a->last_reported_clock_us)
        clock_us = a->last_reported_clock_us;
    step = clock_us - a->last_reported_clock_us;
    if (step > a->clock_largest_step_us) a->clock_largest_step_us = step;
    a->last_reported_clock_us = clock_us;
    return clock_us;
}

unsigned long audio_elapsed_ms(mr_audio *a)
{
    return (unsigned long)(audio_elapsed_us(a) / 1000ULL);
}

unsigned long audio_buffered_ms(mr_audio *a)
{
    unsigned long samples;
    uint64_t now;
    int i;
    if (!a || !a->output_rate) return 0;
    now = audio_now_us();
    samples = a->count;
    for (i = 0; i < NBUF; i++) {
        audio_request_timeline *r = &a->request[i];
        if (r->state == AUDIO_REQ_PLAYING || r->state == AUDIO_REQ_QUEUED) {
            unsigned played = request_estimated_played(a, i, now);
            if (played < r->submitted_samples) samples += r->submitted_samples - played;
        }
    }
    return samples * 1000UL / a->output_rate;
}

void audio_diagnostics(mr_audio *a, mr_audio_diagnostics *diag)
{
    uint64_t now;
    if (!diag) return;
    memset(diag, 0, sizeof *diag);
    if (!a) return;
    now = audio_now_us();
    diag->hardware_starvations = a->hardware_starvations;
    diag->minimum_buffered_ms = a->minimum_buffered_ms == ULONG_MAX
                              ? 0 : a->minimum_buffered_ms;
    diag->minimum_active_ms = a->minimum_active_ms == ULONG_MAX
                            ? 0 : a->minimum_active_ms;
    diag->longest_service_gap_ms =
        (unsigned long)(a->longest_service_gap * 1000 / CLOCKS_PER_SEC);
    diag->longest_no_active_ms =
        (unsigned long)(a->longest_no_active * 1000 / CLOCKS_PER_SEC);
    diag->fifo_samples = a->count;
    diag->audio_clock_us = audio_elapsed_us(a);
    diag->fifo_buffered_ms = a->count * 1000UL / a->output_rate;
    diag->clock_largest_step_us = a->clock_largest_step_us;
    diag->startup_clock_largest_step_us = a->startup_clock_largest_step_us;
    {
        int i;
        for (i = 0; i < NBUF; i++) {
            audio_request_timeline *r = &a->request[i];
            diag->request_samples[i] = a->busy[i] ? (unsigned long)a->nsub[i] : 0;
            diag->request_state[i] = !a->busy[i] ? 0 :
                (CheckIO((struct IORequest *)a->io[i]) ? 2 : 1);
            if (diag->request_state[i] == 1) diag->active_requests++;
            diag->request_timeline_state[i] = (unsigned char)r->state;
            if ((r->state == AUDIO_REQ_PLAYING || r->state == AUDIO_REQ_QUEUED) &&
                now >= r->scheduled_start_us && now < r->scheduled_end_us) {
                diag->request_timeline_state[i] = AUDIO_REQ_PLAYING;
                unsigned played = request_estimated_played(a, i, now);
                unsigned remaining = played < r->submitted_samples
                                   ? r->submitted_samples - played : 0;
                diag->hardware_playing_remaining_ms =
                    remaining * 1000UL / a->output_rate;
                diag->oldest_request_sequence = r->sequence;
            } else if (r->state == AUDIO_REQ_QUEUED) {
                diag->hardware_queued_ms +=
                    r->submitted_samples * 1000UL / a->output_rate;
                if (!diag->oldest_request_sequence ||
                    r->sequence < diag->oldest_request_sequence)
                    diag->oldest_request_sequence = r->sequence;
            }
        }
    }
    diag->total_buffered_ms = diag->fifo_buffered_ms +
        diag->hardware_playing_remaining_ms + diag->hardware_queued_ms;
}

int audio_active_requests(mr_audio *a)
{
    return a ? active_request_count(a) : 0;
}

void audio_set_running(mr_audio *a, int running)
{
    if (!a) return;
    if (running && !a->ever_started) {
        a->startup_clock_largest_step_us = a->clock_largest_step_us;
        a->clock_largest_step_us = 0;
        a->ever_started = 1;
    }
    a->running = running != 0;
    a->hardware_starved = 0;
    a->no_active_since = 0;
}

int audio_starved(mr_audio *a)
{
    if (!a) return 1;
    return oldest_request(a) < 0 && a->count == 0;
}
