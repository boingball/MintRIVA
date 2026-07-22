/*
 * Host test harness: demux an AVI, decode its video with the MintRIVA core,
 * and (optionally) validate every frame against a directory of reference PPMs
 * produced by ffmpeg. This is how the portable decoders are proven correct
 * before any of this touches a 68k toolchain.
 *
 *   mr_decode <file.avi>                    - print stream info + frame count
 *   mr_decode <file.avi> --ppm <outdir>     - write decoded frames as PPM
 *   mr_decode <file.avi> --check <refdir>    - compare vs refdir/fNNN.ppm
 */
#include "../core/mr_demux.h"
#include "../core/mr_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc(n);
    if (b && fread(b, 1, n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (len) *len = (size_t)n;
    return b;
}

static void write_ppm(const char *path, const mr_frame *fr)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", fr->width, fr->height);
    int y;
    for (y = 0; y < fr->height; y++)
        fwrite(fr->data + (size_t)y * fr->stride, 1, (size_t)fr->width * 3, f);
    fclose(f);
}

/* Mean absolute error vs a reference PPM, returned as MAE*1000 (fixed point,
 * integer-only so no soft-float is pulled into any build); -1 if the reference
 * is missing or its geometry does not match. maxerr gets the largest single
 * channel absolute difference. */
static long check_ppm(const char *path, const mr_frame *fr, int *maxerr)
{
    size_t len;
    uint8_t *b = slurp(path, &len);
    if (!b) return -1;
    /* parse minimal P6 header */
    int w = 0, h = 0, mx = 0;
    const char *p = (const char *)b;
    if (sscanf(p, "P6 %d %d %d", &w, &h, &mx) != 3) { free(b); return -1; }
    /* advance past header: three whitespace-separated ints after "P6" then 1 ws */
    int fields = 0; size_t i = 2;
    while (i < len && fields < 3) {
        while (i < len && (b[i]==' '||b[i]=='\n'||b[i]=='\t'||b[i]=='\r')) i++;
        while (i < len && b[i] >= '0' && b[i] <= '9') i++;
        fields++;
    }
    i++; /* single whitespace after maxval */
    if (w != fr->width || h != fr->height) { free(b); return -1; }
    uint64_t sum = 0; int mxe = 0; size_t n = (size_t)w * h * 3;
    size_t k;
    for (k = 0; k < n; k++) {
        int row = (int)(k / (w * 3));
        int col = (int)(k % (w * 3));
        int dv = (int)fr->data[(size_t)row * fr->stride + col] - (int)b[i + k];
        if (dv < 0) dv = -dv;
        sum += (uint64_t)dv;
        if (dv > mxe) mxe = dv;
    }
    free(b);
    if (maxerr) *maxerr = mxe;
    return (long)((sum * 1000) / n);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: mr_decode <file.avi> [--ppm dir|--check dir]\n"); return 2; }
    const char *mode = argc > 2 ? argv[2] : NULL;
    const char *dir  = argc > 3 ? argv[3] : NULL;

    size_t len;
    uint8_t *buf = slurp(argv[1], &len);
    if (!buf) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    mr_demux *dx = mr_demux_open(buf, len);
    if (!dx) {
        fprintf(stderr, "not a supported container (need AVI or MOV)\n");
        free(buf); return 2;
    }
    const mr_video_info *vi = mr_demux_video(dx);
    const mr_audio_info *ai = mr_demux_audio(dx);
    uint32_t fc = vi->fourcc;
    printf("container: %s\n", mr_demux_container_name(dx));
    /* uint32_t is 'unsigned long' on m68k-amigaos but 'unsigned int' on the
     * host, so cast explicitly to keep the formats portable and warning-clean:
     * %lu + unsigned long, and %c fourcc bytes promoted to int. */
    printf("video: %dx%d fourcc='%c%c%c%c' rate=%lu/%lu (~%lu fps)\n",
           vi->width, vi->height,
           (int)(fc & 0xff), (int)((fc >> 8) & 0xff),
           (int)((fc >> 16) & 0xff), (int)((fc >> 24) & 0xff),
           (unsigned long)vi->rate, (unsigned long)vi->scale,
           (unsigned long)(vi->scale ? vi->rate / vi->scale : 0u));
    if (ai->valid)
        printf("audio: tag=0x%04x %lu Hz %u ch %u-bit\n",
               (unsigned)ai->format_tag,
               (unsigned long)ai->sample_rate,
               (unsigned)ai->channels, (unsigned)ai->bits);

    const mr_codec *codec = mr_codec_find(fc);
    if (!codec) { printf("no decoder for this fourcc\n");
                  mr_demux_close(dx); free(buf); return 1; }

    mr_decoder dec;
    if (mr_decoder_open(&dec, codec, vi->width, vi->height) != MR_OK) {
        fprintf(stderr, "decoder open failed\n");
        mr_demux_close(dx); free(buf); return 1;
    }

    int frame = 0, bad = 0;
    long worst_mae = 0;   /* MAE * 1000 */
    unsigned long audio_bytes = 0, audio_pkts = 0;
    mr_packet pkt;
    while (mr_demux_next_packet(dx, &pkt) == MR_OK) {
        if (!pkt.is_video) { audio_bytes += pkt.len; audio_pkts++; continue; }
        if (pkt.len == 0) continue;
        if (mr_decoder_decode(&dec, pkt.data, pkt.len) != MR_OK) {
            fprintf(stderr, "decode error at frame %d\n", frame); break;
        }
        frame++;
        char path[512];
        if (mode && !strcmp(mode, "--ppm") && dir) {
            snprintf(path, sizeof path, "%s/f%03d.ppm", dir, frame);
            write_ppm(path, &dec.frame);
        } else if (mode && !strcmp(mode, "--check") && dir) {
            snprintf(path, sizeof path, "%s/f%03d.ppm", dir, frame);
            int mxe = 0; long mae = check_ppm(path, &dec.frame, &mxe);
            if (mae < 0) { printf("  frame %3d: no reference\n", frame); }
            else {
                if (mae > worst_mae) worst_mae = mae;
                if (mae > 6000) { bad++;
                    printf("  frame %3d: MAE=%ld.%03ld maxerr=%d  <-- high\n",
                           frame, mae / 1000, mae % 1000, mxe); }
            }
        }
    }
    printf("decoded %d frames\n", frame);
    if (audio_pkts)
        printf("audio: %lu packets, %lu bytes\n", audio_pkts, audio_bytes);
    if (mode && !strcmp(mode, "--check")) {
        printf("worst per-frame MAE=%ld.%03ld, frames over threshold=%d\n",
               worst_mae / 1000, worst_mae % 1000, bad);
        mr_decoder_close(&dec); mr_demux_close(dx); free(buf);
        return bad ? 1 : 0;
    }
    mr_decoder_close(&dec); mr_demux_close(dx); free(buf);
    return 0;
}
