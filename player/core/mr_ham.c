/*
 * MintRIVA - HAM8/HAM6 encoder + decoder.
 */
#include "mr_ham.h"

static int iabs(int v) { return v < 0 ? -v : v; }

/* Divide-free channel quantisers, built once (divides are slow on 68030).
 * q4[v]   -> nearest of the 4 HAM8 cube levels, index 0..3
 * s4[v]   -> that level's value (q4[v]*85)
 * q16[v]  -> nearest of 16 grey levels (HAM6), index 0..15
 * Values are identical to the previous (v*N+127)/255 arithmetic. */
static uint8_t q4[256], s4[256], q16[256], serr8[256];
static int     ham_lut_ready = 0;

static void build_ham_lut(void)
{
    int v;
    for (v = 0; v < 256; v++) {
        q4[v]    = (uint8_t)((v * 3 + 127) / 255);
        s4[v]    = (uint8_t)(q4[v] * 85);
        q16[v]   = (uint8_t)((v * 15 + 127) / 255);
        serr8[v] = (uint8_t)iabs(v - q4[v] * 85);   /* HAM8 per-channel set err */
    }
    ham_lut_ready = 1;
}

void mr_ham_palette(uint8_t *pal, int bits)
{
    int i;
    if (bits >= 8) {
        /* 4x4x4 RGB cube (levels 0,85,170,255) */
        for (i = 0; i < 64; i++) {
            pal[i * 3 + 0] = (uint8_t)(((i >> 4) & 3) * 85);
            pal[i * 3 + 1] = (uint8_t)(((i >> 2) & 3) * 85);
            pal[i * 3 + 2] = (uint8_t)((i & 3) * 85);
        }
    } else {
        /* 16-entry grey ramp */
        for (i = 0; i < 16; i++)
            pal[i * 3 + 0] = pal[i * 3 + 1] = pal[i * 3 + 2] = (uint8_t)(i * 17);
    }
}

void mr_ham_encode(const uint8_t *rgb, int w, int h, int rgb_stride,
                   uint8_t *out, int out_stride, int bits)
{
    int data_bits = bits - 2;                 /* 6 (HAM8) or 4 (HAM6)        */
    int mshift    = 8 - data_bits;            /* modify component <<2 or <<4 */
    int cshift    = data_bits;                /* control bits sit above data */
    int lowmask   = (1 << mshift) - 1;        /* truncation error = v&lowmask */
    int color     = (bits >= 8);
    int x, y;

    if (!ham_lut_ready) build_ham_lut();
    for (y = 0; y < h; y++) {
        const uint8_t *sr = rgb + (size_t)y * rgb_stride;
        uint8_t       *dr = out + (size_t)y * out_stride;
        int pr = 0, pg = 0, pb = 0;           /* held colour (line start = 0)*/
        for (x = 0; x < w; x++) {
            int R = sr[x * 3 + 0], G = sr[x * 3 + 1], B = sr[x * 3 + 2];
            /* held-channel errors shared between the modify options */
            int dpr = iabs(R - pr), dpg = iabs(G - pg), dpb = iabs(B - pb);
            /* modify error is just the low bits lost to truncation (v>=v&~mask) */
            int e_r = (R & lowmask) + dpg + dpb;
            int e_g = dpr + (G & lowmask) + dpb;
            int e_b = dpr + dpg + (B & lowmask);
            int e_set, set_idx, cr, cg, cb;

            if (color) {                       /* HAM8: per-channel cube      */
                e_set   = serr8[R] + serr8[G] + serr8[B];
                set_idx = (q4[R] << 4) | (q4[G] << 2) | q4[B];
                cr = s4[R]; cg = s4[G]; cb = s4[B];
            } else {                           /* HAM6: grey ramp             */
                int grey = (R + G + B) / 3, gv = q16[grey] * 17;
                e_set   = iabs(R - gv) + iabs(G - gv) + iabs(B - gv);
                set_idx = q16[grey]; cr = cg = cb = gv;
            }

            if (e_set <= e_r && e_set <= e_g && e_set <= e_b) {
                dr[x] = (uint8_t)((0 << cshift) | set_idx);
                pr = cr; pg = cg; pb = cb;
            } else if (e_r <= e_g && e_r <= e_b) {
                int q = R >> mshift;
                dr[x] = (uint8_t)((2 << cshift) | q); pr = q << mshift;
            } else if (e_g <= e_b) {
                int q = G >> mshift;
                dr[x] = (uint8_t)((3 << cshift) | q); pg = q << mshift;
            } else {
                int q = B >> mshift;
                dr[x] = (uint8_t)((1 << cshift) | q); pb = q << mshift;
            }
        }
    }
}

void mr_ham_decode(const uint8_t *ham, int w, int h, int in_stride,
                   const uint8_t *pal, uint8_t *rgb, int rgb_stride, int bits)
{
    int data_bits = bits - 2;
    int mshift    = 8 - data_bits;
    int data_mask = (1 << data_bits) - 1;
    int x, y;

    for (y = 0; y < h; y++) {
        const uint8_t *sr = ham + (size_t)y * in_stride;
        uint8_t       *dr = rgb + (size_t)y * rgb_stride;
        int r = 0, g = 0, b = 0;
        for (x = 0; x < w; x++) {
            int px   = sr[x];
            int ctrl = px >> data_bits;
            int data = px & data_mask;
            switch (ctrl) {
            case 0: r = pal[data * 3 + 0]; g = pal[data * 3 + 1];
                    b = pal[data * 3 + 2]; break;
            case 2: r = data << mshift; break;      /* modify red   */
            case 3: g = data << mshift; break;      /* modify green */
            default: b = data << mshift; break;     /* 1 = modify blue */
            }
            dr[x * 3 + 0] = (uint8_t)r;
            dr[x * 3 + 1] = (uint8_t)g;
            dr[x * 3 + 2] = (uint8_t)b;
        }
    }
}
