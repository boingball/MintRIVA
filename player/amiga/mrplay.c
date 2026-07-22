/*
 * MintRIVA - Amiga player.
 *
 * Ties the proven portable core (demux + decoder) to the Amiga display + audio
 * backends: load file -> auto-detect container -> decode frames / enqueue audio
 * -> blit, with audio as the A/V master clock (video frames are held until
 * Paula playback reaches their timestamp). Falls back to frame-rate pacing when
 * there is no audio. ESC or the close gadget quits.
 *
 * This first version loads the whole file into RAM. Async streaming from disk
 * is a later step.
 *
 *   mrplay <file.avi|file.mov>
 */
#include "../core/mr_demux.h"
#include "../core/mr_codec.h"
#include "amiga_display.h"
#include "mr_audio.h"

#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(int argc, char **argv)
{
    long len;
    unsigned char *buf;
    mr_demux *dx;
    const mr_video_info *vi;
    const mr_codec *codec;
    mr_decoder dec;
    amiga_display *disp;
    mr_audio *audio = NULL;
    mr_packet pkt;
    long ticks;
    int frames = 0;

    /* Unbuffered so every diagnostic reaches the shell immediately, even if a
     * later step hangs or crashes (libnix stdout can otherwise block-buffer). */
    setvbuf(stdout, NULL, _IONBF, 0);

    if (argc < 2) { printf("usage: mrplay <file.avi|file.mov>\n"); return 5; }
    printf("mrplay: opening %s\n", argv[1]);

    buf = slurp(argv[1], &len);
    if (!buf) { printf("cannot read %s\n", argv[1]); return 10; }
    printf("loaded %ld bytes\n", len);

    dx = mr_demux_open(buf, (size_t)len);
    if (!dx) { printf("unsupported container (need AVI or MOV)\n");
               free(buf); return 10; }

    vi = mr_demux_video(dx);
    codec = mr_codec_find(vi->fourcc);
    if (!codec) { printf("no decoder for this video codec\n");
                  mr_demux_close(dx); free(buf); return 10; }

    if (mr_decoder_open(&dec, codec, vi->width, vi->height) != MR_OK) {
        printf("decoder init failed\n");
        mr_demux_close(dx); free(buf); return 10;
    }

    printf("%dx%d, opening RTG window...\n", vi->width, vi->height);
    disp = display_open(vi->width, vi->height, "MintRIVA");
    if (!disp) { printf("cannot open display - need a truecolour RTG screen "
                        "and cybergraphics.library\n");
                 mr_decoder_close(&dec); mr_demux_close(dx); free(buf); return 10; }

    /* Open Paula audio if the file has a PCM track we can convert. */
    {
        const mr_audio_info *ai = mr_demux_audio(dx);
        if (ai->valid && (ai->bits == 8 || ai->bits == 16)) {
            audio = audio_open(ai->sample_rate, ai->channels, ai->bits);
            if (audio) printf("audio: Paula out, %lu Hz (src %u-bit %u ch)\n",
                              (unsigned long)ai->sample_rate,
                              (unsigned)ai->bits, (unsigned)ai->channels);
            else       printf("audio: Paula open failed, playing silent\n");
        }
    }

    ticks = frame_ticks(vi->rate, vi->scale);
    printf("playing (ESC or close gadget to quit)...\n");

    while (mr_demux_next_packet(dx, &pkt) == MR_OK) {
        if (!pkt.is_video) {
            if (audio) { audio_write(audio, pkt.data, pkt.len);
                         audio_service(audio); }
            continue;
        }
        if (pkt.len == 0) continue;
        if (mr_decoder_decode(&dec, pkt.data, pkt.len) != MR_OK) break;

        if (audio) {
            /* Audio-master: hold this frame until playback reaches its
             * timestamp, but never hang if the audio clock has stalled. */
            unsigned long target = (unsigned long)frames * 1000UL *
                                   (vi->scale ? vi->scale : 1) /
                                   (vi->rate ? vi->rate : 1);
            for (;;) {
                audio_service(audio);
                if (display_poll_quit(disp)) goto done;
                if (audio_elapsed_ms(audio) >= target) break;
                if (audio_starved(audio)) break;
                Delay(1);
            }
        } else {
            if (display_poll_quit(disp)) break;
            Delay(ticks);
        }

        display_show_rgb(disp, dec.frame.data, dec.frame.width,
                         dec.frame.height, dec.frame.stride);
        frames++;
        if (audio) audio_service(audio);
    }

done:
    /* Let any queued audio drain (bounded, so a wedged clock can't loop). */
    if (audio) {
        int guard = 0;
        while (!audio_starved(audio) && guard++ < 4000) {
            audio_service(audio); Delay(1);
        }
    }

    printf("played %d frames - press ESC or close the window to exit\n", frames);
    while (!display_poll_quit(disp)) {
        if (audio) audio_service(audio);
        Delay(2);
    }

    if (audio) audio_close(audio);
    display_close(disp);
    mr_decoder_close(&dec);
    mr_demux_close(dx);
    free(buf);
    return 0;
}
