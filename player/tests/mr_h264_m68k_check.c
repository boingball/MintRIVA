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
/* ---- inter_pred_luma_horz_qpel (dydx 1/3) / vert_qpel (dydx 4/12) ------ */
/* Independent re-derivation of ih264_inter_pred_luma_horz_qpel /
 * ..._vert_qpel: one 6-tap pass (from pu1_src itself, not from an offset
 * predictor) averaged with +1 rounding against a predictor offset by
 * (dydx&3)>>1 columns (horz) or ((dydx>>2)&3)>>1 rows (vert) from pu1_src. */
static void ref_interp_horz_qpel(const uint8_t *src, uint8_t *dst,
                                 int src_strd, int dst_strd, int ht, int wd,
                                 int dydx)
{
    int xoff_half = (dydx & 3) >> 1;
    const uint8_t *pred0 = src + xoff_half;
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = 0; col < wd; col++) {
            int16_t t = (int16_t)(1 * (src[col - 2] + src[col + 3])
                                  - 5 * (src[col - 1] + src[col + 2])
                                  + 20 * (src[col] + src[col + 1]));
            int v = (int)((t + 16) >> 5);
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            dst[col] = (uint8_t)((v + pred0[col] + 1) >> 1);
        }
        src += src_strd; dst += dst_strd; pred0 += src_strd;
    }
}

static void ref_interp_vert_qpel(const uint8_t *src, uint8_t *dst,
                                 int src_strd, int dst_strd, int ht, int wd,
                                 int dydx)
{
    int yoff_half = ((dydx >> 2) & 3) >> 1;
    const uint8_t *pred0 = src + (size_t)yoff_half * src_strd;
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = 0; col < wd; col++) {
            int16_t t = (int16_t)(
                1 * (src[col - 2 * src_strd] + src[col + 3 * src_strd])
                - 5 * (src[col - 1 * src_strd] + src[col + 2 * src_strd])
                + 20 * (src[col] + src[col + 1 * src_strd]));
            int v = (int)((t + 16) >> 5);
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            dst[col] = (uint8_t)((v + pred0[col] + 1) >> 1);
        }
        src += src_strd; dst += dst_strd; pred0 += src_strd;
    }
}

static void check_interp_qpel(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    static const int horz_dydxs[] = { 1, 3 };
    static const int vert_dydxs[] = { 4, 12 };
    uint32_t seed = 7;
    unsigned i, di;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        for (di = 0; di < 2; di++) {
            int extreme;
            for (extreme = 0; extreme < 2; extreme++) {
                int wd = wds[i], ht = hts[i];
                int pad = 4, src_strd = wd + 2 * pad, dst_strd = wd + 5;
                uint8_t src[24 * 24], dst_m68k[16 * 21], dst_ref[16 * 21];
                const uint8_t *src0 = src + pad * src_strd + pad;
                int row, col, j;
                for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                    src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                     : (uint8_t)(xrand(&seed) & 0xff);

                memset(dst_m68k, 0xAA, sizeof dst_m68k);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                mr_ih264_inter_pred_luma_horz_qpel_m68k(
                    (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                    NULL, horz_dydxs[di]);
                ref_interp_horz_qpel(src0, dst_ref, src_strd, dst_strd, ht,
                                     wd, horz_dydxs[di]);
                for (row = 0; row < ht; row++)
                    for (col = 0; col < wd; col++) {
                        int idx = row * dst_strd + col;
                        if (dst_m68k[idx] != dst_ref[idx])
                            report("interp_horz_qpel", row, col,
                                   dst_ref[idx], dst_m68k[idx]);
                    }

                memset(dst_m68k, 0xAA, sizeof dst_m68k);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                mr_ih264_inter_pred_luma_vert_qpel_m68k(
                    (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                    NULL, vert_dydxs[di]);
                ref_interp_vert_qpel(src0, dst_ref, src_strd, dst_strd, ht,
                                     wd, vert_dydxs[di]);
                for (row = 0; row < ht; row++)
                    for (col = 0; col < wd; col++) {
                        int idx = row * dst_strd + col;
                        if (dst_m68k[idx] != dst_ref[idx])
                            report("interp_vert_qpel", row, col,
                                   dst_ref[idx], dst_m68k[idx]);
                    }
            }
        }
    }
}
/* ---- inter_pred_luma_horz_hpel_vert_hpel (dydx 10) --------------------- */
/* Independent re-derivation of ih264_inter_pred_luma_horz_hpel_vert_hpel:
 * a genuine two-stage separable 6-tap, not a single pass averaged against
 * an offset predictor like every other filter above. Stage 1 is a vertical
 * 6-tap raw sum (no shift/clip) at every column from -2 to wd+2 inclusive;
 * stage 2 is a horizontal 6-tap over those *sums*, accumulated in 32-bit
 * (a single-axis 16-bit sum times up to 20, summed six ways, does not fit
 * 16 bits), finished with the combined (+512)>>10 shift and CLIP_U8. */
static void ref_interp_hpel_hpel(const uint8_t *src, uint8_t *dst,
                                 int src_strd, int dst_strd, int ht, int wd)
{
    int32_t tmp[16 * 21]; /* row pitch (wd+5), matches the WORD16 scratch */
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = -2; col < wd + 3; col++) {
            int16_t s = (int16_t)(
                1 * (src[col - 2 * src_strd] + src[col + 3 * src_strd])
                - 5 * (src[col - 1 * src_strd] + src[col + 2 * src_strd])
                + 20 * (src[col] + src[col + 1 * src_strd]));
            tmp[row * (wd + 5) + (col + 2)] = s;
        }
        src += src_strd;
    }
    for (row = 0; row < ht; row++) {
        const int32_t *p = &tmp[row * (wd + 5) + 2]; /* p[0] == logical col 0 */
        for (col = 0; col < wd; col++) {
            int32_t t = 1 * (p[col - 2] + p[col + 3])
                       - 5 * (p[col - 1] + p[col + 2])
                       + 20 * (p[col] + p[col + 1]);
            t = (t + 512) >> 10;
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            dst[col] = (uint8_t)t;
        }
        dst += dst_strd;
    }
}

static void check_interp_hpel_hpel(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    uint32_t seed = 8;
    unsigned i;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        int extreme;
        for (extreme = 0; extreme < 2; extreme++) {
            int wd = wds[i], ht = hts[i];
            int pad = 5, src_strd = wd + 2 * pad, dst_strd = wd + 5;
            uint8_t src[26 * 26], dst_m68k[16 * 21], dst_ref[16 * 21];
            uint8_t tmp_m68k[(16 + 5) * 16 * 2]; /* WORD16 scratch, worst case */
            const uint8_t *src0 = src + pad * src_strd + pad;
            int row, col, j;
            for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                 : (uint8_t)(xrand(&seed) & 0xff);

            memset(dst_m68k, 0xAA, sizeof dst_m68k);
            memset(dst_ref, 0xAA, sizeof dst_ref);
            memset(tmp_m68k, 0, sizeof tmp_m68k);
            mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k(
                (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                tmp_m68k, 0);
            ref_interp_hpel_hpel(src0, dst_ref, src_strd, dst_strd, ht, wd);
            for (row = 0; row < ht; row++)
                for (col = 0; col < wd; col++) {
                    int idx = row * dst_strd + col;
                    if (dst_m68k[idx] != dst_ref[idx])
                        report("interp_hpel_hpel", row, col, dst_ref[idx],
                               dst_m68k[idx]);
                }
        }
    }
}
/* ---- deblk_luma_vert_bs4 / horz_bs4 (H.264 8.7.2.4, bS==4) -------------- */
/* Independent re-derivation of ih264_deblk_luma_vert_bs4/horz_bs4 straight
 * from the filtering-decision + strong/weak formulas in the spec section
 * title, not from this asm. Filters one 8-sample window (p3..p0,q0..q3);
 * out[] starts as a copy of the input so untouched samples (the "skip"
 * case, or the p side / q side when only the other strengthens) come back
 * unchanged, matching the reference C which simply never assigns them. */
static void ref_deblk_bs4_sample(uint8_t p3, uint8_t p2, uint8_t p1,
                                 uint8_t p0, uint8_t q0, uint8_t q1,
                                 uint8_t q2, uint8_t q3, int alpha, int beta,
                                 uint8_t out[8])
{
    out[0] = p3; out[1] = p2; out[2] = p1; out[3] = p0;
    out[4] = q0; out[5] = q1; out[6] = q2; out[7] = q3;
    if (abs((int)p0 - (int)q0) >= alpha || abs((int)q1 - (int)q0) >= beta ||
        abs((int)p1 - (int)p0) >= beta)
        return;
    if (abs((int)p0 - (int)q0) < (alpha >> 2) + 2) {
        int a_p = abs((int)p2 - (int)p0);
        int a_q = abs((int)q2 - (int)q0);
        if (a_p < beta) {
            out[3] = (uint8_t)((p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3);
            out[2] = (uint8_t)((p2 + p1 + p0 + q0 + 2) >> 2);
            out[1] = (uint8_t)((2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3);
        } else {
            out[3] = (uint8_t)((2*p1 + p0 + q1 + 2) >> 2);
        }
        if (a_q < beta) {
            out[4] = (uint8_t)((p1 + 2*p0 + 2*q0 + 2*q1 + q2 + 4) >> 3);
            out[5] = (uint8_t)((p0 + q0 + q1 + q2 + 2) >> 2);
            out[6] = (uint8_t)((2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3);
        } else {
            out[4] = (uint8_t)((2*q1 + q0 + p1 + 2) >> 2);
        }
    } else {
        out[3] = (uint8_t)((2*p1 + p0 + q1 + 2) >> 2);
        out[4] = (uint8_t)((2*q1 + q0 + p1 + 2) >> 2);
    }
}

static void deblk_bs4_fill_sample(uint32_t *seed, int row, uint8_t s[8])
{
    int k;
    if ((row & 3) == 0) {
        /* Bias every 4th sample toward the skip/strong/weak decision
         * boundaries instead of leaving them purely to chance - p0/q0 and
         * p1/q1 close together (near the skip threshold), p2/p3/q2/q3
         * fully random (near the strong-vs-weak a_p/a_q threshold once
         * beta is factored in across the alpha/beta sweep below). */
        int p0 = 80 + (int)(xrand(seed) % 40);
        s[3] = (uint8_t)p0;
        s[4] = (uint8_t)(p0 + (int)(xrand(seed) % 9) - 4);
        s[2] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
        s[5] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
        s[1] = (uint8_t)(xrand(seed) & 0xff);
        s[0] = (uint8_t)(xrand(seed) & 0xff);
        s[6] = (uint8_t)(xrand(seed) & 0xff);
        s[7] = (uint8_t)(xrand(seed) & 0xff);
    } else {
        for (k = 0; k < 8; k++) s[k] = (uint8_t)(xrand(seed) & 0xff);
    }
}

static void check_deblk_bs4(void)
{
    static const int alphas[] = { 0, 4, 12, 30, 60, 130, 255 };
    static const int betas[]  = { 0, 2, 5, 9, 14, 18 };
    uint32_t seed = 9;
    unsigned ai, bi;
    for (ai = 0; ai < sizeof alphas / sizeof alphas[0]; ai++) {
        for (bi = 0; bi < sizeof betas / sizeof betas[0]; bi++) {
            int alpha = alphas[ai], beta = betas[bi];
            int row, k;

            /* vertical: 16 rows, 8 columns per row (p3..q3), edge/pu1_src
             * at column 4 (q0) of row 0; src_strd = 8. */
            {
                int src_strd = 8;
                uint8_t buf_m68k[16 * 8], buf_ref[16 * 8], s[8], out[8];
                for (row = 0; row < 16; row++) {
                    deblk_bs4_fill_sample(&seed, row, s);
                    for (k = 0; k < 8; k++)
                        buf_m68k[row * src_strd + k] = s[k];
                    ref_deblk_bs4_sample(s[0], s[1], s[2], s[3], s[4], s[5],
                                        s[6], s[7], alpha, beta, out);
                    for (k = 0; k < 8; k++)
                        buf_ref[row * src_strd + k] = out[k];
                }
                mr_ih264_deblk_luma_vert_bs4_m68k(buf_m68k + 4, src_strd,
                                                  alpha, beta);
                for (row = 0; row < 16; row++)
                    for (k = 0; k < 8; k++) {
                        int idx = row * src_strd + k;
                        if (buf_m68k[idx] != buf_ref[idx])
                            report("deblk_vert_bs4", row, k, buf_ref[idx],
                                   buf_m68k[idx]);
                    }
            }

            /* horizontal: 8 rows, 16 columns; edge/pu1_src at row 4, each
             * column an independent sample sharing the same alpha/beta. */
            {
                int src_strd = 16;
                uint8_t buf_m68k[8 * 16], buf_ref[8 * 16], s[8], out[8];
                int col;
                for (col = 0; col < 16; col++) {
                    deblk_bs4_fill_sample(&seed, col, s);
                    for (k = 0; k < 8; k++)
                        buf_m68k[k * src_strd + col] = s[k];
                    ref_deblk_bs4_sample(s[0], s[1], s[2], s[3], s[4], s[5],
                                        s[6], s[7], alpha, beta, out);
                    for (k = 0; k < 8; k++)
                        buf_ref[k * src_strd + col] = out[k];
                }
                mr_ih264_deblk_luma_horz_bs4_m68k(buf_m68k + 4 * src_strd,
                                                  src_strd, alpha, beta);
                for (k = 0; k < 8; k++)
                    for (col = 0; col < 16; col++) {
                        int idx = k * src_strd + col;
                        if (buf_m68k[idx] != buf_ref[idx])
                            report("deblk_horz_bs4", k, col, buf_ref[idx],
                                   buf_m68k[idx]);
                    }
            }
        }
    }
}

/* ---- deblk_luma_vert_bslt4 / horz_bslt4 (H.264 8.7.2.3, bS<4) ---------- */
/* Independent re-derivation straight from the spec-section formulas, not
 * from this asm. Unlike bS==4, p1'/q1' here are written via C's
 * `pu1_src_temp[pos] += CLIP3(...)` - an unsigned-byte add with implicit
 * truncation, no explicit 0..255 clamp - so out[1]/out[4] below are cast
 * straight to uint8_t (truncating), matching that exactly; only p0'/q0'
 * get a real CLIP_U8. */
static void ref_deblk_bslt4_sample(uint8_t p2, uint8_t p1, uint8_t p0,
                                   uint8_t q0, uint8_t q1, uint8_t q2,
                                   int alpha, int beta, int tc0,
                                   uint8_t out[6] /* p2,p1,p0,q0,q1,q2 */)
{
    out[0] = p2; out[1] = p1; out[2] = p0;
    out[3] = q0; out[4] = q1; out[5] = q2;
    if (abs((int)p0 - (int)q0) >= alpha || abs((int)q1 - (int)q0) >= beta ||
        abs((int)p1 - (int)p0) >= beta)
        return;
    {
        int a_p = abs((int)p2 - (int)p0);
        int a_q = abs((int)q2 - (int)q0);
        int tc = tc0 + (a_p < beta ? 1 : 0) + (a_q < beta ? 1 : 0);
        int val = (((int)q0 - (int)p0) * 4 + ((int)p1 - (int)q1) + 4) >> 3;
        int delta = val < -tc ? -tc : (val > tc ? tc : val);
        int p0n = (int)p0 + delta;
        int q0n = (int)q0 - delta;
        p0n = p0n < 0 ? 0 : (p0n > 255 ? 255 : p0n);
        q0n = q0n < 0 ? 0 : (q0n > 255 ? 255 : q0n);
        if (a_p < beta) {
            int v = ((int)p2 + (((int)p0 + (int)q0 + 1) >> 1) -
                    2 * (int)p1) >> 1;
            int c = v < -tc0 ? -tc0 : (v > tc0 ? tc0 : v);
            out[1] = (uint8_t)((int)p1 + c);
        }
        if (a_q < beta) {
            int v = ((int)q2 + (((int)p0 + (int)q0 + 1) >> 1) -
                    2 * (int)q1) >> 1;
            int c = v < -tc0 ? -tc0 : (v > tc0 ? tc0 : v);
            out[4] = (uint8_t)((int)q1 + c);
        }
        out[2] = (uint8_t)p0n;
        out[3] = (uint8_t)q0n;
    }
}

static void deblk_bslt4_fill_sample(uint32_t *seed, int row, uint8_t s[6])
{
    int k;
    if ((row & 3) == 0) {
        int p0 = 80 + (int)(xrand(seed) % 40);
        s[2] = (uint8_t)p0;
        s[3] = (uint8_t)(p0 + (int)(xrand(seed) % 9) - 4);
        s[1] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
        s[4] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
        s[0] = (uint8_t)(xrand(seed) & 0xff);
        s[5] = (uint8_t)(xrand(seed) & 0xff);
    } else {
        for (k = 0; k < 6; k++) s[k] = (uint8_t)(xrand(seed) & 0xff);
    }
}

static void check_deblk_bslt4(void)
{
    static const int alphas[] = { 4, 12, 30, 60, 130, 255 };
    static const int betas[]  = { 2, 5, 9, 14, 18 };
    static const uint8_t cliptab[4] = { 0, 1, 3, 9 };
    uint32_t seed = 11;
    unsigned ai, bi;
    for (ai = 0; ai < sizeof alphas / sizeof alphas[0]; ai++) {
        for (bi = 0; bi < sizeof betas / sizeof betas[0]; bi++) {
            int alpha = alphas[ai], beta = betas[bi];
            /* Groups 0..3 get bS 1,2,3,0 - the last exercises the
             * skip-the-whole-group path (bS==0), the other three sweep
             * the rest of the cliptab. */
            uint32_t u4_bs = ((uint32_t)1 << 24) | ((uint32_t)2 << 16) |
                             ((uint32_t)3 << 8) | (uint32_t)0;
            int row, k;

            /* vertical: 16 rows, 6 columns per row (p2..q2); pu1_src at
             * column 3 (q0); src_strd = 6. */
            {
                int src_strd = 6;
                uint8_t buf_m68k[16 * 6], buf_ref[16 * 6], s[6], out[6];
                for (row = 0; row < 16; row++) {
                    int group = row / 4;
                    int bs = (int)((u4_bs >> ((3 - group) * 8)) & 0xff);
                    deblk_bslt4_fill_sample(&seed, row, s);
                    for (k = 0; k < 6; k++) buf_m68k[row * src_strd + k] = s[k];
                    if (bs == 0) {
                        for (k = 0; k < 6; k++) out[k] = s[k];
                    } else {
                        ref_deblk_bslt4_sample(s[0], s[1], s[2], s[3], s[4],
                                              s[5], alpha, beta, cliptab[bs],
                                              out);
                    }
                    for (k = 0; k < 6; k++) buf_ref[row * src_strd + k] = out[k];
                }
                mr_ih264_deblk_luma_vert_bslt4_m68k(buf_m68k + 3, src_strd,
                                                    alpha, beta, u4_bs,
                                                    cliptab);
                for (row = 0; row < 16; row++)
                    for (k = 0; k < 6; k++) {
                        int idx = row * src_strd + k;
                        if (buf_m68k[idx] != buf_ref[idx])
                            report("deblk_vert_bslt4", row, k, buf_ref[idx],
                                   buf_m68k[idx]);
                    }
            }

            /* horizontal: 6 rows, 16 columns; pu1_src at row 3. */
            {
                int src_strd = 16;
                uint8_t buf_m68k[6 * 16], buf_ref[6 * 16], s[6], out[6];
                int col;
                for (col = 0; col < 16; col++) {
                    int group = col / 4;
                    int bs = (int)((u4_bs >> ((3 - group) * 8)) & 0xff);
                    deblk_bslt4_fill_sample(&seed, col, s);
                    for (k = 0; k < 6; k++) buf_m68k[k * src_strd + col] = s[k];
                    if (bs == 0) {
                        for (k = 0; k < 6; k++) out[k] = s[k];
                    } else {
                        ref_deblk_bslt4_sample(s[0], s[1], s[2], s[3], s[4],
                                              s[5], alpha, beta, cliptab[bs],
                                              out);
                    }
                    for (k = 0; k < 6; k++) buf_ref[k * src_strd + col] = out[k];
                }
                mr_ih264_deblk_luma_horz_bslt4_m68k(buf_m68k + 3 * src_strd,
                                                    src_strd, alpha, beta,
                                                    u4_bs, cliptab);
                for (k = 0; k < 6; k++)
                    for (col = 0; col < 16; col++) {
                        int idx = k * src_strd + col;
                        if (buf_m68k[idx] != buf_ref[idx])
                            report("deblk_horz_bslt4", k, col, buf_ref[idx],
                                   buf_m68k[idx]);
                    }
            }
        }
    }
}

/* ---- deblk_chroma_vert/horz_bs4 / bslt4 (H.264 8.7.2.4/8.7.2.3, chroma) */
/* Independent re-derivation of ih264_deblk_chroma_vert_bs4/horz_bs4/
 * vert_bslt4/horz_bslt4 (ih264_deblk_edge_filters.c) - the *_cb/_cr
 * functions actually wired into the codec's function-pointer table, not
 * the single-alpha/beta "_bp" variants also present in that file. cb and
 * cr parameters are deliberately different in every check below so a
 * U/V or cb/cr mixup in the asm would show up as a mismatch. */
static void ref_deblk_chroma_bs4_pixel(uint8_t p1, uint8_t p0, uint8_t q0,
                                       uint8_t q1, int alpha, int beta,
                                       uint8_t out[4] /* p1,p0,q0,q1 */)
{
    out[0] = p1; out[1] = p0; out[2] = q0; out[3] = q1;
    if (abs((int)p0 - (int)q0) < alpha && abs((int)q1 - (int)q0) < beta &&
        abs((int)p1 - (int)p0) < beta) {
        out[1] = (uint8_t)((2 * (int)p1 + (int)p0 + (int)q1 + 2) >> 2);
        out[2] = (uint8_t)((2 * (int)q1 + (int)q0 + (int)p1 + 2) >> 2);
    }
}

static void deblk_chroma_fill_sample(uint32_t *seed, int row, uint8_t s[4])
{
    int k;
    if ((row & 1) == 0) {
        int p0 = 80 + (int)(xrand(seed) % 40);
        s[1] = (uint8_t)p0;
        s[2] = (uint8_t)(p0 + (int)(xrand(seed) % 9) - 4);
        s[0] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
        s[3] = (uint8_t)(p0 + (int)(xrand(seed) % 7) - 3);
    } else {
        for (k = 0; k < 4; k++) s[k] = (uint8_t)(xrand(seed) & 0xff);
    }
}

static void check_deblk_chroma_bs4(void)
{
    static const int cb_alpha[] = { 4, 30, 130 };
    static const int cb_beta[]  = { 2, 9, 18 };
    static const int cr_alpha[] = { 12, 60, 255 };
    static const int cr_beta[]  = { 5, 14, 18 };
    uint32_t seed = 13;
    unsigned i;
    for (i = 0; i < sizeof cb_alpha / sizeof cb_alpha[0]; i++) {
        int alpha_cb = cb_alpha[i], beta_cb = cb_beta[i];
        int alpha_cr = cr_alpha[i], beta_cr = cr_beta[i];
        int row, k;

        /* vertical: 8 rows, 4 columns per row (p1_u,p0_u|... no - U/V
         * interleaved: byte layout per row is p1_u,p1_v,p0_u,p0_v,q0_u,
         * q0_v,q1_u,q1_v; pu1_src at column 4 (q0_u); src_strd = 8. */
        {
            int src_strd = 8;
            uint8_t buf_m68k[8 * 8], buf_ref[8 * 8], su[4], sv[4], out[4];
            for (row = 0; row < 8; row++) {
                deblk_chroma_fill_sample(&seed, row, su);
                deblk_chroma_fill_sample(&seed, row + 1, sv);
                buf_m68k[row * src_strd + 0] = su[0]; /* p1_u */
                buf_m68k[row * src_strd + 1] = sv[0]; /* p1_v */
                buf_m68k[row * src_strd + 2] = su[1]; /* p0_u */
                buf_m68k[row * src_strd + 3] = sv[1]; /* p0_v */
                buf_m68k[row * src_strd + 4] = su[2]; /* q0_u */
                buf_m68k[row * src_strd + 5] = sv[2]; /* q0_v */
                buf_m68k[row * src_strd + 6] = su[3]; /* q1_u */
                buf_m68k[row * src_strd + 7] = sv[3]; /* q1_v */

                ref_deblk_chroma_bs4_pixel(su[0], su[1], su[2], su[3],
                                           alpha_cb, beta_cb, out);
                buf_ref[row * src_strd + 0] = out[0];
                buf_ref[row * src_strd + 2] = out[1];
                buf_ref[row * src_strd + 4] = out[2];
                buf_ref[row * src_strd + 6] = out[3];
                ref_deblk_chroma_bs4_pixel(sv[0], sv[1], sv[2], sv[3],
                                           alpha_cr, beta_cr, out);
                buf_ref[row * src_strd + 1] = out[0];
                buf_ref[row * src_strd + 3] = out[1];
                buf_ref[row * src_strd + 5] = out[2];
                buf_ref[row * src_strd + 7] = out[3];
            }
            mr_ih264_deblk_chroma_vert_bs4_m68k(buf_m68k + 4, src_strd,
                                                alpha_cb, beta_cb, alpha_cr,
                                                beta_cr);
            for (row = 0; row < 8; row++)
                for (k = 0; k < 8; k++) {
                    int idx = row * src_strd + k;
                    if (buf_m68k[idx] != buf_ref[idx])
                        report("deblk_chroma_vert_bs4", row, k,
                               buf_ref[idx], buf_m68k[idx]);
                }
        }

        /* horizontal: 4 rows, 16 columns; pu1_src at row 2. */
        {
            int src_strd = 16;
            uint8_t buf_m68k[4 * 16], buf_ref[4 * 16], su[4], sv[4], out[4];
            int col;
            /* Horizontal chroma still interleaves U/V within each row by
             * column parity (pu1_src / pu1_src+1), same convention as the
             * vertical case's +1-byte-per-row interleave. */
            for (col = 0; col < 16; col += 2) {
                deblk_chroma_fill_sample(&seed, col, su);
                deblk_chroma_fill_sample(&seed, col + 1, sv);
                buf_m68k[0 * src_strd + col] = su[0];     /* p1_u */
                buf_m68k[0 * src_strd + col + 1] = sv[0]; /* p1_v */
                buf_m68k[1 * src_strd + col] = su[1];     /* p0_u */
                buf_m68k[1 * src_strd + col + 1] = sv[1]; /* p0_v */
                buf_m68k[2 * src_strd + col] = su[2];     /* q0_u */
                buf_m68k[2 * src_strd + col + 1] = sv[2]; /* q0_v */
                buf_m68k[3 * src_strd + col] = su[3];     /* q1_u */
                buf_m68k[3 * src_strd + col + 1] = sv[3]; /* q1_v */

                ref_deblk_chroma_bs4_pixel(su[0], su[1], su[2], su[3],
                                           alpha_cb, beta_cb, out);
                buf_ref[0 * src_strd + col] = out[0];
                buf_ref[1 * src_strd + col] = out[1];
                buf_ref[2 * src_strd + col] = out[2];
                buf_ref[3 * src_strd + col] = out[3];
                ref_deblk_chroma_bs4_pixel(sv[0], sv[1], sv[2], sv[3],
                                           alpha_cr, beta_cr, out);
                buf_ref[0 * src_strd + col + 1] = out[0];
                buf_ref[1 * src_strd + col + 1] = out[1];
                buf_ref[2 * src_strd + col + 1] = out[2];
                buf_ref[3 * src_strd + col + 1] = out[3];
            }
            mr_ih264_deblk_chroma_horz_bs4_m68k(buf_m68k + 2 * src_strd,
                                                src_strd, alpha_cb, beta_cb,
                                                alpha_cr, beta_cr);
            for (k = 0; k < 4; k++)
                for (col = 0; col < 16; col++) {
                    int idx = k * src_strd + col;
                    if (buf_m68k[idx] != buf_ref[idx])
                        report("deblk_chroma_horz_bs4", k, col,
                               buf_ref[idx], buf_m68k[idx]);
                }
        }
    }
}

static void ref_deblk_chroma_bslt4_pixel(uint8_t p1, uint8_t p0, uint8_t q0,
                                         uint8_t q1, int alpha, int beta,
                                         int tc0, uint8_t out[4])
{
    out[0] = p1; out[1] = p0; out[2] = q0; out[3] = q1;
    if (abs((int)p0 - (int)q0) < alpha && abs((int)q1 - (int)q0) < beta &&
        abs((int)p1 - (int)p0) < beta) {
        int tc = tc0 + 1;
        int val = (((int)q0 - (int)p0) * 4 + ((int)p1 - (int)q1) + 4) >> 3;
        int delta = val < -tc ? -tc : (val > tc ? tc : val);
        int p0n = (int)p0 + delta, q0n = (int)q0 - delta;
        p0n = p0n < 0 ? 0 : (p0n > 255 ? 255 : p0n);
        q0n = q0n < 0 ? 0 : (q0n > 255 ? 255 : q0n);
        out[1] = (uint8_t)p0n;
        out[2] = (uint8_t)q0n;
    }
}

static void check_deblk_chroma_bslt4(void)
{
    static const int cb_alpha[] = { 12, 60, 130 };
    static const int cb_beta[]  = { 5, 9, 14 };
    static const int cr_alpha[] = { 30, 130, 255 };
    static const int cr_beta[]  = { 2, 14, 18 };
    static const uint8_t cliptab_cb[4] = { 0, 1, 3, 9 };
    static const uint8_t cliptab_cr[4] = { 0, 2, 5, 11 };
    uint32_t seed = 17;
    unsigned i;
    for (i = 0; i < sizeof cb_alpha / sizeof cb_alpha[0]; i++) {
        int alpha_cb = cb_alpha[i], beta_cb = cb_beta[i];
        int alpha_cr = cr_alpha[i], beta_cr = cr_beta[i];
        /* Groups 0..3 get bS 1,2,3,0 - group 3 exercises the
         * skip-the-whole-group path. */
        uint32_t u4_bs = ((uint32_t)1 << 24) | ((uint32_t)2 << 16) |
                         ((uint32_t)3 << 8) | (uint32_t)0;
        int row, k;

        /* vertical: 8 rows, 8 columns (U/V interleaved p1,p0,q0,q1). */
        {
            int src_strd = 8;
            uint8_t buf_m68k[8 * 8], buf_ref[8 * 8], su[4], sv[4], out[4];
            for (row = 0; row < 8; row++) {
                int group = row / 2;
                int bs = (int)((u4_bs >> ((3 - group) * 8)) & 0xff);
                deblk_chroma_fill_sample(&seed, row, su);
                deblk_chroma_fill_sample(&seed, row + 1, sv);
                buf_m68k[row * src_strd + 0] = su[0];
                buf_m68k[row * src_strd + 1] = sv[0];
                buf_m68k[row * src_strd + 2] = su[1];
                buf_m68k[row * src_strd + 3] = sv[1];
                buf_m68k[row * src_strd + 4] = su[2];
                buf_m68k[row * src_strd + 5] = sv[2];
                buf_m68k[row * src_strd + 6] = su[3];
                buf_m68k[row * src_strd + 7] = sv[3];

                if (bs == 0) {
                    for (k = 0; k < 8; k++)
                        buf_ref[row * src_strd + k] = buf_m68k[row * src_strd + k];
                } else {
                    ref_deblk_chroma_bslt4_pixel(su[0], su[1], su[2], su[3],
                                                 alpha_cb, beta_cb,
                                                 cliptab_cb[bs], out);
                    buf_ref[row * src_strd + 0] = out[0];
                    buf_ref[row * src_strd + 2] = out[1];
                    buf_ref[row * src_strd + 4] = out[2];
                    buf_ref[row * src_strd + 6] = out[3];
                    ref_deblk_chroma_bslt4_pixel(sv[0], sv[1], sv[2], sv[3],
                                                 alpha_cr, beta_cr,
                                                 cliptab_cr[bs], out);
                    buf_ref[row * src_strd + 1] = out[0];
                    buf_ref[row * src_strd + 3] = out[1];
                    buf_ref[row * src_strd + 5] = out[2];
                    buf_ref[row * src_strd + 7] = out[3];
                }
            }
            mr_ih264_deblk_chroma_vert_bslt4_m68k(buf_m68k + 4, src_strd,
                                                  alpha_cb, beta_cb,
                                                  alpha_cr, beta_cr, u4_bs,
                                                  cliptab_cb, cliptab_cr);
            for (row = 0; row < 8; row++)
                for (k = 0; k < 8; k++) {
                    int idx = row * src_strd + k;
                    if (buf_m68k[idx] != buf_ref[idx])
                        report("deblk_chroma_vert_bslt4", row, k,
                               buf_ref[idx], buf_m68k[idx]);
                }
        }

        /* horizontal: 4 rows, 16 columns (U/V interleaved by column). */
        {
            int src_strd = 16;
            uint8_t buf_m68k[4 * 16], buf_ref[4 * 16], su[4], sv[4], out[4];
            int col;
            for (col = 0; col < 16; col += 2) {
                int group = (col / 2) / 2;
                int bs = (int)((u4_bs >> ((3 - group) * 8)) & 0xff);
                deblk_chroma_fill_sample(&seed, col, su);
                deblk_chroma_fill_sample(&seed, col + 1, sv);
                buf_m68k[0 * src_strd + col] = su[0];
                buf_m68k[0 * src_strd + col + 1] = sv[0];
                buf_m68k[1 * src_strd + col] = su[1];
                buf_m68k[1 * src_strd + col + 1] = sv[1];
                buf_m68k[2 * src_strd + col] = su[2];
                buf_m68k[2 * src_strd + col + 1] = sv[2];
                buf_m68k[3 * src_strd + col] = su[3];
                buf_m68k[3 * src_strd + col + 1] = sv[3];

                if (bs == 0) {
                    for (k = 0; k < 4; k++) {
                        buf_ref[k * src_strd + col] = buf_m68k[k * src_strd + col];
                        buf_ref[k * src_strd + col + 1] =
                            buf_m68k[k * src_strd + col + 1];
                    }
                } else {
                    ref_deblk_chroma_bslt4_pixel(su[0], su[1], su[2], su[3],
                                                 alpha_cb, beta_cb,
                                                 cliptab_cb[bs], out);
                    buf_ref[0 * src_strd + col] = out[0];
                    buf_ref[1 * src_strd + col] = out[1];
                    buf_ref[2 * src_strd + col] = out[2];
                    buf_ref[3 * src_strd + col] = out[3];
                    ref_deblk_chroma_bslt4_pixel(sv[0], sv[1], sv[2], sv[3],
                                                 alpha_cr, beta_cr,
                                                 cliptab_cr[bs], out);
                    buf_ref[0 * src_strd + col + 1] = out[0];
                    buf_ref[1 * src_strd + col + 1] = out[1];
                    buf_ref[2 * src_strd + col + 1] = out[2];
                    buf_ref[3 * src_strd + col + 1] = out[3];
                }
            }
            mr_ih264_deblk_chroma_horz_bslt4_m68k(buf_m68k + 2 * src_strd,
                                                  src_strd, alpha_cb, beta_cb,
                                                  alpha_cr, beta_cr, u4_bs,
                                                  cliptab_cb, cliptab_cr);
            for (k = 0; k < 4; k++)
                for (col = 0; col < 16; col++) {
                    int idx = k * src_strd + col;
                    if (buf_m68k[idx] != buf_ref[idx])
                        report("deblk_chroma_horz_bslt4", k, col,
                               buf_ref[idx], buf_m68k[idx]);
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
    check_interp_qpel();
    check_interp_hpel_hpel();
    check_deblk_bs4();
    check_deblk_bslt4();
    check_deblk_chroma_bs4();
    check_deblk_chroma_bslt4();
#endif

    if (g_failures) {
        fprintf(stderr, "mr_h264_m68k_check: %d mismatches\n", g_failures);
        return 1;
    }
    printf("mr_h264_m68k_check: OK\n");
    return 0;
}
