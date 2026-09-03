/*
 * MintVID - WMV2 (Windows Media Video 8) decoder.
 *
 * WMV2 extends WMV1's (mr_wmv.c) bitstream with: a container-level
 * extension header (fps/bitrate/mspel/loop_filter/abt_flag/j_type/
 * top_left_mv_flag/per_mb_rl_bit/slice-count, read once at open() time from
 * dec->config -- the AVI BITMAPINFOHEADER's trailing bytes, this project's
 * equivalent of ffmpeg's avctx->extradata -- instead of embedded in every
 * I-frame like WMV1's own fps/bitrate/flipflop fields), bitplane-coded
 * macroblock skip (a whole skip/non-skip map for the picture, read up
 * front instead of one bit per macroblock inline), three qscale-selected
 * P-frame mb_type+cbp VLC tables (instead of WMV1's one fixed table),
 * adaptive top-left motion-vector prediction, MSPEL luma motion
 * compensation (a sharper interpolation filter, selectable per picture),
 * the Adaptive Block Transform (ABT: an inter residual block may split
 * into two 8x4 or two 4x8 sub-blocks, each with its own scan order and a
 * genuinely different-sized IDCT, instead of always one plain 8x8 block),
 * and an H.263 Annex-J-style in-loop deblocking filter. Everything WMV2
 * does NOT add on top of WMV1 -- the DC/AC/coefficient VLC grammar
 * (escape 1/2/3, per-picture RL/DC/MV table selection, coded-block-pattern
 * prediction on intra blocks, the combined motion-vector VLC tables, the
 * plain 8x8 IDCT) -- is bit-for-bit the SAME grammar as WMV1 (confirmed
 * against ffmpeg's own source: WMV1 and WMV2 share one internal decoder
 * context and one ff_msmpeg4_decode_block()/ff_msmpeg4_decode_motion()
 * implementation), so those pieces are reused here as an intentional
 * near-verbatim copy of mr_wmv.c's own implementation -- this project's
 * codec plugins are each self-contained per object file (see mr_wmv.c),
 * not shared via a common library, so duplication here is deliberate.
 *
 * Not implemented: IntraX8 ("J-frame") coding, a completely separate
 * intra-only sub-codec (shared with VC-1) that WMV2 can invoke instead of
 * its normal macroblock grammar when the extension header's j_type_bit
 * capability is set AND a per-picture j_type flag selects it. This is an
 * optional, rarely-used high-quality-intra mode, not part of typical WMV2
 * encoder output; streams that use it are explicitly rejected (MR_EFORMAT)
 * rather than silently decoded wrong, matching this project's existing
 * compatibility policy for unsupported bitstream tools (see DESIGN.md and
 * mr_wmv.c's own inter_intra_pred rejection).
 *
 * The picture-header/macroblock/VLC/quantiser/ABT/loop-filter semantics
 * were cross-checked against FFmpeg's LGPL wmv2dec.c/wmv2.h/wmv2data.c/
 * h263.c/h263dsp.c/simple_idct.c; the three extra CBP VLC tables and two
 * ABT scan tables in mr_wmv2_tables.inc are mechanically extracted (see
 * gen_wmv2_tables.py) from FFmpeg's msmpeg4data.c/wmv2data.c, since those
 * bit patterns are fixed by the bitstream format itself, not FFmpeg's
 * expression of it. The ABT partial-IDCT fixed-point constants/shifts
 * below are transcribed verbatim from FFmpeg's simple_idct.c/
 * simple_idct_template.c (not re-derived) precisely because an 8-point and
 * a 4-point pass mixed into one 2D separable transform must share a
 * consistent normalization, and getting that consistency wrong is exactly
 * the class of subtle bug mr_wmv.c's own development history warns about.
 */
#include "mr_wmv2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- bit reader (MSB first) -- identical to mr_wmv.c's -------------- */
typedef struct {
    const uint8_t *buf;
    int len;
    int pos;
} bitreader;

static void br_init(bitreader *b, const uint8_t *data, int len)
{
    b->buf = data;
    b->len = len;
    b->pos = 0;
}

static unsigned br_bit(bitreader *b)
{
    int byte = b->pos >> 3;
    int shift = 7 - (b->pos & 7);
    b->pos++;
    if (byte >= b->len)
        return 0;
    return (b->buf[byte] >> shift) & 1u;
}

static unsigned br_bits(bitreader *b, int n)
{
    unsigned v = 0;
    while (n-- > 0)
        v = (v << 1) | br_bit(b);
    return v;
}

static unsigned br_peek(bitreader *b, int n)
{
    int save = b->pos;
    unsigned v = br_bits(b, n);
    b->pos = save;
    return v;
}

static void br_skip(bitreader *b, int n)
{
    b->pos += n;
}

static int br_overrun(const bitreader *b)
{
    return b->pos > b->len * 8;
}

static int wmv2_debug(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("MRDBG") != NULL;
    return enabled;
}

/* Microsoft's ternary "012" VLC: "0"->0, "10"->1, "11"->2. */
static int decode012(bitreader *b)
{
    if (!br_bit(b))
        return 0;
    return (int)br_bit(b) + 1;
}

/* ---- table types (must match mr_mpeg4_tables.inc's expectations; see
 * mr_wmv.c's identical comment). wtab_t/wmvmv_t are wide-codeword types
 * shared with mr_wmv_tables.inc's format. ------------------------------ */
typedef struct {
    uint16_t code;
    uint8_t len, last, run;
    int16_t level;
} tcoef_t;
typedef struct { uint16_t code; uint8_t len, val; } vlc3_t;
typedef struct { uint16_t code; uint8_t len, mbtype, cbpc; } mcbpc_t;
typedef struct { uint16_t code; uint8_t len, intra, inter; } cbpy_t;
typedef struct { uint16_t code; uint8_t len; int16_t data; } mvd_t;

typedef struct { uint32_t code; uint8_t len, val; } wtab_t;
typedef struct { uint32_t code; uint8_t len; uint16_t sym; } wmvmv_t;

#include "mr_mpeg4_tables.inc"   /* tcoef_intra / tcoef_inter (rl index 2) */
#include "mr_wmv_tables.inc"     /* WMV1 tables, reused verbatim by WMV2  */
#include "mr_wmv2_tables.inc"    /* WMV2's own extra CBP/scan tables      */

/* ---- run/level ("RL") coefficient table groups -- identical to mr_wmv.c;
 * WMV2 selects among the exact same three table sets per picture. ------ */
typedef struct {
    const tcoef_t *tab;
    int count;
    unsigned esc_code;
    int esc_len;
    const uint8_t (*maxlevel)[65];
    const uint8_t (*maxrun)[65];
} rlgroup_t;

static const rlgroup_t rl_intra_tabs[3] = {
    { wmv_rl_intra_low, 132, WMV_RL_INTRA_LOW_ESC_CODE, WMV_RL_INTRA_LOW_ESC_LEN,
      wmv_rl_intra_low_maxlevel, wmv_rl_intra_low_maxrun },
    { wmv_rl_intra_high, 185, WMV_RL_INTRA_HIGH_ESC_CODE, WMV_RL_INTRA_HIGH_ESC_LEN,
      wmv_rl_intra_high_maxlevel, wmv_rl_intra_high_maxrun },
    { tcoef_intra, 102, 0x03, 7,
      wmv_rl_mid_intra_maxlevel, wmv_rl_mid_intra_maxrun },
};
static const rlgroup_t rl_inter_tabs[3] = {
    { wmv_rl_inter_low, 148, WMV_RL_INTER_LOW_ESC_CODE, WMV_RL_INTER_LOW_ESC_LEN,
      wmv_rl_inter_low_maxlevel, wmv_rl_inter_low_maxrun },
    { wmv_rl_inter_high, 168, WMV_RL_INTER_HIGH_ESC_CODE, WMV_RL_INTER_HIGH_ESC_LEN,
      wmv_rl_inter_high_maxlevel, wmv_rl_inter_high_maxrun },
    { tcoef_inter, 102, 0x03, 7,
      wmv_rl_mid_inter_maxlevel, wmv_rl_mid_inter_maxrun },
};

static const wtab_t *const wmv_dc_tab_all[2][2] = {
    { wmv_dc_tab_0_0, wmv_dc_tab_0_1 },
    { wmv_dc_tab_1_0, wmv_dc_tab_1_1 },
};

static const wtab_t *const wmv2_cbp_tabs[3] = {
    wmv2_cbp_tab0, wmv2_cbp_tab1, wmv2_cbp_tab2,
};

/* qscale -> which of the 3 WMV2 P-frame mb_type+cbp VLC tables applies,
 * selected together with a 2-bit cbp_index from the picture header --
 * ported verbatim from FFmpeg's wmv2.h wmv2_get_cbp_table_index(). */
static int wmv2_cbp_table_index(int qscale, int cbp_index)
{
    static const uint8_t map[3][3] = {
        { 0, 2, 1 },
        { 1, 0, 2 },
        { 2, 1, 0 },
    };
    int row = (qscale > 10) + (qscale > 20);
    return map[row][cbp_index];
}

/* ---- generic VLC match helpers -- identical to mr_wmv.c's ------------ */
static int match_wtab(bitreader *b, const wtab_t *tab, int count, int *val)
{
    unsigned w = br_peek(b, 26);
    int i;
    for (i = 0; i < count; i++) {
        if ((w >> (26 - tab[i].len)) == tab[i].code) {
            br_skip(b, tab[i].len);
            *val = tab[i].val;
            return 0;
        }
    }
    return -1;
}

static int match_tcoef_base(bitreader *b, const tcoef_t *tab, int count,
                            int *last, int *run, int *level)
{
    unsigned w = br_peek(b, 16);
    int i;
    for (i = 0; i < count; i++) {
        const tcoef_t *e = &tab[i];
        if ((w >> (16 - e->len)) == e->code) {
            br_skip(b, e->len);
            *last = e->last;
            *run = e->run;
            *level = e->level;
            return 0;
        }
    }
    return -1;
}

static int match_tcoef(bitreader *b, const tcoef_t *tab, int count,
                       int *last, int *run, int *level)
{
    if (match_tcoef_base(b, tab, count, last, run, level))
        return -1;
    if (br_bit(b))
        *level = -*level;
    return br_overrun(b) ? -1 : 0;
}

/* ---- adaptive "escape 3" coefficient coding -- identical to mr_wmv.c's,
 * same per-picture-adaptive level/run bit-width negotiation. ----------- */
typedef struct wmv2_ctx wmv2_ctx;

static int decode_esc3(bitreader *b, wmv2_ctx *c, int qscale,
                       int *last, int *run, int *level);

static int decode_rl_event(bitreader *b, wmv2_ctx *c, const rlgroup_t *g,
                           int qscale, int *last, int *run, int *level)
{
    unsigned w = br_peek(b, 16);

    if ((w >> (16 - g->esc_len)) != g->esc_code)
        return match_tcoef(b, g->tab, g->count, last, run, level);

    br_skip(b, g->esc_len);
    if (br_bit(b)) {                       /* escape 1: extend LEVEL */
        int add;
        if (match_tcoef_base(b, g->tab, g->count, last, run, level))
            return -1;
        add = g->maxlevel[*last][*run];
        *level += add;
        if (br_bit(b))
            *level = -*level;
    } else if (br_bit(b)) {                /* escape 2: extend RUN */
        int add;
        if (match_tcoef_base(b, g->tab, g->count, last, run, level))
            return -1;
        add = g->maxrun[*last][*level];
        *run += add + 1;
        if (br_bit(b))
            *level = -*level;
    } else {                               /* escape 3 */
        if (decode_esc3(b, c, qscale, last, run, level))
            return -1;
    }
    return br_overrun(b) ? -1 : 0;
}

/* ---- transform, prediction and reconstruction (see mr_wmv.c's identical
 * provenance note on the integer IDCT). --------------------------------- */
#define IDCT_P 13
#define IDCT_SHIFT (2 * IDCT_P + 2)
static const int32_t idct_tab[8][8] = {
    {5793, 5793, 5793, 5793, 5793, 5793, 5793, 5793},
    {8035, 6811, 4551, 1598,-1598,-4551,-6811,-8035},
    {7568, 3135,-3135,-7568,-7568,-3135, 3135, 7568},
    {6811,-1598,-8035,-4551, 4551, 8035, 1598,-6811},
    {5793,-5793,-5793, 5793, 5793,-5793,-5793, 5793},
    {4551,-8035, 1598, 6811,-6811,-1598, 8035,-4551},
    {3135,-7568, 7568,-3135,-3135, 7568,-7568, 3135},
    {1598,-4551, 6811,-8035, 8035,-6811, 4551,-1598}
};

static int idct_round(int64_t acc)
{
    int64_t half = (int64_t)1 << (IDCT_SHIFT - 1);
    return acc >= 0 ? (int)((acc + half) >> IDCT_SHIFT)
                    : -(int)((-acc + half) >> IDCT_SHIFT);
}

static void idct_8x8(const int in[8][8], int out[8][8])
{
    int32_t tmp[8][8];
    int rowmask = 0;
    int u, x, v, y;

    for (v = 0; v < 8; v++) {
        const int *ir = in[v];
        if (!(ir[0] | ir[1] | ir[2] | ir[3] | ir[4] | ir[5] | ir[6] | ir[7]))
            continue;
        rowmask |= 1 << v;
        for (x = 0; x < 8; x++) {
            int32_t a = 0;
            for (u = 0; u < 8; u++)
                a += ir[u] * idct_tab[u][x];
            tmp[v][x] = a;
        }
    }

    if (rowmask == 0) {
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                out[y][x] = 0;
        return;
    }
    if (rowmask == 1) {
        for (x = 0; x < 8; x++)
            for (y = 0; y < 8; y++)
                out[y][x] = idct_round((int64_t)tmp[0][x] * idct_tab[0][y]);
        return;
    }

    for (x = 0; x < 8; x++) {
        for (y = 0; y < 8; y++) {
            int64_t a = 0;
            for (v = 0; v < 8; v++)
                if (rowmask & (1 << v))
                    a += (int64_t)tmp[v][x] * idct_tab[v][y];
            out[y][x] = idct_round(a);
        }
    }
}

static int clip8(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static int median3(int a, int b, int c)
{
    int hi = a > b ? a : b;
    int lo = a < b ? a : b;
    if (c > hi) return hi;
    if (c < lo) return lo;
    return c;
}

static int floor_div(int n, int d)
{
    int q = n / d;
    int r = n % d;
    if (r < 0)
        q--;
    return q;
}

static int fetch_px(const uint8_t *p, int w, int h, int stride, int x, int y)
{
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return p[(size_t)y * stride + x];
}

/* no_rounding selects the "unbiased" (round-down) half-pel averaging
 * filter; unlike WMV1's *bitstream-conditional* flipflop, WMV2 toggles
 * this unconditionally every P-frame (see decode_picture()). */
static void mc_block(const uint8_t *ref, int w, int h, int stride,
                     int px, int py, int mvx, int mvy, int no_rounding,
                     int out[8][8])
{
    int ix = floor_div(mvx, 2), iy = floor_div(mvy, 2);
    int hx = mvx - ix * 2, hy = mvy - iy * 2;
    int bx = px + ix, by = py + iy;
    int radd2 = no_rounding ? 0 : 1;
    int radd4 = no_rounding ? 1 : 2;
    int y, x;

    if (bx >= 0 && by >= 0 &&
        bx + 8 + (hx ? 1 : 0) <= w && by + 8 + (hy ? 1 : 0) <= h) {
        const uint8_t *base = ref + (size_t)by * stride + bx;
        for (y = 0; y < 8; y++) {
            const uint8_t *r = base + (size_t)y * stride;
            if (!hx && !hy)
                for (x = 0; x < 8; x++) out[y][x] = r[x];
            else if (hx && !hy)
                for (x = 0; x < 8; x++) out[y][x] = (r[x] + r[x + 1] + radd2) >> 1;
            else if (!hx && hy)
                for (x = 0; x < 8; x++)
                    out[y][x] = (r[x] + r[x + stride] + radd2) >> 1;
            else
                for (x = 0; x < 8; x++)
                    out[y][x] = (r[x] + r[x + 1] +
                                 r[x + stride] + r[x + stride + 1] + radd4) >> 2;
        }
        return;
    }

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int sx = px + x + ix, sy = py + y + iy;
            int a = fetch_px(ref, w, h, stride, sx, sy);
            int b = fetch_px(ref, w, h, stride, sx + 1, sy);
            int c = fetch_px(ref, w, h, stride, sx, sy + 1);
            int d = fetch_px(ref, w, h, stride, sx + 1, sy + 1);
            if (!hx && !hy) out[y][x] = a;
            else if (hx && !hy) out[y][x] = (a + b + radd2) >> 1;
            else if (!hx && hy) out[y][x] = (a + c + radd2) >> 1;
            else out[y][x] = (a + b + c + d + radd4) >> 2;
        }
    }
}

static int chroma_mv(int mv)
{
    static const uint8_t roundtab[4] = { 0, 1, 1, 1 };
    int q = floor_div(mv, 4);
    int r = mv - q * 4;
    return q * 2 + roundtab[r];
}

static int dequant_ac(int level, int q)
{
    int qadd = (q - 1) | 1;
    if (level > 0) return level * (q << 1) + qadd;
    if (level < 0) return level * (q << 1) - qadd;
    return 0;
}

/* ---- MSPEL: WMV2's own sharper luma interpolation filter --------------
 * Selected per picture (h->c.mspel bitstream flag, only meaningful when
 * the extension header's mspel_bit capability is set) in place of the
 * standard bilinear filter (mc_block(), above) for that picture's non-skip
 * inter macroblocks' LUMA plane only -- chroma always uses mc_block().
 * Ported from FFmpeg's wmv2dec.c wmv2_mspel8_h_lowpass/v_lowpass and the
 * put_mspel8_mc*_c dispatch table, but expressed per-pixel via fetch_px()
 * (clamped-edge) rather than FFmpeg's intermediate-buffer + emulated-edge
 * implementation -- same arithmetic, restructured to fit this file's
 * existing block-reconstruction style. The diagonal (both-axes) phases
 * apply the horizontal filter first with its own 8-bit clamp, THEN the
 * vertical filter over those already-clamped intermediate values (not a
 * single unclamped double-precision pass) -- FFmpeg's halfH buffer is
 * itself uint8_t, so this intermediate quantisation step is part of the
 * bitstream's defined output, not an implementation detail to smooth over.
 */
static int mspel_hfilt(const uint8_t *ref, int w, int h, int stride, int sx, int sy)
{
    int p_1 = fetch_px(ref, w, h, stride, sx - 1, sy);
    int p0  = fetch_px(ref, w, h, stride, sx,     sy);
    int p1  = fetch_px(ref, w, h, stride, sx + 1, sy);
    int p2  = fetch_px(ref, w, h, stride, sx + 2, sy);
    return clip8((9 * (p0 + p1) - (p_1 + p2) + 8) >> 4);
}

static int mspel_vfilt(const uint8_t *ref, int w, int h, int stride, int sx, int sy)
{
    int p_1 = fetch_px(ref, w, h, stride, sx, sy - 1);
    int p0  = fetch_px(ref, w, h, stride, sx, sy);
    int p1  = fetch_px(ref, w, h, stride, sx, sy + 1);
    int p2  = fetch_px(ref, w, h, stride, sx, sy + 2);
    return clip8((9 * (p0 + p1) - (p_1 + p2) + 8) >> 4);
}

static int mspel_hvfilt(const uint8_t *ref, int w, int h, int stride, int sx, int sy)
{
    int i, tmp[4];
    for (i = 0; i < 4; i++)
        tmp[i] = mspel_hfilt(ref, w, h, stride, sx, sy - 1 + i);
    return clip8((9 * (tmp[1] + tmp[2]) - (tmp[0] + tmp[3]) + 8) >> 4);
}

/* dxy in 0..7: bit0 = horizontal half-pel, bit1 = vertical half-pel (of the
 * base 2-bit phase), combined with the extra "hshift" precision bit as
 * dxy = 2*((vert_half<<1)|horiz_half) + hshift (see wmv2_decode_motion()). */
static void mspel_mc_block(const uint8_t *ref, int w, int h, int stride,
                           int px, int py, int dxy, int out[8][8])
{
    int x, y;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int sx = px + x, sy = py + y;
            switch (dxy) {
            case 0:
                out[y][x] = fetch_px(ref, w, h, stride, sx, sy);
                break;
            case 1:
                out[y][x] = (fetch_px(ref, w, h, stride, sx, sy) +
                            mspel_hfilt(ref, w, h, stride, sx, sy) + 1) >> 1;
                break;
            case 2:
                out[y][x] = mspel_hfilt(ref, w, h, stride, sx, sy);
                break;
            case 3:
                out[y][x] = (fetch_px(ref, w, h, stride, sx + 1, sy) +
                            mspel_hfilt(ref, w, h, stride, sx, sy) + 1) >> 1;
                break;
            case 4:
                out[y][x] = mspel_vfilt(ref, w, h, stride, sx, sy);
                break;
            case 5:
                out[y][x] = (mspel_vfilt(ref, w, h, stride, sx, sy) +
                            mspel_hvfilt(ref, w, h, stride, sx, sy) + 1) >> 1;
                break;
            case 6:
                out[y][x] = mspel_hvfilt(ref, w, h, stride, sx, sy);
                break;
            default:
                out[y][x] = (mspel_vfilt(ref, w, h, stride, sx + 1, sy) +
                            mspel_hvfilt(ref, w, h, stride, sx, sy) + 1) >> 1;
                break;
            }
        }
    }
}

/* ---- Adaptive Block Transform partial IDCTs ---------------------------
 * A "84" (8-wide x 4-tall) or "48" (4-wide x 8-tall) ABT sub-block is a
 * genuinely different-sized separable transform, not the normal idct_8x8
 * above with rows/columns zeroed -- ported verbatim (constants, shifts,
 * and the specific "add-half-after" vs "divide-then-multiply" rounding
 * FFmpeg uses in each of the four passes) from simple_idct.c/
 * simple_idct_template.c so the two passes of each composite transform
 * stay consistently normalised with each other. Only ever used for ABT
 * sub-blocks (abt_type!=0); the normal idct_8x8 above is unrelated and
 * untouched. */
#define ABT_W1 22725
#define ABT_W2 21407
#define ABT_W3 19266
#define ABT_W4 16383
#define ABT_W5 12873
#define ABT_W6 8867
#define ABT_W7 4520
#define ABT_ROW_SHIFT 11
#define ABT_COL_SHIFT 20
#define ABT_R1 30274
#define ABT_R2 12540
#define ABT_R3 23170
#define ABT_R_SHIFT 11
#define ABT_C1 3784
#define ABT_C2 1567
#define ABT_C3 2896
#define ABT_C_SHIFT 17

/* 8-point row pass (frequency -> spatial-x), in place. */
static void abt_row8(int64_t row[8])
{
    int64_t a0, a1, a2, a3, b0, b1, b2, b3;
    a0 = ABT_W4 * row[0] + ((int64_t)1 << (ABT_ROW_SHIFT - 1));
    a1 = a0; a2 = a0; a3 = a0;
    a0 += ABT_W2 * row[2]; a1 += ABT_W6 * row[2];
    a2 -= ABT_W6 * row[2]; a3 -= ABT_W2 * row[2];
    b0 = ABT_W1 * row[1] + ABT_W3 * row[3];
    b1 = ABT_W3 * row[1] - ABT_W7 * row[3];
    b2 = ABT_W5 * row[1] - ABT_W1 * row[3];
    b3 = ABT_W7 * row[1] - ABT_W5 * row[3];
    a0 += ABT_W4 * row[4] + ABT_W6 * row[6];
    a1 += -ABT_W4 * row[4] - ABT_W2 * row[6];
    a2 += -ABT_W4 * row[4] + ABT_W2 * row[6];
    a3 += ABT_W4 * row[4] - ABT_W6 * row[6];
    b0 += ABT_W5 * row[5] + ABT_W7 * row[7];
    b1 += -ABT_W1 * row[5] - ABT_W5 * row[7];
    b2 += ABT_W7 * row[5] + ABT_W3 * row[7];
    b3 += ABT_W3 * row[5] - ABT_W1 * row[7];
    row[0] = (a0 + b0) >> ABT_ROW_SHIFT; row[7] = (a0 - b0) >> ABT_ROW_SHIFT;
    row[1] = (a1 + b1) >> ABT_ROW_SHIFT; row[6] = (a1 - b1) >> ABT_ROW_SHIFT;
    row[2] = (a2 + b2) >> ABT_ROW_SHIFT; row[5] = (a2 - b2) >> ABT_ROW_SHIFT;
    row[3] = (a3 + b3) >> ABT_ROW_SHIFT; row[4] = (a3 - b3) >> ABT_ROW_SHIFT;
}

/* 8-point column pass (frequency-y -> spatial-y), writing (not adding). */
static void abt_col8(const int64_t col[8], int out[8])
{
    int64_t a0, a1, a2, a3, b0, b1, b2, b3;
    int64_t off = ((int64_t)1 << (ABT_COL_SHIFT - 1)) / ABT_W4;
    a0 = ABT_W4 * (col[0] + off);
    a1 = a0; a2 = a0; a3 = a0;
    a0 += ABT_W2 * col[2]; a1 += ABT_W6 * col[2];
    a2 -= ABT_W6 * col[2]; a3 -= ABT_W2 * col[2];
    b0 = ABT_W1 * col[1]; b1 = ABT_W3 * col[1];
    b2 = ABT_W5 * col[1]; b3 = ABT_W7 * col[1];
    b0 += ABT_W3 * col[3]; b1 -= ABT_W7 * col[3];
    b2 -= ABT_W1 * col[3]; b3 -= ABT_W5 * col[3];
    a0 += ABT_W4 * col[4]; a1 -= ABT_W4 * col[4];
    a2 -= ABT_W4 * col[4]; a3 += ABT_W4 * col[4];
    b0 += ABT_W5 * col[5]; b1 -= ABT_W1 * col[5];
    b2 += ABT_W7 * col[5]; b3 += ABT_W3 * col[5];
    a0 += ABT_W6 * col[6]; a1 -= ABT_W2 * col[6];
    a2 += ABT_W2 * col[6]; a3 -= ABT_W6 * col[6];
    b0 += ABT_W7 * col[7]; b1 -= ABT_W5 * col[7];
    b2 += ABT_W3 * col[7]; b3 -= ABT_W1 * col[7];
    out[0] = (int)((a0 + b0) >> ABT_COL_SHIFT); out[1] = (int)((a1 + b1) >> ABT_COL_SHIFT);
    out[2] = (int)((a2 + b2) >> ABT_COL_SHIFT); out[3] = (int)((a3 + b3) >> ABT_COL_SHIFT);
    out[4] = (int)((a3 - b3) >> ABT_COL_SHIFT); out[5] = (int)((a2 - b2) >> ABT_COL_SHIFT);
    out[6] = (int)((a1 - b1) >> ABT_COL_SHIFT); out[7] = (int)((a0 - b0) >> ABT_COL_SHIFT);
}

/* 4-point row pass, in place. */
static void abt_row4(int64_t row[4])
{
    int64_t a0 = row[0], a1 = row[1], a2 = row[2], a3 = row[3];
    int64_t c0 = (a0 + a2) * ABT_C3 + ((int64_t)1 << (ABT_R_SHIFT - 1));
    int64_t c2 = (a0 - a2) * ABT_C3 + ((int64_t)1 << (ABT_R_SHIFT - 1));
    int64_t c1 = a1 * ABT_R1 + a3 * ABT_R2;
    int64_t c3 = a1 * ABT_R2 - a3 * ABT_R1;
    row[0] = (c0 + c1) >> ABT_R_SHIFT; row[1] = (c2 + c3) >> ABT_R_SHIFT;
    row[2] = (c2 - c3) >> ABT_R_SHIFT; row[3] = (c0 - c1) >> ABT_R_SHIFT;
}

/* 4-point column pass, writing (not adding). */
static void abt_col4(const int64_t col[4], int out[4])
{
    int64_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
    int64_t c0 = (a0 + a2) * ABT_C3 + ((int64_t)1 << (ABT_C_SHIFT - 1));
    int64_t c2 = (a0 - a2) * ABT_C3 + ((int64_t)1 << (ABT_C_SHIFT - 1));
    int64_t c1 = a1 * ABT_C1 + a3 * ABT_C2;
    int64_t c3 = a1 * ABT_C2 - a3 * ABT_C1;
    out[0] = (int)((c0 + c1) >> ABT_C_SHIFT); out[1] = (int)((c2 + c3) >> ABT_C_SHIFT);
    out[2] = (int)((c2 - c3) >> ABT_C_SHIFT); out[3] = (int)((c0 - c1) >> ABT_C_SHIFT);
}

/* coeff rows 0-3 (all 8 cols) -> 4 spatial rows (all 8 cols): 8-pt row IDCT
 * on each of the 4 populated rows, then a genuine 4-pt column IDCT (NOT the
 * normal 8-pt idct_8x8 with the top half zeroed -- a different-sized basis
 * entirely). */
static void idct_8h4v(const int coeff[8][8], int out4[4][8])
{
    int64_t tmp[4][8];
    int v, x;
    for (v = 0; v < 4; v++) {
        for (x = 0; x < 8; x++)
            tmp[v][x] = coeff[v][x];
        abt_row8(tmp[v]);
    }
    for (x = 0; x < 8; x++) {
        int64_t col[4];
        int out[4];
        for (v = 0; v < 4; v++)
            col[v] = tmp[v][x];
        abt_col4(col, out);
        for (v = 0; v < 4; v++)
            out4[v][x] = out[v];
    }
}

/* coeff cols 0-3 (all 8 rows) -> 8 spatial rows (4 cols): 4-pt row IDCT on
 * each of the 8 rows, then a genuine 8-pt column IDCT. */
static void idct_4h8v(const int coeff[8][8], int out8[8][4])
{
    int64_t tmp[8][4];
    int v, x;
    for (v = 0; v < 8; v++) {
        int64_t row[4];
        for (x = 0; x < 4; x++)
            row[x] = coeff[v][x];
        abt_row4(row);
        for (x = 0; x < 4; x++)
            tmp[v][x] = row[x];
    }
    for (x = 0; x < 4; x++) {
        int64_t col[8];
        int out[8];
        for (v = 0; v < 8; v++)
            col[v] = tmp[v][x];
        abt_col8(col, out);
        for (v = 0; v < 8; v++)
            out8[v][x] = out[v];
    }
}

/* ---- H.263 Annex-J-style in-loop deblocking filter --------------------
 * Applied per macroblock, immediately after that macroblock's pixels are
 * finally reconstructed, in raster order (a single causal pass -- later
 * macroblocks see already-filtered pixels from earlier ones as their
 * "top"/"left" neighbours). WMV2 has one fixed qscale per picture (no
 * per-MB delta), so the "neighbour's qp" FFmpeg tracks per-macroblock
 * collapses to "picture qscale, or 0 if that neighbour was skipped" here.
 * Ported verbatim from FFmpeg's h263.c ff_h263_loop_filter() and
 * h263dsp.c's h263_h/v_loop_filter_c(), using this file's own clip8()
 * instead of FFmpeg's sign-trick clamp (identical result, see mr_wmv2.c's
 * header note on this being a mechanical, not creative, transcription). */
static const uint8_t lf_strength[32] = {
    0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7,
    7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12
};

static void lf_edge(int p0, int p1, int p2, int p3, int qscale,
                    int *o0, int *o1, int *o2, int *o3)
{
    int s = lf_strength[qscale & 31];
    int d = (p0 - p3 + 4 * (p2 - p1)) / 8;
    int d1, d2, ad1;
    if (d < -2 * s) d1 = 0;
    else if (d < -s) d1 = -2 * s - d;
    else if (d < s) d1 = d;
    else if (d < 2 * s) d1 = 2 * s - d;
    else d1 = 0;
    p1 += d1; p2 -= d1;
    ad1 = (d1 < 0 ? -d1 : d1) >> 1;
    d2 = (p0 - p3) / 4;
    if (d2 < -ad1) d2 = -ad1; else if (d2 > ad1) d2 = ad1;
    *o0 = clip8(p0 - d2); *o1 = clip8(p1); *o2 = clip8(p2); *o3 = clip8(p3 + d2);
}

/* filters a VERTICAL block boundary: 8 rows, each row's 4 horizontally
 * adjacent pixels straddling the boundary at src[-2..1]. */
static void lf_h(uint8_t *src, int stride, int qscale)
{
    int y, o0, o1, o2, o3;
    for (y = 0; y < 8; y++) {
        uint8_t *p = src + (size_t)y * stride;
        lf_edge(p[-2], p[-1], p[0], p[1], qscale, &o0, &o1, &o2, &o3);
        p[-2] = (uint8_t)o0; p[-1] = (uint8_t)o1; p[0] = (uint8_t)o2; p[1] = (uint8_t)o3;
    }
}

/* filters a HORIZONTAL block boundary: 8 columns, each column's 4
 * vertically adjacent pixels straddling the boundary at src[-2*stride..
 * stride]. */
static void lf_v(uint8_t *src, int stride, int qscale)
{
    int x, o0, o1, o2, o3;
    for (x = 0; x < 8; x++) {
        uint8_t *p = src + x;
        lf_edge(p[-2 * stride], p[-stride], p[0], p[stride], qscale, &o0, &o1, &o2, &o3);
        p[-2 * stride] = (uint8_t)o0; p[-stride] = (uint8_t)o1;
        p[0] = (uint8_t)o2; p[stride] = (uint8_t)o3;
    }
}

/* ---- per-block predictor grid -- identical to mr_wmv.c's (see its design
 * note: reset every picture, dcval sentinel 1024, no slice-boundary reset
 * needed for this bitstream family). ------------------------------------ */
typedef struct {
    int32_t dcval;
    int16_t row[8], col[8];
    uint8_t coded;
} wblk_t;

static void wblk_reset(wblk_t *grid, size_t n)
{
    size_t k;
    memset(grid, 0, n * sizeof(*grid));
    for (k = 0; k < n; k++)
        grid[k].dcval = 1024;
}

static wblk_t *wblk_at(wblk_t *grid, int gw, int gh, int x, int y)
{
    if (x < 0 || y < 0 || x >= gw || y >= gh)
        return NULL;
    return &grid[(size_t)y * gw + x];
}

static int wblk_dcval(wblk_t *grid, int gw, int gh, int x, int y)
{
    wblk_t *p = wblk_at(grid, gw, gh, x, y);
    return p ? (int)p->dcval : 1024;
}

static int wblk_coded(wblk_t *grid, int gw, int gh, int x, int y)
{
    wblk_t *p = wblk_at(grid, gw, gh, x, y);
    return p ? (int)p->coded : 0;
}

typedef struct {
    int x, y;
} mvblk_t;

struct wmv2_ctx {
    int w, h, mb_w, mb_h, cw, ch;
    int ystride, cstride;
    uint8_t *cur[3], *ref[3];
    uint8_t *rgb;
    wblk_t *pl, *pcb, *pcr;
    mvblk_t *mv;
    uint8_t *skip;                  /* bitplane-coded MB skip, mb_w*mb_h  */

    /* per-picture header state */
    int qscale;
    int rl_table_index, rl_chroma_table_index;
    int dc_table_index, mv_table_index;
    int per_mb_rl_table;
    int esc3_level_length, esc3_run_length;
    int cbp_table_index;            /* P-frame: which of wmv2_cbp_tabs[3] */
    int mspel;                      /* P-frame: MSPEL luma MC this pic?   */
    int per_mb_abt;                 /* P-frame: abt_type chosen per-MB?   */
    int abt_type;                   /* current picture/MB-level ABT type  */
    int per_block_abt;              /* current MB: abt_type per-block?    */

    /* extension header (read once at open(), from dec->config) */
    int64_t bit_rate;
    int mspel_bit, loop_filter, abt_flag, j_type_bit, top_left_mv_flag;
    int per_mb_rl_bit;

    int no_rounding;                /* toggles every P-frame, always      */
};

static int decode_esc3(bitreader *b, wmv2_ctx *c, int qscale,
                       int *last, int *run, int *level)
{
    int sign;
    *last = (int)br_bit(b);
    if (c->esc3_level_length == 0) {
        int ll;
        if (qscale < 8) {
            ll = (int)br_bits(b, 3);
            if (ll == 0)
                ll = 8 + (int)br_bit(b);
        } else {
            ll = 2;
            while (ll < 8 && br_peek(b, 1) == 0) {
                ll++;
                br_skip(b, 1);
            }
            if (ll < 8)
                br_skip(b, 1);
        }
        c->esc3_level_length = ll;
        c->esc3_run_length = (int)br_bits(b, 2) + 3;
    }
    *run = (int)br_bits(b, c->esc3_run_length);
    sign = (int)br_bit(b);
    *level = (int)br_bits(b, c->esc3_level_length);
    if (sign)
        *level = -*level;
    return br_overrun(b) ? -1 : 0;
}

/* ---- DC coding -- identical to mr_wmv.c's ----------------------------- */
static int decode_dc_symbol(bitreader *b, int table_index, int chroma, int *out)
{
    int idx;
    if (match_wtab(b, wmv_dc_tab_all[table_index][chroma], 120, &idx))
        return -1;
    if (idx == 119) {
        int v = (int)br_bits(b, 8);
        if (br_bit(b))
            v = -v;
        *out = v;
    } else if (idx != 0) {
        if (br_bit(b))
            idx = -idx;
        *out = idx;
    } else {
        *out = 0;
    }
    return br_overrun(b) ? -1 : 0;
}

static int pred_dc(wblk_t *grid, int gw, int gh, int gx, int gy, int scale,
                   int *dir_ptr)
{
    int a = wblk_dcval(grid, gw, gh, gx - 1, gy);
    int bb = wblk_dcval(grid, gw, gh, gx - 1, gy - 1);
    int cc = wblk_dcval(grid, gw, gh, gx, gy - 1);
    int qa = (a + (scale >> 1)) / scale;
    int qb = (bb + (scale >> 1)) / scale;
    int qc = (cc + (scale >> 1)) / scale;
    if (abs(qa - qb) < abs(qb - qc)) {
        *dir_ptr = 1;
        return qc;
    }
    *dir_ptr = 0;
    return qa;
}

static void select_block(wmv2_ctx *c, int block, int mbx, int mby,
                         wblk_t **grid, int *gw, int *gh, int *gx, int *gy,
                         uint8_t **plane, int *stride, int *px, int *py)
{
    if (block < 4) {
        *grid = c->pl; *gw = c->mb_w * 2; *gh = c->mb_h * 2;
        *gx = mbx * 2 + (block & 1); *gy = mby * 2 + (block >> 1);
        *plane = c->cur[0]; *stride = c->ystride;
        *px = mbx * 16 + (block & 1) * 8;
        *py = mby * 16 + (block >> 1) * 8;
    } else {
        *grid = block == 4 ? c->pcb : c->pcr;
        *gw = c->mb_w; *gh = c->mb_h; *gx = mbx; *gy = mby;
        *plane = block == 4 ? c->cur[1] : c->cur[2];
        *stride = c->cstride; *px = mbx * 8; *py = mby * 8;
    }
}

/* ---- intra block decode + reconstruction -- identical to mr_wmv.c's --- */
static int decode_intra_block(wmv2_ctx *c, bitreader *b, int block,
                              int mbx, int mby, int coded, int ac_pred,
                              const rlgroup_t *rl, int scale, int qscale)
{
    int chroma = block >= 4;
    int serial[64], qf[8][8], coeff[8][8], spatial[8][8];
    int diff, dir_top, pred, u, v;
    wblk_t *grid, *left, *top, *self;
    int gw, gh, gx, gy, stride, px, py;
    uint8_t *plane;
    const uint8_t *scan;

    if (decode_dc_symbol(b, c->dc_table_index, chroma, &diff))
        return -1;

    select_block(c, block, mbx, mby, &grid, &gw, &gh, &gx, &gy,
                &plane, &stride, &px, &py);
    pred = pred_dc(grid, gw, gh, gx, gy, scale, &dir_top);

    memset(serial, 0, sizeof serial);
    serial[0] = pred + diff;
    if (serial[0] < -256 * scale || serial[0] > 256 * scale)
        return -1;

    {
        int pos = 1;
        if (coded) {
            for (;;) {
                int last, run, level, target;
                if (decode_rl_event(b, c, rl, qscale, &last, &run, &level))
                    return -1;
                target = pos + run;
                if (target >= 64) {
                    if (target == 64 && last)
                        break;
                    return -1;
                }
                serial[target] = level;
                pos = target + 1;
                if (last)
                    break;
            }
        }
    }

    scan = ac_pred ? (dir_top ? wmv_scantable_2 : wmv_scantable_3) : wmv_scantable_1;
    for (v = 0; v < 8; v++)
        for (u = 0; u < 8; u++)
            qf[v][u] = 0;
    for (u = 0; u < 64; u++)
        qf[scan[u] >> 3][scan[u] & 7] = serial[u];

    left = wblk_at(grid, gw, gh, gx - 1, gy);
    top  = wblk_at(grid, gw, gh, gx, gy - 1);
    if (ac_pred) {
        if (dir_top) {
            if (top)
                for (u = 1; u < 8; u++)
                    qf[0][u] += top->row[u];
        } else if (left) {
            for (v = 1; v < 8; v++)
                qf[v][0] += left->col[v];
        }
    }

    self = &grid[(size_t)gy * gw + gx];
    self->dcval = (int32_t)(qf[0][0]) * scale;
    for (u = 1; u < 8; u++) self->row[u] = (int16_t)qf[0][u];
    for (v = 1; v < 8; v++) self->col[v] = (int16_t)qf[v][0];
    if (block < 4)
        self->coded = (uint8_t)coded;

    for (v = 0; v < 8; v++) {
        for (u = 0; u < 8; u++) {
            int val = (u == 0 && v == 0) ? qf[0][0] * scale
                                         : dequant_ac(qf[v][u], qscale);
            if (val < -32768) val = -32768;
            if (val > 32767) val = 32767;
            coeff[v][u] = val;
        }
    }
    idct_8x8(coeff, spatial);
    for (v = 0; v < 8; v++) {
        uint8_t *dst = plane + (size_t)(py + v) * stride + px;
        for (u = 0; u < 8; u++)
            dst[u] = (uint8_t)clip8(spatial[v][u]);
    }
    return 0;
}

/* Decodes one inter residual block's coefficients into an 8x8 raster grid
 * via the given scan table, WITHOUT running an IDCT -- shared by the plain
 * (wmv_scantable_0) and both ABT half (wmv2_scantable_a/_b) paths, which
 * differ only in which scan table and which IDCT (idct_8x8 vs idct_8h4v/
 * idct_4h8v) the caller applies afterward. */
static int decode_residual_scan(bitreader *b, wmv2_ctx *c, const rlgroup_t *rl,
                                int qscale, const uint8_t *scan, int coeff[8][8])
{
    int serial[64], pos = 0, u, v;
    memset(serial, 0, sizeof serial);
    for (;;) {
        int last, run, level, target;
        if (decode_rl_event(b, c, rl, qscale, &last, &run, &level))
            return -1;
        target = pos + run;
        if (target >= 64) {
            if (last) break;
            return -1;
        }
        serial[target] = dequant_ac(level, qscale);
        pos = target + 1;
        if (last)
            break;
    }
    for (v = 0; v < 8; v++)
        for (u = 0; u < 8; u++)
            coeff[v][u] = 0;
    for (u = 0; u < 64; u++)
        coeff[scan[u] >> 3][scan[u] & 7] = serial[u];
    return 0;
}

/* Decodes (if coded) and adds one inter residual block to its prediction,
 * dispatching on this block's Adaptive Block Transform type -- 0 (plain
 * 8x8), 1 (two 8x4 halves, top+bottom) or 2 (two 4x8 halves, left+right).
 * abt_type itself may need to be read here (one VLC symbol) when this MB's
 * per_block_abt is active; see decode_picture()'s secondary-header note. */
static int wmv2_add_inter_block(wmv2_ctx *c, bitreader *b, int cbp_bit,
                                int qscale, uint8_t *dst, int stride,
                                int pred[8][8])
{
    const rlgroup_t *rl = &rl_inter_tabs[c->rl_table_index];
    int y, x;

    if (!cbp_bit) {
        for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
                dst[(size_t)y * stride + x] = (uint8_t)clip8(pred[y][x]);
        return 0;
    }

    {
        int abt_type = c->per_block_abt ? decode012(b) : c->abt_type;

        if (abt_type == 0) {
            int coeff[8][8], res[8][8];
            if (decode_residual_scan(b, c, rl, qscale, wmv_scantable_0, coeff))
                return -1;
            idct_8x8(coeff, res);
            for (y = 0; y < 8; y++)
                for (x = 0; x < 8; x++)
                    dst[(size_t)y * stride + x] = (uint8_t)clip8(pred[y][x] + res[y][x]);
            return 0;
        }

        {
            static const int sub_cbp_table[3] = { 2, 3, 1 };
            const uint8_t *scan = abt_type == 1 ? wmv2_scantable_a : wmv2_scantable_b;
            int sub_cbp = sub_cbp_table[decode012(b)];
            int coeff1[8][8], coeff2[8][8];

            memset(coeff1, 0, sizeof coeff1);
            memset(coeff2, 0, sizeof coeff2);
            if (sub_cbp & 1) {
                if (decode_residual_scan(b, c, rl, qscale, scan, coeff1))
                    return -1;
            }
            if (sub_cbp & 2) {
                if (decode_residual_scan(b, c, rl, qscale, scan, coeff2))
                    return -1;
            }

            if (abt_type == 1) {
                int out1[4][8], out2[4][8];
                idct_8h4v(coeff1, out1);
                idct_8h4v(coeff2, out2);
                for (y = 0; y < 4; y++)
                    for (x = 0; x < 8; x++)
                        dst[(size_t)y * stride + x] =
                            (uint8_t)clip8(pred[y][x] + out1[y][x]);
                for (y = 0; y < 4; y++)
                    for (x = 0; x < 8; x++)
                        dst[(size_t)(y + 4) * stride + x] =
                            (uint8_t)clip8(pred[y + 4][x] + out2[y][x]);
            } else {
                int out1[8][4], out2[8][4];
                idct_4h8v(coeff1, out1);
                idct_4h8v(coeff2, out2);
                for (y = 0; y < 8; y++)
                    for (x = 0; x < 4; x++)
                        dst[(size_t)y * stride + x] =
                            (uint8_t)clip8(pred[y][x] + out1[y][x]);
                for (y = 0; y < 8; y++)
                    for (x = 0; x < 4; x++)
                        dst[(size_t)y * stride + x + 4] =
                            (uint8_t)clip8(pred[y][x + 4] + out2[y][x]);
            }
        }
    }
    return br_overrun(b) ? -1 : 0;
}

/* ---- motion vectors ----------------------------------------------------
 * The combined MV VLC tables/search are identical to mr_wmv.c's; only the
 * *predictor* (this function) and the extra MSPEL "hshift" refinement bit
 * (wmv2_decode_motion(), below) are WMV2-specific. */
static void wmv2_motion_predict(bitreader *b, const mvblk_t *mv, int mb_w,
                                int mbx, int mby, int top_left_mv_flag,
                                int mspel, int *px, int *py)
{
    const mvblk_t *left = mbx > 0 ? &mv[mby * mb_w + mbx - 1] : NULL;
    const mvblk_t *top = mby > 0 ? &mv[(mby - 1) * mb_w + mbx] : NULL;
    const mvblk_t *tr = (mby > 0 && mbx + 1 < mb_w) ? &mv[(mby - 1) * mb_w + mbx + 1] : NULL;
    int lx = left ? left->x : 0, ly = left ? left->y : 0;
    int diff, type;

    if (mbx > 0 && mby > 0 && !mspel && top_left_mv_flag) {
        int dx = lx - top->x, dy = ly - top->y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        diff = dx > dy ? dx : dy;
    } else {
        diff = 0;
    }

    if (diff >= 8)
        type = (int)br_bit(b);
    else
        type = 2;

    if (type == 0) {
        *px = lx; *py = ly;
    } else if (type == 1) {
        *px = top->x; *py = top->y;
    } else if (mby == 0) {
        *px = lx; *py = ly;
    } else {
        *px = median3(lx, top->x, tr ? tr->x : 0);
        *py = median3(ly, top->y, tr ? tr->y : 0);
    }
}

static int decode_motion(bitreader *b, int table_index, int predx, int predy,
                         int *outx, int *outy)
{
    const wmvmv_t *tab = table_index ? wmv_mv_tab1 : wmv_mv_tab0;
    unsigned w = br_peek(b, WMV_MV_KEY_BITS);
    int lo = 0, hi = 1099, found = -1, mx, my;
    unsigned sym;

    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (tab[mid].code <= w) { found = mid; lo = mid + 1; }
        else hi = mid - 1;
    }
    if (found < 0)
        return -1;
    if ((w >> (WMV_MV_KEY_BITS - tab[found].len)) !=
        (tab[found].code >> (WMV_MV_KEY_BITS - tab[found].len)))
        return -1;
    br_skip(b, tab[found].len);

    sym = tab[found].sym;
    if (sym == 0) {
        mx = (int)br_bits(b, 6);
        my = (int)br_bits(b, 6);
    } else {
        mx = (int)(sym >> 8);
        my = (int)(sym & 0xff);
    }
    mx += predx - 32;
    my += predy - 32;
    if (mx <= -64) mx += 64; else if (mx >= 64) mx -= 64;
    if (my <= -64) my += 64; else if (my >= 64) my -= 64;
    *outx = mx; *outy = my;
    return br_overrun(b) ? -1 : 0;
}

/* Decodes the base MV, then (only when this picture has MSPEL active and
 * the MV has a half-pel component) the extra hshift refinement bit that
 * selects between two MSPEL interpolation "flavours" for that same
 * nominal half-pel position -- see mspel_mc_block()'s dxy convention. */
static int wmv2_decode_motion(bitreader *b, wmv2_ctx *c, int predx, int predy,
                              int *outx, int *outy, int *hshift)
{
    if (decode_motion(b, c->mv_table_index, predx, predy, outx, outy))
        return -1;
    if (((*outx | *outy) & 1) && c->mspel)
        *hshift = (int)br_bit(b);
    else
        *hshift = 0;
    return br_overrun(b) ? -1 : 0;
}

/* ---- one non-skip inter macroblock: motion compensation + residual --- */
static int reconstruct_inter_mb(wmv2_ctx *c, bitreader *b, int mbx, int mby,
                                int cbp, int qscale, int mvx, int mvy, int hshift)
{
    int block;
    for (block = 0; block < 6; block++) {
        int chroma = block >= 4;
        int plane_no = chroma ? block - 3 : 0;
        uint8_t *plane = c->cur[plane_no];
        const uint8_t *ref = c->ref[plane_no];
        int stride = chroma ? c->cstride : c->ystride;
        int rw = chroma ? c->cw / 2 : c->cw;
        int rh = chroma ? c->ch / 2 : c->ch;
        int px = chroma ? mbx * 8 : mbx * 16 + (block & 1) * 8;
        int py = chroma ? mby * 8 : mby * 16 + (block >> 1) * 8;
        int pred[8][8];
        int coded = (cbp >> (5 - block)) & 1;
        uint8_t *dst = plane + (size_t)py * stride + px;

        if (!chroma && c->mspel) {
            int ix = floor_div(mvx, 2), iy = floor_div(mvy, 2);
            int dxy = 2 * (((mvy & 1) << 1) | (mvx & 1)) + hshift;
            mspel_mc_block(ref, rw, rh, stride, px + ix, py + iy, dxy, pred);
        } else {
            int bx = chroma ? chroma_mv(mvx) : mvx;
            int by = chroma ? chroma_mv(mvy) : mvy;
            mc_block(ref, rw, rh, stride, px, py, bx, by, c->no_rounding, pred);
        }

        if (wmv2_add_inter_block(c, b, coded, qscale, dst, stride, pred)) {
            if (wmv2_debug())
                fprintf(stderr, "[wmv2] inter residual failed at MB %d,%d block %d\n",
                        mbx, mby, block);
            return -1;
        }
    }
    return 0;
}

/* ---- bitplane-coded macroblock skip -----------------------------------
 * A whole skip/non-skip map for the picture, read up front (instead of one
 * inline bit per macroblock like WMV1) in one of four schemes. Ported
 * verbatim from FFmpeg's wmv2dec.c parse_mb_skip(). */
#define WMV2_SKIP_NONE 0
#define WMV2_SKIP_MPEG 1
#define WMV2_SKIP_ROW  2
#define WMV2_SKIP_COL  3

static int parse_mb_skip(bitreader *b, uint8_t *skip, int mb_w, int mb_h)
{
    int skip_type = (int)br_bits(b, 2);
    int x, y;
    switch (skip_type) {
    case WMV2_SKIP_NONE:
        memset(skip, 0, (size_t)mb_w * mb_h);
        break;
    case WMV2_SKIP_MPEG:
        for (y = 0; y < mb_h; y++)
            for (x = 0; x < mb_w; x++)
                skip[y * mb_w + x] = (uint8_t)br_bit(b);
        break;
    case WMV2_SKIP_ROW:
        for (y = 0; y < mb_h; y++) {
            if (br_bit(b)) {
                for (x = 0; x < mb_w; x++)
                    skip[y * mb_w + x] = 1;
            } else {
                for (x = 0; x < mb_w; x++)
                    skip[y * mb_w + x] = (uint8_t)br_bit(b);
            }
        }
        break;
    default: /* WMV2_SKIP_COL */
        for (x = 0; x < mb_w; x++) {
            if (br_bit(b)) {
                for (y = 0; y < mb_h; y++)
                    skip[y * mb_w + x] = 1;
            } else {
                for (y = 0; y < mb_h; y++)
                    skip[y * mb_w + x] = (uint8_t)br_bit(b);
            }
        }
        break;
    }
    return br_overrun(b) ? -1 : 0;
}

/* A whole-picture "everything skipped" pattern: a peeked flag bit followed
 * by an all-ones escape run across the skip bitplane's full width/height.
 * When matched, this picture carries no macroblock data at all -- the
 * decoded frame is simply the previous one, unchanged. Ported verbatim
 * from FFmpeg's wmv2_decode_picture_header(). */
static int frame_entirely_skipped(bitreader *b, int mb_w, int mb_h)
{
    bitreader t;
    int skip_type, run;
    if (br_peek(b, 1) == 0)
        return 0;
    t = *b;
    skip_type = (int)br_bits(&t, 2);
    run = skip_type == WMV2_SKIP_COL ? mb_w : mb_h;
    while (run > 0) {
        int block = run < 25 ? run : 25;
        unsigned v = br_bits(&t, block);
        if (v + 1 != (1u << block))
            return 0;
        run -= block;
    }
    return 1;
}

/* ---- per-macroblock loop filter orchestration -------------------------
 * See this file's header comment on lf_h/lf_v above: WMV2's single
 * picture-wide qscale collapses FFmpeg's per-macroblock qscale_table
 * lookups to "qscale, or 0 if that neighbour macroblock was skipped". */
static void wmv2_loop_filter_mb(wmv2_ctx *c, int mbx, int mby, int qscale)
{
    int mb_w = c->mb_w;
    int xy = mby * mb_w + mbx;
    uint8_t *dy = c->cur[0] + (size_t)mby * 16 * c->ystride + (size_t)mbx * 16;
    uint8_t *dcb = c->cur[1] + (size_t)mby * 8 * c->cstride + (size_t)mbx * 8;
    uint8_t *dcr = c->cur[2] + (size_t)mby * 8 * c->cstride + (size_t)mbx * 8;
    int qp_c;

    if (!c->skip[xy]) {
        qp_c = qscale;
        lf_v(dy + 8 * c->ystride, c->ystride, qp_c);
        lf_v(dy + 8 * c->ystride + 8, c->ystride, qp_c);
    } else {
        qp_c = 0;
    }

    if (mby) {
        int qp_tt = c->skip[xy - mb_w] ? 0 : qscale;
        int qp_tc = qp_c ? qp_c : qp_tt;
        if (qp_tc) {
            lf_v(dy, c->ystride, qp_tc);
            lf_v(dy + 8, c->ystride, qp_tc);
            lf_v(dcb, c->cstride, qp_tc);
            lf_v(dcr, c->cstride, qp_tc);
        }
        if (qp_tt)
            lf_h(dy - 8 * c->ystride + 8, c->ystride, qp_tt);
        if (mbx) {
            int qp_dt = (qp_tt || c->skip[xy - 1 - mb_w]) ? qp_tt : qscale;
            if (qp_dt) {
                lf_h(dy - 8 * c->ystride, c->ystride, qp_dt);
                lf_h(dcb - 8 * c->cstride, c->cstride, qp_dt);
                lf_h(dcr - 8 * c->cstride, c->cstride, qp_dt);
            }
        }
    }

    if (qp_c) {
        lf_h(dy + 8, c->ystride, qp_c);
        if (mby + 1 == c->mb_h)
            lf_h(dy + 8 * c->ystride + 8, c->ystride, qp_c);
    }

    if (mbx) {
        int qp_lc = (qp_c || c->skip[xy - 1]) ? qp_c : qscale;
        if (qp_lc) {
            lf_h(dy, c->ystride, qp_lc);
            if (mby + 1 == c->mb_h) {
                lf_h(dy + 8 * c->ystride, c->ystride, qp_lc);
                lf_h(dcb, c->cstride, qp_lc);
                lf_h(dcr, c->cstride, qp_lc);
            }
        }
    }
}

/* ---- picture decode ----------------------------------------------------- */
static int decode_picture(wmv2_ctx *c, bitreader *b)
{
    int picture_type = (int)br_bit(b) + 1; /* 1=I, 2=P */
    int qscale;
    int y_scale, c_scale;
    int mbx, mby;

    if (picture_type == 1)
        br_skip(b, 7); /* per-picture I-frame marker; not otherwise used */
    qscale = (int)br_bits(b, 5);
    if (qscale <= 0)
        return -1;

    if (picture_type == 2 && frame_entirely_skipped(b, c->mb_w, c->mb_h)) {
        int i;
        for (i = 0; i < 3; i++) {
            size_t size = i == 0 ? (size_t)c->ystride * c->ch
                                 : (size_t)c->cstride * (c->ch >> 1);
            memcpy(c->cur[i], c->ref[i], size);
        }
        return 0;
    }

    c->qscale = qscale;
    y_scale = wmv_y_dc_scale[qscale];
    c_scale = wmv_c_dc_scale[qscale];

    if (picture_type == 1) {
        /* IntraX8 ("J-frame") coding: a completely separate intra-only
         * sub-codec (shared with VC-1), selectable per I-frame when the
         * extension header's j_type_bit capability is set. Not
         * implemented -- explicitly rejected, see this file's header
         * note (matches mr_wmv.c's inter_intra_pred rejection). */
        if (c->j_type_bit && br_bit(b))
            return -1;
        c->per_mb_rl_table = c->per_mb_rl_bit ? (int)br_bit(b) : 0;
        if (!c->per_mb_rl_table) {
            c->rl_chroma_table_index = decode012(b);
            c->rl_table_index = decode012(b);
        }
        c->dc_table_index = (int)br_bit(b);
        c->no_rounding = 1;
    } else {
        int cbp_index;
        if (parse_mb_skip(b, c->skip, c->mb_w, c->mb_h))
            return -1;
        cbp_index = decode012(b);
        c->cbp_table_index = wmv2_cbp_table_index(qscale, cbp_index);

        c->mspel = c->mspel_bit ? (int)br_bit(b) : 0;

        c->per_mb_abt = 0;
        c->abt_type = 0;
        if (c->abt_flag) {
            c->per_mb_abt = (int)br_bit(b) ^ 1;
            if (!c->per_mb_abt)
                c->abt_type = decode012(b);
        }

        c->per_mb_rl_table = c->per_mb_rl_bit ? (int)br_bit(b) : 0;
        if (!c->per_mb_rl_table) {
            c->rl_table_index = decode012(b);
            c->rl_chroma_table_index = c->rl_table_index;
        }

        c->dc_table_index = (int)br_bit(b);
        c->mv_table_index = (int)br_bit(b);

        c->no_rounding ^= 1;
    }
    c->esc3_level_length = 0;
    c->esc3_run_length = 0;
    if (br_overrun(b))
        return -1;

    wblk_reset(c->pl, (size_t)c->mb_w * 2 * c->mb_h * 2);
    wblk_reset(c->pcb, (size_t)c->mb_w * c->mb_h);
    wblk_reset(c->pcr, (size_t)c->mb_w * c->mb_h);
    memset(c->mv, 0, (size_t)c->mb_w * c->mb_h * sizeof(*c->mv));
    if (picture_type == 1)
        memset(c->skip, 0, (size_t)c->mb_w * c->mb_h);

    for (mby = 0; mby < c->mb_h; mby++) {
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            int block;
            mvblk_t *mv = &c->mv[mby * c->mb_w + mbx];

            if (picture_type == 2 && c->skip[mby * c->mb_w + mbx]) {
                mv->x = mv->y = 0;
                {
                    int y, x;
                    for (block = 0; block < 6; block++) {
                        int chroma = block >= 4;
                        int plane_no = chroma ? block - 3 : 0;
                        uint8_t *plane = c->cur[plane_no];
                        const uint8_t *ref = c->ref[plane_no];
                        int stride = chroma ? c->cstride : c->ystride;
                        int px = chroma ? mbx * 8 : mbx * 16 + (block & 1) * 8;
                        int py = chroma ? mby * 8 : mby * 16 + (block >> 1) * 8;
                        for (y = 0; y < 8; y++)
                            for (x = 0; x < 8; x++)
                                plane[(size_t)(py + y) * stride + px + x] =
                                    ref[(size_t)(py + y) * stride + px + x];
                    }
                }
                if (c->loop_filter)
                    wmv2_loop_filter_mb(c, mbx, mby, qscale);
                continue;
            }

            if (picture_type == 1) {
                int code, cbp, ac_pred;
                if (match_wtab(b, wmv_mbi_tab, 64, &code))
                    return -1;
                cbp = 0;
                {
                    int i;
                    for (i = 0; i < 6; i++) {
                        int val = (code >> (5 - i)) & 1;
                        if (i < 4) {
                            int gx = mbx * 2 + (i & 1), gy = mby * 2 + (i >> 1);
                            int gw = c->mb_w * 2, gh = c->mb_h * 2;
                            int a = wblk_coded(c->pl, gw, gh, gx - 1, gy);
                            int bb = wblk_coded(c->pl, gw, gh, gx - 1, gy - 1);
                            int cc = wblk_coded(c->pl, gw, gh, gx, gy - 1);
                            int predbit = (bb == cc) ? a : cc;
                            val ^= predbit;
                            c->pl[(size_t)gy * gw + gx].coded = (uint8_t)val;
                        }
                        cbp |= val << (5 - i);
                    }
                }
                ac_pred = (int)br_bit(b);
                if (c->per_mb_rl_table && cbp) {
                    c->rl_table_index = decode012(b);
                    c->rl_chroma_table_index = c->rl_table_index;
                }
                mv->x = mv->y = 0;
                for (block = 0; block < 6; block++) {
                    int coded = (cbp >> (5 - block)) & 1;
                    int chroma = block >= 4;
                    const rlgroup_t *rl = chroma ? &rl_inter_tabs[c->rl_chroma_table_index]
                                                 : &rl_intra_tabs[c->rl_table_index];
                    int scale = chroma ? c_scale : y_scale;
                    if (decode_intra_block(c, b, block, mbx, mby, coded, ac_pred,
                                           rl, scale, qscale)) {
                        if (wmv2_debug())
                            fprintf(stderr, "[wmv2] intra block %d failed at MB %d,%d\n",
                                    block, mbx, mby);
                        return -1;
                    }
                }
            } else {
                int code, mb_intra, cbp;
                if (match_wtab(b, wmv2_cbp_tabs[c->cbp_table_index], 128, &code))
                    return -1;
                mb_intra = (~code & 0x40) != 0;
                cbp = code & 0x3f;

                if (mb_intra) {
                    int ac_pred = (int)br_bit(b);
                    if (c->per_mb_rl_table && cbp) {
                        c->rl_table_index = decode012(b);
                        c->rl_chroma_table_index = c->rl_table_index;
                    }
                    mv->x = mv->y = 0;
                    for (block = 0; block < 6; block++) {
                        int coded = (cbp >> (5 - block)) & 1;
                        int chroma = block >= 4;
                        const rlgroup_t *rl = chroma ? &rl_inter_tabs[c->rl_chroma_table_index]
                                                     : &rl_intra_tabs[c->rl_table_index];
                        int scale = chroma ? c_scale : y_scale;
                        if (decode_intra_block(c, b, block, mbx, mby, coded, ac_pred,
                                               rl, scale, qscale)) {
                            if (wmv2_debug())
                                fprintf(stderr, "[wmv2] intra block %d failed at MB %d,%d (P-frame)\n",
                                        block, mbx, mby);
                            return -1;
                        }
                    }
                } else {
                    int predx, predy, mvx, mvy, hshift;
                    if (c->per_mb_rl_table && cbp) {
                        c->rl_table_index = decode012(b);
                        c->rl_chroma_table_index = c->rl_table_index;
                    }
                    if (cbp) {
                        if (c->abt_flag && c->per_mb_abt) {
                            c->per_block_abt = (int)br_bit(b);
                            if (!c->per_block_abt)
                                c->abt_type = decode012(b);
                        } else {
                            c->per_block_abt = 0;
                        }
                    } else {
                        c->per_block_abt = 0;
                    }
                    wmv2_motion_predict(b, c->mv, c->mb_w, mbx, mby,
                                       c->top_left_mv_flag, c->mspel, &predx, &predy);
                    if (wmv2_decode_motion(b, c, predx, predy, &mvx, &mvy, &hshift))
                        return -1;
                    mv->x = mvx; mv->y = mvy;
                    if (reconstruct_inter_mb(c, b, mbx, mby, cbp, qscale, mvx, mvy, hshift))
                        return -1;
                }
            }
            if (c->loop_filter)
                wmv2_loop_filter_mb(c, mbx, mby, qscale);
            if (br_overrun(b))
                return -1;
        }
    }
    return 0;
}

static void yuv_to_rgb(wmv2_ctx *c)
{
    int x, y;
    for (y = 0; y < c->h; y++) {
        const uint8_t *yl = c->cur[0] + (size_t)y * c->ystride;
        const uint8_t *cb = c->cur[1] + (size_t)(y >> 1) * c->cstride;
        const uint8_t *cr = c->cur[2] + (size_t)(y >> 1) * c->cstride;
        uint8_t *dst = c->rgb + (size_t)y * c->w * 3;
        for (x = 0; x < c->w; x++) {
            int yy = yl[x] - 16;
            int u = cb[x >> 1] - 128;
            int v = cr[x >> 1] - 128;
            int r = (298 * yy + 409 * v + 128) >> 8;
            int g = (298 * yy - 100 * u - 208 * v + 128) >> 8;
            int bb = (298 * yy + 516 * u + 128) >> 8;
            *dst++ = (uint8_t)clip8(r);
            *dst++ = (uint8_t)clip8(g);
            *dst++ = (uint8_t)clip8(bb);
        }
    }
}

/* ---- codec lifecycle ---------------------------------------------------- */
static mr_status wmv2_open(mr_decoder *dec)
{
    wmv2_ctx *c;
    bitreader eb;
    int i, ext_code;

    /* Extension header: 4 bytes (fps(5)+bitrate(11)+7 capability/count
     * bits, see decode_ext_header() in this file's header comment) live in
     * the AVI BITMAPINFOHEADER's trailing bytes, exposed here as
     * dec->config with mr_avi.c's own 2-byte bits-per-pixel prefix ahead
     * of them (see mr_avi.c's strf parsing). Unlike WMV1, this is NOT in
     * the per-frame bitstream, so it must be available at open() time. */
    if (!dec->config || dec->config_len < 6)
        return MR_EFORMAT;

    c = (wmv2_ctx *)calloc(1, sizeof(*c));
    if (!c)
        return MR_ENOMEM;

    br_init(&eb, dec->config + 2, (int)(dec->config_len - 2));
    br_skip(&eb, 5); /* fps, unused by this decoder */
    c->bit_rate = (int64_t)br_bits(&eb, 11) * 1024;
    c->mspel_bit = (int)br_bit(&eb);
    c->loop_filter = (int)br_bit(&eb);
    c->abt_flag = (int)br_bit(&eb);
    c->j_type_bit = (int)br_bit(&eb);
    c->top_left_mv_flag = (int)br_bit(&eb);
    c->per_mb_rl_bit = (int)br_bit(&eb);
    ext_code = (int)br_bits(&eb, 3);
    if (ext_code == 0 || br_overrun(&eb)) {
        free(c);
        return MR_EFORMAT;
    }

    c->w = dec->width; c->h = dec->height;
    c->mb_w = (c->w + 15) >> 4; c->mb_h = (c->h + 15) >> 4;
    c->cw = c->mb_w * 16; c->ch = c->mb_h * 16;
    c->ystride = c->cw; c->cstride = c->cw >> 1;
    for (i = 0; i < 3; i++) {
        size_t size = i == 0 ? (size_t)c->ystride * c->ch
                             : (size_t)c->cstride * (c->ch >> 1);
        c->cur[i] = (uint8_t *)malloc(size);
        c->ref[i] = (uint8_t *)malloc(size);
        if (!c->cur[i] || !c->ref[i])
            goto oom;
        memset(c->cur[i], i ? 128 : 16, size);
        memset(c->ref[i], i ? 128 : 16, size);
    }
    c->rgb = (uint8_t *)malloc((size_t)c->w * c->h * 3);
    c->pl = (wblk_t *)calloc((size_t)c->mb_w * 2 * c->mb_h * 2, sizeof(*c->pl));
    c->pcb = (wblk_t *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->pcb));
    c->pcr = (wblk_t *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->pcr));
    c->mv = (mvblk_t *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->mv));
    c->skip = (uint8_t *)calloc((size_t)c->mb_w * c->mb_h, 1);
    if (!c->rgb || !c->pl || !c->pcb || !c->pcr || !c->mv || !c->skip)
        goto oom;

    dec->priv = c;
    dec->frame.width = c->w; dec->frame.height = c->h;
    dec->frame.fmt = MR_PIX_RGB24; dec->frame.stride = c->w * 3;
    dec->frame.data = c->rgb;
    dec->frame.dirty_y0 = 0; dec->frame.dirty_y1 = c->h;
    return MR_OK;

oom:
    for (i = 0; i < 3; i++) {
        free(c->cur[i]);
        free(c->ref[i]);
    }
    free(c->rgb); free(c->pl); free(c->pcb); free(c->pcr); free(c->mv); free(c->skip);
    free(c);
    return MR_ENOMEM;
}

static mr_status wmv2_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    wmv2_ctx *c = (wmv2_ctx *)dec->priv;
    bitreader br;
    int i;
    if (!c || !data || len == 0 || len > 0x7fffffffUL)
        return MR_EFORMAT;
    br_init(&br, data, (int)len);
    if (decode_picture(c, &br))
        return MR_EFORMAT;
    yuv_to_rgb(c);
    for (i = 0; i < 3; i++) {
        uint8_t *tmp = c->ref[i];
        c->ref[i] = c->cur[i];
        c->cur[i] = tmp;
    }
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = c->h;
    return MR_OK;
}

static void wmv2_close(mr_decoder *dec)
{
    wmv2_ctx *c = (wmv2_ctx *)dec->priv;
    int i;
    if (!c)
        return;
    for (i = 0; i < 3; i++) {
        free(c->cur[i]);
        free(c->ref[i]);
    }
    free(c->rgb); free(c->pl); free(c->pcb); free(c->pcr); free(c->mv); free(c->skip);
    free(c);
    dec->priv = NULL;
}

const mr_codec mr_codec_wmv2 = {
    "wmv2",
    { MR_FOURCC('W','M','V','2'), MR_FOURCC('w','m','v','2') },
    wmv2_open,
    wmv2_decode,
    wmv2_close,
    NULL
};
