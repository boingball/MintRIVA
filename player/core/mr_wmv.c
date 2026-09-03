/*
 * MintVID - WMV1 (Windows Media Video 7 / MSMPEG4 version 3) decoder.
 *
 * WMV1's bitstream shares its macroblock/IDCT/motion-compensation skeleton
 * with MSMPEG4v2 (mr_msmpeg4v2.c) but adds: three selectable run/level VLC
 * table sets (instead of one fixed one), two selectable DC VLC tables, two
 * selectable combined motion-vector VLC tables, coded-block-pattern
 * *prediction* on I-frames, a per-frame adaptive "escape 3" coefficient
 * encoding, and alternating ("flipflop") motion-compensation rounding.
 * The picture-header/macroblock/VLC/quantiser semantics were cross-checked
 * against FFmpeg's LGPL msmpeg4dec.c/msmpeg4.c/wmv1 decoder; the VLC/scan/
 * scale tables in mr_wmv_tables.inc are mechanically extracted (see
 * gen_wmv_tables.py) from FFmpeg's msmpeg4data.c/msmpeg4_vc1_data.c, since
 * those bit patterns are fixed by the bitstream format itself, not FFmpeg's
 * expression of it.
 *
 * Not implemented: the "inter_intra_pred" spatial-domain DC/AC prediction
 * variant used only for very low-bitrate (<=128kbit/s), small (<320x240)
 * streams. Such streams are explicitly rejected (MR_EFORMAT) rather than
 * silently decoded wrong, matching this project's existing compatibility
 * policy for unsupported bitstream tools (see DESIGN.md).
 */
#include "mr_wmv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- bit reader (MSB first) ------------------------------------------ */
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

static int wmv_debug(void)
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

/* ---- table types (must match mr_mpeg4_tables.inc's for the shared,
 * reused "mid rate" RL tables; wtab_t/wmvmv_t are this file's own, wider,
 * types for WMV's larger codewords -- see mr_wmv_tables.inc). ---------- */
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
#include "mr_wmv_tables.inc"     /* WMV1-specific VLC/RL/DC/MV/scan tables */

/* ---- run/level ("RL") coefficient table groups ------------------------
 * Each group is either a WMV-specific table (its own dedicated escape
 * codeword) or the shared MPEG-4/H.263 "mid rate" table already in this
 * repo (rl_table_index==2), whose escape codeword is the standard H.263
 * fixed pattern "0000011" (7 bits) -- mr_msmpeg4v2.c already relies on
 * that same constant. */
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

/* ---- generic VLC match helpers ---------------------------------------- */

/* Linear scan (tables here run 4..185 entries; correctness first, matching
 * this project's own history of shipping a linear scan before adding a
 * LUT once profiling on hardware justified it -- see mr_msmpeg4v2.c). */
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

/* Matches the run/level/last VLC only, WITHOUT its trailing sign bit --
 * used as the "base" coefficient inside escape 1/2, where FFmpeg extends
 * the *unsigned* magnitude first and reads exactly one sign bit afterward,
 * applied to the combined value (not one sign bit per sub-fetch). */
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

/* ---- adaptive "escape 3" coefficient coding (WMV1/WMV2) --------------- *
 * The level/run bit-widths are decided by the FIRST escape-3 event in a
 * picture and then held fixed for the rest of that picture (state lives in
 * wmv_ctx, reset once per picture). */
typedef struct wmv_ctx wmv_ctx;

static int decode_esc3(bitreader *b, wmv_ctx *c, int qscale,
                       int *last, int *run, int *level);

static int decode_rl_event(bitreader *b, wmv_ctx *c, const rlgroup_t *g,
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
        *level += add;                     /* still unsigned here */
        if (br_bit(b))
            *level = -*level;              /* one sign bit for the combined value */
    } else if (br_bit(b)) {                /* escape 2: extend RUN */
        int add;
        if (match_tcoef_base(b, g->tab, g->count, last, run, level))
            return -1;
        add = g->maxrun[*last][*level];    /* *level is still unsigned magnitude */
        /* Matches mr_msmpeg4v2.c's escape 2 exactly (byte-verified against
         * FFmpeg's own internal coefficient trace): a prior attempt to add
         * FFmpeg's "run_diff" term here as a *second* +1 on top of this was
         * wrong -- it double-counted a bias already folded into this "+1"
         * and shifted every escape-2 coefficient one slot too far. */
        *run += add + 1;
        if (br_bit(b))
            *level = -*level;
    } else {                               /* escape 3 */
        if (decode_esc3(b, c, qscale, last, run, level))
            return -1;
    }
    return br_overrun(b) ? -1 : 0;
}

/* ---- transform, prediction and reconstruction (shared with mr_msmpeg4v2.c
 * by algorithm, not by object file -- each codec plugin is self-contained
 * per this project's convention; see mr_msmpeg4v2.c for provenance notes
 * on the integer IDCT). ------------------------------------------------- */
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
 * filter, alternated frame-to-frame when the bitstream's flipflop_rounding
 * flag is set -- see decode_picture(). */
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

/* ---- per-block predictor grid ----------------------------------------
 * One cell per 8x8 block. Reset once at the start of EVERY picture
 * (wblk_reset(), called from decode_picture) -- a macroblock coded inter
 * this frame never writes here, so without a per-picture reset an intra
 * MB's neighbour lookup would see a stale DC value left over from
 * whichever earlier picture last wrote that grid cell, instead of
 * FFmpeg's neutral per-frame default. dcval's "no data yet" sentinel is
 * 1024 (== neutral level 128 at the smallest DC scale, 8), applied both
 * by wblk_reset() and by wblk_at()/wblk_dcval() for genuinely
 * out-of-bounds (edge-of-frame) neighbours. row/col/coded default to 0.
 * WMV1 does not reset any of this at intra-frame slice boundaries
 * (unlike MSMPEG4 v1-v3), so, unlike mr_msmpeg4v2.c's predictor grid, no
 * slice tag is needed at all -- see mr_wmv.c's design notes in this
 * file. */
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

struct wmv_ctx {
    int w, h, mb_w, mb_h, cw, ch;
    int ystride, cstride;
    uint8_t *cur[3], *ref[3];
    uint8_t *rgb;
    wblk_t *pl, *pcb, *pcr;         /* luma (2x2/mb) + chroma (1/mb) grids */
    mvblk_t *mv;

    /* per-picture header state */
    int qscale;
    int rl_table_index, rl_chroma_table_index;
    int dc_table_index, mv_table_index;
    int per_mb_rl_table, use_skip_mb_code;
    int esc3_level_length, esc3_run_length;

    /* persists across pictures within a stream */
    int64_t bit_rate;
    int flipflop_rounding;
    int no_rounding;
};

static int decode_esc3(bitreader *b, wmv_ctx *c, int qscale,
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

/* ---- DC coding ---------------------------------------------------------
 * Combined size+level VLC (0..118 direct, 119 = 8-bit escape), distinct
 * from mr_msmpeg4v2.c's H.263-derived size/mantissa scheme. */
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

/* dir=0 => predicted from the left neighbour, dir=1 => from top. WMV1 uses
 * a strict "<" comparison here (mr_msmpeg4v2.c / plain MSMPEG4v3 use "<="
 * -- a genuine, easy-to-miss difference in the bitstream family). */
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

/* ---- predictor-grid block selection (mirrors mr_msmpeg4v2.c) --------- */
static void select_block(wmv_ctx *c, int block, int mbx, int mby,
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

/* ---- intra block decode + reconstruction ------------------------------ */
static int decode_intra_block(wmv_ctx *c, bitreader *b, int block,
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
    /* Matches ffmpeg's own DC overflow bound ("level > 256*dc_scale"), not
     * an arbitrary flat one -- WMV1's dc scale (up to 22) makes a flat
     * +-2048 bound reject legitimate high-qscale DC levels. */
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
                    /* Some encoders (confirmed against ffmpeg's WMV1 output)
                     * terminate a block with its final coefficient one slot
                     * past 63; matching mr_msmpeg4v2.c's inter-residual
                     * tolerance, accept and drop it when last=1. */
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
            /* FFmpeg stores dequantized coefficients in a plain int16_t with
             * no extra clamp at this stage (see dct_unquantize_h263_intra_c);
             * clamp to that natural range, not an arbitrary tighter one --
             * WMV1's DC scale (up to 22) routinely exceeds +-2048 at higher
             * qscale even though mr_msmpeg4v2.c's fixed *8 DC rarely did. */
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

static int decode_inter_residual(bitreader *b, wmv_ctx *c, const rlgroup_t *rl,
                                 int qscale, int out[8][8])
{
    int serial[64], coeff[8][8], pos = 0, u, v;
    memset(serial, 0, sizeof serial);
    for (;;) {
        int last, run, level, target;
        if (decode_rl_event(b, c, rl, qscale, &last, &run, &level))
            return -1;
        target = pos + run;
        if (target >= 64) {
            if (last)
                break;
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
        coeff[wmv_scantable_0[u] >> 3][wmv_scantable_0[u] & 7] = serial[u];
    idct_8x8(coeff, out);
    return 0;
}

/* ---- motion vectors ---------------------------------------------------- */
static void motion_predict(const mvblk_t *mv, int mb_w, int mbx, int mby,
                           int *px, int *py)
{
    const mvblk_t *left = mbx > 0 ? &mv[mby * mb_w + mbx - 1] : NULL;
    const mvblk_t *top = mby > 0 ? &mv[(mby - 1) * mb_w + mbx] : NULL;
    const mvblk_t *tr = (mby > 0 && mbx + 1 < mb_w) ? &mv[(mby - 1) * mb_w + mbx + 1] : NULL;
    if (!top) {
        *px = left ? left->x : 0;
        *py = left ? left->y : 0;
    } else {
        *px = median3(left ? left->x : 0, top->x, tr ? tr->x : 0);
        *py = median3(left ? left->y : 0, top->y, tr ? tr->y : 0);
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

static int reconstruct_inter_mb(wmv_ctx *c, bitreader *b, int mbx, int mby,
                                int cbp, int qscale, int mvx, int mvy)
{
    const rlgroup_t *rl = &rl_inter_tabs[c->rl_table_index];
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
        int bx = chroma ? chroma_mv(mvx) : mvx;
        int by = chroma ? chroma_mv(mvy) : mvy;
        int pred[8][8], residual[8][8];
        int y, x, coded = (cbp >> (5 - block)) & 1;

        mc_block(ref, rw, rh, stride, px, py, bx, by, c->no_rounding, pred);
        if (coded && decode_inter_residual(b, c, rl, qscale, residual))
            return -1;
        for (y = 0; y < 8; y++) {
            uint8_t *dst = plane + (size_t)(py + y) * stride + px;
            for (x = 0; x < 8; x++)
                dst[x] = (uint8_t)clip8(pred[y][x] + (coded ? residual[y][x] : 0));
        }
    }
    return 0;
}

/* ---- picture decode ----------------------------------------------------- */
static int decode_picture(wmv_ctx *c, bitreader *b)
{
    int picture_type = (int)br_bits(b, 2) + 1; /* 1=I, 2=P */
    int qscale = (int)br_bits(b, 5);
    int slice_height = c->mb_h;
    int y_scale, c_scale;
    int mbx, mby;

    if ((picture_type != 1 && picture_type != 2) || qscale == 0)
        return -1;
    c->qscale = qscale;
    y_scale = wmv_y_dc_scale[qscale];
    c_scale = wmv_c_dc_scale[qscale];

    if (picture_type == 1) {
        int code = (int)br_bits(b, 5);
        int slices;
        if (code < 0x17)
            return -1;
        slices = code - 0x16;
        if (slices < 1 || slices > c->mb_h)
            return -1;
        slice_height = c->mb_h / slices;
        if (slice_height < 1)
            return -1;

        /* Extension header: fps(5) + bit_rate(11) + flipflop_rounding(1),
         * always present for WMV1 I-frames. */
        br_skip(b, 5);
        c->bit_rate = (int64_t)br_bits(b, 11) * 1024;
        c->flipflop_rounding = (int)br_bit(b);

        c->per_mb_rl_table = c->bit_rate > 50 * 1024 ? (int)br_bit(b) : 0;
        if (!c->per_mb_rl_table) {
            c->rl_chroma_table_index = decode012(b);
            c->rl_table_index = decode012(b);
        }
        c->dc_table_index = (int)br_bit(b);
        c->no_rounding = 1;
    } else {
        int inter_intra_pred;
        c->use_skip_mb_code = (int)br_bit(b);
        c->per_mb_rl_table = c->bit_rate > 50 * 1024 ? (int)br_bit(b) : 0;
        if (!c->per_mb_rl_table) {
            c->rl_table_index = decode012(b);
            c->rl_chroma_table_index = c->rl_table_index;
        }
        c->dc_table_index = (int)br_bit(b);
        c->mv_table_index = (int)br_bit(b);

        inter_intra_pred = (c->w * c->h < 320 * 240) && (c->bit_rate <= 128 * 1024);
        if (inter_intra_pred)
            return -1; /* explicitly unsupported, see mr_wmv.c's header note */

        if (c->flipflop_rounding)
            c->no_rounding ^= 1;
        else
            c->no_rounding = 0;
    }
    c->esc3_level_length = 0;
    c->esc3_run_length = 0;

    /* c->pl/pcb/pcr (DC/AC/coded predictors) reset every picture, with
     * dcval going back to the neutral 1024 sentinel (not 0 -- see
     * mr_wmv.c's design note above wblk_t): a macroblock decoded inter
     * this frame never touches this grid, so without a per-picture reset
     * an intra MB's neighbour lookup would see a stale DC value from
     * whatever picture last wrote that cell, not FFmpeg's neutral
     * per-frame default. */
    wblk_reset(c->pl, (size_t)c->mb_w * 2 * c->mb_h * 2);
    wblk_reset(c->pcb, (size_t)c->mb_w * c->mb_h);
    wblk_reset(c->pcr, (size_t)c->mb_w * c->mb_h);
    memset(c->mv, 0, (size_t)c->mb_w * c->mb_h * sizeof(*c->mv));

    for (mby = 0; mby < c->mb_h; mby++) {
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            int block;
            mvblk_t *mv = &c->mv[mby * c->mb_w + mbx];

            if (picture_type == 2 && c->use_skip_mb_code && br_bit(b)) {
                mv->x = mv->y = 0;
                if (reconstruct_inter_mb(c, b, mbx, mby, 0, qscale, 0, 0))
                    return -1;
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
                            /* Write back immediately: the next subblock in
                             * this same MB (i+1..3) may look this one up as
                             * its own left/top/diag neighbour. */
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
                        if (wmv_debug())
                            fprintf(stderr, "[wmv1] intra block %d failed at MB %d,%d\n",
                                    block, mbx, mby);
                        return -1;
                    }
                }
            } else {
                int code, mb_intra, cbp;
                if (match_wtab(b, wmv_mbp_tab, 128, &code))
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
                            if (wmv_debug())
                                fprintf(stderr, "[wmv1] intra block %d failed at MB %d,%d (P-frame)\n",
                                        block, mbx, mby);
                            return -1;
                        }
                    }
                } else {
                    int predx, predy, mvx, mvy;
                    if (c->per_mb_rl_table && cbp) {
                        c->rl_table_index = decode012(b);
                        c->rl_chroma_table_index = c->rl_table_index;
                    }
                    motion_predict(c->mv, c->mb_w, mbx, mby, &predx, &predy);
                    if (decode_motion(b, c->mv_table_index, predx, predy, &mvx, &mvy))
                        return -1;
                    mv->x = mvx; mv->y = mvy;
                    if (reconstruct_inter_mb(c, b, mbx, mby, cbp, qscale, mvx, mvy)) {
                        if (wmv_debug())
                            fprintf(stderr, "[wmv1] inter residual failed at MB %d,%d\n", mbx, mby);
                        return -1;
                    }
                }
            }
            if (br_overrun(b))
                return -1;
        }
    }
    (void)slice_height;
    return 0;
}

static void yuv_to_rgb(wmv_ctx *c)
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
static mr_status wmv1_open(mr_decoder *dec)
{
    wmv_ctx *c = (wmv_ctx *)calloc(1, sizeof(*c));
    int i;
    if (!c)
        return MR_ENOMEM;

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
    if (!c->rgb || !c->pl || !c->pcb || !c->pcr || !c->mv)
        goto oom;
    /* decode_picture() resets pl/pcb/pcr (dcval -> 1024) on every frame,
     * including the first, so no separate init is needed here. */

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
    free(c->rgb); free(c->pl); free(c->pcb); free(c->pcr); free(c->mv);
    free(c);
    return MR_ENOMEM;
}

static mr_status wmv1_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    wmv_ctx *c = (wmv_ctx *)dec->priv;
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

static void wmv1_close(mr_decoder *dec)
{
    wmv_ctx *c = (wmv_ctx *)dec->priv;
    int i;
    if (!c)
        return;
    for (i = 0; i < 3; i++) {
        free(c->cur[i]);
        free(c->ref[i]);
    }
    free(c->rgb); free(c->pl); free(c->pcb); free(c->pcr); free(c->mv);
    free(c);
    dec->priv = NULL;
}

const mr_codec mr_codec_wmv1 = {
    "wmv1",
    { MR_FOURCC('W','M','V','1'), MR_FOURCC('w','m','v','1') },
    wmv1_open,
    wmv1_decode,
    wmv1_close,
    NULL
};
