/*
 * MintRIVA - Microsoft MPEG-4 version 2 (MP42) decoder.
 *
 * MP42 is Microsoft's pre-standard H.263-derived bitstream.  It is not an
 * MPEG-4 Visual fourcc alias, so it deliberately has its own codec plugin.
 *
 * The picture-header, macroblock and escape behaviour was cross-checked
 * against FFmpeg's LGPL msmpeg4v2 decoder.  The shared MPEG-4/H.263 VLC
 * values come from MintRIVA's existing, MIT-derived table include.
 */
#include "mr_msmpeg4v2.h"

#include <math.h>
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

/* ---- VLC table types ------------------------------------------------- */
typedef struct {
    uint16_t code;
    uint8_t len, last, run;
    int16_t level;
} tcoef_t;
typedef struct { uint16_t code; uint8_t len, val; } vlc3_t;
typedef struct { uint16_t code; uint8_t len, mbtype, cbpc; } mcbpc_t;
typedef struct { uint16_t code; uint8_t len, intra, inter; } cbpy_t;
typedef struct { uint16_t code; uint8_t len; int16_t data; } mvd_t;

#include "mr_mpeg4_tables.inc"

/* Microsoft v2 macroblock VLCs.  The value is the decoded table index. */
typedef struct { uint8_t code, len, val; } mp42_vlc;

static const mp42_vlc v2_mb_type[] = {
    { 0x01, 1, 0 }, { 0x00, 2, 1 }, { 0x03, 3, 2 }, { 0x09, 5, 3 },
    { 0x05, 4, 4 }, { 0x21, 7, 5 }, { 0x20, 7, 6 }, { 0x11, 6, 7 }
};

static const mp42_vlc v2_intra_cbpc[] = {
    { 0x01, 1, 0 }, { 0x00, 3, 1 }, { 0x01, 3, 2 }, { 0x01, 2, 3 }
};

/* grid[v][u] gives the serial coefficient position for that matrix cell. */
static const uint8_t scan_zigzag[8][8] = {
    { 0, 1, 5, 6,14,15,27,28}, { 2, 4, 7,13,16,26,29,42},
    { 3, 8,12,17,25,30,41,43}, { 9,11,18,24,31,40,44,53},
    {10,19,23,32,39,45,52,54}, {20,22,33,38,46,51,55,60},
    {21,34,37,47,50,56,59,61}, {35,36,48,49,57,58,62,63}
};
static const uint8_t scan_alth[8][8] = {
    { 0, 1, 2, 3,10,11,12,13}, { 4, 5, 8, 9,17,16,15,14},
    { 6, 7,19,18,26,27,28,29}, {20,21,24,25,30,31,32,33},
    {22,23,34,35,42,43,44,45}, {36,37,40,41,46,47,48,49},
    {38,39,50,51,56,57,58,59}, {52,53,54,55,60,61,62,63}
};
static const uint8_t scan_altv[8][8] = {
    { 0, 4, 6,20,22,36,38,52}, { 1, 5, 7,21,23,37,39,53},
    { 2, 8,19,24,34,40,50,54}, { 3, 9,18,25,35,41,51,55},
    {10,17,26,30,42,46,56,60}, {11,16,27,31,43,47,57,61},
    {12,15,28,32,44,48,58,62}, {13,14,29,33,45,49,59,63}
};

typedef struct {
    int valid;
    int slice;
    int dc;                         /* quantised DC; neutral predictor 128 */
    int16_t row[8], col[8];         /* quantised AC edge predictors       */
} predblk;

typedef struct {
    int x, y;
    int slice;
    int valid;
} mvblk;

typedef struct {
    int w, h, mb_w, mb_h, cw, ch;
    int ystride, cstride;
    uint8_t *cur[3], *ref[3];
    uint8_t *rgb;
    predblk *pl, *pcb, *pcr;
    mvblk *mv;                       /* one 16x16 vector per macroblock     */
} mp42_ctx;

static int mp42_debug(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("MRDBG") != NULL;
    return enabled;
}

/* ---- generic VLC helpers --------------------------------------------- */
static int match_small(bitreader *b, const mp42_vlc *tab, int count)
{
    unsigned w = br_peek(b, 7);
    int i;
    for (i = 0; i < count; i++) {
        if ((w >> (7 - tab[i].len)) == tab[i].code) {
            br_skip(b, tab[i].len);
            return tab[i].val;
        }
    }
    return -1;
}

static int match_cbpy(bitreader *b)
{
    unsigned w = br_peek(b, 6);
    int i;
    for (i = 0; i < 16; i++) {
        if ((w >> (6 - cbpy_tab[i].len)) == cbpy_tab[i].code) {
            br_skip(b, cbpy_tab[i].len);
            return cbpy_tab[i].intra;
        }
    }
    return -1;
}

static int match_mvd(bitreader *b)
{
    unsigned w = br_peek(b, 13);
    int i;
    for (i = 0; i < 65; i++) {
        if ((w >> (13 - mvd_tab[i].len)) == mvd_tab[i].code) {
            br_skip(b, mvd_tab[i].len);
            return mvd_tab[i].data;
        }
    }
    return 999;
}

static const tcoef_t *match_tcoef(bitreader *b, const tcoef_t *tab)
{
    unsigned w = br_peek(b, 12);
    int i;
    for (i = 0; i < 102; i++)
        if ((w >> (12 - tab[i].len)) == tab[i].code)
            return &tab[i];
    return NULL;
}

/* Decode Microsoft's inverted H.263 DC differential VLC. */
static int decode_dc_diff(bitreader *b, int chroma, int *diff)
{
    const vlc3_t *tab = chroma ? dcsize_chrom : dcsize_lum;
    unsigned w = br_peek(b, 12);
    int i, size = -1;

    for (i = 0; i < 13; i++) {
        unsigned mask = (1u << tab[i].len) - 1u;
        unsigned code = tab[i].code ^ mask;
        if ((w >> (12 - tab[i].len)) == code) {
            br_skip(b, tab[i].len);
            size = tab[i].val;
            break;
        }
    }
    if (size < 0 || size > 9)
        return -1;
    if (size == 0) {
        *diff = 0;
        return 0;
    }
    {
        unsigned v = br_bits(b, size);
        unsigned half = 1u << (size - 1);
        *diff = (v & half) ? (int)v : (int)v - (int)((1u << size) - 1u);
    }
    if (size > 8 && br_bit(b) != 1)
        return -1;
    return br_overrun(b) ? -1 : 0;
}

/* ---- MSMPEG4 run/level escapes --------------------------------------- */
static int range_lookup(int key, const int *tab, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (key <= tab[i * 2])
            return tab[i * 2 + 1];
    return -1;
}

static int lmax_intra(int last, int run)
{
    static const int nl[] = {0,27, 1,10, 2,5, 3,4, 7,3, 9,2, 14,1};
    static const int yl[] = {0,8, 1,3, 6,2, 20,1};
    return last ? range_lookup(run, yl, 4) : range_lookup(run, nl, 7);
}

static int lmax_inter(int last, int run)
{
    static const int nl[] = {0,12, 1,6, 2,4, 6,3, 10,2, 26,1};
    static const int yl[] = {0,3, 1,2, 40,1};
    return last ? range_lookup(run, yl, 3) : range_lookup(run, nl, 6);
}

static int rmax_intra(int last, int level)
{
    static const int nl[] = {1,14, 2,9, 3,7, 4,3, 5,2, 10,1, 27,0};
    static const int yl[] = {1,20, 2,6, 3,1, 8,0};
    return last ? range_lookup(level, yl, 4) : range_lookup(level, nl, 7);
}

static int rmax_inter(int last, int level)
{
    static const int nl[] = {1,26, 2,10, 3,6, 4,2, 6,1, 12,0};
    static const int yl[] = {1,40, 2,1, 3,0};
    return last ? range_lookup(level, yl, 3) : range_lookup(level, nl, 6);
}

static int decode_plain_rl(bitreader *b, const tcoef_t *tab,
                           int *last, int *run, int *level)
{
    const tcoef_t *e = match_tcoef(b, tab);
    if (!e)
        return -1;
    br_skip(b, e->len);
    *last = e->last;
    *run = e->run;
    *level = e->level;
    if (br_bit(b))
        *level = -*level;
    return br_overrun(b) ? -1 : 0;
}

static int decode_rl_event(bitreader *b, int intra,
                           int *last, int *run, int *level)
{
    const tcoef_t *tab = intra ? tcoef_intra : tcoef_inter;

    if (br_peek(b, 7) != 0x03)
        return decode_plain_rl(b, tab, last, run, level);

    br_skip(b, 7);                  /* escape VLC: 0000011 */
    if (br_bit(b)) {                /* escape 1: extend LEVEL */
        int add;
        if (decode_plain_rl(b, tab, last, run, level))
            return -1;
        add = intra ? lmax_intra(*last, *run) : lmax_inter(*last, *run);
        if (add < 0)
            return -1;
        *level += (*level < 0) ? -add : add;
    } else if (br_bit(b)) {         /* escape 2: extend RUN */
        int add, abslevel;
        if (decode_plain_rl(b, tab, last, run, level))
            return -1;
        abslevel = *level < 0 ? -*level : *level;
        add = intra ? rmax_intra(*last, abslevel)
                    : rmax_inter(*last, abslevel);
        if (add < 0)
            return -1;
        *run += add + 1;
    } else {                        /* escape 3: fixed LAST/RUN/LEVEL */
        int v;
        *last = (int)br_bit(b);
        *run = (int)br_bits(b, 6);
        v = (int)br_bits(b, 8);
        if (v & 0x80)
            v -= 256;
        if (v == 0 || v == -128)
            return -1;
        *level = v;
    }
    return br_overrun(b) ? -1 : 0;
}

/* ---- transform and pixel helpers ------------------------------------- */
static double idct_cos[8][8];
static int idct_ready;

static void idct_init(void)
{
    int u, x;
    for (u = 0; u < 8; u++)
        for (x = 0; x < 8; x++)
            idct_cos[u][x] =
                cos((2.0 * x + 1.0) * u * 3.14159265358979323846 / 16.0);
    idct_ready = 1;
}

static void idct_8x8(const int in[8][8], int out[8][8])
{
    double tmp[8][8];
    const double s = 0.5;
    const double c0 = 0.70710678118654752440;
    int u, x, v, y;

    if (!idct_ready)
        idct_init();
    for (v = 0; v < 8; v++) {
        for (x = 0; x < 8; x++) {
            double a = 0;
            for (u = 0; u < 8; u++)
                a += (u ? 1.0 : c0) * in[v][u] * idct_cos[u][x];
            tmp[v][x] = s * a;
        }
    }
    for (x = 0; x < 8; x++) {
        for (y = 0; y < 8; y++) {
            double a = 0;
            double r;
            for (v = 0; v < 8; v++)
                a += (v ? 1.0 : c0) * tmp[v][x] * idct_cos[v][y];
            r = s * a;
            out[y][x] = r >= 0 ? (int)(r + 0.5) : -(int)(-r + 0.5);
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

static void mc_block(const uint8_t *ref, int w, int h, int stride,
                     int px, int py, int mvx, int mvy, int out[8][8])
{
    int ix = floor_div(mvx, 2), iy = floor_div(mvy, 2);
    int hx = mvx - ix * 2, hy = mvy - iy * 2;
    int y, x;

    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            int sx = px + x + ix, sy = py + y + iy;
            int a = fetch_px(ref, w, h, stride, sx, sy);
            int b = fetch_px(ref, w, h, stride, sx + 1, sy);
            int c = fetch_px(ref, w, h, stride, sx, sy + 1);
            int d = fetch_px(ref, w, h, stride, sx + 1, sy + 1);
            if (!hx && !hy) out[y][x] = a;
            else if (hx && !hy) out[y][x] = (a + b + 1) >> 1;
            else if (!hx && hy) out[y][x] = (a + c + 1) >> 1;
            else out[y][x] = (a + b + c + d + 2) >> 2;
        }
    }
}

/* H.263 chroma MV reduction for one 16x16 luma vector. */
static int chroma_mv(int mv)
{
    static const uint8_t roundtab[4] = { 0, 1, 1, 1 };
    int q = floor_div(mv, 4);
    int r = mv - q * 4;
    return q * 2 + roundtab[r];
}

/* ---- predictor grids ------------------------------------------------- */
static predblk *pred_at(predblk *grid, int gw, int gh, int x, int y, int slice)
{
    predblk *p;
    if (x < 0 || y < 0 || x >= gw || y >= gh)
        return NULL;
    p = &grid[y * gw + x];
    return p->valid && p->slice == slice ? p : NULL;
}

static void select_block(mp42_ctx *c, int block, int mbx, int mby,
                         predblk **grid, int *gw, int *gh, int *gx, int *gy,
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

static void invalidate_intra(mp42_ctx *c, int mbx, int mby)
{
    int i;
    for (i = 0; i < 4; i++)
        c->pl[(mby * 2 + (i >> 1)) * (c->mb_w * 2) +
              mbx * 2 + (i & 1)].valid = 0;
    c->pcb[mby * c->mb_w + mbx].valid = 0;
    c->pcr[mby * c->mb_w + mbx].valid = 0;
}

/* ---- block decode/reconstruction ------------------------------------ */
static int dequant_ac(int level, int q)
{
    int qadd = (q - 1) | 1;
    if (level > 0) return level * (q << 1) + qadd;
    if (level < 0) return level * (q << 1) - qadd;
    return 0;
}

static int decode_intra_block(mp42_ctx *c, bitreader *b, int block,
                              int mbx, int mby, int coded, int ac_pred,
                              int q, int slice)
{
    int chroma = block >= 4;
    int serial[64], qf[8][8], coeff[8][8], spatial[8][8];
    int pos, diff, pred, dir_top, u, v;
    predblk *grid, *left, *diag, *top, *self;
    int gw, gh, gx, gy, stride, px, py;
    uint8_t *plane;
    const uint8_t (*scan)[8];

    if (decode_dc_diff(b, chroma, &diff))
        return -1;

    select_block(c, block, mbx, mby, &grid, &gw, &gh, &gx, &gy,
                 &plane, &stride, &px, &py);
    left = pred_at(grid, gw, gh, gx - 1, gy, slice);
    diag = pred_at(grid, gw, gh, gx - 1, gy - 1, slice);
    top  = pred_at(grid, gw, gh, gx, gy - 1, slice);
    {
        int a = left ? left->dc : 128;
        int bb = diag ? diag->dc : 128;
        int cc = top ? top->dc : 128;
        dir_top = abs(a - bb) <= abs(bb - cc);
        pred = dir_top ? cc : a;
    }

    memset(serial, 0, sizeof serial);
    serial[0] = pred + diff;
    if (serial[0] < -256 || serial[0] > 511)
        return -1;
    pos = 1;
    if (coded) {
        for (;;) {
            int last, run, level, target;
            if (decode_rl_event(b, chroma ? 0 : 1, &last, &run, &level))
                return -1;
            target = pos + run;
            if (target >= 64)
                return -1;
            serial[target] = level;
            pos = target + 1;
            if (last)
                break;
        }
    }

    scan = ac_pred ? (dir_top ? scan_alth : scan_altv) : scan_zigzag;
    for (v = 0; v < 8; v++)
        for (u = 0; u < 8; u++)
            qf[v][u] = serial[scan[v][u]];

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

    self = &grid[gy * gw + gx];
    self->valid = 1;
    self->slice = slice;
    self->dc = qf[0][0];
    for (u = 1; u < 8; u++) self->row[u] = (int16_t)qf[0][u];
    for (v = 1; v < 8; v++) self->col[v] = (int16_t)qf[v][0];

    for (v = 0; v < 8; v++) {
        for (u = 0; u < 8; u++) {
            int val = (u == 0 && v == 0) ? qf[0][0] * 8
                                         : dequant_ac(qf[v][u], q);
            if (val < -2048) val = -2048;
            if (val > 2047) val = 2047;
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

static int decode_inter_residual(bitreader *b, int q, int out[8][8])
{
    int serial[64], coeff[8][8], pos = 0, u, v, event = 0;
    memset(serial, 0, sizeof serial);
    for (;;) {
        int last, run, level, target;
        int event_pos = b->pos;
        if (decode_rl_event(b, 0, &last, &run, &level)) {
            if (mp42_debug())
                fprintf(stderr, "[mp42] bad RL event %d at bit %d peek=%06lx\n",
                        event, event_pos,
                        (unsigned long)br_peek(b, 24));
            return -1;
        }
        target = pos + run;
        if (target >= 64) {
            /* Old Microsoft encoders occasionally terminate a block with a
             * final coefficient one slot past 63.  FFmpeg's non-strict path
             * drops that coefficient and accepts the block. */
            if (last)
                break;
            if (mp42_debug())
                fprintf(stderr, "[mp42] RL overflow event %d pos=%d run=%d "
                        "last=%d level=%d startbit=%d\n",
                        event, pos, run, last, level, event_pos);
            return -1;
        }
        serial[target] = dequant_ac(level, q);
        pos = target + 1;
        event++;
        if (last)
            break;
    }
    for (v = 0; v < 8; v++)
        for (u = 0; u < 8; u++)
            coeff[v][u] = serial[scan_zigzag[v][u]];
    idct_8x8(coeff, out);
    return 0;
}

static void motion_predict(const mp42_ctx *c, int mbx, int mby, int slice,
                           int *px, int *py)
{
    const mvblk *left = NULL, *top = NULL, *tr = NULL;
    if (mbx > 0) {
        const mvblk *m = &c->mv[mby * c->mb_w + mbx - 1];
        if (m->valid && m->slice == slice) left = m;
    }
    if (mby > 0) {
        const mvblk *m = &c->mv[(mby - 1) * c->mb_w + mbx];
        if (m->valid && m->slice == slice) top = m;
        if (mbx + 1 < c->mb_w) {
            m = &c->mv[(mby - 1) * c->mb_w + mbx + 1];
            if (m->valid && m->slice == slice) tr = m;
        }
    }
    if (!top) {
        *px = left ? left->x : 0;
        *py = left ? left->y : 0;
    } else {
        *px = median3(left ? left->x : 0, top->x, tr ? tr->x : 0);
        *py = median3(left ? left->y : 0, top->y, tr ? tr->y : 0);
    }
}

static int decode_motion(bitreader *b, int pred)
{
    int diff = match_mvd(b);
    int val;
    if (diff == 999)
        return 999;
    val = pred + diff;
    if (val <= -64) val += 64;
    else if (val >= 64) val -= 64;
    return val;
}

static int reconstruct_inter_mb(mp42_ctx *c, bitreader *b, int mbx, int mby,
                                int cbp, int q, int mvx, int mvy)
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
        int bx = chroma ? chroma_mv(mvx) : mvx;
        int by = chroma ? chroma_mv(mvy) : mvy;
        int pred[8][8], residual[8][8];
        int y, x, coded = (cbp >> (5 - block)) & 1;

        mc_block(ref, rw, rh, stride, px, py, bx, by, pred);
        if (coded && decode_inter_residual(b, q, residual)) {
            if (mp42_debug())
                fprintf(stderr, "[mp42] residual block %d failed at bit %d/%d\n",
                        block, b->pos, b->len * 8);
            return -1;
        }
        for (y = 0; y < 8; y++) {
            uint8_t *dst = plane + (size_t)(py + y) * stride + px;
            for (x = 0; x < 8; x++)
                dst[x] = (uint8_t)clip8(pred[y][x] +
                                        (coded ? residual[y][x] : 0));
        }
    }
    return 0;
}

/* ---- picture decode -------------------------------------------------- */
static int decode_picture(mp42_ctx *c, bitreader *b)
{
    int picture_type = (int)br_bits(b, 2) + 1; /* 1=I, 2=P */
    int q = (int)br_bits(b, 5);
    int slice_height = c->mb_h;
    int use_skip = 0;
    int mbx, mby, slice = 0;

    if ((picture_type != 1 && picture_type != 2) || q == 0)
        return -1;
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
    } else {
        use_skip = (int)br_bit(b);
    }

    memset(c->pl, 0, (size_t)c->mb_w * 2 * c->mb_h * 2 * sizeof(*c->pl));
    memset(c->pcb, 0, (size_t)c->mb_w * c->mb_h * sizeof(*c->pcb));
    memset(c->pcr, 0, (size_t)c->mb_w * c->mb_h * sizeof(*c->pcr));
    memset(c->mv, 0, (size_t)c->mb_w * c->mb_h * sizeof(*c->mv));

    for (mby = 0; mby < c->mb_h; mby++) {
        if (mby % slice_height == 0)
            slice++;
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            int cbpc, cbpy, cbp, intra, ac_pred = 0, block;
            int mvx = 0, mvy = 0;
            mvblk *mv = &c->mv[mby * c->mb_w + mbx];

            if (picture_type == 2 && use_skip && br_bit(b)) {
                invalidate_intra(c, mbx, mby);
                mv->x = mv->y = 0; mv->slice = slice; mv->valid = 1;
                if (reconstruct_inter_mb(c, b, mbx, mby, 0, q, 0, 0))
                    return -1;
                continue;
            }

            if (picture_type == 1) {
                cbpc = match_small(b, v2_intra_cbpc, 4);
                if (cbpc < 0)
                    return -1;
                intra = 1;
            } else {
                int code = match_small(b, v2_mb_type, 8);
                if (code < 0)
                    return -1;
                intra = code >> 2;
                cbpc = code & 3;
            }

            if (intra) {
                ac_pred = (int)br_bit(b);
                cbpy = match_cbpy(b);
                if (cbpy < 0)
                    return -1;
                cbp = cbpc | (cbpy << 2);
                mv->x = mv->y = 0; mv->slice = slice; mv->valid = 1;
                for (block = 0; block < 6; block++) {
                    int coded = (cbp >> (5 - block)) & 1;
                    if (decode_intra_block(c, b, block, mbx, mby, coded,
                                           ac_pred, q, slice)) {
                        if (mp42_debug())
                            fprintf(stderr, "[mp42] intra block %d failed at "
                                    "MB %d,%d bit %d/%d\n", block, mbx, mby,
                                    b->pos, b->len * 8);
                        return -1;
                    }
                }
            } else {
                int predx, predy;
                cbpy = match_cbpy(b);
                if (cbpy < 0)
                    return -1;
                cbp = cbpc | (cbpy << 2);
                if ((cbp & 3) != 3)
                    cbp ^= 0x3c;
                motion_predict(c, mbx, mby, slice, &predx, &predy);
                mvx = decode_motion(b, predx);
                mvy = decode_motion(b, predy);
                if (mvx == 999 || mvy == 999)
                    return -1;
                mv->x = mvx; mv->y = mvy; mv->slice = slice; mv->valid = 1;
                invalidate_intra(c, mbx, mby);
                if (reconstruct_inter_mb(c, b, mbx, mby, cbp, q, mvx, mvy)) {
                    if (mp42_debug())
                        fprintf(stderr, "[mp42] inter residual failed at "
                                "MB %d,%d cbp=%02x q=%d bit %d/%d\n",
                                mbx, mby, cbp, q, b->pos, b->len * 8);
                    return -1;
                }
            }
            if (br_overrun(b))
                return -1;
        }
    }
    return 0;
}

static void yuv_to_rgb(mp42_ctx *c)
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

/* ---- codec lifecycle ------------------------------------------------- */
static mr_status mp42_open(mr_decoder *dec)
{
    mp42_ctx *c = (mp42_ctx *)calloc(1, sizeof(*c));
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
    c->pl = (predblk *)calloc((size_t)c->mb_w * 2 * c->mb_h * 2,
                              sizeof(*c->pl));
    c->pcb = (predblk *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->pcb));
    c->pcr = (predblk *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->pcr));
    c->mv = (mvblk *)calloc((size_t)c->mb_w * c->mb_h, sizeof(*c->mv));
    if (!c->rgb || !c->pl || !c->pcb || !c->pcr || !c->mv)
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
    free(c->rgb); free(c->pl); free(c->pcb); free(c->pcr); free(c->mv);
    free(c);
    return MR_ENOMEM;
}

static mr_status mp42_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    mp42_ctx *c = (mp42_ctx *)dec->priv;
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

static void mp42_close(mr_decoder *dec)
{
    mp42_ctx *c = (mp42_ctx *)dec->priv;
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

const mr_codec mr_codec_msmpeg4v2 = {
    "msmpeg4v2",
    { MR_FOURCC('M','P','4','2'), MR_FOURCC('m','p','4','2'), 0, 0,
      0, 0, 0, 0 },
    mp42_open,
    mp42_decode,
    mp42_close,
    NULL
};
