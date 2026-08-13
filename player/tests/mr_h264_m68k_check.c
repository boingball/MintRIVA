/*
 * Bit-exactness check for the m68k leaf primitives in
 * vendor/libavc_port/ih264_m68k_optim.c.
 *
 * H.264 inter/intra prediction is bit-exact by spec: any rounding or
 * addressing slip in these routines does not fail loudly, it drifts the
 * reconstructed picture a little further from the reference on every
 * following inter-predicted frame until the next IDR resets it - the same
 * failure shape called out for Cinepak in CLAUDE.md. Since there is no m68k
 * toolchain on the dev host (see CLAUDE.md), this check instead runs the
 * m68k versions against independent scalar reference loops that implement
 * the same H.264 semantics (8.3.3.1/8.3.3.2 intra 16x16 vertical/horizontal,
 * 8.4.2.3.1 default weighted sample prediction) directly from the spec
 * description, so a bug shared between "the port" and "the original" is not
 * masked by comparing the port against itself.
 */
#include "ih264_m68k_optim.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static uint32_t xrand(uint32_t *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7fffu;
}

static void fill_random(uint8_t *buf, size_t n, uint32_t *state)
{
    size_t i;
    for (i = 0; i < n; i++) buf[i] = (uint8_t)(xrand(state) & 0xff);
}

static void report(const char *test, int row, int col, int expected, int got)
{
    if (g_failures < 20)
        fprintf(stderr, "FAIL %s: row=%d col=%d expected=%d got=%d\n",
                test, row, col, expected, got);
    g_failures++;
}

/* ---- inter_pred_luma_copy (plain block copy) --------------------------- */
static void ref_luma_copy(const uint8_t *src, uint8_t *dst,
                          int src_strd, int dst_strd, int ht, int wd)
{
    int row, col;
    for (row = 0; row < ht; row++)
        for (col = 0; col < wd; col++)
            dst[row * dst_strd + col] = src[row * src_strd + col];
}

static void check_luma_copy(void)
{
    static const int widths[] = { 4, 8, 12, 16 };
    uint32_t seed = 1;
    unsigned wi;
    for (wi = 0; wi < sizeof widths / sizeof widths[0]; wi++) {
        int wd = widths[wi], ht = 16;
        int src_strd = wd + 5, dst_strd = wd + 7;
        uint8_t src[16 * 32], dst_m68k[16 * 32], dst_ref[16 * 32];
        int row, col;
        fill_random(src, sizeof src, &seed);
        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_inter_pred_luma_copy_m68k(src, dst_m68k, src_strd, dst_strd,
                                           ht, wd, NULL, 0);
        ref_luma_copy(src, dst_ref, src_strd, dst_strd, ht, wd);
        for (row = 0; row < ht; row++)
            for (col = 0; col < dst_strd; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("luma_copy", row, col, dst_ref[idx], dst_m68k[idx]);
            }
    }
}

/* ---- default_weighted_pred_luma / chroma (rounded average) ------------- */
static void ref_weighted_avg(const uint8_t *s1, const uint8_t *s2, uint8_t *dst,
                             int strd1, int strd2, int dst_strd, int ht, int wd)
{
    int row, col;
    for (row = 0; row < ht; row++)
        for (col = 0; col < wd; col++)
            dst[row * dst_strd + col] =
                (uint8_t)(((unsigned)s1[row * strd1 + col] +
                           s2[row * strd2 + col] + 1u) >> 1);
}

static void check_weighted_pred_luma(void)
{
    static const int wds[] = { 4, 8, 16, 8, 16 };
    static const int hts[] = { 4, 4, 8, 16, 16 };
    uint32_t seed = 2;
    unsigned i;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        int wd = wds[i], ht = hts[i];
        int strd1 = wd + 3, strd2 = wd + 5, dst_strd = wd + 7;
        uint8_t s1[16 * 24], s2[16 * 24], dst_m68k[16 * 24], dst_ref[16 * 24];
        int row, col;
        fill_random(s1, sizeof s1, &seed);
        fill_random(s2, sizeof s2, &seed);
        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_default_weighted_pred_luma_m68k(s1, s2, dst_m68k, strd1,
                                                  strd2, dst_strd, ht, wd);
        ref_weighted_avg(s1, s2, dst_ref, strd1, strd2, dst_strd, ht, wd);
        for (row = 0; row < ht; row++)
            for (col = 0; col < wd; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("weighted_pred_luma", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }
    }
    /* All-0 / all-255 extremes: rounding must match exactly. */
    {
        uint8_t s1[16], s2[16], dst_m68k[16], dst_ref[16];
        memset(s1, 0, sizeof s1);
        memset(s2, 255, sizeof s2);
        mr_ih264_default_weighted_pred_luma_m68k(s1, s2, dst_m68k, 4, 4, 4, 4, 4);
        ref_weighted_avg(s1, s2, dst_ref, 4, 4, 4, 4, 4);
        if (memcmp(dst_m68k, dst_ref, sizeof dst_m68k) != 0)
            report("weighted_pred_luma_extreme", 0, 0, dst_ref[0], dst_m68k[0]);
    }
}

static void check_weighted_pred_chroma(void)
{
    /* wd here is the chroma pair-count (as in the H.264 spec / Ittiam API):
     * mr_ih264_default_weighted_pred_chroma_m68k doubles it internally to
     * cover interleaved U/V bytes, same as the reference below. */
    static const int wds[] = { 2, 4, 4, 8 };
    static const int hts[] = { 2, 4, 8, 8 };
    uint32_t seed = 3;
    unsigned i;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        int wd = wds[i], ht = hts[i];
        int wd2 = wd * 2;
        int strd1 = wd2 + 4, strd2 = wd2 + 6, dst_strd = wd2 + 8;
        uint8_t s1[16 * 24], s2[16 * 24], dst_m68k[16 * 24], dst_ref[16 * 24];
        int row, col;
        fill_random(s1, sizeof s1, &seed);
        fill_random(s2, sizeof s2, &seed);
        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_default_weighted_pred_chroma_m68k(s1, s2, dst_m68k, strd1,
                                                    strd2, dst_strd, ht, wd);
        ref_weighted_avg(s1, s2, dst_ref, strd1, strd2, dst_strd, ht, wd2);
        for (row = 0; row < ht; row++)
            for (col = 0; col < wd2; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("weighted_pred_chroma", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }
    }
}

/* ---- intra 16x16 vertical / horizontal (8.3.3.1 / 8.3.3.2) ------------- */
/* Matches the neighbour-buffer layout ih264_m68k_optim.c assumes: left
 * column at src[0..15], corner at src[16], top row at src[17..32]. */
static void ref_intra16_vert(const uint8_t *src, uint8_t *dst, int dst_strd)
{
    const uint8_t *top = src + 17;
    int row, col;
    for (row = 0; row < 16; row++)
        for (col = 0; col < 16; col++)
            dst[row * dst_strd + col] = top[col];
}

static void ref_intra16_horz(const uint8_t *src, uint8_t *dst, int dst_strd)
{
    int row, col;
    for (row = 0; row < 16; row++) {
        uint8_t left = src[15 - row];
        for (col = 0; col < 16; col++)
            dst[row * dst_strd + col] = left;
    }
}

static void check_intra16(void)
{
    uint32_t seed = 4;
    int trial;
    for (trial = 0; trial < 8; trial++) {
        uint8_t nbr[40];
        uint8_t dst_m68k[16 * 20], dst_ref[16 * 20];
        int dst_strd = 20, row, col;

        fill_random(nbr, sizeof nbr, &seed);

        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_intra_pred_luma_16x16_vert_m68k(nbr, dst_m68k, 0, dst_strd, 0);
        ref_intra16_vert(nbr, dst_ref, dst_strd);
        for (row = 0; row < 16; row++)
            for (col = 0; col < 16; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("intra16_vert", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }

        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_intra_pred_luma_16x16_horz_m68k(nbr, dst_m68k, 0, dst_strd, 0);
        ref_intra16_horz(nbr, dst_ref, dst_strd);
        for (row = 0; row < 16; row++)
            for (col = 0; col < 16; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("intra16_horz", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }
    }
}

/* ---- inter_pred_luma_horz / vert (H.264 8.4.2.2.1, 6-tap half-pel) ----- */
/* mr_ih264_inter_pred_luma_horz_m68k/vert_m68k (ih264_m68k_interp.S) only
 * exist on an m68k build (see ih264_m68k_optim.h) - the hand-written .S is
 * guarded on MR_M68K_ASM so it preprocesses to an empty translation unit
 * everywhere else. This is the only pair of checks in this file that can't
 * run on the host build; `make check-m68k` (real m68k, big-endian, under
 * qemu-m68k) is what exercises it. */
#if defined(MR_M68K_ASM)
static void ref_interp_horz(const uint8_t *src, uint8_t *dst, int src_strd,
                            int dst_strd, int ht, int wd)
{
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = 0; col < wd; col++) {
            int16_t t = (int16_t)(1 * (src[col - 2] + src[col + 3])
                                  - 5 * (src[col - 1] + src[col + 2])
                                  + 20 * (src[col] + src[col + 1]));
            int v = (int)((t + 16) >> 5);
            dst[col] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
        src += src_strd;
        dst += dst_strd;
    }
}

static void ref_interp_vert(const uint8_t *src, uint8_t *dst, int src_strd,
                            int dst_strd, int ht, int wd)
{
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = 0; col < wd; col++) {
            int16_t t = (int16_t)(
                1 * (src[col - 2 * src_strd] + src[col + 3 * src_strd])
                - 5 * (src[col - 1 * src_strd] + src[col + 2 * src_strd])
                + 20 * (src[col] + src[col + 1 * src_strd]));
            int v = (int)((t + 16) >> 5);
            dst[col] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
        src += src_strd;
        dst += dst_strd;
    }
}

static void check_interp(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    uint32_t seed = 5;
    unsigned i;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        int extreme;
        for (extreme = 0; extreme < 2; extreme++) {
            int wd = wds[i], ht = hts[i];
            /* Padding of 3 on every side covers the widest tap offset
             * (col-2, col+3, and the vertical equivalents at +-2/+3 rows). */
            int pad = 3, src_strd = wd + 2 * pad, dst_strd = wd + 5;
            uint8_t src[22 * 22], dst_m68k[16 * 21], dst_ref[16 * 21];
            const uint8_t *src0 = src + pad * src_strd + pad;
            int row, col, j;
            for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                 : (uint8_t)(xrand(&seed) & 0xff);

            memset(dst_m68k, 0xAA, sizeof dst_m68k);
            memset(dst_ref, 0xAA, sizeof dst_ref);
            mr_ih264_inter_pred_luma_horz_m68k((uint8_t *)src0, dst_m68k,
                                               src_strd, dst_strd, ht, wd,
                                               NULL, 0);
            ref_interp_horz(src0, dst_ref, src_strd, dst_strd, ht, wd);
            for (row = 0; row < ht; row++)
                for (col = 0; col < wd; col++) {
                    int idx = row * dst_strd + col;
                    if (dst_m68k[idx] != dst_ref[idx])
                        report("interp_horz", row, col, dst_ref[idx],
                               dst_m68k[idx]);
                }

            memset(dst_m68k, 0xAA, sizeof dst_m68k);
            memset(dst_ref, 0xAA, sizeof dst_ref);
            mr_ih264_inter_pred_luma_vert_m68k((uint8_t *)src0, dst_m68k,
                                               src_strd, dst_strd, ht, wd,
                                               NULL, 0);
            ref_interp_vert(src0, dst_ref, src_strd, dst_strd, ht, wd);
            for (row = 0; row < ht; row++)
                for (col = 0; col < wd; col++) {
                    int idx = row * dst_strd + col;
                    if (dst_m68k[idx] != dst_ref[idx])
                        report("interp_vert", row, col, dst_ref[idx],
                               dst_m68k[idx]);
                }
        }
    }
}
/* ---- inter_pred_luma_horz_qpel_vert_qpel (dydx 5/7/13/15) -------------- */
/* Independent re-derivation of ih264_inter_pred_luma_horz_qpel_vert_qpel
 * (ih264_inter_pred_filters.c): run the same 6-tap filter as ref_interp_horz
 * /ref_interp_vert above once vertically from a predictor offset by
 * (dydx&3)>>1 columns and once horizontally from a predictor offset by
 * ((dydx>>2)&3)>>1 rows, then average the two clipped results with +1
 * rounding - written straight from the spec/reference-C formula, not
 * derived from the asm under test. */
static void ref_interp_horz_qpel_vert_qpel(const uint8_t *src, uint8_t *dst,
                                           int src_strd, int dst_strd,
                                           int ht, int wd, int dydx)
{
    int xoff_half = (dydx & 3) >> 1;
    int yoff_half = ((dydx >> 2) & 3) >> 1;
    const uint8_t *pred_vert0 = src + xoff_half;
    const uint8_t *pred_horz0 = src + yoff_half * src_strd;
    int row, col;
    for (row = 0; row < ht; row++) {
        const uint8_t *pv = pred_vert0 + (size_t)row * src_strd;
        const uint8_t *ph = pred_horz0 + (size_t)row * src_strd;
        for (col = 0; col < wd; col++) {
            int16_t tv = (int16_t)(
                1 * (pv[col - 2 * src_strd] + pv[col + 3 * src_strd])
                - 5 * (pv[col - src_strd] + pv[col + 2 * src_strd])
                + 20 * (pv[col] + pv[col + src_strd]));
            int vv = (int)((tv + 16) >> 5);
            vv = vv < 0 ? 0 : vv > 255 ? 255 : vv;

            int16_t th = (int16_t)(1 * (ph[col - 2] + ph[col + 3])
                                   - 5 * (ph[col - 1] + ph[col + 2])
                                   + 20 * (ph[col] + ph[col + 1]));
            int vh = (int)((th + 16) >> 5);
            vh = vh < 0 ? 0 : vh > 255 ? 255 : vh;

            dst[col] = (uint8_t)((vv + vh + 1) >> 1);
        }
        dst += dst_strd;
    }
}

static void check_interp_qpel_qpel(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    static const int dydxs[] = { 5, 7, 13, 15 };
    uint32_t seed = 6;
    unsigned i, di;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        for (di = 0; di < sizeof dydxs / sizeof dydxs[0]; di++) {
            int extreme;
            for (extreme = 0; extreme < 2; extreme++) {
                int wd = wds[i], ht = hts[i], dydx = dydxs[di];
                /* Padding of 4 covers the widest tap offset (col-2, col+3,
                 * the vertical equivalents at +-2/+3 rows) plus the extra
                 * +1 column/row the predictor base can carry for dydx with
                 * xoff_half/yoff_half set. */
                int pad = 4, src_strd = wd + 2 * pad, dst_strd = wd + 5;
                uint8_t src[24 * 24], dst_m68k[16 * 21], dst_ref[16 * 21];
                const uint8_t *src0 = src + pad * src_strd + pad;
                int row, col, j;
                for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                    src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                     : (uint8_t)(xrand(&seed) & 0xff);

                memset(dst_m68k, 0xAA, sizeof dst_m68k);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k(
                    (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                    NULL, dydx);
                ref_interp_horz_qpel_vert_qpel(src0, dst_ref, src_strd,
                                               dst_strd, ht, wd, dydx);
                for (row = 0; row < ht; row++)
                    for (col = 0; col < wd; col++) {
                        int idx = row * dst_strd + col;
                        if (dst_m68k[idx] != dst_ref[idx])
                            report("interp_horz_qpel_vert_qpel", row, col,
                                   dst_ref[idx], dst_m68k[idx]);
                    }
            }
        }
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
    check_luma_copy();
    check_weighted_pred_luma();
    check_weighted_pred_chroma();
    check_intra16();
#if defined(MR_M68K_ASM)
    check_interp();
    check_interp_qpel_qpel();
#endif

    if (g_failures) {
        fprintf(stderr, "mr_h264_m68k_check: %d mismatches\n", g_failures);
        return 1;
    }
    printf("mr_h264_m68k_check: OK\n");
    return 0;
}
