/*
 * MintVID - conformance check for the m68k MPEG-1 IDCT
 * (core/mr_mpeg1_idct_m68k.S).
 *
 * m68k-only (like mr_mpeg1_blockset_check.c): plm_video_idct_m68k only
 * exists in the .S file, which is only compiled into the m68k build (see
 * tests/run_m68k_check.sh) - there is no portable-C twin to call on the
 * host, so this binary is built and run only under qemu-m68k.
 *
 * Checked against an independent reference reimplementation of pl_mpeg's
 * plm_video_idct (core/pl_mpeg.h) - written fresh here rather than shared
 * with either the asm or pl_mpeg.h's own C fallback, so a bug shared
 * between the asm and its "reference" cannot hide.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void plm_video_idct_m68k(int *block) __asm__("plm_video_idct_m68k");

/* Independent reference: pl_mpeg's plm_video_idct, transcribed directly. */
static void ref_idct(int *block)
{
    int b1, b3, b4, b6, b7, tmp1, tmp2, m0,
        x0, x1, x2, x3, x4, y3, y4, y5, y6, y7;
    int i;

    for (i = 0; i < 8; ++i) {
        b1 = block[4 * 8 + i];
        b3 = block[2 * 8 + i] + block[6 * 8 + i];
        b4 = block[5 * 8 + i] - block[3 * 8 + i];
        tmp1 = block[1 * 8 + i] + block[7 * 8 + i];
        tmp2 = block[3 * 8 + i] + block[5 * 8 + i];
        b6 = block[1 * 8 + i] - block[7 * 8 + i];
        b7 = tmp1 + tmp2;
        m0 = block[0 * 8 + i];
        x4 = ((b6 * 473 - b4 * 196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2) * 362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2 * 8 + i] - block[6 * 8 + i]) * 362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2;
        y4 = x3 + b3;
        y5 = x1 - x2;
        y6 = x3 - b3;
        y7 = -x0 - ((b4 * 473 + b6 * 196 + 128) >> 8);
        block[0 * 8 + i] = b7 + y4;
        block[1 * 8 + i] = x4 + y3;
        block[2 * 8 + i] = y5 - x0;
        block[3 * 8 + i] = y6 - y7;
        block[4 * 8 + i] = y6 + y7;
        block[5 * 8 + i] = x0 + y5;
        block[6 * 8 + i] = y3 - x4;
        block[7 * 8 + i] = y4 - b7;
    }

    for (i = 0; i < 64; i += 8) {
        b1 = block[4 + i];
        b3 = block[2 + i] + block[6 + i];
        b4 = block[5 + i] - block[3 + i];
        tmp1 = block[1 + i] + block[7 + i];
        tmp2 = block[3 + i] + block[5 + i];
        b6 = block[1 + i] - block[7 + i];
        b7 = tmp1 + tmp2;
        m0 = block[0 + i];
        x4 = ((b6 * 473 - b4 * 196 + 128) >> 8) - b7;
        x0 = x4 - (((tmp1 - tmp2) * 362 + 128) >> 8);
        x1 = m0 - b1;
        x2 = (((block[2 + i] - block[6 + i]) * 362 + 128) >> 8) - b3;
        x3 = m0 + b1;
        y3 = x1 + x2;
        y4 = x3 + b3;
        y5 = x1 - x2;
        y6 = x3 - b3;
        y7 = -x0 - ((b4 * 473 + b6 * 196 + 128) >> 8);
        block[0 + i] = (b7 + y4 + 128) >> 8;
        block[1 + i] = (x4 + y3 + 128) >> 8;
        block[2 + i] = (y5 - x0 + 128) >> 8;
        block[3 + i] = (y6 - y7 + 128) >> 8;
        block[4 + i] = (y6 + y7 + 128) >> 8;
        block[5 + i] = (x0 + y5 + 128) >> 8;
        block[6 + i] = (y3 - x4 + 128) >> 8;
        block[7 + i] = (y4 - b7 + 128) >> 8;
    }
}

static unsigned state = 0x69646374U;
static int32_t random_coeff(int32_t bound)
{
    state = state * 1664525U + 1013904223U;
    return (int32_t)((state >> 8) % (unsigned)(2 * bound + 1)) - bound;
}

static int run_case(const int *src, const char *label)
{
    int asm_block[64], ref_block[64];
    int i;
    memcpy(asm_block, src, sizeof asm_block);
    memcpy(ref_block, src, sizeof ref_block);

    plm_video_idct_m68k(asm_block);
    ref_idct(ref_block);

    for (i = 0; i < 64; i++) {
        if (asm_block[i] != ref_block[i]) {
            printf("FAIL %s: index %d asm=%d ref=%d\n",
                   label, i, asm_block[i], ref_block[i]);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    int fails = 0, t, pos;
    int block[64];

    /* All-zero. */
    memset(block, 0, sizeof block);
    fails += run_case(block, "all-zero");

    /* Single nonzero coefficient at each of the 64 positions, a few
     * representative magnitudes (premultiplied AC/DC coefficients can
     * reach roughly +-127000 - see PLM_VIDEO_PREMULTIPLIER_MATRIX and the
     * level clamp to +-2048 in plm_video_decode_block). */
    {
        static const int mags[] = { 1, -1, 255, -255, 126976, -126976 };
        int m;
        for (pos = 0; pos < 64; pos++) {
            for (m = 0; m < 6; m++) {
                memset(block, 0, sizeof block);
                block[pos] = mags[m];
                fails += run_case(block, "single-coeff");
            }
        }
    }

    /* Fully populated random blocks, realistic coefficient magnitude. */
    for (t = 0; t < 200; t++) {
        for (pos = 0; pos < 64; pos++)
            block[pos] = random_coeff(126976);
        fails += run_case(block, "random-full");
    }

    /* Sparse random blocks (a handful of nonzero AC coefficients plus a
     * DC term), the common real-world shape after zig-zag + RLE decode. */
    for (t = 0; t < 200; t++) {
        int n, k;
        memset(block, 0, sizeof block);
        block[0] = random_coeff(126976 << 3); /* DC: extra <<8 headroom, scaled down */
        n = 1 + (int)(random_coeff(12) + 12);
        for (k = 0; k < n; k++)
            block[1 + (random_coeff(31) + 31)] = random_coeff(126976);
        fails += run_case(block, "sparse");
    }

    if (fails) {
        printf("MPEG-1 IDCT checks FAILED: %d mismatch(es)\n", fails);
        return 1;
    }
    printf("MPEG-1 IDCT checks passed\n");
    return 0;
}
