/*
 * MintRIVA - MPEG-4 Part 2 (Visual) decoder.  See mr_mpeg4.h.
 *
 * Checkpoint 1: bitstream reader, start-code scanning, VOS/VO/VOL/GOV/VOP
 * header parsing with a cached VOL config, and a YUV420->RGB output stage.
 * Macroblock decoding is layered on top of this in subsequent stages
 * (I-VOP intra first, then P-VOP, then the ASP tools).
 */
#include "mr_mpeg4.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ---- start codes ------------------------------------------------------- */
#define SC_VO_SEQ_START   0xB0   /* visual_object_sequence_start            */
#define SC_VO_SEQ_END     0xB1
#define SC_VISUAL_OBJ     0xB5
#define SC_USER_DATA      0xB2
#define SC_GOV            0xB3   /* group_of_vop                            */
#define SC_VOP            0xB6   /* video_object_plane                      */
/* video_object_start:        0x00..0x1F                                    */
/* video_object_layer_start:  0x20..0x2F                                    */

/* VOP coding types */
enum { VOP_I = 0, VOP_P = 1, VOP_B = 2, VOP_S = 3 };

/* ---- bit reader (MSB first) -------------------------------------------- */
typedef struct {
    const uint8_t *buf;
    int            len;   /* bytes                                          */
    int            pos;   /* bit position                                   */
} bitreader;

static void br_init(bitreader *b, const uint8_t *d, int len)
{ b->buf = d; b->len = len; b->pos = 0; }

static unsigned br_bit(bitreader *b)
{
    int byte = b->pos >> 3, off = 7 - (b->pos & 7);
    b->pos++;
    if (byte >= b->len) return 0;
    return (b->buf[byte] >> off) & 1u;
}

static unsigned br_bits(bitreader *b, int n)
{
    unsigned v = 0;
    while (n-- > 0) v = (v << 1) | br_bit(b);
    return v;
}

static unsigned br_peek(bitreader *b, int n)
{
    int save = b->pos;
    unsigned v = br_bits(b, n);
    b->pos = save;
    return v;
}

static void br_skip(bitreader *b, int n)  { b->pos += n; }
static void br_align(bitreader *b)         { b->pos = (b->pos + 7) & ~7; }
static int  br_overrun(bitreader *b)       { return b->pos > b->len * 8; }

/* Byte-align, then scan forward to the next 00 00 01 start code. Positions the
 * reader just after the 4-byte code and returns the id byte, or -1 at EOF. */
static int next_start_code(bitreader *b)
{
    int i;
    br_align(b);
    i = b->pos >> 3;
    for (; i + 3 < b->len; i++) {
        if (b->buf[i] == 0 && b->buf[i+1] == 0 && b->buf[i+2] == 1) {
            int id = b->buf[i+3];
            b->pos = (i + 4) * 8;
            return id;
        }
    }
    b->pos = b->len * 8;
    return -1;
}

/* ---- decoder state ----------------------------------------------------- */
typedef struct {
    /* geometry */
    int      w, h;                  /* stream (display) size                */
    int      mb_w, mb_h;            /* macroblocks across / down            */
    int      cw, ch;               /* padded coded size (16*mb)            */

    /* VOL config (cached across packets) */
    int      have_vol;
    int      verid;
    int      shape;                 /* 0 = rectangular                      */
    int      time_inc_bits;
    unsigned time_res;              /* vop_time_increment_resolution         */
    unsigned fixed_vop_inc;         /* 0 for variable-rate streams           */
    int      interlaced;
    int      sprite_enable;         /* 0 none, 1 static, 2 GMC              */
    int      quant_type;            /* 0 = H.263, 1 = MPEG (matrices)       */
    int      quarter_sample;
    int      complexity_est_disable;
    int      resync_disable;
    int      data_partitioned;
    int      quant_precision;
    uint8_t  intra_matrix[64];
    uint8_t  inter_matrix[64];

    /* per-VOP */
    int      coding_type;
    int      rounding;
    int      intra_dc_thr;
    int      quant;
    int      fcode_fwd, fcode_bwd;

    /* B-VOP temporal references (§6.3.5 / §7.6.9): absolute display ticks */
    int      time_base, last_time_base; /* running second base (ticks/res)   */
    int      cur_time;                  /* this VOP's absolute tick          */
    int      fwd_time, bwd_time;        /* forward/backward anchor ticks     */
    int      trb, trd;                  /* B temporal distances              */
    int      anchors;                   /* anchors decoded so far            */

    /* planar YUV 4:2:0 working buffers (coded size). cur/ref rotate for I/P
     * (ref = forward anchor). For B-VOPs cur = forward anchor, ref = backward
     * anchor, and the B frame is decoded into bwork. */
    uint8_t *cur[3];
    uint8_t *ref[3];
    uint8_t *bwork[3];
    int      ystride, cstride;

    /* intra DC/AC predictor grids (per component, block granularity) */
    struct predblk *pl, *pcb, *pcr;   /* luma 2*mb_w x 2*mb_h; chroma mb_w x mb_h */
    struct mvblk   *mv;               /* per luma sub-block MV (2*mb_w x 2*mb_h) */
    struct mvblk   *bwd_mv;           /* backward anchor's MV field (direct mode) */
    uint8_t        *p_skip;           /* this anchor: MB not_coded (mb_w x mb_h) */
    uint8_t        *bwd_skip;         /* backward anchor's skip flags            */

    /* RGB output */
    uint8_t *rgb;
} m4_ctx;

/* One block's stored intra predictors (§7.4.3): inverse-quant DC for the
 * DC-direction test, and the quantised first row/column for AC prediction. */
typedef struct predblk {
    int     intra;        /* 0 = unavailable (edge / non-intra)               */
    int     pkt;          /* video-packet id (prediction can't cross packets) */
    int     dc;           /* inverse-quantised F[0][0]                        */
    int     qp;           /* quantiser scale this block used                  */
    int16_t row[8];       /* QF[0][1..7]                                      */
    int16_t col[8];       /* QF[1..7][0]                                      */
} predblk;

/* One luma sub-block's forward motion vector (half-pel units). */
typedef struct mvblk { int x, y, valid, pkt; } mvblk;

/* Default (JPEG/MPEG) quant matrices, in zigzag-natural order used by MPEG-4. */
static const uint8_t default_intra_matrix[64] = {
     8,17,18,19,21,23,25,27, 17,18,19,21,23,25,27,28,
    20,21,22,23,24,26,28,30, 21,22,23,24,26,28,30,32,
    22,23,24,26,28,30,32,35, 23,24,26,28,30,32,35,38,
    25,26,28,30,32,35,38,41, 27,28,30,32,35,38,41,45
};
static const uint8_t default_inter_matrix[64] = {
    16,17,18,19,20,21,22,23, 17,18,19,20,21,22,23,24,
    18,19,20,21,22,23,24,25, 19,20,21,22,23,24,26,27,
    20,21,22,23,25,26,27,28, 21,22,23,24,26,27,28,30,
    22,23,24,26,27,28,30,31, 23,24,25,27,28,30,31,33
};

/* zigzag scan (natural <- zigzag index) */
static const uint8_t zigzag[64] = {
     0, 1, 8,16, 9, 2, 3,10, 17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34, 27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36, 29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46, 53,60,61,54,47,55,62,63
};

/* ---- debug ------------------------------------------------------------- */
static int dbg(void) { static int v = -1; if (v < 0) v = getenv("MRDBG") ? 1 : 0; return v; }

/* ---- quant matrix load ------------------------------------------------- */
static void load_matrix(bitreader *b, uint8_t *m, const uint8_t *dflt)
{
    int i, last = 0;
    for (i = 0; i < 64; i++) {
        int v = br_bits(b, 8);
        if (v == 0) break;         /* zero terminates: rest = last value    */
        last = v;
        m[zigzag[i]] = (uint8_t)v;
    }
    if (i == 0) { memcpy(m, dflt, 64); return; }
    for (; i < 64; i++) m[zigzag[i]] = (uint8_t)last;
}

/* ---- VOL parse (id 0x20..0x2F already consumed) ------------------------ */
static int parse_vol(m4_ctx *c, bitreader *b)
{
    int ar, verid = 1;
    br_bits(b, 1);                                   /* random_accessible_vol */
    br_bits(b, 8);                                   /* video_object_type_ind */
    if (br_bits(b, 1)) {                             /* is_object_layer_id    */
        verid = br_bits(b, 4);
        br_bits(b, 3);                               /* priority              */
    }
    c->verid = verid;
    ar = br_bits(b, 4);                              /* aspect_ratio_info     */
    if (ar == 0xF) br_bits(b, 16);                   /* extended PAR w,h      */
    if (br_bits(b, 1)) {                             /* vol_control_parameters*/
        br_bits(b, 2);                               /* chroma_format         */
        br_bits(b, 1);                               /* low_delay             */
        if (br_bits(b, 1)) {                         /* vbv_parameters        */
            br_bits(b,15); br_bits(b,1); br_bits(b,15); br_bits(b,1);
            br_bits(b,15); br_bits(b,1); br_bits(b,3);
            br_bits(b,11); br_bits(b,1); br_bits(b,15); br_bits(b,1);
        }
    }
    c->shape = br_bits(b, 2);                        /* video_object_layer_shape */
    if (c->shape != 0) return MR_EUNSUPPORTED;       /* rectangular only       */
    br_bits(b, 1);                                   /* marker                 */
    { unsigned res = br_bits(b, 16);                 /* time_increment_res     */
      int nb = 1; while ((1 << nb) < (int)res) nb++;
      c->time_inc_bits = nb; c->time_res = res ? res : 1; }
    br_bits(b, 1);                                   /* marker                 */
    c->fixed_vop_inc = 0;
    if (br_bits(b, 1))                              /* fixed_vop_rate         */
        c->fixed_vop_inc = br_bits(b, c->time_inc_bits);
    /* rectangular shape: width/height */
    br_bits(b, 1);                                   /* marker                 */
    c->w = br_bits(b, 13);
    br_bits(b, 1);                                   /* marker                 */
    c->h = br_bits(b, 13);
    br_bits(b, 1);                                   /* marker                 */
    c->interlaced = br_bits(b, 1);
    br_bits(b, 1);                                   /* obmc_disable           */
    c->sprite_enable = (verid == 1) ? br_bits(b, 1) : br_bits(b, 2);
    if (c->sprite_enable == 1 || c->sprite_enable == 2) {
        if (c->sprite_enable != 2) {                 /* static needs geometry  */
            br_bits(b,13);br_bits(b,1);br_bits(b,13);br_bits(b,1);
            br_bits(b,13);br_bits(b,1);br_bits(b,13);br_bits(b,1);
        }
        br_bits(b, 6);                               /* no_of_warping_points   */
        br_bits(b, 2);                               /* warping_accuracy       */
        br_bits(b, 1);                               /* brightness_change      */
        if (c->sprite_enable != 2) br_bits(b, 1);    /* low_latency_sprite     */
    }
    if (verid != 1 && c->shape != 0) br_bits(b, 1);  /* sadct_disable (n/a)    */
    if (br_bits(b, 1)) {                             /* not_8_bit              */
        c->quant_precision = br_bits(b, 4);
        br_bits(b, 5);                               /* bits_per_pixel         */
    } else {
        c->quant_precision = 5;
    }
    c->quant_type = br_bits(b, 1);
    if (c->quant_type) {
        if (br_bits(b, 1)) load_matrix(b, c->intra_matrix, default_intra_matrix);
        else               memcpy(c->intra_matrix, default_intra_matrix, 64);
        if (br_bits(b, 1)) load_matrix(b, c->inter_matrix, default_inter_matrix);
        else               memcpy(c->inter_matrix, default_inter_matrix, 64);
    } else {
        memcpy(c->intra_matrix, default_intra_matrix, 64);
        memcpy(c->inter_matrix, default_inter_matrix, 64);
    }
    c->quarter_sample = (verid != 1) ? br_bits(b, 1) : 0;
    c->complexity_est_disable = br_bits(b, 1);
    if (!c->complexity_est_disable) return MR_EUNSUPPORTED;  /* rare; bail     */
    c->resync_disable = br_bits(b, 1);
    c->data_partitioned = br_bits(b, 1);
    if (c->data_partitioned) br_bits(b, 1);          /* reversible_vlc         */
    /* remaining VOL fields (newpred/scalability) are not needed: we rescan to
     * the next start code before decoding. */

    c->mb_w = (c->w + 15) >> 4;
    c->mb_h = (c->h + 15) >> 4;
    c->cw   = c->mb_w * 16;
    c->ch   = c->mb_h * 16;
    c->have_vol = 1;
    if (dbg())
        fprintf(stderr, "[mp4] VOL %dx%d ver%d shape%d tinc%d il%d spr%d "
                "qtype%d qpel%d qp%d resync!%d dp%d\n",
                c->w, c->h, verid, c->shape, c->time_inc_bits, c->interlaced,
                c->sprite_enable, c->quant_type, c->quarter_sample,
                c->quant_precision, c->resync_disable, c->data_partitioned);
    return MR_OK;
}

int mr_mpeg4_probe(const uint8_t *data, size_t len, int *width, int *height,
                   uint32_t *rate, uint32_t *scale)
{
    m4_ctx c;
    bitreader br;
    int id;

    if (!data || !width || !height || !rate || !scale ||
        len < 8 || len > 0x7fffffffUL)
        return 0;
    memset(&c, 0, sizeof c);
    br_init(&br, data, (int)len);

    while ((id = next_start_code(&br)) >= 0) {
        if (id >= 0x20 && id <= 0x2f) {
            if (parse_vol(&c, &br) != MR_OK || !c.have_vol)
                return 0;
            *width = c.w;
            *height = c.h;
            if (c.fixed_vop_inc) {
                *rate = c.time_res;
                *scale = c.fixed_vop_inc;
            } else {
                *rate = 25;            /* raw stream carries no other clock */
                *scale = 1;
            }
            return c.w > 0 && c.h > 0;
        }
    }
    return 0;
}

/* Recover vop_time_increment's bit width for a VOL-less stream.
 *
 * Some MPEG-4 files (e.g. mdat-first MP4s whose esds carries no
 * DecoderSpecificInfo) contain only GOV/VOP start codes and never a VOL, so
 * time_inc_bits is unknown and the legacy default of 4 misaligns every VOP
 * header. The width is recoverable from the first VOP: §6.3.5 mandates a
 * marker_bit == 1 on both sides of vop_time_increment, and the first coded VOP
 * after a GOV is an I-VOP, so the width in [1,16] that makes both markers 1
 * with vop_coding_type == I and vop_coded == 1 is the true one. `b` is
 * positioned at the VOP header (just past the start code) and is left
 * untouched — the probe runs on a copy. Returns 0 when nothing matches, so the
 * caller keeps the legacy default. */
static int detect_time_inc_bits(const bitreader *b)
{
    int tib;
    for (tib = 1; tib <= 16; tib++) {
        bitreader t = *b;
        int ct, m1, m2, coded;
        ct = br_bits(&t, 2);                 /* vop_coding_type               */
        while (br_bit(&t)) {                  /* modulo_time_base ones         */
            if (br_overrun(&t)) break;
        }
        m1    = br_bit(&t);                   /* marker_bit                    */
        br_skip(&t, tib);                     /* vop_time_increment            */
        m2    = br_bit(&t);                   /* marker_bit                    */
        coded = br_bit(&t);                   /* vop_coded                     */
        if (ct == 0 && m1 == 1 && m2 == 1 && coded == 1 && !br_overrun(&t))
            return tib;
    }
    return 0;
}

/* Early OpenDivX/DivX4 AVI files may omit the VOL header entirely and start
 * each chunk with a VOP.  Their implied baseline matches MPEG-4 Simple
 * Profile; the AVI header still supplies the display geometry. */
static void init_legacy_vol(m4_ctx *c)
{
    c->verid = 1;
    c->shape = 0;
    c->time_inc_bits = 4;
    c->time_res = 1;
    c->interlaced = 0;
    c->sprite_enable = 0;
    c->quant_type = 0;
    c->quarter_sample = 0;
    c->complexity_est_disable = 1;
    c->resync_disable = 0;
    c->data_partitioned = 0;
    c->quant_precision = 5;
    memcpy(c->intra_matrix, default_intra_matrix, 64);
    memcpy(c->inter_matrix, default_inter_matrix, 64);
    c->mb_w = (c->w + 15) >> 4;
    c->mb_h = (c->h + 15) >> 4;
    c->cw = c->mb_w * 16;
    c->ch = c->mb_h * 16;
    c->have_vol = 1;
}

/* ---- VOP header parse (id 0xB6 already consumed) ----------------------- */
/* Returns MR_OK (coded), MR_EAGAIN (not coded / skip), or an error. */
static int parse_vop(m4_ctx *c, bitreader *b)
{
    int modulo = 0, tinc;
    c->coding_type = br_bits(b, 2);
    while (br_bits(b, 1)) { modulo++; if (br_overrun(b)) return MR_EFORMAT; } /* modulo_time_base */
    br_bits(b, 1);                                   /* marker                 */
    tinc = br_bits(b, c->time_inc_bits);             /* vop_time_increment     */
    /* Absolute display tick (§6.3.5). B-VOPs use the second base from before
     * the most recent anchor advanced it, so they land in the right interval. */
    if (c->coding_type != VOP_B) {
        c->last_time_base = c->time_base;
        c->time_base += modulo;
        c->cur_time = c->time_base * (int)c->time_res + tinc;
    } else {
        c->cur_time = (c->last_time_base + modulo) * (int)c->time_res + tinc;
    }
    br_bits(b, 1);                                   /* marker                 */
    if (!br_bits(b, 1)) return MR_EAGAIN;            /* vop_coded == 0         */

    if (c->coding_type == VOP_P ||
        (c->coding_type == VOP_S && c->sprite_enable == 2))
        c->rounding = br_bits(b, 1);                 /* vop_rounding_type      */
    else
        c->rounding = 0;

    c->intra_dc_thr = br_bits(b, 3);
    if (c->interlaced) br_bits(b, 2);                /* tff + alt_vert_scan    */

    c->quant = br_bits(b, c->quant_precision);
    if (c->quant < 1) c->quant = 1;
    if (c->coding_type != VOP_I) c->fcode_fwd = br_bits(b, 3);
    if (c->coding_type == VOP_B) c->fcode_bwd = br_bits(b, 3);

    if (dbg())
        fprintf(stderr, "[mp4] VOP type%d q%d fwd%d dcthr%d\n",
                c->coding_type, c->quant, c->fcode_fwd, c->intra_dc_thr);
    return MR_OK;
}

/* ======================================================================== */
/* Intra (I-VOP) macroblock decode.                                         */
/*                                                                          */
/* The VLC tables below are the numeric contents of ISO/IEC 14496-2         */
/* Tables B.13/B.14 (DC size), B.6/B.7 (MCBPC), B.8 (CBPY) and B.16/B.17    */
/* (Tcoef); they were generated from, and cross-checked against, the        */
/* MIT-licensed OxideAV reference decoder. The reconstruction math (DC/AC   */
/* prediction, dequant, scan, IDCT) follows ISO/IEC 14496-2 §7.4.           */
/* ======================================================================== */

#define BPP 8                                   /* not_8_bit == 0            */

typedef struct { uint16_t code; uint8_t len, last, run; int16_t level; } tcoef_t;
typedef struct { uint16_t code; uint8_t len, val; } vlc3_t;
typedef struct { uint16_t code; uint8_t len, mbtype, cbpc; } mcbpc_t;
typedef struct { uint16_t code; uint8_t len, intra, inter; } cbpy_t;
typedef struct { uint16_t code; uint8_t len; int16_t data; } mvd_t;

#include "mr_mpeg4_tables.inc"

/* 2-D scan grids: grid[v][u] = position in the 1-D coefficient stream. */
static const uint8_t scan_zigzag[8][8] = {
    { 0, 1, 5, 6,14,15,27,28},{ 2, 4, 7,13,16,26,29,42},
    { 3, 8,12,17,25,30,41,43},{ 9,11,18,24,31,40,44,53},
    {10,19,23,32,39,45,52,54},{20,22,33,38,46,51,55,60},
    {21,34,37,47,50,56,59,61},{35,36,48,49,57,58,62,63}
};
static const uint8_t scan_alth[8][8] = {
    { 0, 1, 2, 3,10,11,12,13},{ 4, 5, 8, 9,17,16,15,14},
    { 6, 7,19,18,26,27,28,29},{20,21,24,25,30,31,32,33},
    {22,23,34,35,42,43,44,45},{36,37,40,41,46,47,48,49},
    {38,39,50,51,56,57,58,59},{52,53,54,55,60,61,62,63}
};
static const uint8_t scan_altv[8][8] = {
    { 0, 4, 6,20,22,36,38,52},{ 1, 5, 7,21,23,37,39,53},
    { 2, 8,19,24,34,40,50,54},{ 3, 9,18,25,35,41,51,55},
    {10,17,26,30,42,46,56,60},{11,16,27,31,43,47,57,61},
    {12,15,28,32,44,48,58,62},{13,14,29,33,45,49,59,63}
};

/* ---- VLC match helpers (peek max-length window, compare prefixes) ------ */
static const tcoef_t *match_tcoef(bitreader *b, const tcoef_t *t, int n)
{
    unsigned w = br_peek(b, 12);
    int i;
    for (i = 0; i < n; i++)
        if ((w >> (12 - t[i].len)) == t[i].code) return &t[i];
    return NULL;
}
static int match_vlc3(bitreader *b, const vlc3_t *t, int n)   /* -> val, len via skip */
{
    unsigned w = br_peek(b, 12);
    int i;
    for (i = 0; i < n; i++)
        if ((w >> (12 - t[i].len)) == t[i].code) { br_skip(b, t[i].len); return t[i].val; }
    return -1;
}
static const mcbpc_t *match_mcbpc(bitreader *b, const mcbpc_t *t, int n)
{
    unsigned w = br_peek(b, 12);
    int i;
    for (i = 0; i < n; i++)
        if ((w >> (12 - t[i].len)) == t[i].code) { br_skip(b, t[i].len); return &t[i]; }
    return NULL;
}
static int match_cbpy(bitreader *b, int intra)   /* returns 4-bit pattern or -1 */
{
    unsigned w = br_peek(b, 6);
    int i;
    for (i = 0; i < 16; i++)
        if ((w >> (6 - cbpy_tab[i].len)) == cbpy_tab[i].code) {
            br_skip(b, cbpy_tab[i].len);
            return intra ? cbpy_tab[i].intra : cbpy_tab[i].inter;
        }
    return -1;
}

/* ---- Tcoef AC EVENT decode (Table B.16/B.17 + §7.4.1.3 escapes) -------- */
/* Look up a value by (threshold-upper-bound, value) pairs; -1 if none match. */
static int range_lookup(int key, const int *tbl, int n)
{
    int i;
    for (i = 0; i < n; i++)
        if (key <= tbl[i*2]) return tbl[i*2 + 1];
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

/* Decode a plain Tcoef VLC + trailing sign: -> last,run,signed level. */
static int decode_tcoef_vlc(bitreader *b, int intra, int *last, int *run, int *level)
{
    const tcoef_t *e = match_tcoef(b, intra ? tcoef_intra : tcoef_inter,
                                   intra ? 102 : 102);
    if (!e) return -1;
    br_skip(b, e->len);
    *last = e->last; *run = e->run; *level = e->level;
    if (br_bit(b)) *level = -*level;
    return 0;
}

/* One (LAST,RUN,LEVEL) EVENT. Returns 0 on success, -1 on error. */
static int decode_ac_event(bitreader *b, int intra, int *last, int *run, int *level)
{
    if (br_peek(b, 7) != 0x03) {                 /* not the escape prefix    */
        return decode_tcoef_vlc(b, intra, last, run, level);
    }
    br_skip(b, 7);                               /* consume ESC 0000011      */
    if (!br_bit(b)) {                            /* Type 1: LEVEL += LMAX     */
        int lm;
        if (decode_tcoef_vlc(b, intra, last, run, level)) return -1;
        lm = intra ? lmax_intra(*last, *run) : lmax_inter(*last, *run);
        if (lm < 0) return -1;
        if (*level < 0) *level = -((-*level) + lm); else *level = *level + lm;
        return 0;
    }
    if (!br_bit(b)) {                            /* Type 2: RUN += RMAX + 1   */
        int rm, av;
        if (decode_tcoef_vlc(b, intra, last, run, level)) return -1;
        av = *level < 0 ? -*level : *level;
        rm = intra ? rmax_intra(*last, av) : rmax_inter(*last, av);
        if (rm < 0) return -1;
        *run += rm + 1;
        return 0;
    }
    /* Type 3: LAST(1) RUN(6) marker LEVEL(12) marker */
    *last = br_bit(b);
    *run  = br_bits(b, 6);
    if (!br_bit(b)) return -1;
    { int v = br_bits(b, 12);
      if (v >= 0x800) v -= 0x1000;
      if (v == 0 || v == -2048) return -1;
      *level = v; }
    if (!br_bit(b)) return -1;
    return 0;
}

/* ---- IDCT (Annex A, separable double-precision) ------------------------ */
static double idct_cos[8][8];
static int    idct_ready = 0;
static void idct_init(void)
{
    int u, x;
    for (u = 0; u < 8; u++)
        for (x = 0; x < 8; x++)
            idct_cos[u][x] = cos((2.0*x + 1.0) * u * 3.14159265358979323846 / 16.0);
    idct_ready = 1;
}
static void idct_8x8(const int in[8][8], int out[8][8])
{
    double tmp[8][8];
    const double s = 0.5;                        /* sqrt(2/8)                */
    const double c0 = 0.70710678118654752440;    /* 1/sqrt(2)                */
    int u, x, v, y;
    if (!idct_ready) idct_init();
    for (v = 0; v < 8; v++)                       /* rows                     */
        for (x = 0; x < 8; x++) {
            double a = 0;
            for (u = 0; u < 8; u++)
                a += (u ? 1.0 : c0) * in[v][u] * idct_cos[u][x];
            tmp[v][x] = s * a;
        }
    for (x = 0; x < 8; x++)                       /* columns                  */
        for (y = 0; y < 8; y++) {
            double a = 0;
            for (v = 0; v < 8; v++)
                a += (v ? 1.0 : c0) * tmp[v][x] * idct_cos[v][y];
            { double r = s * a;
              int iv = (r >= 0) ? (int)(r + 0.5) : -(int)(-r + 0.5);
              if (iv < -(1<<BPP)) iv = -(1<<BPP);            /* §7.4.5 clamp   */
              else if (iv > (1<<BPP)-1) iv = (1<<BPP)-1;
              out[y][x] = iv; }
        }
}

/* ---- dequant (method 2, H.263) + dc_scaler ----------------------------- */
static int div_round(int n, int d)               /* §4.1 // : round half away */
{
    return (n >= 0) ? (n + d/2) / d : -(((-n) + d/2) / d);
}
static int dc_scaler(int chroma, int q)
{
    if (!chroma) {
        if (q < 5)  return 8;
        if (q < 9)  return 2*q;
        if (q < 25) return q + 8;
        return 2*q - 16;
    }
    if (q < 5)  return 8;
    if (q < 25) return (q + 13) / 2;
    return q - 6;
}
static int deq2(int qf, int q)                   /* non-DC coeff, method 2    */
{
    int a, m;
    if (qf == 0) return 0;
    a = qf < 0 ? -qf : qf;
    m = (q & 1) ? (2*a + 1) * q : (2*a + 1) * q - 1;
    return qf < 0 ? -m : m;
}

/* ---- predictor-grid access --------------------------------------------- */
static predblk *pb_at(predblk *g, int gw, int gh, int x, int y)
{
    if (x < 0 || y < 0 || x >= gw || y >= gh) return NULL;
    return &g[y * gw + x];
}

/* Decode one intra block (index i in 0..5) of MB (mbx,mby) into the current
 * plane, updating that block's predictor grid entry. */
static int decode_intra_block(m4_ctx *c, bitreader *b, int i, int mbx, int mby,
                              int coded, int use_dc_vlc, int ac_pred, int q, int pkt)
{
    int chroma = (i >= 4);
    int intra_dc_present = use_dc_vlc;
    int qfs[64], pqf[8][8], qf[8][8], f[8][8], sp[8][8];
    int fa, fb, fc, chosen, dir_above, satlo, sathi;
    int u, v, pos, dcv = 0;
    predblk *A, *B, *C, *self;
    predblk *grid; int gw, gh, gx, gy;
    uint8_t *plane; int stride, px, py;

    /* select predictor grid + plane geometry for this block */
    if (!chroma) { grid = c->pl;  gw = c->mb_w*2; gh = c->mb_h*2;
                   gx = mbx*2 + (i&1); gy = mby*2 + (i>>1);
                   plane = c->cur[0]; stride = c->ystride;
                   px = mbx*16 + (i&1)*8; py = mby*16 + (i>>1)*8; }
    else { grid = (i == 4) ? c->pcb : c->pcr; gw = c->mb_w; gh = c->mb_h;
           gx = mbx; gy = mby;
           plane = (i == 4) ? c->cur[1] : c->cur[2]; stride = c->cstride;
           px = mbx*8; py = mby*8; }

    /* intra DC (differential VLC) */
    if (intra_dc_present) {
        int size = match_vlc3(b, chroma ? dcsize_chrom : dcsize_lum, 13);
        if (size < 0) return -1;
        if (size == 0) dcv = 0;
        else {
            int add = br_bits(b, size), half = 1 << (size - 1);
            dcv = (add >= half) ? add : (add + 1) - 2*half;
            if (size > 8 && !br_bit(b)) return -1;      /* marker            */
        }
    }

    /* AC EVENT loop -> qfs[] */
    for (pos = 0; pos < 64; pos++) qfs[pos] = 0;
    pos = 0;
    if (intra_dc_present) { qfs[0] = dcv; pos = 1; }
    if (coded) {
        for (;;) {
            int last, run, level, tgt;
            if (decode_ac_event(b, 1, &last, &run, &level)) return -1;
            tgt = pos + run;
            if (tgt >= 64) return -1;
            qfs[tgt] = level;
            pos = tgt + 1;
            if (last) break;
        }
    }

    /* neighbours (§7.4.3.1) */
    A = pb_at(grid, gw, gh, gx-1, gy);
    B = pb_at(grid, gw, gh, gx-1, gy-1);
    C = pb_at(grid, gw, gh, gx,   gy-1);
    if (A && !(A->intra && A->pkt == pkt)) A = NULL;   /* not across packets   */
    if (B && !(B->intra && B->pkt == pkt)) B = NULL;
    if (C && !(C->intra && C->pkt == pkt)) C = NULL;
    fa = A ? A->dc : 1024;
    fb = B ? B->dc : 1024;
    fc = C ? C->dc : 1024;
    dir_above = (abs(fa - fb) < abs(fb - fc));    /* else -> from left        */
    chosen = dir_above ? fc : fa;

    /* inverse scan */
    { const uint8_t (*g)[8] = ac_pred ? (dir_above ? scan_alth : scan_altv)
                                      : scan_zigzag;
      for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) pqf[v][u] = qfs[g[v][u]]; }

    /* DC + AC spatial prediction -> quantised qf[][] */
    { int scl = dc_scaler(chroma, q);
      for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) qf[v][u] = pqf[v][u];
      qf[0][0] = pqf[0][0] + div_round(chosen, scl);
      if (ac_pred) {
          if (!dir_above) {                       /* predict column from A    */
              if (A)
                  for (v = 1; v < 8; v++)
                      qf[v][0] = pqf[v][0] + div_round(A->col[v] * A->qp, q);
          } else {                                /* predict row from C       */
              if (C)
                  for (u = 1; u < 8; u++)
                      qf[0][u] = pqf[0][u] + div_round(C->row[u] * C->qp, q);
          }
      }
    }
    /* saturate qf to [-2048,2047] (§7.4.3.4) */
    for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) {
        if (qf[v][u] < -2048) qf[v][u] = -2048; else if (qf[v][u] > 2047) qf[v][u] = 2047;
    }

    /* store predictors for later neighbours (before dequant) */
    self = &grid[gy * gw + gx];
    self->intra = 1;
    self->pkt = pkt;
    self->qp = q;
    self->dc = dc_scaler(chroma, q) * qf[0][0];    /* inverse-quant DC F[0][0] */
    for (u = 1; u < 8; u++) self->row[u] = (int16_t)qf[0][u];
    for (v = 1; v < 8; v++) self->col[v] = (int16_t)qf[v][0];

    /* inverse quantisation (method 2) */
    satlo = -(1 << (BPP + 3)); sathi = (1 << (BPP + 3)) - 1;
    for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) {
        int val = (u == 0 && v == 0) ? dc_scaler(chroma, q) * qf[0][0]
                                     : deq2(qf[v][u], q);
        if (val < satlo) val = satlo; else if (val > sathi) val = sathi;
        f[v][u] = val;
    }

    /* IDCT + clip to [0,255], write into the plane */
    idct_8x8(f, sp);
    for (v = 0; v < 8; v++) {
        uint8_t *d = plane + (size_t)(py + v) * stride + px;
        for (u = 0; u < 8; u++) {
            int s = sp[v][u];
            if (s < 0) s = 0; else if (s > 255) s = 255;
            d[u] = (uint8_t)s;
        }
    }
    return 0;
}

static int use_intra_dc_vlc(int thr, int q)
{
    int t;
    switch (thr & 7) {
        case 0: return 1;                          /* DC VLC for whole VOP     */
        case 1: t = 13; break; case 2: t = 15; break; case 3: t = 17; break;
        case 4: t = 19; break; case 5: t = 21; break; case 6: t = 23; break;
        default: t = 1; break;                     /* AC VLC for whole VOP     */
    }
    return q < t;
}

/* If a video packet (§5.2.5 stuffing + resync marker + §6.2.5 header) begins
 * at the current bit position, consume it, reset *q to the packet quant, bump
 * the packet id, and return 1. Otherwise leave the reader untouched, return 0.
 * I-VOP only (17-bit resync marker, no fcode fields). */
static int maybe_resync(m4_ctx *c, bitreader *b, int *q, int *pkt)
{
    int save = b->pos, mb_bits = 1, total = c->mb_w * c->mb_h;
    int zeros;
    if (c->coding_type == VOP_I) zeros = 16;
    else if (c->coding_type == VOP_B) {          /* §6.3.3: max(15+max fcode,17) */
        int f = c->fcode_fwd > c->fcode_bwd ? c->fcode_fwd : c->fcode_bwd;
        zeros = (15 + f > 17) ? 15 + f : 17;
    } else zeros = 15 + c->fcode_fwd;
    if (c->resync_disable) return 0;
    if (br_bit(b) != 0) { b->pos = save; return 0; }      /* stuffing starts 0 */
    while (b->pos & 7) if (br_bit(b) != 1) { b->pos = save; return 0; }
    /* resync marker: `zeros` zero bits then a 1 */
    { int i; for (i = 0; i < zeros; i++) if (br_bit(b) != 0) { b->pos = save; return 0; } }
    if (br_bit(b) != 1) { b->pos = save; return 0; }
    while ((1 << mb_bits) < total) mb_bits++;              /* macroblock_number */
    br_bits(b, mb_bits);
    *q = br_bits(b, c->quant_precision);                   /* quant_scale       */
    if (*q < 1) *q = 1;
    if (br_bit(b)) {                                       /* header_extension  */
        int ct;
        while (br_bit(b)) { if (br_overrun(b)) break; }    /* modulo_time_base  */
        br_bit(b);                                         /* marker            */
        br_bits(b, c->time_inc_bits);                      /* vop_time_increment*/
        br_bit(b);                                         /* marker            */
        ct = br_bits(b, 2);                                /* vop_coding_type   */
        c->intra_dc_thr = br_bits(b, 3);                   /* intra_dc_vlc_thr  */
        if (ct != VOP_I) c->fcode_fwd = br_bits(b, 3);     /* vop_fcode_forward */
        if (ct == VOP_B) c->fcode_bwd = br_bits(b, 3);
    }
    (*pkt)++;
    return 1;
}

/* Decode a full I-VOP macroblock grid into c->cur. */
static int decode_ivop(m4_ctx *c, bitreader *b)
{
    int mbx, mby, q = c->quant, pkt = 1;
    size_t nl = (size_t)(c->mb_w*2) * (c->mb_h*2), nc = (size_t)c->mb_w * c->mb_h;
    memset(c->pl,  0, nl * sizeof(predblk));
    memset(c->pcb, 0, nc * sizeof(predblk));
    memset(c->pcr, 0, nc * sizeof(predblk));
    memset(c->mv,  0, nl * sizeof(mvblk));      /* intra: zero co-located MVs */
    memset(c->p_skip, 0, nc);                   /* intra: no skipped MBs      */

    for (mby = 0; mby < c->mb_h; mby++) {
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            const mcbpc_t *mc;
            int cbpy, ac_pred, use_dc, i, coded[6], cbpc;
            maybe_resync(c, b, &q, &pkt);          /* new video packet?        */
            do { mc = match_mcbpc(b, mcbpc_i, 9); if (!mc) return MR_EFORMAT; }
            while (mc->mbtype == 5);               /* stuffing                */
            cbpc    = mc->cbpc;
            ac_pred = br_bit(b);
            cbpy    = match_cbpy(b, 1);
            if (cbpy < 0) return MR_EFORMAT;
            if (mc->mbtype == 4) {                 /* intra+q: dquant          */
                static const int dq[4] = { -1, -2, 1, 2 };
                q += dq[br_bits(b, 2)];
                if (q < 1) q = 1; else if (q > 31) q = 31;
            }
            coded[0] = (cbpy>>3)&1; coded[1] = (cbpy>>2)&1;
            coded[2] = (cbpy>>1)&1; coded[3] = cbpy&1;
            coded[4] = (cbpc>>1)&1; coded[5] = cbpc&1;
            use_dc = use_intra_dc_vlc(c->intra_dc_thr, q);
            for (i = 0; i < 6; i++)
                if (decode_intra_block(c, b, i, mbx, mby, coded[i], use_dc, ac_pred, q, pkt))
                    return MR_EFORMAT;
        }
    }
    return MR_OK;
}

/* ======================================================================== */
/* Inter (P-VOP) macroblock decode: motion vectors + half-pel MC + residual. */
/* ======================================================================== */

static int median3(int a, int b, int c)
{
    int mx = a > b ? a : b, mn = a < b ? a : b;
    if (c > mx) return mx;
    if (c < mn) return mn;
    return c;
}
static int floordiv(int a, int d)                /* d > 0, floor toward -inf  */
{
    int q = a / d, r = a % d;
    if (r != 0 && r < 0) q--;
    return q;
}

/* Motion-vector data VLC (Table B.12): returns 2*vector_difference, or the
 * sentinel -999 on a bad code. */
static int match_mvd(bitreader *b)
{
    unsigned w = br_peek(b, 13);
    int i;
    for (i = 0; i < 65; i++)
        if ((w >> (13 - mvd_tab[i].len)) == mvd_tab[i].code) {
            br_skip(b, mvd_tab[i].len);
            return mvd_tab[i].data;
        }
    return -999;
}
static int recon_comp(int d, int res, int f)
{
    int m;
    if (f == 1 || d == 0) return d;
    m = (abs(d) - 1) * f + res + 1;
    return d < 0 ? -m : m;
}
/* Decode one motion vector (§6.2.6.2 + §7.6.3) given predictor (px,py). */
static int decode_mv(bitreader *b, int fcode, int px, int py, int *ox, int *oy)
{
    int f = 1 << (fcode - 1), rr = fcode - 1;
    int hd, hres, vd, vres, dx, dy, low, high, range, x, y;
    hd = match_mvd(b); if (hd == -999) return -1;
    hres = (f != 1 && hd != 0) ? (int)br_bits(b, rr) : 0;
    vd = match_mvd(b); if (vd == -999) return -1;
    vres = (f != 1 && vd != 0) ? (int)br_bits(b, rr) : 0;
    dx = recon_comp(hd, hres, f); dy = recon_comp(vd, vres, f);
    low = -32*f; high = 32*f - 1; range = 64*f;
    x = px + dx; if (x < low) x += range; if (x > high) x -= range;
    y = py + dy; if (y < low) y += range; if (y > high) y -= range;
    *ox = x; *oy = y;
    return 0;
}

/* §7.6.5 median predictor for luma block `blk` (0..3) of MB (mbx,mby). */
static void mv_predict(m4_ctx *c, int mbx, int mby, int blk, int pkt,
                       int *px, int *py)
{
    int gw = c->mb_w*2, gh = c->mb_h*2, r = mby, cc = mbx;
    int p[3][2], vx[3], vy[3], valid[3], nvalid = 0, i;
    switch (blk) {
    case 0: p[0][0]=2*r;   p[0][1]=2*cc-1; p[1][0]=2*r-1; p[1][1]=2*cc;
            p[2][0]=2*r-1; p[2][1]=2*cc+2; break;
    case 1: p[0][0]=2*r;   p[0][1]=2*cc;   p[1][0]=2*r-1; p[1][1]=2*cc+1;
            p[2][0]=2*r-1; p[2][1]=2*cc+2; break;
    case 2: p[0][0]=2*r+1; p[0][1]=2*cc-1; p[1][0]=2*r;   p[1][1]=2*cc;
            p[2][0]=2*r;   p[2][1]=2*cc+1; break;
    default:p[0][0]=2*r+1; p[0][1]=2*cc;   p[1][0]=2*r;   p[1][1]=2*cc;
            p[2][0]=2*r;   p[2][1]=2*cc+1; break;
    }
    for (i = 0; i < 3; i++) {
        int sr = p[i][0], sc = p[i][1];
        valid[i] = 0;
        if (sr < 0 || sc < 0 || sr >= gh || sc >= gw) continue;
        { mvblk *m = &c->mv[sr*gw + sc];
          if (m->valid && m->pkt == pkt) { vx[i]=m->x; vy[i]=m->y; valid[i]=1; nvalid++; } }
    }
    if (nvalid == 0) { *px = 0; *py = 0; return; }
    if (nvalid == 1) { for (i=0;i<3;i++) if (valid[i]) { *px=vx[i]; *py=vy[i]; return; } }
    for (i = 0; i < 3; i++) if (!valid[i]) { vx[i]=0; vy[i]=0; }
    *px = median3(vx[0], vx[1], vx[2]);
    *py = median3(vy[0], vy[1], vy[2]);
}

/* §7.6.5 chroma MV: sum the K luma MVs, reduce onto the half-pel grid. */
static int reduce_chroma(int sum, int k)
{
    static const uint8_t t13[4]  = {0,1,1,1};
    static const uint8_t t10[16] = {0,0,0,1,1,1,1,1,1,1,1,1,1,1,2,2};
    int fourk = 4*k, wp = floordiv(sum, fourk), idx = sum - wp*fourk;
    return 2*wp + ((k == 1) ? t13[idx] : t10[idx]);
}

/* §7.6.2 half-pel motion compensation of one 8x8 block from a reference plane
 * (edge-clamped), into out[8][8]. */
static int fetch_px(const uint8_t *p, int w, int h, int st, int x, int y)
{
    if (x < 0) x = 0; else if (x >= w) x = w - 1;
    if (y < 0) y = 0; else if (y >= h) y = h - 1;
    return p[(size_t)y*st + x];
}
static void mc_block(const uint8_t *ref, int w, int h, int st, int px, int py,
                     int mvx, int mvy, int rc, int out[8][8])
{
    int ix = mvx >> 1, iy = mvy >> 1, hx = mvx & 1, hy = mvy & 1, yy, xx;
    for (yy = 0; yy < 8; yy++)
        for (xx = 0; xx < 8; xx++) {
            int X = px + xx + ix, Y = py + yy + iy, val;
            int A = fetch_px(ref, w, h, st, X,   Y);
            int B = fetch_px(ref, w, h, st, X+1, Y);
            int C = fetch_px(ref, w, h, st, X,   Y+1);
            int D = fetch_px(ref, w, h, st, X+1, Y+1);
            if (!hx && !hy)      val = A;
            else if (hx && !hy)  val = (A + B + 1 - rc) / 2;
            else if (!hx && hy)  val = (A + C + 1 - rc) / 2;
            else                 val = (A + B + C + D + 2 - rc) / 4;
            out[yy][xx] = val;
        }
}

/* ---- quarter-pel MC (§7.6.2.2): 8-tap FIR + bilinear over a block-boundary-
 * mirrored reference block (Figure 7-30). mb[] is the 15x15 mirror of the 9x9
 * integer-pel interior; fetch is mb[3+y][3+x] in interior coordinates. */
static int qfir8(const int s[8], int rc)
{
    int acc = 160*(s[3]+s[4]) - 48*(s[2]+s[5]) + 24*(s[1]+s[6]) - 8*(s[0]+s[7]);
    int v = (acc + 128 - rc) / 256;
    if (v < 0) v = 0; else if (v > 255) v = 255;
    return v;
}
static int qbilin(int x, int y, int rc)
{ int v = (x + y + 1 - rc) / 2; return v > 255 ? 255 : v; }
#define QF(mb,x,y) ((int)(mb)[3+(y)][3+(x)])
static int qhb(const uint8_t mb[15][15], int x, int y, int rc)   /* horiz half-pel */
{ int s[8], k; for (k=0;k<8;k++) s[k]=QF(mb,x-3+k,y); return qfir8(s, rc); }
static int qhc(const uint8_t mb[15][15], int x, int y, int rc)   /* vert half-pel  */
{ int s[8], k; for (k=0;k<8;k++) s[k]=QF(mb,x,y-3+k); return qfir8(s, rc); }
static int qhd(const uint8_t mb[15][15], int x, int y, int rc)   /* centre half-pel*/
{ int s[8], k; for (k=0;k<8;k++) s[k]=qhb(mb,x,y-3+k,rc); return qfir8(s, rc); }
static int qck(const uint8_t mb[15][15], int x, int y, int rc)   /* left quarter col*/
{ int s[8], k; for (k=0;k<8;k++) s[k]=qbilin(QF(mb,x,y-3+k), qhb(mb,x,y-3+k,rc), rc);
  return qfir8(s, rc); }
static int qcl(const uint8_t mb[15][15], int x, int y, int rc)   /* right quarter col*/
{ int s[8], k; for (k=0;k<8;k++) s[k]=qbilin(qhb(mb,x,y-3+k,rc), QF(mb,x+1,y-3+k), rc);
  return qfir8(s, rc); }

static int qpel_pixel(const uint8_t mb[15][15], int x, int y, int qx, int qy, int rc)
{
    switch (qy*4 + qx) {
    case  0: return QF(mb,x,y);
    case  1: return qbilin(QF(mb,x,y), qhb(mb,x,y,rc), rc);
    case  2: return qhb(mb,x,y,rc);
    case  3: return qbilin(qhb(mb,x,y,rc), QF(mb,x+1,y), rc);
    case  4: return qbilin(QF(mb,x,y), qhc(mb,x,y,rc), rc);
    case  5: return qbilin(qbilin(QF(mb,x,y),qhb(mb,x,y,rc),rc), qck(mb,x,y,rc), rc);
    case  6: return qbilin(qhb(mb,x,y,rc), qhd(mb,x,y,rc), rc);
    case  7: return qbilin(qcl(mb,x,y,rc), qbilin(qhb(mb,x,y,rc),QF(mb,x+1,y),rc), rc);
    case  8: return qhc(mb,x,y,rc);
    case  9: return qck(mb,x,y,rc);
    case 10: return qhd(mb,x,y,rc);
    case 11: return qcl(mb,x,y,rc);
    case 12: return qbilin(qhc(mb,x,y,rc), QF(mb,x,y+1), rc);
    case 13: return qbilin(qck(mb,x,y,rc), qbilin(QF(mb,x,y+1),qhb(mb,x,y+1,rc),rc), rc);
    case 14: return qbilin(qhd(mb,x,y,rc), qhb(mb,x,y+1,rc), rc);
    default: return qbilin(qcl(mb,x,y,rc), qbilin(qhb(mb,x,y+1,rc),QF(mb,x+1,y+1),rc), rc);
    }
}
static void mc_block_qpel(const uint8_t *ref, int w, int h, int st, int bx, int by,
                          int mvx, int mvy, int rc, int out[8][8])
{
    int ix = mvx >> 2, iy = mvy >> 2, qx = mvx & 3, qy = mvy & 3, r, c, ox, oy;
    uint8_t mb[15][15];
    for (r = 0; r < 15; r++) {
        int sr = r-3; if (sr < 0) sr = -sr-1; else if (sr > 8) sr = 17-sr;
        for (c = 0; c < 15; c++) {
            int sc = c-3; if (sc < 0) sc = -sc-1; else if (sc > 8) sc = 17-sc;
            mb[r][c] = (uint8_t)fetch_px(ref, w, h, st, bx+ix+sc, by+iy+sr);
        }
    }
    for (oy = 0; oy < 8; oy++)
        for (ox = 0; ox < 8; ox++)
            out[oy][ox] = qpel_pixel(mb, ox, oy, qx, qy, rc);
}

/* Decode one inter block's residual (Table B.17, zigzag, method-2 dequant). */
static int decode_inter_resid(bitreader *b, int coded, int q, int resid[8][8])
{
    int qfs[64], pqf[8][8], f[8][8], u, v, pos, satlo, sathi;
    for (pos = 0; pos < 64; pos++) qfs[pos] = 0;
    if (coded) {
        pos = 0;
        for (;;) {
            int last, run, level, tgt;
            if (decode_ac_event(b, 0, &last, &run, &level)) return -1;
            tgt = pos + run;
            if (tgt >= 64) return -1;
            qfs[tgt] = level;
            pos = tgt + 1;
            if (last) break;
        }
    }
    for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) pqf[v][u] = qfs[scan_zigzag[v][u]];
    satlo = -(1 << (BPP + 3)); sathi = (1 << (BPP + 3)) - 1;
    for (v = 0; v < 8; v++) for (u = 0; u < 8; u++) {
        int val = deq2(pqf[v][u], q);
        if (val < satlo) val = satlo; else if (val > sathi) val = sathi;
        f[v][u] = val;
    }
    idct_8x8(f, resid);
    return 0;
}

/* Reconstruct one inter/skip block: prediction + residual, clip to [0,255]. */
static int inter_block(m4_ctx *c, bitreader *b, int i, int mbx, int mby,
                       int coded, int q, int mvx, int mvy, int rc)
{
    int chroma = (i >= 4), pred[8][8], resid[8][8], u, v;
    const uint8_t *ref;
    uint8_t *dst;
    int rw, rh, st, px, py;
    if (!chroma) { ref = c->ref[0]; rw = c->cw; rh = c->ch; st = c->ystride;
                   dst = c->cur[0]; px = mbx*16 + (i&1)*8; py = mby*16 + (i>>1)*8; }
    else { ref = (i==4) ? c->ref[1] : c->ref[2]; rw = c->cw>>1; rh = c->ch>>1;
           st = c->cstride; dst = (i==4) ? c->cur[1] : c->cur[2];
           px = mbx*8; py = mby*8; }
    if (!chroma && c->quarter_sample)              /* luma qpel; chroma half-pel */
        mc_block_qpel(ref, rw, rh, st, px, py, mvx, mvy, rc, pred);
    else
        mc_block(ref, rw, rh, st, px, py, mvx, mvy, rc, pred);
    if (coded) { if (decode_inter_resid(b, coded, q, resid)) return -1; }
    else       { for (v=0;v<8;v++) for (u=0;u<8;u++) resid[v][u] = 0; }
    for (v = 0; v < 8; v++) {
        uint8_t *d = dst + (size_t)(py + v)*st + px;
        for (u = 0; u < 8; u++) {
            int s = pred[v][u] + resid[v][u];
            if (s < 0) s = 0; else if (s > 255) s = 255;
            d[u] = (uint8_t)s;
        }
    }
    return 0;
}

/* Record a MB's four luma sub-block MVs in the prediction grid. */
static void store_mv(m4_ctx *c, int mbx, int mby, int pkt,
                     const int lx[4], const int ly[4])
{
    int gw = c->mb_w*2, i;
    for (i = 0; i < 4; i++) {
        mvblk *m = &c->mv[(2*mby + (i>>1))*gw + (2*mbx + (i&1))];
        m->x = lx[i]; m->y = ly[i]; m->valid = 1; m->pkt = pkt;
    }
}

/* Decode a full P-VOP macroblock grid into c->cur (referencing c->ref). */
static int decode_pvop(m4_ctx *c, bitreader *b)
{
    int mbx, mby, q = c->quant, pkt = 1, rc = c->rounding, i;
    size_t nl = (size_t)(c->mb_w*2) * (c->mb_h*2), nc = (size_t)c->mb_w * c->mb_h;
    memset(c->pl,  0, nl * sizeof(predblk));
    memset(c->pcb, 0, nc * sizeof(predblk));
    memset(c->pcr, 0, nc * sizeof(predblk));
    memset(c->mv,  0, nl * sizeof(mvblk));
    memset(c->p_skip, 0, nc);

    for (mby = 0; mby < c->mb_h; mby++) {
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            const mcbpc_t *mc;
            int cbpy, ac_pred, use_dc, coded[6], cbpc, mbtype;
            int lx[4], ly[4];
            maybe_resync(c, b, &q, &pkt);
            if (br_bit(b)) {                       /* not_coded == 1: skip MB  */
                c->p_skip[mby * c->mb_w + mbx] = 1;
                for (i = 0; i < 4; i++) { lx[i]=0; ly[i]=0; }
                for (i = 0; i < 6; i++)
                    if (inter_block(c, b, i, mbx, mby, 0, q, 0, 0, rc)) return MR_EFORMAT;
                store_mv(c, mbx, mby, pkt, lx, ly);
                continue;
            }
            do { mc = match_mcbpc(b, mcbpc_p, 21); if (!mc) return MR_EFORMAT; }
            while (mc->mbtype == 5);               /* stuffing                */
            mbtype = mc->mbtype; cbpc = mc->cbpc;
            ac_pred = (mbtype >= 3) ? br_bit(b) : 0;
            cbpy = match_cbpy(b, mbtype >= 3);
            if (cbpy < 0) return MR_EFORMAT;
            if (mbtype == 1 || mbtype == 4) {      /* inter+q / intra+q dquant */
                static const int dq[4] = { -1, -2, 1, 2 };
                q += dq[br_bits(b, 2)];
                if (q < 1) q = 1; else if (q > 31) q = 31;
            }
            coded[0]=(cbpy>>3)&1; coded[1]=(cbpy>>2)&1;
            coded[2]=(cbpy>>1)&1; coded[3]=cbpy&1;
            coded[4]=(cbpc>>1)&1; coded[5]=cbpc&1;

            if (mbtype >= 3) {                     /* intra MB inside a P-VOP  */
                use_dc = use_intra_dc_vlc(c->intra_dc_thr, q);
                for (i = 0; i < 6; i++)
                    if (decode_intra_block(c, b, i, mbx, mby, coded[i], use_dc, ac_pred, q, pkt))
                        return MR_EFORMAT;
                for (i = 0; i < 4; i++) { lx[i]=0; ly[i]=0; }
                store_mv(c, mbx, mby, pkt, lx, ly);
                continue;
            }

            /* inter MB: motion vectors (1 or 4), then MC + residual */
            if (mbtype == 2) {                     /* inter4v: 4 MVs           */
                for (i = 0; i < 4; i++) {
                    int px, py;
                    mv_predict(c, mbx, mby, i, pkt, &px, &py);
                    if (decode_mv(b, c->fcode_fwd, px, py, &lx[i], &ly[i])) return MR_EFORMAT;
                    /* store immediately so later blocks of this MB predict from it */
                    { int gw=c->mb_w*2; mvblk *m=&c->mv[(2*mby+(i>>1))*gw+(2*mbx+(i&1))];
                      m->x=lx[i]; m->y=ly[i]; m->valid=1; m->pkt=pkt; }
                }
            } else {                               /* 1 MV shared by 4 blocks  */
                int px, py, mx, my;
                mv_predict(c, mbx, mby, 0, pkt, &px, &py);
                if (decode_mv(b, c->fcode_fwd, px, py, &mx, &my)) return MR_EFORMAT;
                for (i = 0; i < 4; i++) { lx[i]=mx; ly[i]=my; }
            }
            store_mv(c, mbx, mby, pkt, lx, ly);

            /* luma blocks use per-block MV; chroma uses the reduced MV */
            { int sx = 0, sy = 0, k = (mbtype == 2) ? 4 : 1, cmx, cmy;
              /* §7.6.5: in quarter-sample mode the luma MVs are halved (toward
               * zero) before summation, collapsing onto the half-pel grid. */
              for (i = 0; i < k; i++) {
                  sx += c->quarter_sample ? lx[i]/2 : lx[i];
                  sy += c->quarter_sample ? ly[i]/2 : ly[i];
              }
              cmx = reduce_chroma(sx, k); cmy = reduce_chroma(sy, k);
              for (i = 0; i < 4; i++)
                  if (inter_block(c, b, i, mbx, mby, coded[i], q, lx[i], ly[i], rc))
                      return MR_EFORMAT;
              for (i = 4; i < 6; i++)
                  if (inter_block(c, b, i, mbx, mby, coded[i], q, cmx, cmy, rc))
                      return MR_EFORMAT;
            }
        }
    }
    return MR_OK;
}

/* ======================================================================== */
/* B-VOP decode: bidirectional / direct prediction, display-order reordered  */
/* by the caller.  forward ref = c->cur, backward ref = c->ref.              */
/* ======================================================================== */

/* B-VOP mb_type (Table B.4): 0=direct 1=interpolated 2=backward 3=forward. */
enum { BMB_DIRECT = 0, BMB_INTERP = 1, BMB_BACKWARD = 2, BMB_FORWARD = 3 };

static int b_modb(bitreader *b)            /* 0='1', 1='01', 2='00'          */
{
    if (br_bit(b)) return 0;
    if (br_bit(b)) return 1;
    return 2;
}
static int b_mbtype(bitreader *b)
{
    if (br_bit(b)) return BMB_DIRECT;
    if (br_bit(b)) return BMB_INTERP;
    if (br_bit(b)) return BMB_BACKWARD;
    if (br_bit(b)) return BMB_FORWARD;
    return -1;
}
static int b_dbquant(bitreader *b)         /* 0->0, 10->-2, 11->+2           */
{ if (!br_bit(b)) return 0; return br_bit(b) ? 2 : -2; }

/* Motion-compensate one 8x8 block, forward and/or backward, and combine. */
static void b_predict(m4_ctx *c, int chroma, int idx, int px, int py,
                      int use_f, int mvfx, int mvfy,
                      int use_b, int mvbx, int mvby, int out[8][8])
{
    int fp[8][8], bp[8][8], u, v, rw, rh, st;
    const uint8_t *fref, *bref;
    if (!chroma) { fref = c->cur[0]; bref = c->ref[0]; rw = c->cw; rh = c->ch; st = c->ystride; }
    else { fref = (idx==4)?c->cur[1]:c->cur[2]; bref = (idx==4)?c->ref[1]:c->ref[2];
           rw = c->cw>>1; rh = c->ch>>1; st = c->cstride; }
    if (use_f) {
        if (!chroma && c->quarter_sample) mc_block_qpel(fref, rw, rh, st, px, py, mvfx, mvfy, 0, fp);
        else                              mc_block(fref, rw, rh, st, px, py, mvfx, mvfy, 0, fp);
    }
    if (use_b) {
        if (!chroma && c->quarter_sample) mc_block_qpel(bref, rw, rh, st, px, py, mvbx, mvby, 0, bp);
        else                              mc_block(bref, rw, rh, st, px, py, mvbx, mvby, 0, bp);
    }
    for (v = 0; v < 8; v++) for (u = 0; u < 8; u++)
        out[v][u] = (use_f && use_b) ? (fp[v][u] + bp[v][u] + 1) >> 1
                  : (use_f ? fp[v][u] : bp[v][u]);
}

/* Write prediction + residual (if coded) into c->bwork, clipped. */
static int b_block(m4_ctx *c, bitreader *b, int idx, int mbx, int mby, int coded, int q,
                   int use_f, int mvfx, int mvfy, int use_b, int mvbx, int mvby)
{
    int chroma = (idx >= 4), pred[8][8], resid[8][8], u, v, st, px, py;
    uint8_t *dst;
    if (!chroma) { dst = c->bwork[0]; st = c->ystride; px = mbx*16 + (idx&1)*8; py = mby*16 + (idx>>1)*8; }
    else { dst = (idx==4)?c->bwork[1]:c->bwork[2]; st = c->cstride; px = mbx*8; py = mby*8; }
    b_predict(c, chroma, idx, px, py, use_f, mvfx, mvfy, use_b, mvbx, mvby, pred);
    if (coded) { if (decode_inter_resid(b, coded, q, resid)) return -1; }
    else       { for (v=0;v<8;v++) for (u=0;u<8;u++) resid[v][u] = 0; }
    for (v = 0; v < 8; v++) {
        uint8_t *d = dst + (size_t)(py + v)*st + px;
        for (u = 0; u < 8; u++) {
            int s = pred[v][u] + resid[v][u];
            if (s < 0) s = 0; else if (s > 255) s = 255;
            d[u] = (uint8_t)s;
        }
    }
    return 0;
}

static int decode_bvop(m4_ctx *c, bitreader *b)
{
    int mbx, mby, q = c->quant, gw = c->mb_w*2, i;
    int trb = c->trb, trd = c->trd;
    for (mby = 0; mby < c->mb_h; mby++) {
        int pfx = 0, pfy = 0, pbx = 0, pby = 0;   /* running MV predictors    */
        for (mbx = 0; mbx < c->mb_w; mbx++) {
            int modb, type, cbpb = 0, coded[6];
            int mvfx=0,mvfy=0,mvbx=0,mvby=0, dfx=0,dfy=0;
            int use_f, use_b, direct, mvf4[4][2], mvb4[4][2];
            int pkt = 1;
            if (maybe_resync(c, b, &q, &pkt)) { pfx=pfy=pbx=pby=0; }  /* new packet */

            /* §7.6.9.6: co-located anchor MB skipped -> this B MB is skipped
             * too (no bits): direct mode, zero motion, no residual. */
            if (c->bwd_skip[mby * c->mb_w + mbx]) {
                for (i = 0; i < 6; i++)
                    if (b_block(c, b, i, mbx, mby, 0, q, 1, 0, 0, 1, 0, 0))
                        return MR_EFORMAT;
                continue;
            }
            modb = b_modb(b);
            type = (modb == 0) ? BMB_DIRECT : b_mbtype(b);
            if (type < 0) return MR_EFORMAT;
            if (modb == 2) cbpb = br_bits(b, 6);
            direct = (type == BMB_DIRECT);
            if (!direct && (type==BMB_FORWARD||type==BMB_BACKWARD||type==BMB_INTERP) && cbpb)
                { q += b_dbquant(b); if (q < 1) q = 1; else if (q > 31) q = 31; }
            /* motion */
            if (type==BMB_FORWARD || type==BMB_INTERP) {
                if (decode_mv(b, c->fcode_fwd, pfx, pfy, &mvfx, &mvfy)) return MR_EFORMAT;
                pfx = mvfx; pfy = mvfy;
            }
            if (type==BMB_BACKWARD || type==BMB_INTERP) {
                if (decode_mv(b, c->fcode_bwd, pbx, pby, &mvbx, &mvby)) return MR_EFORMAT;
                pbx = mvbx; pby = mvby;
            }
            if (direct && modb != 0) {
                if (decode_mv(b, 1, 0, 0, &dfx, &dfy)) return MR_EFORMAT;
            }
            coded[0]=(cbpb>>5)&1; coded[1]=(cbpb>>4)&1; coded[2]=(cbpb>>3)&1;
            coded[3]=(cbpb>>2)&1; coded[4]=(cbpb>>1)&1; coded[5]=cbpb&1;

            use_f = (type==BMB_FORWARD || type==BMB_INTERP || direct);
            use_b = (type==BMB_BACKWARD || type==BMB_INTERP || direct);

            if (direct) {                          /* per-block co-located MVs */
                for (i = 0; i < 4; i++) {
                    mvblk *m = &c->bwd_mv[(2*mby+(i>>1))*gw + (2*mbx+(i&1))];
                    int mx = m->x, my = m->y;      /* co-located P forward MV  */
                    mvf4[i][0] = (trb*mx)/trd + dfx;
                    mvf4[i][1] = (trb*my)/trd + dfy;
                    mvb4[i][0] = (dfx==0) ? ((trb-trd)*mx)/trd : mvf4[i][0]-mx;
                    mvb4[i][1] = (dfy==0) ? ((trb-trd)*my)/trd : mvf4[i][1]-my;
                }
            }
            /* luma */
            for (i = 0; i < 4; i++) {
                int fx = direct ? mvf4[i][0] : mvfx, fy = direct ? mvf4[i][1] : mvfy;
                int bx = direct ? mvb4[i][0] : mvbx, by = direct ? mvb4[i][1] : mvby;
                if (b_block(c, b, i, mbx, mby, coded[i], q, use_f, fx, fy, use_b, bx, by))
                    return MR_EFORMAT;
            }
            /* chroma: derive from the luma MVs (§7.6.5) */
            { int sfx=0,sfy=0,sbx=0,sby=0,k = direct?4:1, cfx,cfy,cbx,cby;
              if (direct) { for (i=0;i<4;i++){ sfx+=mvf4[i][0];sfy+=mvf4[i][1];sbx+=mvb4[i][0];sby+=mvb4[i][1]; } }
              else        { sfx=mvfx;sfy=mvfy;sbx=mvbx;sby=mvby; }
              cfx=reduce_chroma(sfx,k); cfy=reduce_chroma(sfy,k);
              cbx=reduce_chroma(sbx,k); cby=reduce_chroma(sby,k);
              for (i = 4; i < 6; i++)
                  if (b_block(c, b, i, mbx, mby, coded[i], q, use_f, cfx, cfy, use_b, cbx, cby))
                      return MR_EFORMAT;
            }
        }
    }
    return MR_OK;
}

/* ---- YUV420 -> RGB24 --------------------------------------------------- */
static void yuv_to_rgb(m4_ctx *c, uint8_t *const pl[3])
{
    int x, y;
    for (y = 0; y < c->h; y++) {
        const uint8_t *yl = pl[0] + (size_t)y * c->ystride;
        const uint8_t *cb = pl[1] + (size_t)(y >> 1) * c->cstride;
        const uint8_t *cr = pl[2] + (size_t)(y >> 1) * c->cstride;
        uint8_t *d = c->rgb + (size_t)y * c->w * 3;
        for (x = 0; x < c->w; x++) {
            int Y = yl[x] - 16, U = cb[x >> 1] - 128, V = cr[x >> 1] - 128;
            int r = (298 * Y + 409 * V + 128) >> 8;
            int g = (298 * Y - 100 * U - 208 * V + 128) >> 8;
            int bb = (298 * Y + 516 * U + 128) >> 8;
            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (bb < 0) bb = 0; else if (bb > 255) bb = 255;
            *d++ = (uint8_t)r; *d++ = (uint8_t)g; *d++ = (uint8_t)bb;
        }
    }
}

/* ---- codec lifecycle --------------------------------------------------- */
static mr_status m4_open(mr_decoder *dec)
{
    m4_ctx *c = (m4_ctx *)calloc(1, sizeof *c);
    if (!c) return MR_ENOMEM;
    c->w = dec->width; c->h = dec->height;
    c->rgb = (uint8_t *)calloc((size_t)c->w * c->h * 3, 1);
    if (!c->rgb) { free(c); return MR_ENOMEM; }
    dec->priv = c;
    dec->frame.width  = c->w;
    dec->frame.height = c->h;
    dec->frame.fmt    = MR_PIX_RGB24;
    dec->frame.stride = c->w * 3;
    dec->frame.data   = c->rgb;
    dec->frame.dirty_y0 = 0;
    dec->frame.dirty_y1 = c->h;
    return MR_OK;
}

static int alloc_planes(m4_ctx *c)
{
    int i;
    c->ystride = c->cw;
    c->cstride = c->cw >> 1;
    for (i = 0; i < 3; i++) {
        int sz = (i == 0) ? c->ystride * c->ch : c->cstride * (c->ch >> 1);
        int fresh = !c->cur[i];
        if (!c->cur[i])   c->cur[i]   = (uint8_t *)malloc(sz);
        if (!c->ref[i])   c->ref[i]   = (uint8_t *)malloc(sz);
        if (!c->bwork[i]) c->bwork[i] = (uint8_t *)malloc(sz);
        if (!c->cur[i] || !c->ref[i] || !c->bwork[i]) return 0;
        /* Only clear on first allocation - a repeated VOL header (each GOP)
         * must not wipe the reference frames a B-VOP still needs. */
        if (fresh) {
            if (i == 0) { memset(c->cur[i], 16, sz); memset(c->ref[i], 16, sz); }
            else        { memset(c->cur[i],128, sz); memset(c->ref[i],128, sz); }
        }
    }
    if (!c->pl) {
        size_t nl = (size_t)(c->mb_w*2) * (c->mb_h*2);
        c->pl  = (predblk *)malloc(nl * sizeof(predblk));
        c->pcb = (predblk *)malloc((size_t)c->mb_w * c->mb_h * sizeof(predblk));
        c->pcr = (predblk *)malloc((size_t)c->mb_w * c->mb_h * sizeof(predblk));
        c->mv       = (mvblk *)malloc(nl * sizeof(mvblk));
        c->bwd_mv   = (mvblk *)calloc(nl, sizeof(mvblk));
        c->p_skip   = (uint8_t *)calloc((size_t)c->mb_w * c->mb_h, 1);
        c->bwd_skip = (uint8_t *)calloc((size_t)c->mb_w * c->mb_h, 1);
        if (!c->pl || !c->pcb || !c->pcr || !c->mv || !c->bwd_mv ||
            !c->p_skip || !c->bwd_skip) return 0;
    }
    return 1;
}

static mr_status m4_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    m4_ctx *c = (m4_ctx *)dec->priv;
    bitreader br;
    int id, rc, decoded = 0;

    br_init(&br, data, (int)len);
    for (;;) {
        id = next_start_code(&br);
        if (id < 0) break;
        if (id >= 0x20 && id <= 0x2F) {              /* video_object_layer     */
            rc = parse_vol(c, &br);
            if (rc != MR_OK) return rc;
            if (!alloc_planes(c)) return MR_ENOMEM;
        } else if (id == SC_VOP) {
            if (!c->have_vol) {
                int tib;
                init_legacy_vol(c);
                if ((tib = detect_time_inc_bits(&br)) != 0)
                    c->time_inc_bits = tib;
                if (!alloc_planes(c)) return MR_ENOMEM;
            }
            rc = parse_vop(c, &br);
            if (rc == MR_EAGAIN) { decoded = 1; break; } /* not coded: repeat  */
            if (rc != MR_OK) return rc;

            if (c->coding_type == VOP_B) {
                /* B-VOP: forward = cur (previous anchor), backward = ref
                 * (current anchor). Emitted immediately, in display order. */
                if (c->anchors < 2) { decoded = 1; break; }   /* no ref pair   */
                c->trd = c->bwd_time - c->fwd_time;
                c->trb = c->cur_time - c->fwd_time;
                if (c->trd <= 0) c->trd = 1;
                rc = decode_bvop(c, &br);                if (rc != MR_OK) return rc;
                yuv_to_rgb(c, c->bwork);
                decoded = 1;
                break;
            }

            /* I / P anchor */
            if (c->coding_type == VOP_I) rc = decode_ivop(c, &br);
            else if (c->coding_type == VOP_P) rc = decode_pvop(c, &br);
            else return MR_EUNSUPPORTED;                      /* S(GMC): later */
            if (rc != MR_OK) return rc;

            /* rotate: the just-decoded anchor becomes the backward ref, the
             * old backward becomes the forward ref. */
            { int k; for (k = 0; k < 3; k++)
                { uint8_t *t = c->cur[k]; c->cur[k] = c->ref[k]; c->ref[k] = t; } }
            { size_t nl = (size_t)(c->mb_w*2) * (c->mb_h*2);
              memcpy(c->bwd_mv, c->mv, nl * sizeof(mvblk)); }
            memcpy(c->bwd_skip, c->p_skip, (size_t)c->mb_w * c->mb_h);
            c->fwd_time = c->bwd_time; c->bwd_time = c->cur_time;
            c->anchors++;
            if (c->anchors == 1) { decoded = 0; break; }      /* nothing yet   */
            yuv_to_rgb(c, c->cur);          /* display the previous anchor      */
            decoded = 1;
            break;
        }
        /* VOS / VO / user_data / GOV: nothing to extract, keep scanning. */
    }
    return decoded ? MR_OK : MR_EAGAIN;
}

/* Flush: emit the last held backward anchor (in ref) at end of stream. */
static mr_status m4_flush(mr_decoder *dec)
{
    m4_ctx *c = (m4_ctx *)dec->priv;
    if (!c || c->anchors < 1) return MR_EAGAIN;
    c->anchors = -1;                    /* one-shot: only the final anchor    */
    yuv_to_rgb(c, c->ref);
    return MR_OK;
}

static void m4_close(mr_decoder *dec)
{
    m4_ctx *c = (m4_ctx *)dec->priv;
    int i;
    if (!c) return;
    for (i = 0; i < 3; i++) { free(c->cur[i]); free(c->ref[i]); free(c->bwork[i]); }
    free(c->pl); free(c->pcb); free(c->pcr); free(c->mv); free(c->bwd_mv);
    free(c->p_skip); free(c->bwd_skip);
    free(c->rgb);
    free(c);
    dec->priv = NULL;
}

const mr_codec mr_codec_mpeg4 = {
    "mpeg4",
    { MR_FOURCC('F','M','P','4'), MR_FOURCC('X','V','I','D'),
      MR_FOURCC('D','I','V','X'), MR_FOURCC('D','X','5','0'),
      MR_FOURCC('m','p','4','v'), MR_FOURCC('M','P','4','V'),
      MR_FOURCC('x','v','i','d'), MR_FOURCC('d','i','v','x') },
    m4_open,
    m4_decode,
    m4_close,
    m4_flush,
};
