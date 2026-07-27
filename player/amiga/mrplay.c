/*
 * MintRIVA - Amiga player.
 *
 * Ties the proven portable core (demux + decoder) to the Amiga display + audio
 * backends: load file -> auto-detect container -> decode frames / enqueue audio
 * -> blit, with audio as the A/V master clock (video frames are held until
 * Paula playback reaches their timestamp). Falls back to frame-rate pacing when
 * there is no audio. ESC or the close gadget quits.
 *
 * AVI, MOV/MP4 and MPEG-TS containers are file-backed: only metadata and the
 * current compressed packet live in RAM. Raw elementary streams and MPEG-1
 * retain the original whole-file fallback.
 *
 *   mrplay <file.avi|file.mov>
 */
#include "../core/mr_demux.h"
#include "../core/mr_http.h"
#include "../core/mr_codec.h"
#include "../core/mr_rawvideo.h"
#include "../core/mr_mpeg1.h"
#include "../audio/mr_audio_decode.h"
#include "amiga_display.h"
#include "mr_audio.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <devices/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * libavc's P/B-slice reference-list setup has stack frames above 20 KiB
 * before its callers and the AmigaOS libraries are accounted for.  Classic
 * Shells commonly provide only 4 KiB, which corrupts memory during H.264
 * playback and makes the eventual EOF/ESC teardown appear to crash.  AmigaOS
 * versions with stack-cookie support raise the process stack to this minimum;
 * older systems can use "Stack 320000" before launching mrplay.
 */
static const char mr_min_stack[] __attribute__((used)) = "$STACK:320000";

#define VIDEO_QUEUE_CAP 4
#define STATS_INTERVAL_US 3000000ULL

static struct MsgPort *timer_port;
static struct timerequest *timer_request;
struct Device *TimerBase;

static int playback_timer_open(void)
{
    timer_port = CreateMsgPort();
    if (!timer_port) return 0;
    timer_request = (struct timerequest *)CreateIORequest(timer_port,
                                                          sizeof *timer_request);
    if (!timer_request) return 0;
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)timer_request, 0) != 0) return 0;
    TimerBase = timer_request->tr_node.io_Device;
    return 1;
}

static void playback_timer_close(void)
{
    if (TimerBase && timer_request)
        CloseDevice((struct IORequest *)timer_request);
    TimerBase = NULL;
    if (timer_request) DeleteIORequest((struct IORequest *)timer_request);
    if (timer_port) DeleteMsgPort(timer_port);
    timer_request = NULL; timer_port = NULL;
}

typedef struct queued_video {
    unsigned char *rgb;
    size_t capacity;
    int width, height, stride, dirty_y0, dirty_y1;
    uint64_t pts_us;
    uint64_t decoded_at_us;
    unsigned long decode_us;
} queued_video;

typedef struct playback_stats {
    uint64_t since_us, network_us, demux_us, audio_decode_us, video_decode_us;
    uint64_t convert_us, scale_us, display_us, sleep_requested_us, sleep_actual_us;
    uint64_t latency_us;
    unsigned long video_decode_max_us, display_max_us, sleep_max_error_us;
    unsigned decoded, presented, late, dropped, samples;
} playback_stats;

/* EClock is per-machine monotonic and normally much finer than the 20 ms DOS
 * tick. Keeping all scheduling in integer microseconds avoids truncating a
 * 25 fps period into alternating/coarse Delay() ticks. */
static uint64_t monotonic_us(void)
{
    struct EClockVal value;
    ULONG frequency = TimerBase ? ReadEClock(&value) : 0;
    uint64_t ticks = ((uint64_t)value.ev_hi << 32) | value.ev_lo;
    return frequency ? ticks * 1000000ULL / frequency :
           (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC;
}

static void paced_sleep(uint64_t usec, mr_audio *audio, playback_stats *st)
{
    uint64_t begin, end;
    if (!usec) return;
    begin = monotonic_us();
    st->sleep_requested_us += usec;
    /* Delay is only the coarse backoff. Recheck the absolute deadline so an
     * oversleep is measured rather than carried into the next frame. */
    while ((end = monotonic_us()) - begin < usec) {
        uint64_t left = usec - (end - begin);
        if (audio) audio_service(audio);
        if (left > 22000) Delay((LONG)((left - 10000) / 20000));
        else Delay(1);
    }
    end = monotonic_us();
    st->sleep_actual_us += end - begin;
    if (end - begin > usec && end - begin - usec > st->sleep_max_error_us)
        st->sleep_max_error_us = (unsigned long)(end - begin - usec);
}

static int queue_copy(queued_video *q, const mr_frame *fr, uint64_t pts,
                      uint64_t decoded_at, unsigned long decode_us)
{
    size_t bytes = (size_t)fr->stride * fr->height;
    if (q->capacity < bytes) {
        unsigned char *p = (unsigned char *)realloc(q->rgb, bytes);
        if (!p) return 0;
        q->rgb = p; q->capacity = bytes;
    }
    memcpy(q->rgb, fr->data, bytes);
    q->width = fr->width; q->height = fr->height; q->stride = fr->stride;
    q->dirty_y0 = fr->dirty_y0; q->dirty_y1 = fr->dirty_y1;
    q->pts_us = pts; q->decoded_at_us = decoded_at; q->decode_us = decode_us;
    return 1;
}

static void report_stats(playback_stats *st, mr_audio *audio, int depth,
                         uint64_t now)
{
    double seconds = (double)(now - st->since_us) / 1000000.0;
    mr_source_timing io;
    if (seconds < 0.001) seconds = 0.001;
    mr_source_timing_get(&io);
    printf("rtg timing: vdecode=%.2f/%lu ms network-blocked=%lu ms "
           "hls-segment=%lu ms demux=%.2f ms adecode=%.2f ms "
           "convert=%.2f ms scale=%.2f ms display=%.2f/%lu ms "
           "audio-buffered=%lu ms vqueue=%d late=%u dropped=%u "
           "presented=%.2f fps decoded=%.2f fps sleep=%lu/%lu ms "
           "sleep-max-error=%lu us latency=%.2f ms\n",
           st->decoded ? (double)st->video_decode_us / st->decoded / 1000.0 : 0.0,
           st->video_decode_max_us / 1000, (unsigned long)(st->network_us / 1000) + io.network_ms,
           io.hls_segment_ms,
           st->samples ? (double)st->demux_us / st->samples / 1000.0 : 0.0,
           st->samples ? (double)st->audio_decode_us / st->samples / 1000.0 : 0.0,
           st->presented ? (double)st->convert_us / st->presented / 1000.0 : 0.0,
           st->presented ? (double)st->scale_us / st->presented / 1000.0 : 0.0,
           st->presented ? (double)st->display_us / st->presented / 1000.0 : 0.0,
           st->display_max_us / 1000, audio ? audio_buffered_ms(audio) : 0,
           depth, st->late, st->dropped, st->presented / seconds,
           st->decoded / seconds, (unsigned long)(st->sleep_requested_us / 1000),
           (unsigned long)(st->sleep_actual_us / 1000), st->sleep_max_error_us,
           st->presented ? (double)st->latency_us / st->presented / 1000.0 : 0.0);
    memset(st, 0, sizeof *st); st->since_us = now;
    mr_source_timing_reset();
}

static unsigned char *slurp(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (out_len) *out_len = n;
    return b;
}

/* Ticks are 1/50 s (dos Delay). frame period = 50*scale/rate, min 1. */
static long frame_ticks(unsigned long rate, unsigned long scale)
{
    long t;
    if (!rate) return 4;
    t = (long)((50UL * scale + rate / 2) / rate);
    return t < 1 ? 1 : t;
}

static void decoded_audio_sink(void *user, const int16_t *pcm,
                               unsigned frames, unsigned channels)
{
    audio_write_s16((mr_audio *)user, (const short *)pcm, frames,
                    (int)channels);
}

/* The ReAction controller uses Ctrl-F for a normal stop so AmigaDOS does not
 * abort the CLI process before display/audio cleanup runs.  Shell Ctrl-C is
 * still accepted when mrplay sees it itself; Ctrl-D toggles pause and Ctrl-E
 * toggles fast-forward. */
static int control_signal_event(void)
{
    ULONG sig = SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D |
                             SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
    if (sig & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F)) return MR_EV_QUIT;
    if (sig & SIGBREAKF_CTRL_D) return MR_EV_PAUSE;
    if (sig & SIGBREAKF_CTRL_E) return MR_EV_SEEK_FWD;
    return MR_EV_NONE;
}

static int player_event(amiga_display *disp)
{
    int ev = control_signal_event();
    return ev != MR_EV_NONE ? ev : display_poll_event(disp);
}

/* MPEG-1 program streams (.mpg/.mpeg) play through pl_mpeg (video + MP2 audio),
 * reusing the display and Paula audio backends. Separate from the AVI/MOV +
 * codec path because .mpg is a self-contained stream. */
static int play_mpeg1(const unsigned char *buf, long len, int loop, int want_time)
{
    mr_mpeg1      *mp;
    amiga_display *disp;
    mr_audio      *audio = NULL;
    unsigned       sr;
    int            w, h, frames = 0, paused = 0, quit = 0, fast_forward = 0;
    unsigned long  period, clock_base = 0;
    long           ntick;
    unsigned char *abuf;                         /* heap, not stack (4.6 KB)  */
    clock_t        t_dec = 0, t_show = 0;
    mr_frame       fr;
    int64_t        pts_us;
    unsigned       fps_millihz;

    mp = mr_mpeg1_open((const uint8_t *)buf, (size_t)len);
    if (!mp) { printf("cannot open MPEG-1 stream\n"); return 10; }
    abuf = (unsigned char *)malloc(1152 * 4);    /* max: 1152 frames stereo16 */
    if (!abuf) { mr_mpeg1_close(mp); return 10; }
    w = mr_mpeg1_width(mp); h = mr_mpeg1_height(mp);
    printf("mpeg1: %dx%d, opening display...\n", w, h);
    disp = display_open(w, h, "MintRIVA");
    if (!disp) { printf("cannot open a display\n"); mr_mpeg1_close(mp); return 10; }
    printf("display backend: %s\n", display_backend_name(disp));

    sr = mr_mpeg1_samplerate(mp);
    if (sr) {
        audio = audio_open(sr, 2, 16);
        printf(audio ? "audio: Paula out, %u Hz (MP2 stereo)\n"
                     : "audio: Paula open failed, silent\n", sr);
    }
    fps_millihz = mr_mpeg1_framerate_millihz(mp);
    period = fps_millihz ? (1000000UL + fps_millihz / 2) / fps_millihz : 40;
    if (period < 1) period = 1;
    ntick = (long)((period + 19) / 20);
    if (ntick < 1) ntick = 1;

    printf("playing: space=pause, ESC=quit%s...\n", loop ? ", loop on" : "");

    while (!quit) {
        int got;
        while (paused && !quit) {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1; else if (ev == MR_EV_PAUSE) paused = 0;
            Delay(2);
        }
        if (quit) break;

        { clock_t a = clock(); got = mr_mpeg1_next(mp, &fr, &pts_us); t_dec += clock() - a; }
        if (!got) {
            if (loop) { mr_mpeg1_rewind(mp); frames = 0;
                        clock_base = audio ? audio_elapsed_ms(audio) : 0; continue; }
            break;
        }
        if (audio) {                             /* top up audio (bounded)    */
            int n, k = 0;
            /* ~2 MP2 frames per video frame keeps Paula just ahead; draining
             * everything here would stall video before the first frame shows. */
            while (k < 2 && (n = mr_mpeg1_audio(mp, abuf)) > 0) {
                audio_write(audio, abuf, (unsigned)(n * 4));
                audio_service(audio);
                k++;
            }
        }

        if (audio) {                             /* pace to the audio clock   */
            unsigned long target = clock_base + (unsigned long)frames * period;
            for (;;) {
                int ev = player_event(disp);
                if (ev == MR_EV_QUIT)  { quit = 1; break; }
                if (ev == MR_EV_PAUSE) { paused = 1; break; }
                if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
                audio_service(audio);
                if (fast_forward) break;
                if (audio_elapsed_ms(audio) >= target) break;
                if (audio_starved(audio)) break;
                Delay(1);
            }
        } else {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            else if (ev == MR_EV_PAUSE) paused = 1;
            else if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
            if (!fast_forward) Delay(ntick);
        }
        if (quit) break;

        { clock_t a = clock();
          display_show_rgb(disp, fr.data, fr.width, fr.height, fr.stride,
                           fr.dirty_y0, fr.dirty_y1);
          t_show += clock() - a; }
        frames++;
        if (audio) audio_service(audio);
    }

    if (want_time && frames > 0) {
        unsigned long e = 0, bl = 0;
        display_aga_timing(&e, &bl);
        printf("timing/%d frames: decode=%lu ms, display=%lu ms (encode=%lu, blit=%lu)\n",
               frames, (unsigned long)(t_dec * 1000 / CLOCKS_PER_SEC),
               (unsigned long)(t_show * 1000 / CLOCKS_PER_SEC), e, bl);
        if (display_aga_kalms_timing(&bl))
            printf("Kalms conversion: %lu ms\n", bl);
    }
    if (audio) {
        int g = 0;
        while (!audio_starved(audio) && g++ < 4000) {
            if (player_event(disp) == MR_EV_QUIT) {
                quit = 1;
                break;
            }
            audio_service(audio);
            Delay(1);
        }
    }
    if (!quit) {
        printf("played %d frames - press ESC or close the window to exit\n",
               frames);
        while (player_event(disp) != MR_EV_QUIT) {
            if (audio) audio_service(audio);
            Delay(2);
        }
    }
    if (audio) audio_close(audio);
    display_close(disp);
    mr_mpeg1_close(mp);
    free(abuf);
    return 0;
}

int main(int argc, char **argv)
{
    long len = 0;
    unsigned char *buf = NULL;
    mr_demux *dx;
    const mr_video_info *vi;
    const mr_codec *codec;
    mr_decoder dec;
    amiga_display *disp;
    mr_audio *audio = NULL;
    mr_audio_decoder *audio_dec = NULL;
    mr_packet pkt;
    long ticks;
    int frames = 0;
    int want_time = 0, loop = 0, paused = 0, quit = 0, fast_forward = 0;
    int raw_diag_printed = 0;
    int hls_low = 0;
    unsigned hls_max_width = 0, hls_max_height = 0, hls_max_fps = 0;
    const char *media_path = NULL;
    const char *user_agent = NULL;
    const char *referer = NULL;
    mr_http_options http_options;
    int have_http_options = 0;
    unsigned long clock_base = 0;
    clock_t t_dec = 0, t_show = 0;
    queued_video vq[VIDEO_QUEUE_CAP];
    int qhead = 0, qcount = 0, input_eof = 0;
    uint64_t decoded_index = 0, mono_base_us = 0;
    playback_stats stats;

    /* Unbuffered so every diagnostic reaches the shell immediately, even if a
     * later step hangs or crashes (libnix stdout can otherwise block-buffer). */
    setvbuf(stdout, NULL, _IONBF, 0);
    if (!playback_timer_open())
        printf("warning: timer.device unavailable; pacing timer is coarse\n");

    if (argc < 2) {
        printf("usage: mrplay [--user-agent <value>] [--referer <value>] "
               "<file.avi|file.mov|file.ts|file.m2ts|"
               "file.mjpeg|file.m4v> "
               "[--aga] [--ham] [--ham6] "
               "[--2x] [--lace] [--loop] [--wpa|--c2p|--riva-c2p|--kalms-c2p] "
               "[--cd32] [--hls-low] [--time]\n");
        return 5;
    }
    {   /* display options anywhere on the command line */
        int i;
        for (i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--user-agent")) {
                if (++i >= argc) {
                    printf("--user-agent requires a value\n");
                    return 5;
                }
                user_agent = argv[i];
            } else if (!strcmp(argv[i], "--referer")) {
                if (++i >= argc) {
                    printf("--referer requires a value\n");
                    return 5;
                }
                referer = argv[i];
            }
            else if (!strcmp(argv[i], "--aga"))  display_set_force_aga(1);
            else if (!strcmp(argv[i], "--ham"))  display_set_ham(8);
            else if (!strcmp(argv[i], "--ham6")) display_set_ham(6);
            else if (!strcmp(argv[i], "--2x"))   display_set_scale(2);
            else if (!strcmp(argv[i], "--wpa"))  display_set_c2p(0);
            else if (!strcmp(argv[i], "--c2p"))  display_set_c2p(1);
            else if (!strcmp(argv[i], "--riva-c2p")) display_set_riva_c2p(1);
            else if (!strcmp(argv[i], "--kalms-c2p")) display_set_kalms_c2p(1);
            else if (!strcmp(argv[i], "--loop")) loop = 1;
            else if (!strcmp(argv[i], "--lace")) display_set_lace(1);
            else if (!strcmp(argv[i], "--cd32")) display_set_akiko(1);
            else if (!strcmp(argv[i], "--time")) want_time = 1;
            else if (!strcmp(argv[i], "--hls-low")) hls_low = 1;
            else if (!strncmp(argv[i], "--hls-max-width=", 16))
                hls_max_width = (unsigned)strtoul(argv[i] + 16, NULL, 10);
            else if (!strncmp(argv[i], "--hls-max-height=", 17))
                hls_max_height = (unsigned)strtoul(argv[i] + 17, NULL, 10);
            else if (!strncmp(argv[i], "--hls-max-fps=", 14))
                hls_max_fps = (unsigned)strtoul(argv[i] + 14, NULL, 10);
            else if (argv[i][0] != '-' && !media_path) media_path = argv[i];
        }
    }
    if (!media_path) {
        printf("no media URL or filename supplied\n");
        return 5;
    }
    if (!mr_http_options_init(&http_options, user_agent, referer)) {
        printf("invalid HTTP options: %s\n", mr_source_last_error());
        return 5;
    }
    http_options.hls_low = hls_low;
    http_options.hls_max_width = hls_max_width;
    http_options.hls_max_height = hls_max_height;
    http_options.hls_max_fps = hls_max_fps;
    have_http_options = user_agent || referer || hls_low || hls_max_width ||
                        hls_max_height || hls_max_fps;
    if (want_time) {
        printf("HTTP User-Agent: %s\n",
               user_agent ? user_agent : "MintRIVA/0.1 AmigaOS");
        if (referer) printf("HTTP Referer: %s\n", referer);
        if (hls_low)
            printf("HLS preference: low bandwidth (max %ux%u @ %u fps)\n",
                   hls_max_width, hls_max_height, hls_max_fps);
    }
    printf("mrplay: opening %s\n", media_path);

    dx = mr_demux_open_file_ex(media_path,
                               have_http_options ? &http_options : NULL);
    if (dx) {
        printf("streaming %s from %s\n", mr_demux_container_name(dx),
               !strncmp(media_path, "http://", 7) ||
               !strncmp(media_path, "https://", 8) ? "network" : "disk");
    } else {
        if (mr_demux_is_file_backed_container(media_path)) {
            printf("cannot open stream: %s\n", mr_demux_last_open_error());
            return 10;
        }
        /* MPEG-1 and raw elementary streams still require a contiguous input
         * buffer because their current decoders parse directly from it. */
        buf = slurp(media_path, &len);
        if (!buf) { printf("cannot read %s\n", media_path); return 10; }
        printf("loaded %ld bytes\n", len);

        if (mr_mpeg1_probe(buf, (size_t)len)) {  /* .mpg via pl_mpeg         */
            int rc = play_mpeg1(buf, len, loop, want_time);
            free(buf);
            return rc;
        }
        dx = mr_demux_open(buf, (size_t)len);
        if (!dx) {
            printf("unsupported container (need AVI, MOV/MP4, MPEG-TS/PS, "
                   "raw MJPEG/M4V or MPEG-1)\n");
            free(buf);
            return 10;
        }
    }

    vi = mr_demux_video(dx);
    codec = mr_codec_find(vi->fourcc);
    if (!codec) { printf("no decoder for this video codec\n");
                  mr_demux_close(dx); free(buf); return 10; }

    if (want_time)
        printf("video fourcc='%c%c%c%c'\n", (int)(vi->fourcc & 255),
               (int)((vi->fourcc >> 8) & 255),
               (int)((vi->fourcc >> 16) & 255),
               (int)((vi->fourcc >> 24) & 255));

    if (mr_decoder_open_config(&dec, codec, vi->width, vi->height,
                               vi->config, vi->config_len) != MR_OK) {
        printf("decoder init failed\n");
        mr_demux_close(dx); free(buf); return 10;
    }

    printf("media: file=%s, container=%s, video=%s (%c%c%c%c), "
           "%dx%d, %lu.%03lu fps\n", media_path, mr_demux_container_name(dx),
           codec->name, (int)(vi->fourcc & 255), (int)((vi->fourcc >> 8) & 255),
           (int)((vi->fourcc >> 16) & 255), (int)((vi->fourcc >> 24) & 255),
           vi->width, vi->height,
           (unsigned long)(vi->rate / (vi->scale ? vi->scale : 1)),
           (unsigned long)(((vi->rate % (vi->scale ? vi->scale : 1)) * 1000) /
                           (vi->scale ? vi->scale : 1)));
    printf("%dx%d, opening display...\n", vi->width, vi->height);
    disp = display_open(vi->width, vi->height, "MintRIVA");
    if (!disp) { printf("cannot open a display (RTG or AGA)\n");
                 mr_decoder_close(&dec); mr_demux_close(dx); free(buf); return 10; }
    printf("display backend: %s\n", display_backend_name(disp));

    /* Every decoder feeds signed S16 to the common Paula sink.  In particular,
     * PCM byte signedness is resolved before downmixing and S16-to-S8 output. */
    {
        const mr_audio_info *ai = mr_demux_audio(dx);
        if (ai->valid && ai->format_tag == MR_AUDIO_FORMAT_PCM) {
            audio_dec = mr_audio_decoder_open(ai);
            if (want_time && audio_dec) {
                if (ai->codec_tag > 0xffff)
                    printf("audio: %s %s %lu Hz (%c%c%c%c)\n",
                           mr_audio_decoder_name(audio_dec),
                           ai->channels == 1 ? "mono" : "stereo",
                           (unsigned long)ai->sample_rate,
                           (int)(ai->codec_tag & 255),
                           (int)((ai->codec_tag >> 8) & 255),
                           (int)((ai->codec_tag >> 16) & 255),
                           (int)((ai->codec_tag >> 24) & 255));
                else
                    printf("audio: %s %s %lu Hz\n",
                           mr_audio_decoder_name(audio_dec),
                           ai->channels == 1 ? "mono" : "stereo",
                           (unsigned long)ai->sample_rate);
            }
            if (audio_dec)
                audio = audio_open(mr_audio_decoder_rate(audio_dec),
                                   (int)mr_audio_decoder_channels(audio_dec), 16);
            if (audio && audio_dec)
                printf("audio: Paula out, %u Hz (%s, %u ch)\n",
                       mr_audio_decoder_rate(audio_dec),
                       mr_audio_decoder_name(audio_dec),
                       mr_audio_decoder_channels(audio_dec));
            else {
                printf("audio: unsupported PCM layout or Paula open failed, "
                       "playing silent\n");
                if (audio_dec) {
                    mr_audio_decoder_close(audio_dec);
                    audio_dec = NULL;
                }
            }
        } else if (ai->valid &&
                   (ai->format_tag == MR_AUDIO_FORMAT_MP2 ||
                    ai->format_tag == MR_AUDIO_FORMAT_MP3 ||
                    ai->format_tag == MR_AUDIO_FORMAT_AAC)) {
            audio_dec = mr_audio_decoder_open(ai);
            if (audio_dec)
                audio = audio_open(mr_audio_decoder_rate(audio_dec),
                                   (int)mr_audio_decoder_channels(audio_dec), 16);
            if (audio && audio_dec)
                printf("audio: Paula out, %u Hz (%s, %u ch)\n",
                       mr_audio_decoder_rate(audio_dec),
                       mr_audio_decoder_name(audio_dec),
                       mr_audio_decoder_channels(audio_dec));
            else {
                printf("audio: unsupported %s setup or Paula open failed, "
                       "playing silent\n",
                       ai->format_tag == MR_AUDIO_FORMAT_MP2 ? "MP2" :
                       ai->format_tag == MR_AUDIO_FORMAT_MP3 ? "MP3" : "AAC");
                if (audio_dec) {
                    mr_audio_decoder_close(audio_dec);
                    audio_dec = NULL;
                }
            }
        }
    }

    ticks = frame_ticks(vi->rate, vi->scale);
    {
        unsigned long period = vi->rate ? (1000UL * (vi->scale ? vi->scale : 1)
                                           / vi->rate) : 83;
        if (period < 1) period = 1;

    printf("playing: space=pause, </>=seek, ESC=quit%s...\n",
           loop ? ", loop on" : "");

    memset(vq, 0, sizeof vq);
    memset(&stats, 0, sizeof stats);
    stats.since_us = monotonic_us();
    mr_source_timing_reset();
    {
        struct EClockVal ev;
        ULONG hz = TimerBase ? ReadEClock(&ev) : 0;
        if (want_time)
            printf("timer: EClock=%lu Hz (%.3f us nominal), DOS tick=20.000 ms\n",
                   (unsigned long)hz, hz ? 1000000.0 / hz : 0.0);
    }

    while (!quit && (!input_eof || qcount || loop)) {
        if (input_eof && !qcount && loop) {
            mr_demux_rewind(dx);
            if (mr_decoder_reset(&dec) != MR_OK) break;
            if (audio_dec) mr_audio_decoder_reset(audio_dec);
            input_eof = 0; decoded_index = 0; mono_base_us = 0;
            clock_base = audio ? audio_elapsed_ms(audio) : 0;
        }
        /* Decode ahead to a bounded four-frame queue. Demux/audio work is done
         * whenever there is space, rather than immediately before a blit. */
        while (!input_eof && qcount < VIDEO_QUEUE_CAP) {
            uint64_t a = monotonic_us();
            mr_status next = mr_demux_next_packet(dx, &pkt);
            uint64_t b = monotonic_us();
            stats.demux_us += b - a; stats.samples++;
            if (mr_source_is_url(media_path)) stats.network_us += b - a;
            if (next != MR_OK) { input_eof = 1; break; }
            if (!pkt.is_video) {
                if (audio && audio_dec) {
                    a = monotonic_us();
                    mr_audio_decoder_feed(audio_dec, pkt.data, pkt.len,
                                          decoded_audio_sink, audio);
                    stats.audio_decode_us += monotonic_us() - a;
                }
                if (audio) audio_service(audio);
                continue;
            }
            if (!pkt.len) continue;
            if (want_time && !raw_diag_printed &&
                mr_rawvideo_is_uyvy422(vi->fourcc)) {
                uint32_t stride = mr_rawvideo_source_stride(&dec);
                printf("raw UYVY422: %dx%d stride=%lu packet=%lu\n",
                       vi->width, vi->height, (unsigned long)stride,
                       (unsigned long)pkt.len);
                raw_diag_printed = 1;
            }
            a = monotonic_us();
            if (mr_decoder_decode(&dec, pkt.data, pkt.len) == MR_OK) {
                unsigned long decode_us = (unsigned long)(monotonic_us() - a);
                uint64_t pts = vi->rate
                    ? decoded_index * (uint64_t)(vi->scale ? vi->scale : 1) *
                      1000000ULL / vi->rate
                    : decoded_index * 83333ULL;
                queued_video *tail = &vq[(qhead + qcount) % VIDEO_QUEUE_CAP];
                stats.video_decode_us += decode_us; stats.decoded++;
                if (decode_us > stats.video_decode_max_us)
                    stats.video_decode_max_us = decode_us;
                if (!queue_copy(tail, &dec.frame, pts, monotonic_us(), decode_us)) {
                    quit = 1; break;
                }
                qcount++; decoded_index++;
            }
            if (audio) audio_service(audio);
        }
        if (quit || !qcount) break;

        while (paused && !quit) {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            else if (ev == MR_EV_PAUSE) { paused = 0; mono_base_us = 0; }
            if (audio) audio_service(audio);
            Delay(1);
        }
        if (quit) break;

        {
            queued_video *front = &vq[qhead];
            uint64_t now = monotonic_us();
            uint64_t period_us = vi->rate
                ? (uint64_t)(vi->scale ? vi->scale : 1) * 1000000ULL / vi->rate
                : 83333ULL;
            int64_t late_us;
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) { quit = 1; break; }
            if (ev == MR_EV_PAUSE) { paused = 1; continue; }
            if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
            if (!mono_base_us) mono_base_us = now - front->pts_us;

            if (audio && !audio_starved(audio))
                late_us = (int64_t)audio_elapsed_ms(audio) * 1000 -
                          (int64_t)(clock_base * 1000UL + front->pts_us);
            else
                late_us = (int64_t)(now - mono_base_us) - (int64_t)front->pts_us;

            if (!fast_forward && late_us < -1000) {
                paced_sleep((uint64_t)(-late_us), audio, &stats);
                now = monotonic_us();
                if (audio && !audio_starved(audio))
                    late_us = (int64_t)audio_elapsed_ms(audio) * 1000 -
                              (int64_t)(clock_base * 1000UL + front->pts_us);
                else
                    late_us = (int64_t)(now - mono_base_us) -
                              (int64_t)front->pts_us;
            }
            if (late_us > 0) stats.late++;

            /* Frames are already decoded, so discarding their display image is
             * reference-safe. Keep one frame queued and never compound delay. */
            if (!fast_forward && late_us > (int64_t)period_us && qcount > 1) {
                stats.dropped++; qhead = (qhead + 1) % VIDEO_QUEUE_CAP; qcount--;
                if (late_us > (int64_t)(period_us * 4) && !audio)
                    mono_base_us = now - vq[qhead].pts_us;
                continue;
            }

            now = monotonic_us();
            display_show_rgb(disp, front->rgb, front->width, front->height,
                             front->stride, front->dirty_y0, front->dirty_y1);
            {
                unsigned long show_us = (unsigned long)(monotonic_us() - now);
                unsigned long enc_ms = 0, blit_ms = 0;
                display_aga_frame_timing(&enc_ms, &blit_ms);
                stats.convert_us += (uint64_t)enc_ms * 1000;
                stats.display_us += show_us;
                if (show_us > stats.display_max_us) stats.display_max_us = show_us;
            }
            stats.latency_us += monotonic_us() - front->decoded_at_us;
            stats.presented++; frames++;
            qhead = (qhead + 1) % VIDEO_QUEUE_CAP; qcount--;
            if (audio) audio_service(audio);
            now = monotonic_us();
            if (want_time && now - stats.since_us >= STATS_INTERVAL_US)
                report_stats(&stats, audio, qcount, now);
        }
    }

    /* MPEG-4 B-frame/display reordering holds the final anchor until EOF.
     * Drain it through the same pacing and display path so the player does not
     * silently finish one frame short (e.g. 129/130 on legacy OpenDivX). */
    while (!quit) {
        clock_t a = clock();
        mr_status ds = mr_decoder_flush(&dec);
        t_dec += clock() - a;
        if (ds != MR_OK) break;

        if (audio) {
            unsigned long target = clock_base + (unsigned long)frames * period;
            while (audio_elapsed_ms(audio) < target &&
                   !audio_starved(audio)) {
                int ev = player_event(disp);
                if (ev == MR_EV_QUIT) { quit = 1; break; }
                audio_service(audio);
                Delay(1);
            }
        } else {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            Delay(ticks);
        }
        if (quit) break;

        a = clock();
        display_show_rgb(disp, dec.frame.data, dec.frame.width,
                         dec.frame.height, dec.frame.stride,
                         dec.frame.dirty_y0, dec.frame.dirty_y1);
        t_show += clock() - a;
        frames++;
    }
    }
    if (want_time && frames > 0) {
        unsigned long enc_ms = 0, blit_ms = 0;
        display_aga_timing(&enc_ms, &blit_ms);
        printf("timing/%d frames: decode=%lu ms, display=%lu ms"
               " (encode=%lu ms, blit=%lu ms)\n", frames,
               (unsigned long)(t_dec  * 1000 / CLOCKS_PER_SEC),
               (unsigned long)(t_show * 1000 / CLOCKS_PER_SEC),
               enc_ms, blit_ms);
        if (display_aga_kalms_timing(&blit_ms))
            printf("Kalms conversion: %lu ms\n", blit_ms);
    }
    /* Let any queued audio drain (bounded, so a wedged clock can't loop). */
    if (audio) {
        int guard = 0;
        while (!audio_starved(audio) && guard++ < 4000) {
            if (player_event(disp) == MR_EV_QUIT) {
                quit = 1;
                break;
            }
            audio_service(audio);
            Delay(1);
        }
    }

    if (!quit) {
        printf("played %d frames - press ESC or close the window to exit\n",
               frames);
        while (player_event(disp) != MR_EV_QUIT) {
            if (audio) audio_service(audio);
            Delay(2);
        }
    }

    { int qi; for (qi = 0; qi < VIDEO_QUEUE_CAP; qi++) free(vq[qi].rgb); }
    if (audio_dec) mr_audio_decoder_close(audio_dec);
    if (audio) audio_close(audio);
    display_close(disp);
    mr_decoder_close(&dec);
    mr_demux_close(dx);
    free(buf);
    playback_timer_close();
    return 0;
}
