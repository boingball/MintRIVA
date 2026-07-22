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
#include "../core/mr_avi.h"
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

/* Mean absolute error vs a reference PPM; returns -1 if ref missing/mismatch. */
static double check_ppm(const char *path, const mr_frame *fr, double *maxerr)
{
    size_t len;
    uint8_t *b = slurp(path, &len);
    if (!b) return -1.0;
    /* parse minimal P6 header */
    int w = 0, h = 0, mx = 0;
    const char *p = (const char *)b;
    if (sscanf(p, "P6 %d %d %d", &w, &h, &mx) != 3) { free(b); return -1.0; }
    /* advance past header: three whitespace-separated ints after "P6" then 1 ws */
    int fields = 0; size_t i = 2;
    while (i < len && fields < 3) {
        while (i < len && (b[i]==' '||b[i]=='\n'||b[i]=='\t'||b[i]=='\r')) i++;
        while (i < len && b[i] >= '0' && b[i] <= '9') i++;
        fields++;
    }
    i++; /* single whitespace after maxval */
    if (w != fr->width || h != fr->height) { free(b); return -1.0; }
    double sum = 0; double mxe = 0; size_t n = (size_t)w * h * 3;
    size_t k;
    for (k = 0; k < n; k++) {
        int row = (int)(k / (w * 3));
        int col = (int)(k % (w * 3));
        int dv = (int)fr->data[(size_t)row * fr->stride + col] - (int)b[i + k];
        if (dv < 0) dv = -dv;
        sum += dv;
        if (dv > mxe) mxe = dv;
    }
    free(b);
    if (maxerr) *maxerr = mxe;
    return sum / (double)n;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: mr_decode <file.avi> [--ppm dir|--check dir]\n"); return 2; }
    const char *mode = argc > 2 ? argv[2] : NULL;
    const char *dir  = argc > 3 ? argv[3] : NULL;

    size_t len;
    uint8_t *buf = slurp(argv[1], &len);
    if (!buf) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    mr_avi avi;
    if (mr_avi_open(&avi, buf, len) != MR_OK) {
        fprintf(stderr, "not a supported AVI\n"); return 2;
    }
    uint32_t fc = avi.video.fourcc;
    printf("video: %dx%d fourcc='%c%c%c%c' rate=%u/%u (%.2f fps)\n",
           avi.video.width, avi.video.height,
           fc & 0xff, (fc>>8)&0xff, (fc>>16)&0xff, (fc>>24)&0xff,
           avi.video.rate, avi.video.scale,
           avi.video.scale ? (double)avi.video.rate / avi.video.scale : 0.0);
    if (avi.audio.valid)
        printf("audio: tag=0x%04x %u Hz %u ch %u-bit\n",
               avi.audio.format_tag, avi.audio.sample_rate,
               avi.audio.channels, avi.audio.bits);

    const mr_codec *codec = mr_codec_find(fc);
    if (!codec) { printf("no decoder for this fourcc\n"); free(buf); return 1; }

    mr_decoder dec;
    if (mr_decoder_open(&dec, codec, avi.video.width, avi.video.height) != MR_OK) {
        fprintf(stderr, "decoder open failed\n"); free(buf); return 1;
    }

    int frame = 0, bad = 0;
    double worst_mae = 0;
    mr_avi_packet pkt;
    while (mr_avi_next_packet(&avi, &pkt) == MR_OK) {
        if (!pkt.is_video || pkt.len == 0) continue;
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
            double mxe = 0, mae = check_ppm(path, &dec.frame, &mxe);
            if (mae < 0) { printf("  frame %3d: no reference\n", frame); }
            else {
                if (mae > worst_mae) worst_mae = mae;
                if (mae > 6.0) { bad++;
                    printf("  frame %3d: MAE=%.2f maxerr=%.0f  <-- high\n",
                           frame, mae, mxe); }
            }
        }
    }
    printf("decoded %d frames\n", frame);
    if (mode && !strcmp(mode, "--check")) {
        printf("worst per-frame MAE=%.3f, frames over threshold=%d\n",
               worst_mae, bad);
        mr_decoder_close(&dec); free(buf);
        return bad ? 1 : 0;
    }
    mr_decoder_close(&dec); free(buf);
    return 0;
}
