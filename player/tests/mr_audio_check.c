/*
 * Host smoke test for the MintAMP packet adapter.  It demuxes a real A/V
 * container, feeds every compressed audio packet through Helix and checks
 * that a plausible amount of non-silent PCM is produced. Runs each fixture
 * twice - once at the normal decoded rate, once with --audio-rate=low's
 * low_rate flag - and checks the low-rate run's decoded_rate against the
 * normal run's, since mr_audio_decoder_open()'s low_rate always doubles
 * whatever stride normal mode picked (see compute_stride() in
 * audio/mr_audio_decode.c), so the two must differ by exactly 2x.
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

/* Decodes the whole file once under the given low_rate setting. Returns 0 on
 * success (and *decoded_rate_out is the decoder's effective output rate), or
 * a nonzero exit code on failure. */
static int run_once(const char *path, const char *kind, int low_rate,
                    unsigned *decoded_rate_out)
{
    mr_demux *dx;
    const mr_audio_info *ai;
    mr_audio_decoder *dec;
    mr_packet pkt;
    struct pcm_stats stats = { 0, 0 };
    unsigned long packets = 0;
    unsigned decoded_rate;

    dx = mr_demux_open_file(path);
    if (!dx) return 2;
    ai = mr_demux_audio(dx);
    if ((!strcmp(kind, "mp3") && ai->format_tag != MR_AUDIO_FORMAT_MP3) ||
        ((!strcmp(kind, "aac") || !strcmp(kind, "latm")) &&
         ai->format_tag != MR_AUDIO_FORMAT_AAC) ||
        (!strcmp(kind, "latm") &&
         ai->codec_tag != MR_FOURCC('L','A','T','M')) ||
        (!strcmp(kind, "ac3") &&
         ai->format_tag != MR_AUDIO_FORMAT_AC3)) {
        fprintf(stderr, "wrong demuxed audio setup: tag=0x%04x config=%u\n",
                (unsigned)ai->format_tag, (unsigned)ai->config_len);
        mr_demux_close(dx); return 1;
    }
    dec = mr_audio_decoder_open(ai, low_rate);
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
    decoded_rate = mr_audio_decoder_rate(dec);
    printf("%s%s: %lu packets, %lu PCM frames at %u Hz, %lu nonzero samples\n",
           mr_audio_decoder_name(dec), low_rate ? " (low)" : "",
           packets, stats.frames, decoded_rate, stats.nonzero);
    mr_audio_decoder_close(dec);
    mr_demux_close(dx);
    if (decoded_rate_out) *decoded_rate_out = decoded_rate;
    /* MP4 exposes one AAC access unit per packet; TS coalesces several ADTS
     * frames into each PES packet, so packet count is not a quality signal. */
    if (packets < 1 || !decoded_rate ||
        stats.frames < (unsigned long)decoded_rate * 17 / 10 ||
        stats.frames > (unsigned long)decoded_rate * 23 / 10 ||
        stats.nonzero < stats.frames / 2)
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    unsigned rate_normal = 0, rate_low = 0;
    int rc;

    if (argc != 3) {
        fprintf(stderr,
                "usage: mr_audio_check <media> <mp3|aac|latm|ac3>\n");
        return 2;
    }
    rc = run_once(argv[1], argv[2], 0, &rate_normal);
    if (rc) return rc;
    rc = run_once(argv[1], argv[2], 1, &rate_low);
    if (rc) return rc;
    if (rate_low * 2 != rate_normal) {
        fprintf(stderr, "low-rate mismatch: normal=%u Hz low=%u Hz "
                        "(expected exactly half)\n", rate_normal, rate_low);
        return 1;
    }
    return 0;
}
