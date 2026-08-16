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

/* ---- intra 16x16 DC (8.3.3.3) ------------------------------------------ */
static void ref_intra16_dc(const uint8_t *src, uint8_t *dst, int dst_strd,
                           int neighbour_available)
{
    const uint8_t *top = src + 17;
    const uint8_t *left = src + 15;
    int use_left = neighbour_available & 0x01;
    int use_top = (neighbour_available >> 2) & 0x01;
    int val = 0, row, col, y;
    if (use_left) {
        for (y = 0; y < 16; y++) val += left[-y];
        val += 8;
    }
    if (use_top) {
        for (y = 0; y < 16; y++) val += top[y];
        val += 8;
    }
    val = val ? (val >> (3 + use_left + use_top)) : 128;
    for (row = 0; row < 16; row++)
        for (col = 0; col < 16; col++)
            dst[row * dst_strd + col] = (uint8_t)val;
}

static void check_intra16_dc(void)
{
    static const int avail[] = { 0x00, 0x01, 0x04, 0x05 };
    uint32_t seed = 5;
    int trial, ai;
    for (trial = 0; trial < 4; trial++) {
        uint8_t nbr[40];
        fill_random(nbr, sizeof nbr, &seed);
        for (ai = 0; ai < 4; ai++) {
            uint8_t dst_m68k[16 * 20], dst_ref[16 * 20];
            int dst_strd = 20, row, col;
            memset(dst_m68k, 0xAA, sizeof dst_m68k);
            memset(dst_ref, 0xAA, sizeof dst_ref);
            mr_ih264_intra_pred_luma_16x16_dc_m68k(nbr, dst_m68k, 0, dst_strd,
                                                    avail[ai]);
            ref_intra16_dc(nbr, dst_ref, dst_strd, avail[ai]);
            for (row = 0; row < 16; row++)
                for (col = 0; col < 16; col++) {
                    int idx = row * dst_strd + col;
                    if (dst_m68k[idx] != dst_ref[idx])
                        report("intra16_dc", row, col, dst_ref[idx],
                               dst_m68k[idx]);
                }
        }
    }
}

/* ---- intra 4x4 vertical / horizontal / DC (8.3.1.2.1/2/3) -------------- */
/* Same neighbour-buffer convention as intra16 above, just BLK_SIZE=4:
 * left column at src[0..3], corner at src[4], top row at src[5..8]. */
static void ref_intra4_vert(const uint8_t *src, uint8_t *dst, int dst_strd)
{
    const uint8_t *top = src + 5;
    int row, col;
    for (row = 0; row < 4; row++)
        for (col = 0; col < 4; col++)
            dst[row * dst_strd + col] = top[col];
}

static void ref_intra4_horz(const uint8_t *src, uint8_t *dst, int dst_strd)
{
    int row, col;
    for (row = 0; row < 4; row++) {
        uint8_t left = src[3 - row];
        for (col = 0; col < 4; col++)
            dst[row * dst_strd + col] = left;
    }
}

static void ref_intra4_dc(const uint8_t *src, uint8_t *dst, int dst_strd,
                          int neighbour_available)
{
    const uint8_t *top = src + 5;
    const uint8_t *left = src + 3;
    int use_left = neighbour_available & 0x01;
    int use_top = (neighbour_available >> 2) & 0x01;
    int val = 0, row, col;
    if (use_left)
        val += left[0] + left[-1] + left[-2] + left[-3] + 2;
    if (use_top)
        val += top[0] + top[1] + top[2] + top[3] + 2;
    val = val ? (val >> (1 + use_left + use_top)) : 128;
    for (row = 0; row < 4; row++)
        for (col = 0; col < 4; col++)
            dst[row * dst_strd + col] = (uint8_t)val;
}

static void check_intra4(void)
{
    static const int avail[] = { 0x00, 0x01, 0x04, 0x05 };
    uint32_t seed = 12;
    int trial, ai;
    for (trial = 0; trial < 8; trial++) {
        uint8_t nbr[16];
        uint8_t dst_m68k[4 * 8], dst_ref[4 * 8];
        int dst_strd = 8, row, col;

        fill_random(nbr, sizeof nbr, &seed);

        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_intra_pred_luma_4x4_vert_m68k(nbr, dst_m68k, 0, dst_strd, 0);
        ref_intra4_vert(nbr, dst_ref, dst_strd);
        for (row = 0; row < 4; row++)
            for (col = 0; col < 4; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("intra4_vert", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }

        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_intra_pred_luma_4x4_horz_m68k(nbr, dst_m68k, 0, dst_strd, 0);
        ref_intra4_horz(nbr, dst_ref, dst_strd);
        for (row = 0; row < 4; row++)
            for (col = 0; col < 4; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("intra4_horz", row, col, dst_ref[idx],
                           dst_m68k[idx]);
            }

        for (ai = 0; ai < 4; ai++) {
            memset(dst_m68k, 0xAA, sizeof dst_m68k);
            memset(dst_ref, 0xAA, sizeof dst_ref);
            mr_ih264_intra_pred_luma_4x4_dc_m68k(nbr, dst_m68k, 0, dst_strd,
                                                  avail[ai]);
            ref_intra4_dc(nbr, dst_ref, dst_strd, avail[ai]);
            for (row = 0; row < 4; row++)
                for (col = 0; col < 4; col++) {
                    int idx = row * dst_strd + col;
                    if (dst_m68k[idx] != dst_ref[idx])
                        report("intra4_dc", row, col, dst_ref[idx],
                               dst_m68k[idx]);
                }
        }
    }
}

/* ---- inter_pred_luma_horz / vert (H.264 8.4.2.2.1, 6-tap half-pel) ----- */
/* mr_ih264_inter_pred_luma_horz_m68k/vert_m68k (ih264_m68k_interp.S) only
 * exist on an m68k build (see ih264_m68k_optim.h) - the hand-written .S is
 * guarded on MR_M68K_ASM so it preprocesses to an empty translation unit
 * everywhere else. These asm-only checks cannot run on the host build;
 * `make check-m68k` (real m68k, big-endian, under qemu-m68k) is what
 * exercises them. */
#if defined(MR_M68K_ASM)
/* ---- explicit weighted prediction (H.264 8.4.2.3.2) ------------------- */
static int32_t ref_asr(int32_t value, unsigned shift)
{
    int64_t magnitude;
    if (!shift || value >= 0) return value >> shift;
    magnitude = -(int64_t)value;
    return (int32_t)-((magnitude + (((int64_t)1 << shift) - 1)) >> shift);
}

static uint8_t ref_clip_u8(int32_t value)
{
    return (uint8_t)(value < 0 ? 0 : value > 255 ? 255 : value);
}

static int16_t test_rand_s16(uint32_t *seed)
{
    uint32_t value = xrand(seed) | (xrand(seed) << 15);
    return (int16_t)value;
}

static int8_t test_rand_s8(uint32_t *seed)
{
    return (int8_t)xrand(seed);
}

static int32_t pack_weights(int16_t u, int16_t v)
{
    return (int32_t)(((uint32_t)(uint16_t)v << 16) | (uint16_t)u);
}

static int32_t pack_offsets(int8_t u, int8_t v)
{
    return (int32_t)(((uint32_t)(uint8_t)v << 8) | (uint8_t)u);
}

static void ref_explicit_weight_luma(const uint8_t *src, uint8_t *dst,
                                     int src_strd, int dst_strd, int log_wd,
                                     int16_t wt, int8_t ofst, int ht, int wd)
{
    int bias = ofst;
    int row, col;
    if (log_wd >= 1)
        bias = (1 << (log_wd - 1)) + ofst * (1 << log_wd);
    for (row = 0; row < ht; row++)
        for (col = 0; col < wd; col++) {
            int value = wt * src[row * src_strd + col] + bias;
            if (log_wd >= 1) value = ref_asr(value, (unsigned)log_wd);
            dst[row * dst_strd + col] = ref_clip_u8(value);
        }
}

static void ref_explicit_weight_chroma(const uint8_t *src, uint8_t *dst,
                                       int src_strd, int dst_strd, int log_wd,
                                       int16_t wt_u, int16_t wt_v,
                                       int8_t ofst_u, int8_t ofst_v,
                                       int ht, int wd)
{
    int bias_u = ofst_u, bias_v = ofst_v;
    int row, pair;
    if (log_wd >= 1) {
        int rounding = 1 << (log_wd - 1);
        bias_u = rounding + ofst_u * (1 << log_wd);
        bias_v = rounding + ofst_v * (1 << log_wd);
    }
    for (row = 0; row < ht; row++)
        for (pair = 0; pair < wd; pair++) {
            int idx = row * src_strd + 2 * pair;
            int out = row * dst_strd + 2 * pair;
            int u = wt_u * src[idx] + bias_u;
            int v = wt_v * src[idx + 1] + bias_v;
            if (log_wd >= 1) {
                u = ref_asr(u, (unsigned)log_wd);
                v = ref_asr(v, (unsigned)log_wd);
            }
            dst[out] = ref_clip_u8(u);
            dst[out + 1] = ref_clip_u8(v);
        }
}

static int ref_offset_average(int8_t a, int8_t b)
{
    return ref_asr((int32_t)a + b + 1, 1);
}

static void ref_explicit_bi_weight_luma(const uint8_t *src1,
                                        const uint8_t *src2, uint8_t *dst,
                                        int src_strd1, int src_strd2,
                                        int dst_strd, int log_wd,
                                        int16_t wt1, int16_t wt2,
                                        int8_t ofst1, int8_t ofst2,
                                        int ht, int wd)
{
    int shift = log_wd + 1;
    int bias = (1 << log_wd) +
               ref_offset_average(ofst1, ofst2) * (1 << shift);
    int row, col;
    for (row = 0; row < ht; row++)
        for (col = 0; col < wd; col++) {
            int value = wt1 * src1[row * src_strd1 + col] +
                        wt2 * src2[row * src_strd2 + col] + bias;
            dst[row * dst_strd + col] =
                ref_clip_u8(ref_asr(value, (unsigned)shift));
        }
}

static void ref_explicit_bi_weight_chroma(
    const uint8_t *src1, const uint8_t *src2, uint8_t *dst,
    int src_strd1, int src_strd2, int dst_strd, int log_wd,
    int16_t wt1_u, int16_t wt1_v, int16_t wt2_u, int16_t wt2_v,
    int8_t ofst1_u, int8_t ofst1_v, int8_t ofst2_u, int8_t ofst2_v,
    int ht, int wd)
{
    int shift = log_wd + 1;
    int bias_u = (1 << log_wd) +
        ref_offset_average(ofst1_u, ofst2_u) * (1 << shift);
    int bias_v = (1 << log_wd) +
        ref_offset_average(ofst1_v, ofst2_v) * (1 << shift);
    int row, pair;
    for (row = 0; row < ht; row++)
        for (pair = 0; pair < wd; pair++) {
            int i1 = row * src_strd1 + 2 * pair;
            int i2 = row * src_strd2 + 2 * pair;
            int out = row * dst_strd + 2 * pair;
            int u = wt1_u * src1[i1] + wt2_u * src2[i2] + bias_u;
            int v = wt1_v * src1[i1 + 1] + wt2_v * src2[i2 + 1] + bias_v;
            dst[out] = ref_clip_u8(ref_asr(u, (unsigned)shift));
            dst[out + 1] = ref_clip_u8(ref_asr(v, (unsigned)shift));
        }
}

static void report_buffer_mismatch(const char *name, const uint8_t *ref,
                                   const uint8_t *got, size_t size,
                                   int stride)
{
    size_t i;
    for (i = 0; i < size; i++)
        if (ref[i] != got[i]) {
            report(name, (int)(i / (size_t)stride),
                   (int)(i % (size_t)stride), ref[i], got[i]);
            return;
        }
}

static void check_explicit_weighted_pred(void)
{
    static const uint8_t luma_wd[] = { 4, 8, 4, 8, 16, 8, 16 };
    static const uint8_t luma_ht[] = { 4, 4, 8, 8, 8, 16, 16 };
    static const uint8_t chroma_wd[] = { 2, 4, 2, 4, 8, 4, 8 };
    static const uint8_t chroma_ht[] = { 2, 2, 4, 4, 4, 8, 8 };
    uint32_t seed = 0x57454947u;
    unsigned size_i, log_wd, iter;

    for (size_i = 0; size_i < sizeof luma_wd; size_i++)
        for (log_wd = 0; log_wd <= 7; log_wd++)
            for (iter = 0; iter < 10; iter++) {
                enum { BUF_SIZE = 16 * 32 };
                uint8_t src1[BUF_SIZE], src2[BUF_SIZE];
                uint8_t dst_ref[BUF_SIZE], dst_asm[BUF_SIZE];
                int wd = luma_wd[size_i], ht = luma_ht[size_i];
                int strd1 = wd + 5, strd2 = wd + 9, dst_strd = wd + 13;
                int16_t wt1 = iter == 0 ? INT16_MAX :
                              iter == 1 ? INT16_MIN : test_rand_s16(&seed);
                int16_t wt2 = iter == 0 ? INT16_MIN :
                              iter == 1 ? INT16_MAX : test_rand_s16(&seed);
                int8_t ofst1 = iter == 0 ? INT8_MIN :
                               iter == 1 ? INT8_MAX : test_rand_s8(&seed);
                int8_t ofst2 = iter == 0 ? INT8_MAX :
                               iter == 1 ? INT8_MIN : test_rand_s8(&seed);

                fill_random(src1, sizeof src1, &seed);
                fill_random(src2, sizeof src2, &seed);
                if (iter == 0) memset(src1, 0, sizeof src1);
                if (iter == 1) memset(src1, 255, sizeof src1);

                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(dst_asm, 0xAA, sizeof dst_asm);
                ref_explicit_weight_luma(src1, dst_ref, strd1, dst_strd,
                                         (int)log_wd, wt1, ofst1, ht, wd);
                mr_ih264_weighted_pred_luma_m68k(
                    src1, dst_asm, strd1, dst_strd, (int)log_wd, wt1,
                    ofst1, ht, wd);
                report_buffer_mismatch("explicit_weight_luma", dst_ref,
                                       dst_asm, sizeof dst_ref, dst_strd);

                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(dst_asm, 0xAA, sizeof dst_asm);
                ref_explicit_bi_weight_luma(
                    src1, src2, dst_ref, strd1, strd2, dst_strd,
                    (int)log_wd, wt1, wt2, ofst1, ofst2, ht, wd);
                mr_ih264_weighted_bi_pred_luma_m68k(
                    src1, src2, dst_asm, strd1, strd2, dst_strd,
                    (int)log_wd, wt1, wt2, ofst1, ofst2, ht, wd);
                report_buffer_mismatch("explicit_bi_weight_luma", dst_ref,
                                       dst_asm, sizeof dst_ref, dst_strd);
            }

    for (size_i = 0; size_i < sizeof chroma_wd; size_i++)
        for (log_wd = 0; log_wd <= 7; log_wd++)
            for (iter = 0; iter < 10; iter++) {
                enum { BUF_SIZE = 16 * 32 };
                uint8_t src1[BUF_SIZE], src2[BUF_SIZE];
                uint8_t dst_ref[BUF_SIZE], dst_asm[BUF_SIZE];
                int wd = chroma_wd[size_i], ht = chroma_ht[size_i];
                int bytes = 2 * wd;
                int strd1 = bytes + 4, strd2 = bytes + 8;
                int dst_strd = bytes + 12;
                int16_t wt1_u = iter == 0 ? INT16_MAX : test_rand_s16(&seed);
                int16_t wt1_v = iter == 1 ? INT16_MIN : test_rand_s16(&seed);
                int16_t wt2_u = iter == 0 ? INT16_MIN : test_rand_s16(&seed);
                int16_t wt2_v = iter == 1 ? INT16_MAX : test_rand_s16(&seed);
                int8_t o1u = iter == 0 ? INT8_MIN : test_rand_s8(&seed);
                int8_t o1v = iter == 1 ? INT8_MAX : test_rand_s8(&seed);
                int8_t o2u = iter == 0 ? INT8_MAX : test_rand_s8(&seed);
                int8_t o2v = iter == 1 ? INT8_MIN : test_rand_s8(&seed);
                int32_t wt1 = pack_weights(wt1_u, wt1_v);
                int32_t wt2 = pack_weights(wt2_u, wt2_v);
                int32_t ofst1 = pack_offsets(o1u, o1v);
                int32_t ofst2 = pack_offsets(o2u, o2v);

                fill_random(src1, sizeof src1, &seed);
                fill_random(src2, sizeof src2, &seed);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(dst_asm, 0xAA, sizeof dst_asm);
                ref_explicit_weight_chroma(
                    src1, dst_ref, strd1, dst_strd, (int)log_wd, wt1_u,
                    wt1_v, o1u, o1v, ht, wd);
                mr_ih264_weighted_pred_chroma_m68k(
                    src1, dst_asm, strd1, dst_strd, (int)log_wd, wt1,
                    ofst1, ht, wd);
                report_buffer_mismatch("explicit_weight_chroma", dst_ref,
                                       dst_asm, sizeof dst_ref, dst_strd);

                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(dst_asm, 0xAA, sizeof dst_asm);
                ref_explicit_bi_weight_chroma(
                    src1, src2, dst_ref, strd1, strd2, dst_strd,
                    (int)log_wd, wt1_u, wt1_v, wt2_u, wt2_v,
                    o1u, o1v, o2u, o2v, ht, wd);
                mr_ih264_weighted_bi_pred_chroma_m68k(
                    src1, src2, dst_asm, strd1, strd2, dst_strd,
                    (int)log_wd, wt1, wt2, ofst1, ofst2, ht, wd);
                report_buffer_mismatch("explicit_bi_weight_chroma", dst_ref,
                                       dst_asm, sizeof dst_ref, dst_strd);
            }
}

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
/* ---- inter_pred_luma_horz_qpel_vert_hpel (dydx 9/11) -------------------- */
/* Independent re-derivation of ih264_inter_pred_luma_horz_qpel_vert_hpel:
 * stages 1+2 are the same vertical-then-horizontal raw/rounded 6-tap
 * pipeline as ref_interp_hpel_hpel above (the H(1/2,1/2) diagonal half-pel
 * pixel, into dst). Stage 3 re-walks stage 1's raw vertical-6-tap buffer at
 * column offset 2 + ((dydx&3)>>1) (the "+2" is the column-(-2) buffer base,
 * matching the vendored pi2_pred1 = pu1_tmp + 2 WORD16s), applies the
 * single-tap (+16)>>5 round+clip, and averages with the H(1/2,1/2) pixel
 * already in dst. */
static void ref_interp_horz_qpel_vert_hpel(const uint8_t *src, uint8_t *dst,
                                           int src_strd, int dst_strd, int ht,
                                           int wd, int dydx)
{
    int32_t tmp[16 * 21]; /* row pitch (wd+5), raw vertical 6-tap sums */
    int xo = (dydx & 3) >> 1; /* 0 or 1 */
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = -2; col < wd + 3; col++) {
            int32_t s = 1 * (src[col - 2 * src_strd] + src[col + 3 * src_strd])
                       - 5 * (src[col - 1 * src_strd] + src[col + 2 * src_strd])
                       + 20 * (src[col] + src[col + 1 * src_strd]);
            tmp[row * (wd + 5) + (col + 2)] = s;
        }
        src += src_strd;
    }
    for (row = 0; row < ht; row++) {
        const int32_t *p = &tmp[row * (wd + 5) + 2];
        for (col = 0; col < wd; col++) {
            int32_t t = 1 * (p[col - 2] + p[col + 3])
                       - 5 * (p[col - 1] + p[col + 2])
                       + 20 * (p[col] + p[col + 1]);
            t = (t + 512) >> 10;
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            dst[row * dst_strd + col] = (uint8_t)t;
        }
    }
    for (row = 0; row < ht; row++) {
        const int32_t *p = &tmp[row * (wd + 5) + 2 + xo];
        for (col = 0; col < wd; col++) {
            int32_t v = (p[col] + 16) >> 5;
            int32_t d;
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            d = dst[row * dst_strd + col];
            dst[row * dst_strd + col] = (uint8_t)((d + v + 1) >> 1);
        }
    }
}

static void check_interp_horz_qpel_vert_hpel(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    static const int dydxs[] = { 9, 11 };
    uint32_t seed = 9;
    unsigned i, di;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        for (di = 0; di < 2; di++) {
            int extreme;
            for (extreme = 0; extreme < 2; extreme++) {
                int wd = wds[i], ht = hts[i];
                int pad = 5, src_strd = wd + 2 * pad, dst_strd = wd + 5;
                uint8_t src[26 * 26], dst_m68k[16 * 21], dst_ref[16 * 21];
                uint8_t tmp_m68k[(16 + 5) * 16 * 2]; /* WORD16 scratch */
                const uint8_t *src0 = src + pad * src_strd + pad;
                int row, col, j;
                for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                    src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                     : (uint8_t)(xrand(&seed) & 0xff);

                memset(dst_m68k, 0xAA, sizeof dst_m68k);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(tmp_m68k, 0, sizeof tmp_m68k);
                mr_ih264_inter_pred_luma_horz_qpel_vert_hpel_m68k(
                    (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                    tmp_m68k, dydxs[di]);
                ref_interp_horz_qpel_vert_hpel(src0, dst_ref, src_strd,
                                               dst_strd, ht, wd, dydxs[di]);
                for (row = 0; row < ht; row++)
                    for (col = 0; col < wd; col++) {
                        int idx = row * dst_strd + col;
                        if (dst_m68k[idx] != dst_ref[idx])
                            report("interp_horz_qpel_vert_hpel", row, col,
                                   dst_ref[idx], dst_m68k[idx]);
                    }
            }
        }
    }
}

/* ---- inter_pred_luma_horz_hpel_vert_qpel (dydx 6/14) -------------------- */
/* Independent re-derivation of ih264_inter_pred_luma_horz_hpel_vert_qpel:
 * the row/column transpose of horz_qpel_vert_hpel above. Stage 1 is a
 * horizontal raw 6-tap run once per source row for ht+5 rows (-2..ht+2),
 * into a WORD16 buffer with row pitch wd (no column padding - the padding
 * is extra *rows* this time). Stage 2 is the vertical 6-tap over those
 * intermediates -> H(1/2,1/2) into dst. Stage 3 re-walks stage 1's raw
 * buffer at row offset 2 + ((dydx>>2 & 3)>>1) (again "+2" is the row -2
 * buffer base), applies (+16)>>5 round+clip, and averages with dst. */
static void ref_interp_horz_hpel_vert_qpel(const uint8_t *src, uint8_t *dst,
                                           int src_strd, int dst_strd, int ht,
                                           int wd, int dydx)
{
    int32_t tmp[(16 + 5) * 16]; /* (ht+5) rows x wd cols, raw horiz 6-tap */
    int yo = ((dydx >> 2) & 3) >> 1; /* 0 or 1 */
    const uint8_t *s = src - 2 * src_strd;
    int row, col, k;
    for (k = 0; k < ht + 5; k++) {
        for (col = 0; col < wd; col++) {
            int32_t v = 1 * (s[col - 2] + s[col + 3])
                       - 5 * (s[col - 1] + s[col + 2])
                       + 20 * (s[col] + s[col + 1]);
            tmp[k * wd + col] = v;
        }
        s += src_strd;
    }
    for (row = 0; row < ht; row++) {
        const int32_t *p = &tmp[(row + 2) * wd];
        for (col = 0; col < wd; col++) {
            int32_t t = 1 * (p[col - 2 * wd] + p[col + 3 * wd])
                       - 5 * (p[col - 1 * wd] + p[col + 2 * wd])
                       + 20 * (p[col] + p[col + 1 * wd]);
            t = (t + 512) >> 10;
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            dst[row * dst_strd + col] = (uint8_t)t;
        }
    }
    for (row = 0; row < ht; row++) {
        const int32_t *p = &tmp[(row + 2 + yo) * wd];
        for (col = 0; col < wd; col++) {
            int32_t v = (p[col] + 16) >> 5;
            int32_t d;
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            d = dst[row * dst_strd + col];
            dst[row * dst_strd + col] = (uint8_t)((d + v + 1) >> 1);
        }
    }
}

static void check_interp_horz_hpel_vert_qpel(void)
{
    static const int wds[] = { 4, 4, 8, 8, 16, 16 };
    static const int hts[] = { 4, 8, 4, 16, 8, 16 };
    static const int dydxs[] = { 6, 14 };
    uint32_t seed = 10;
    unsigned i, di;
    for (i = 0; i < sizeof wds / sizeof wds[0]; i++) {
        for (di = 0; di < 2; di++) {
            int extreme;
            for (extreme = 0; extreme < 2; extreme++) {
                int wd = wds[i], ht = hts[i];
                int pad = 5, src_strd = wd + 2 * pad, dst_strd = wd + 5;
                uint8_t src[26 * 26], dst_m68k[16 * 21], dst_ref[16 * 21];
                uint8_t tmp_m68k[(16 + 5) * 16 * 2]; /* WORD16 scratch */
                const uint8_t *src0 = src + pad * src_strd + pad;
                int row, col, j;
                for (j = 0; j < src_strd * (ht + 2 * pad); j++)
                    src[j] = extreme ? (uint8_t)((xrand(&seed) & 1) ? 255 : 0)
                                     : (uint8_t)(xrand(&seed) & 0xff);

                memset(dst_m68k, 0xAA, sizeof dst_m68k);
                memset(dst_ref, 0xAA, sizeof dst_ref);
                memset(tmp_m68k, 0, sizeof tmp_m68k);
                mr_ih264_inter_pred_luma_horz_hpel_vert_qpel_m68k(
                    (uint8_t *)src0, dst_m68k, src_strd, dst_strd, ht, wd,
                    tmp_m68k, dydxs[di]);
                ref_interp_horz_hpel_vert_qpel(src0, dst_ref, src_strd,
                                               dst_strd, ht, wd, dydxs[di]);
                for (row = 0; row < ht; row++)
                    for (col = 0; col < wd; col++) {
                        int idx = row * dst_strd + col;
                        if (dst_m68k[idx] != dst_ref[idx])
                            report("interp_horz_hpel_vert_qpel", row, col,
                                   dst_ref[idx], dst_m68k[idx]);
                    }
            }
        }
    }
}

/* ---- iquant_itrans_recon_4x4 / _dc / _chroma_4x4 / _chroma_4x4_dc ------ */
/* Independent re-derivation of ih264_iquant_itrans_recon_4x4[_dc] and
 * ih264_iquant_itrans_recon_chroma_4x4[_dc] (ih264_iquant_itrans_recon.c),
 * including the INV_QUANT macro's own arithmetic (ih264_trans_macros.h):
 * value = ((raw*iscal*weight + rnd) << qp_div_6) >> 4, rnd = qp_div_6 < 4 ?
 * 1<<(3-qp_div_6) : 0. q0..q3 stay WORD32 (untruncated - x2/x3 shift the
 * *full* dequantised value); x0..x3 and i_macro are WORD16, truncating on
 * assignment exactly like the vendored C, which matters once a random
 * fuzz input drives q0+q2 etc. past 16 bits. */
static WORD32 ref_inv_quant(WORD16 raw, UWORD16 iscal, UWORD16 weight,
                            UWORD32 qp_div_6)
{
    WORD32 rnd = (qp_div_6 < 4) ? (WORD32)(1 << (3 - qp_div_6)) : 0;
    WORD32 v = (WORD32)raw;
    v *= (WORD32)iscal;
    v *= (WORD32)weight;
    v += rnd;
    v <<= qp_div_6;
    v >>= 4;
    return v;
}

static void ref_iquant_itrans_recon_4x4(const WORD16 *pi2_src,
                                        const UWORD8 *pu1_pred,
                                        UWORD8 *pu1_out, int pred_strd,
                                        int out_strd, const UWORD16 *iscal,
                                        const UWORD16 *weigh,
                                        UWORD32 qp_div_6, int iq_start_idx,
                                        const WORD16 *pi2_dc_ld_addr)
{
    WORD16 tmp[16];
    int i;
    for (i = 0; i < 4; i++) {
        const WORD16 *s = pi2_src + i * 4;
        const UWORD16 *ic = iscal + i * 4;
        const UWORD16 *we = weigh + i * 4;
        WORD32 q0 = ref_inv_quant(s[0], ic[0], we[0], qp_div_6);
        WORD32 q2, q1, q3;
        WORD16 x0, x1, x2, x3;
        if (i == 0 && iq_start_idx == 1) q0 = pi2_dc_ld_addr[0];
        q2 = ref_inv_quant(s[2], ic[2], we[2], qp_div_6);
        x0 = (WORD16)(q0 + q2);
        x1 = (WORD16)(q0 - q2);
        q1 = ref_inv_quant(s[1], ic[1], we[1], qp_div_6);
        q3 = ref_inv_quant(s[3], ic[3], we[3], qp_div_6);
        x2 = (WORD16)((q1 >> 1) - q3);
        x3 = (WORD16)(q1 + (q3 >> 1));
        tmp[i * 4 + 0] = (WORD16)(x0 + x3);
        tmp[i * 4 + 1] = (WORD16)(x1 + x2);
        tmp[i * 4 + 2] = (WORD16)(x1 - x2);
        tmp[i * 4 + 3] = (WORD16)(x0 - x3);
    }
    for (i = 0; i < 4; i++) {
        const UWORD8 *pred = pu1_pred + i;
        UWORD8 *out = pu1_out + i;
        WORD16 x0 = (WORD16)(tmp[0 * 4 + i] + tmp[2 * 4 + i]);
        WORD16 x1 = (WORD16)(tmp[0 * 4 + i] - tmp[2 * 4 + i]);
        WORD16 x2 = (WORD16)((tmp[1 * 4 + i] >> 1) - tmp[3 * 4 + i]);
        WORD16 x3 = (WORD16)(tmp[1 * 4 + i] + (tmp[3 * 4 + i] >> 1));
        int vals[4], r;
        vals[0] = x0 + x3; vals[1] = x1 + x2;
        vals[2] = x1 - x2; vals[3] = x0 - x3;
        for (r = 0; r < 4; r++) {
            WORD16 im = (WORD16)vals[r];
            int t = ((int)im + 32) >> 6;
            t += pred[r * pred_strd];
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            out[r * out_strd] = (UWORD8)t;
        }
    }
}

static void ref_iquant_itrans_recon_4x4_dc(const WORD16 *pi2_src,
                                           const UWORD8 *pu1_pred,
                                           UWORD8 *pu1_out, int pred_strd,
                                           int out_strd, const UWORD16 *iscal,
                                           const UWORD16 *weigh,
                                           UWORD32 qp_div_6, int iq_start_idx,
                                           const WORD16 *pi2_dc_ld_addr)
{
    WORD32 q0 = (iq_start_idx == 0)
                    ? ref_inv_quant(pi2_src[0], iscal[0], weigh[0], qp_div_6)
                    : pi2_dc_ld_addr[0];
    WORD16 i_macro = (WORD16)((q0 + 32) >> 6);
    int col, row;
    for (col = 0; col < 4; col++)
        for (row = 0; row < 4; row++) {
            int t = (int)i_macro + pu1_pred[col + row * pred_strd];
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            pu1_out[col + row * out_strd] = (UWORD8)t;
        }
}

static void ref_iquant_itrans_recon_chroma_4x4(const WORD16 *pi2_src,
                                               const UWORD8 *pu1_pred,
                                               UWORD8 *pu1_out, int pred_strd,
                                               int out_strd,
                                               const UWORD16 *iscal,
                                               const UWORD16 *weigh,
                                               UWORD32 qp_div_6,
                                               const WORD16 *pi2_dc_src)
{
    WORD16 tmp[16];
    int i;
    for (i = 0; i < 4; i++) {
        const WORD16 *s = pi2_src + i * 4;
        const UWORD16 *ic = iscal + i * 4;
        const UWORD16 *we = weigh + i * 4;
        WORD32 q0 = (i == 0) ? pi2_dc_src[0]
                              : ref_inv_quant(s[0], ic[0], we[0], qp_div_6);
        WORD32 q2, q1, q3;
        WORD16 x0, x1, x2, x3;
        q2 = ref_inv_quant(s[2], ic[2], we[2], qp_div_6);
        x0 = (WORD16)(q0 + q2);
        x1 = (WORD16)(q0 - q2);
        q1 = ref_inv_quant(s[1], ic[1], we[1], qp_div_6);
        q3 = ref_inv_quant(s[3], ic[3], we[3], qp_div_6);
        x2 = (WORD16)((q1 >> 1) - q3);
        x3 = (WORD16)(q1 + (q3 >> 1));
        tmp[i * 4 + 0] = (WORD16)(x0 + x3);
        tmp[i * 4 + 1] = (WORD16)(x1 + x2);
        tmp[i * 4 + 2] = (WORD16)(x1 - x2);
        tmp[i * 4 + 3] = (WORD16)(x0 - x3);
    }
    for (i = 0; i < 4; i++) {
        const UWORD8 *pred = pu1_pred + i * 2;
        UWORD8 *out = pu1_out + i * 2;
        WORD16 x0 = (WORD16)(tmp[0 * 4 + i] + tmp[2 * 4 + i]);
        WORD16 x1 = (WORD16)(tmp[0 * 4 + i] - tmp[2 * 4 + i]);
        WORD16 x2 = (WORD16)((tmp[1 * 4 + i] >> 1) - tmp[3 * 4 + i]);
        WORD16 x3 = (WORD16)(tmp[1 * 4 + i] + (tmp[3 * 4 + i] >> 1));
        int vals[4], r;
        vals[0] = x0 + x3; vals[1] = x1 + x2;
        vals[2] = x1 - x2; vals[3] = x0 - x3;
        for (r = 0; r < 4; r++) {
            WORD16 im = (WORD16)vals[r];
            int t = ((int)im + 32) >> 6;
            t += pred[r * pred_strd];
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            out[r * out_strd] = (UWORD8)t;
        }
    }
}

static void ref_iquant_itrans_recon_chroma_4x4_dc(const UWORD8 *pu1_pred,
                                                   UWORD8 *pu1_out,
                                                   int pred_strd, int out_strd,
                                                   const WORD16 *pi2_dc_src)
{
    WORD32 q0 = pi2_dc_src[0];
    WORD16 i_macro = (WORD16)((q0 + 32) >> 6);
    int col, row;
    for (col = 0; col < 4; col++)
        for (row = 0; row < 4; row++) {
            int t = (int)i_macro + pu1_pred[col * 2 + row * pred_strd];
            t = t < 0 ? 0 : t > 255 ? 255 : t;
            pu1_out[col * 2 + row * out_strd] = (UWORD8)t;
        }
}

static void check_iquant_itrans_recon(void)
{
    static const UWORD32 qp_divs[] = { 0, 1, 3, 4, 5, 7, 8, 12 };
    uint32_t seed = 11;
    unsigned qi;
    for (qi = 0; qi < sizeof qp_divs / sizeof qp_divs[0]; qi++) {
        int extreme;
        for (extreme = 0; extreme < 2; extreme++) {
            WORD16 src[16], tmp_m68k[16], dc_val;
            UWORD16 iscal[16], weigh[16];
            UWORD8 pred[8 * 4], out_m68k[8 * 4], out_ref[8 * 4];
            int i, iq_start_idx, pred_strd = 6, out_strd = 8;

            for (i = 0; i < 16; i++) {
                if (extreme) {
                    src[i] = (xrand(&seed) & 1) ? 2047 : -2048;
                    iscal[i] = (xrand(&seed) & 1) ? 0xffff : 0;
                    weigh[i] = (xrand(&seed) & 1) ? 0xffff : 0;
                } else {
                    src[i] = (WORD16)((xrand(&seed) & 0x1ff) - 256);
                    iscal[i] = (UWORD16)(10 + (xrand(&seed) % 40));
                    weigh[i] = (UWORD16)(16 + (xrand(&seed) % 200));
                }
            }
            dc_val = (WORD16)((xrand(&seed) & 0x1ff) - 256);
            for (i = 0; i < (int)(sizeof pred); i++)
                pred[i] = (uint8_t)(xrand(&seed) & 0xff);

            for (iq_start_idx = 0; iq_start_idx < 2; iq_start_idx++) {
                memset(out_m68k, 0xAA, sizeof out_m68k);
                memset(out_ref, 0xAA, sizeof out_ref);
                memset(tmp_m68k, 0, sizeof tmp_m68k);
                mr_ih264_iquant_itrans_recon_4x4_m68k(
                    src, pred, out_m68k, pred_strd, out_strd, iscal, weigh,
                    qp_divs[qi], tmp_m68k, iq_start_idx, &dc_val);
                ref_iquant_itrans_recon_4x4(src, pred, out_ref, pred_strd,
                                            out_strd, iscal, weigh,
                                            qp_divs[qi], iq_start_idx,
                                            &dc_val);
                for (i = 0; i < 4; i++) {
                    int row;
                    for (row = 0; row < 4; row++) {
                        int idx = i + row * out_strd;
                        if (out_m68k[idx] != out_ref[idx])
                            report("iquant_itrans_recon_4x4", row, i,
                                   out_ref[idx], out_m68k[idx]);
                    }
                }

                memset(out_m68k, 0xAA, sizeof out_m68k);
                memset(out_ref, 0xAA, sizeof out_ref);
                mr_ih264_iquant_itrans_recon_4x4_dc_m68k(
                    src, pred, out_m68k, pred_strd, out_strd, iscal, weigh,
                    qp_divs[qi], tmp_m68k, iq_start_idx, &dc_val);
                ref_iquant_itrans_recon_4x4_dc(src, pred, out_ref, pred_strd,
                                               out_strd, iscal, weigh,
                                               qp_divs[qi], iq_start_idx,
                                               &dc_val);
                for (i = 0; i < 4; i++) {
                    int row;
                    for (row = 0; row < 4; row++) {
                        int idx = i + row * out_strd;
                        if (out_m68k[idx] != out_ref[idx])
                            report("iquant_itrans_recon_4x4_dc", row, i,
                                   out_ref[idx], out_m68k[idx]);
                    }
                }
            }

            memset(out_m68k, 0xAA, sizeof out_m68k);
            memset(out_ref, 0xAA, sizeof out_ref);
            memset(tmp_m68k, 0, sizeof tmp_m68k);
            mr_ih264_iquant_itrans_recon_chroma_4x4_m68k(
                src, pred, out_m68k, pred_strd, out_strd, iscal, weigh,
                qp_divs[qi], tmp_m68k, &dc_val);
            ref_iquant_itrans_recon_chroma_4x4(src, pred, out_ref, pred_strd,
                                               out_strd, iscal, weigh,
                                               qp_divs[qi], &dc_val);
            for (i = 0; i < 4; i++) {
                int row;
                for (row = 0; row < 4; row++) {
                    int idx = i * 2 + row * out_strd;
                    if (out_m68k[idx] != out_ref[idx])
                        report("iquant_itrans_recon_chroma_4x4", row, i,
                               out_ref[idx], out_m68k[idx]);
                }
            }

            memset(out_m68k, 0xAA, sizeof out_m68k);
            memset(out_ref, 0xAA, sizeof out_ref);
            mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k(
                src, pred, out_m68k, pred_strd, out_strd, iscal, weigh,
                qp_divs[qi], tmp_m68k, &dc_val);
            ref_iquant_itrans_recon_chroma_4x4_dc(pred, out_ref, pred_strd,
                                                  out_strd, &dc_val);
            for (i = 0; i < 4; i++) {
                int row;
                for (row = 0; row < 4; row++) {
                    int idx = i * 2 + row * out_strd;
                    if (out_m68k[idx] != out_ref[idx])
                        report("iquant_itrans_recon_chroma_4x4_dc", row, i,
                               out_ref[idx], out_m68k[idx]);
                }
            }
        }
    }
}

/* ---- intra 16x16 Plane (8.3.3.4) ---------------------------------------- */
/* Independent re-derivation of ih264_intra_pred_luma_16x16_mode_plane -
 * unlike ih264_m68k_intra_pred.S's own closed-form re-derivation (which
 * uses real multiplies), this transcribes Ittiam's literal "no
 * multiplications" shift-add chain directly, so a mistake in either the
 * asm's derivation or this transcription would have to independently
 * agree to hide a bug. */
static void ref_intra16_plane(const uint8_t *src, uint8_t *dst, int dst_strd)
{
    const uint8_t *top = src + 17;
    const uint8_t *left = src + 15;
    const uint8_t *topleft = src + 16;
    int32_t a, b, c, tmp;
    const uint8_t *tmp1, *tmp2;

    a = (top[15] + left[-15]) << 4;

    tmp1 = top + 8;
    tmp2 = tmp1 - 2;
    b = (*tmp1++ - *tmp2--);
    b += (*tmp1++ - *tmp2--) << 1;
    tmp = (*tmp1++ - *tmp2--);
    b += (tmp << 1) + tmp;
    b += (*tmp1++ - *tmp2--) << 2;
    tmp = (*tmp1++ - *tmp2--);
    b += (tmp << 2) + tmp;
    tmp = (*tmp1++ - *tmp2--);
    b += (tmp << 2) + (tmp << 1);
    tmp = (*tmp1++ - *tmp2--);
    b += (tmp << 3) - tmp;
    b += (*tmp1 - *topleft) << 3;
    b = ((b << 2) + b + 32) >> 6;

    tmp1 = left - 8;
    tmp2 = tmp1 + 2;
    c = (*tmp1 - *tmp2);
    tmp1--; tmp2++;
    c += (*tmp1 - *tmp2) << 1;
    tmp1--; tmp2++;
    tmp = (*tmp1 - *tmp2);
    c += (tmp << 1) + tmp;
    tmp1--; tmp2++;
    c += (*tmp1 - *tmp2) << 2;
    tmp1--; tmp2++;
    tmp = (*tmp1 - *tmp2);
    c += (tmp << 2) + tmp;
    tmp1--; tmp2++;
    tmp = (*tmp1 - *tmp2);
    c += (tmp << 2) + (tmp << 1);
    tmp1--; tmp2++;
    tmp = (*tmp1 - *tmp2);
    c += (tmp << 3) - tmp;
    tmp1--;
    c += (*tmp1 - *topleft) << 3;
    c = ((c << 2) + c + 32) >> 6;

    {
        int32_t tv, tmpx, tmpx_init, i, j;
        tmpx_init = -(b << 3);
        tmp = a - (c << 3) + 16;
        for (i = 0; i < 16; i++) {
            tmp += c;
            tmpx = tmpx_init;
            for (j = 0; j < 16; j++) {
                tmpx += b;
                tv = (tmp + tmpx) >> 5;
                tv = tv < 0 ? 0 : tv > 255 ? 255 : tv;
                dst[i * dst_strd + j] = (uint8_t)tv;
            }
        }
    }
}

static void check_intra16_plane(void)
{
    uint32_t seed = 6;
    int trial;
    for (trial = 0; trial < 20; trial++) {
        uint8_t nbr[40];
        uint8_t dst_m68k[16 * 20], dst_ref[16 * 20];
        int dst_strd = 20, row, col;

        if (trial < 10)
            fill_random(nbr, sizeof nbr, &seed);
        else {
            unsigned j;
            for (j = 0; j < sizeof nbr; j++)
                nbr[j] = (xrand(&seed) & 1) ? 255 : 0;
        }

        memset(dst_m68k, 0xAA, sizeof dst_m68k);
        memset(dst_ref, 0xAA, sizeof dst_ref);
        mr_ih264_intra_pred_luma_16x16_plane_m68k(nbr, dst_m68k, 0, dst_strd,
                                                   0);
        ref_intra16_plane(nbr, dst_ref, dst_strd);
        for (row = 0; row < 16; row++)
            for (col = 0; col < 16; col++) {
                int idx = row * dst_strd + col;
                if (dst_m68k[idx] != dst_ref[idx])
                    report("intra16_plane", row, col, dst_ref[idx],
                           dst_m68k[idx]);
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

/* ---- CABAC regular-bin decode (ih264_m68k_cabac.S) ---------------------
 * Mirrors dec_bit_stream_t's layout (offset 0 = u4_ofst, offset 4 =
 * pu4_buffer) and decoding_envirnoment_t's (offset 0 = range, offset 4 =
 * val_ofst, offset 8 = cabac_table) without including libavc's decoder
 * headers - this test harness stays portable C, same as every other check
 * here. */
typedef struct {
    uint32_t u4_ofst;
    uint32_t *pu4_buffer;
} test_bitstrm_t;

typedef struct {
    uint32_t u4_code_int_range;
    uint32_t u4_code_int_val_ofst;
    const uint32_t *cabac_table;
} test_cabac_env_t;

/* Independent reference, transcribed directly from ih264d_decode_bin()'s C
 * (DECODE_ONE_BIN_MACRO in ih264d_cabac.h), not from the asm. */
static uint32_t ref_decode_bin_cabac(uint32_t ctx_inc, uint8_t *ctx_array,
                                     test_bitstrm_t *bs, test_cabac_env_t *env)
{
    uint8_t *p_ctx = ctx_array + ctx_inc;
    uint32_t range = env->u4_code_int_range;
    uint32_t val_ofst = env->u4_code_int_val_ofst;
    uint32_t mps_state = *p_ctx;
    uint32_t clz = range ? (uint32_t)__builtin_clz(range) : 31u;
    uint32_t qnt = (uint32_t)(((uint64_t)range << clz) >> 29) & 3u;
    uint32_t table_lookup = env->cabac_table[(mps_state << 2) + qnt];
    uint32_t range_lps = table_lookup & 0xffu;
    uint32_t symbol;

    range_lps <<= (23 - clz);
    range -= range_lps;
    symbol = (mps_state >> 6) & 1u;
    mps_state = (table_lookup >> 8) & 0x7Fu;

    if (val_ofst >= range) {
        symbol = 1 - symbol;
        val_ofst -= range;
        range = range_lps;
        mps_state = (table_lookup >> 15) & 0x7Fu;
    }

    if (range < 256u) {
        uint32_t offset = bs->u4_ofst;
        uint32_t clz2 = range ? (uint32_t)__builtin_clz(range) : 31u;
        uint32_t bitpos = offset + 23u;
        uint32_t word_off = bitpos >> 5;
        uint32_t bit_off = bitpos & 0x1Fu;
        uint32_t word = bs->pu4_buffer[word_off] << bit_off;
        uint32_t read_bits;

        if (bit_off)
            word |= bs->pu4_buffer[word_off + 1] >> (32u - bit_off);
        read_bits = word >> (32u - clz2);

        offset += clz2;
        range <<= clz2;
        val_ofst = (val_ofst << clz2) | read_bits;
        bs->u4_ofst = offset;
    }

    env->u4_code_int_range = range;
    env->u4_code_int_val_ofst = val_ofst;
    *p_ctx = (uint8_t)mps_state;
    return symbol;
}

static void check_decode_bin_cabac(void)
{
    enum { N_CTX = 4, N_TABLE = 512, N_BUF = 16, N_ITER = 20000 };
    uint32_t seed = 99;
    int iter;

    for (iter = 0; iter < N_ITER; iter++) {
        uint32_t table[N_TABLE];
        uint32_t buf_ref[N_BUF], buf_asm[N_BUF];
        uint8_t ctx_ref[N_CTX], ctx_asm[N_CTX];
        test_bitstrm_t bs_ref, bs_asm;
        test_cabac_env_t env_ref, env_asm;
        uint32_t ctx_inc, start_range, start_val_ofst, start_offset;
        uint32_t sym_ref, sym_asm;
        int i;

        for (i = 0; i < N_TABLE; i++)
            table[i] = xrand(&seed) | (xrand(&seed) << 15) |
                       (xrand(&seed) << 30);
        for (i = 0; i < N_BUF; i++)
            buf_ref[i] = buf_asm[i] = xrand(&seed) | (xrand(&seed) << 15) |
                                      (xrand(&seed) << 30);
        for (i = 0; i < N_CTX; i++)
            ctx_ref[i] = ctx_asm[i] = (uint8_t)(xrand(&seed) & 0x7f);

        ctx_inc = xrand(&seed) % N_CTX;
        /* range must land in the same [256,510] band every real caller
         * already guarantees (the post-renormalisation invariant) - outside
         * that band the reference's (23-clz) shift is undefined behaviour
         * in C, a precondition violation rather than something either
         * implementation needs to tolerate. */
        start_range = 256u + (xrand(&seed) % 255u);
        /* Deliberately up to 2x range so both the MPS and LPS branches of
         * CHECK_IF_LPS get exercised often. */
        start_val_ofst = xrand(&seed) % 1024u;
        start_offset = xrand(&seed) % 200u;

        env_ref.u4_code_int_range = env_asm.u4_code_int_range = start_range;
        env_ref.u4_code_int_val_ofst = env_asm.u4_code_int_val_ofst =
            start_val_ofst;
        env_ref.cabac_table = env_asm.cabac_table = table;
        bs_ref.u4_ofst = bs_asm.u4_ofst = start_offset;
        bs_ref.pu4_buffer = buf_ref;
        bs_asm.pu4_buffer = buf_asm;

        sym_ref = ref_decode_bin_cabac(ctx_inc, ctx_ref, &bs_ref, &env_ref);
        sym_asm = mr_ih264d_decode_bin_m68k(ctx_inc, ctx_asm, &bs_asm,
                                            &env_asm);

        if (sym_ref != sym_asm)
            report("decode_bin_cabac(symbol)", iter, 0, (int)sym_ref,
                   (int)sym_asm);
        if (env_ref.u4_code_int_range != env_asm.u4_code_int_range)
            report("decode_bin_cabac(range)", iter, 0,
                   (int)env_ref.u4_code_int_range,
                   (int)env_asm.u4_code_int_range);
        if (env_ref.u4_code_int_val_ofst != env_asm.u4_code_int_val_ofst)
            report("decode_bin_cabac(val_ofst)", iter, 0,
                   (int)env_ref.u4_code_int_val_ofst,
                   (int)env_asm.u4_code_int_val_ofst);
        if (bs_ref.u4_ofst != bs_asm.u4_ofst)
            report("decode_bin_cabac(bitstrm_ofst)", iter, 0,
                   (int)bs_ref.u4_ofst, (int)bs_asm.u4_ofst);
        for (i = 0; i < N_CTX; i++)
            if (ctx_ref[i] != ctx_asm[i])
                report("decode_bin_cabac(ctx)", iter, i, ctx_ref[i],
                       ctx_asm[i]);
    }
}

/* ---- chroma motion-compensation interpolation (ih264_m68k_chroma_mc.S) -
 * Independent reference transcribed from H.264 spec 8.4.2.2.2, not from
 * the asm (and not from Ittiam's C, beyond sharing the same spec formula
 * any conformant implementation must use). */
static void ref_inter_pred_chroma(const uint8_t *src, uint8_t *dst,
                                  int src_strd, int dst_strd, int dx, int dy,
                                  int ht, int wd)
{
    int row, col;
    for (row = 0; row < ht; row++) {
        for (col = 0; col < 2 * wd; col++) {
            int s00 = src[col];
            int s10 = src[col + 2];
            int s01 = src[src_strd + col];
            int s11 = src[src_strd + col + 2];
            int tmp = (8 - dx) * (8 - dy) * s00 + dx * (8 - dy) * s10 +
                      (8 - dx) * dy * s01 + dx * dy * s11;
            tmp = (tmp + 32) >> 6;
            dst[col] = (uint8_t)tmp;
        }
        src += src_strd;
        dst += dst_strd;
    }
}

static void check_inter_pred_chroma(void)
{
    static const int wds[] = { 2, 4, 8 };
    static const int hts[] = { 2, 4, 8 };
    enum { SRC_STRD = 32, DST_STRD = 20, N_SRC_ROWS = 9 };
    uint32_t seed = 7;
    int wi, hi, iter;

    for (wi = 0; wi < 3; wi++) {
        for (hi = 0; hi < 3; hi++) {
            int wd = wds[wi], ht = hts[hi];
            for (iter = 0; iter < 20; iter++) {
                uint8_t src[N_SRC_ROWS * SRC_STRD];
                uint8_t dst_ref[8 * DST_STRD], dst_asm[8 * DST_STRD];
                int dx = xrand(&seed) % 8, dy = xrand(&seed) % 8;
                int row, col;

                fill_random(src, sizeof src, &seed);
                memset(dst_ref, 0, sizeof dst_ref);
                memset(dst_asm, 0, sizeof dst_asm);

                ref_inter_pred_chroma(src, dst_ref, SRC_STRD, DST_STRD, dx,
                                      dy, ht, wd);
                mr_ih264_inter_pred_chroma_m68k(src, dst_asm, SRC_STRD,
                                                DST_STRD, dx, dy, ht, wd);

                for (row = 0; row < ht; row++)
                    for (col = 0; col < 2 * wd; col++) {
                        int idx = row * DST_STRD + col;
                        if (dst_ref[idx] != dst_asm[idx])
                            report("inter_pred_chroma", row, col,
                                   dst_ref[idx], dst_asm[idx]);
                    }
            }
        }
    }
}
/* ---- motion vector prediction (ih264_m68k_mvpred.S) -------------------
 * Independent reference, transcribed directly from
 * ih264d_get_motion_vector_predictor()'s C (spec 8.4.1.2.1), not from the
 * asm. test_mv_pred_t mirrors mv_pred_t's layout (ih264d_structs.h) byte
 * for byte - i2_mv[4] at offset 0, i1_ref_frame[2] at offset 8 - since the
 * asm indexes into it by those fixed offsets. */
typedef struct {
    int16_t i2_mv[4];
    int8_t  i1_ref_frame[2];
    uint8_t u1_col_ref_pic_idx;
    uint8_t u1_pic_type;
} test_mv_pred_t;

static const int8_t g_mv_pred_condition[8] = { -1, 0, 1, -1, 2, -1, -1, -1 };

static void ref_mv_predictor(test_mv_pred_t *result, test_mv_pred_t **mv_pred,
                             uint8_t ref_idx, uint8_t b, const int8_t *cond)
{
    int8_t c_temp;
    uint8_t b2 = (uint8_t)(b << 1);

    c_temp = (int8_t)((mv_pred[0]->i1_ref_frame[b] == ref_idx) |
                      ((mv_pred[1]->i1_ref_frame[b] == ref_idx) << 1) |
                      ((mv_pred[2]->i1_ref_frame[b] == ref_idx) << 2));
    c_temp = cond[c_temp];

    if (c_temp != -1) {
        result->i2_mv[b2 + 0] = mv_pred[c_temp]->i2_mv[b2 + 0];
        result->i2_mv[b2 + 1] = mv_pred[c_temp]->i2_mv[b2 + 1];
    } else {
        int i, d0, d1;
        for (i = 0; i < 2; i++) {
            int a = mv_pred[0]->i2_mv[b2 + i];
            int t = mv_pred[1]->i2_mv[b2 + i];
            int tr = mv_pred[2]->i2_mv[b2 + i];
            d0 = a < t ? a : t;
            d1 = a > t ? a : t;
            d1 = d1 < tr ? d1 : tr;
            result->i2_mv[b2 + i] = (int16_t)(d0 > d1 ? d0 : d1);
        }
    }
}

static void check_mv_predictor(void)
{
    enum { N_ITER = 20000 };
    uint32_t seed = 137;
    int iter;

    for (iter = 0; iter < N_ITER; iter++) {
        test_mv_pred_t cand[3], result_ref, result_asm;
        test_mv_pred_t *ptrs[3] = { &cand[0], &cand[1], &cand[2] };
        uint8_t ref_idx, b;
        int i, k;

        for (i = 0; i < 3; i++) {
            for (k = 0; k < 4; k++)
                cand[i].i2_mv[k] = (int16_t)(xrand(&seed) % 8192) - 4096;
            /* Small in-domain range (-1..2) so every c_temp bitmask value
             * (0..7) and both the shortcut and median paths get exercised
             * often, not just the near-certain "no match" case a full
             * int8_t range would produce. */
            cand[i].i1_ref_frame[0] = (int8_t)((int)(xrand(&seed) % 4) - 1);
            cand[i].i1_ref_frame[1] = (int8_t)((int)(xrand(&seed) % 4) - 1);
        }
        ref_idx = (uint8_t)(xrand(&seed) % 3);
        b = (uint8_t)(xrand(&seed) % 2);
        memset(&result_ref, 0xAA, sizeof result_ref);
        memset(&result_asm, 0xAA, sizeof result_asm);

        ref_mv_predictor(&result_ref, ptrs, ref_idx, b, g_mv_pred_condition);
        mr_ih264d_get_motion_vector_predictor_m68k(
            (uint8_t *)&result_asm, (uint8_t **)ptrs, ref_idx, b,
            (const uint8_t *)g_mv_pred_condition);

        for (k = 0; k < 4; k++)
            if (result_ref.i2_mv[k] != result_asm.i2_mv[k])
                report("mv_predictor", iter, k, result_ref.i2_mv[k],
                       result_asm.i2_mv[k]);
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
    check_luma_copy();
    check_weighted_pred_luma();
    check_weighted_pred_chroma();
    check_intra16();
    check_intra16_dc();
    check_intra4();
#if defined(MR_M68K_ASM)
    check_explicit_weighted_pred();
    check_interp();
    check_interp_qpel_qpel();
    check_interp_qpel();
    check_interp_hpel_hpel();
    check_interp_horz_qpel_vert_hpel();
    check_interp_horz_hpel_vert_qpel();
    check_iquant_itrans_recon();
    check_intra16_plane();
    check_deblk_bs4();
    check_deblk_bslt4();
    check_deblk_chroma_bs4();
    check_deblk_chroma_bslt4();
    check_decode_bin_cabac();
    check_inter_pred_chroma();
    check_mv_predictor();
#endif

    if (g_failures) {
        fprintf(stderr, "mr_h264_m68k_check: %d mismatches\n", g_failures);
        return 1;
    }
    printf("mr_h264_m68k_check: OK\n");
    return 0;
}
