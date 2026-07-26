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
#include "../core/mr_codec.h"
#include "../core/mr_rawvideo.h"
#include "../core/mr_mpeg1.h"
#include "../audio/mr_audio_decode.h"
#include "amiga_display.h"
#include "mr_audio.h"

#include <proto/dos.h>
#include <proto/exec.h>
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
    unsigned long clock_base = 0;
    clock_t t_dec = 0, t_show = 0;

    /* Unbuffered so every diagnostic reaches the shell immediately, even if a
     * later step hangs or crashes (libnix stdout can otherwise block-buffer). */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) {
        printf("usage: mrplay <file.avi|file.mov|file.ts|file.m2ts|"
               "file.mjpeg|file.m4v> "
               "[--aga] [--ham] [--ham6] "
               "[--2x] [--lace] [--loop] [--wpa|--c2p|--riva-c2p|--kalms-c2p] "
               "[--cd32] [--time]\n");
        return 5;
    }
    {   /* display options anywhere on the command line */
        int i;
        for (i = 1; i < argc; i++) {
            if      (!strcmp(argv[i], "--aga"))  display_set_force_aga(1);
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
        }
    }
    printf("mrplay: opening %s\n", argv[1]);

    dx = mr_demux_open_file(argv[1]);
    if (dx) {
        printf("streaming %s from %s\n", mr_demux_container_name(dx),
               !strncmp(argv[1], "http://", 7) ||
               !strncmp(argv[1], "https://", 8) ? "network" : "disk");
    } else {
        if (mr_demux_is_file_backed_container(argv[1])) {
            printf("cannot open stream: %s\n", mr_demux_last_open_error());
            return 10;
        }
        /* MPEG-1 and raw elementary streams still require a contiguous input
         * buffer because their current decoders parse directly from it. */
        buf = slurp(argv[1], &len);
        if (!buf) { printf("cannot read %s\n", argv[1]); return 10; }
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
           "%dx%d, %lu.%03lu fps\n", argv[1], mr_demux_container_name(dx),
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

    /* PCM feeds Paula directly. MP3/AAC packets go through MintAMP's fixed-
     * point Helix decoders first; AAC-LC mp4a setup comes from the demuxed ASC. */
    {
        const mr_audio_info *ai = mr_demux_audio(dx);
        if (ai->valid && ai->format_tag == MR_AUDIO_FORMAT_PCM &&
            (ai->bits == 8 || ai->bits == 16)) {
            audio = audio_open(ai->sample_rate, ai->channels, ai->bits);
            if (audio) printf("audio: Paula out, %lu Hz (src %u-bit %u ch)\n",
                              (unsigned long)ai->sample_rate,
                              (unsigned)ai->bits, (unsigned)ai->channels);
            else       printf("audio: Paula open failed, playing silent\n");
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

    while (!quit) {
        /* Frozen while paused: keep taking input, do no work. */
        while (paused && !quit) {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            else if (ev == MR_EV_PAUSE) paused = 0;
            Delay(2);
        }
        if (quit) break;

        if (mr_demux_next_packet(dx, &pkt) != MR_OK) {   /* end of stream    */
            if (loop) {
                mr_demux_rewind(dx);
                if (mr_decoder_reset(&dec) != MR_OK) break;
                if (audio_dec) mr_audio_decoder_reset(audio_dec);
                frames = 0;
                clock_base = audio ? audio_elapsed_ms(audio) : 0;
                continue;
            }
            break;
        }
        if (!pkt.is_video) {
            if (audio) {
                if (audio_dec)
                    mr_audio_decoder_feed(audio_dec, pkt.data, pkt.len,
                                          decoded_audio_sink, audio);
                else
                    audio_write(audio, pkt.data, pkt.len);
                audio_service(audio);
            }
            continue;
        }
        if (pkt.len == 0) continue;
        if (want_time && !raw_diag_printed &&
            mr_rawvideo_is_uyvy422(vi->fourcc)) {
            uint32_t stride = vi->config_len >= 4 ? mr_rl32(vi->config) : 0;
            uint32_t rows = (vi->height > 0 &&
                             stride <= pkt.len / (uint32_t)vi->height)
                          ? stride * (uint32_t)vi->height : pkt.len;
            printf("raw UYVY422: %dx%d, packet=%lu, stride=%lu, trailing=%lu\n",
                   vi->width, vi->height, (unsigned long)pkt.len,
                   (unsigned long)stride,
                   (unsigned long)(rows <= pkt.len ? pkt.len - rows : 0));
            raw_diag_printed = 1;
        }
        {
            clock_t a = clock();
            mr_status ds = mr_decoder_decode(&dec, pkt.data, pkt.len);
            t_dec += clock() - a;
            /* A bad frame skips, it does not stop playback (some clips have
             * the odd frame a decoder can't handle). */
            if (ds != MR_OK) { if (audio) audio_service(audio); continue; }
        }

        /* Pace this frame (audio-master, or frame-rate when silent), handling
         * input while we wait. */
        if (audio) {
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
            if (ev == MR_EV_QUIT)  quit = 1;
            else if (ev == MR_EV_PAUSE) paused = 1;
            else if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
            if (!fast_forward) Delay(ticks);
        }
        if (quit) break;

        {
            clock_t a = clock();
            display_show_rgb(disp, dec.frame.data, dec.frame.width,
                             dec.frame.height, dec.frame.stride,
                             dec.frame.dirty_y0, dec.frame.dirty_y1);
            t_show += clock() - a;
        }
        frames++;
        if (audio) audio_service(audio);
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

    if (audio_dec) mr_audio_decoder_close(audio_dec);
    if (audio) audio_close(audio);
    display_close(disp);
    mr_decoder_close(&dec);
    mr_demux_close(dx);
    free(buf);
    return 0;
}
