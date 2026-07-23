/*
 * Host smoke test for the MintAMP packet adapter.  It demuxes a real A/V
 * container, feeds every compressed audio packet through Helix and checks
 * that a plausible amount of non-silent PCM is produced.
 */
#include "../core/mr_demux.h"
#include "../audio/mr_audio_decode.h"

#include <stdio.h>
#include <string.h>

struct pcm_stats {
    unsigned long frames;
    unsigned long nonzero;
};

static void count_pcm(void *user, const int16_t *pcm,
                      unsigned frames, unsigned channels)
{
    struct pcm_stats *s = (struct pcm_stats *)user;
    unsigned i, total = frames * channels;
    s->frames += frames;
    for (i = 0; i < total; i++)
        if (pcm[i] > 8 || pcm[i] < -8) s->nonzero++;
}

int main(int argc, char **argv)
{
    mr_demux *dx;
    const mr_audio_info *ai;
    mr_audio_decoder *dec;
    mr_packet pkt;
    struct pcm_stats stats = { 0, 0 };
    unsigned long packets = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: mr_audio_check <avi-or-mp4> <mp3|aac>\n");
        return 2;
    }
    dx = mr_demux_open_file(argv[1]);
    if (!dx) return 2;
    ai = mr_demux_audio(dx);
    if ((!strcmp(argv[2], "mp3") && ai->format_tag != MR_AUDIO_FORMAT_MP3) ||
        (!strcmp(argv[2], "aac") &&
         (ai->format_tag != MR_AUDIO_FORMAT_AAC || ai->config_len < 2))) {
        fprintf(stderr, "wrong demuxed audio setup: tag=0x%04x config=%u\n",
                (unsigned)ai->format_tag, (unsigned)ai->config_len);
        mr_demux_close(dx); return 1;
    }
    dec = mr_audio_decoder_open(ai);
    if (!dec) {
        fprintf(stderr, "unsupported audio setup: tag=0x%04x config=%u\n",
                (unsigned)ai->format_tag, (unsigned)ai->config_len);
        mr_demux_close(dx); return 1;
    }
    while (mr_demux_next_packet(dx, &pkt) == MR_OK) {
        if (!pkt.is_video) {
            uint32_t pos = 0;
            /* AVI is allowed to split an MP3 frame across chunks. Exercise the
             * join buffer deterministically instead of relying on ffmpeg's
             * particular packet sizes. MP4 AAC must retain whole access units. */
            do {
                uint32_t n = pkt.len - pos;
                if (ai->format_tag == MR_AUDIO_FORMAT_MP3 && n > 37) n = 37;
                if (mr_audio_decoder_feed(dec, pkt.data + pos, n,
                                          count_pcm, &stats) < 0) {
                    fprintf(stderr, "fatal audio decode error\n");
                    mr_audio_decoder_close(dec);
                    mr_demux_close(dx); return 1;
                }
                pos += n;
            } while (pos < pkt.len);
            if (!pkt.len) {
                mr_audio_decoder_feed(dec, pkt.data, 0, count_pcm, &stats);
            }
            packets++;
        }
    }
    printf("%s: %lu packets, %lu PCM frames at %u Hz, %lu nonzero samples\n",
           mr_audio_decoder_name(dec), packets, stats.frames,
           mr_audio_decoder_rate(dec), stats.nonzero);
    mr_audio_decoder_close(dec);
    mr_demux_close(dx);
    if (packets < 50 || stats.frames < 40000 || stats.frames > 50000 ||
        stats.nonzero < 40000)
        return 1;
    return 0;
}
