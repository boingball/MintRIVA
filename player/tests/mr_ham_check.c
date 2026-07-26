#include "../core/mr_ham.h"

#include <stdio.h>
#include <string.h>

static unsigned state = 0x68616d38U;
static int absi(int v) { return v < 0 ? -v : v; }

static unsigned char random_byte(void)
{
    state = state * 1664525U + 1013904223U;
    return (unsigned char)(state >> 24);
}

/* Straightforward original greedy encoder retained here as a bit-exact oracle. */
static void reference(const unsigned char *rgb, int w, int h, int stride,
                      unsigned char *out, int out_stride, int bits)
{
    int data_bits = bits - 2, mshift = 8 - data_bits;
    int cshift = data_bits, lowmask = (1 << mshift) - 1, x, y;
    for (y = 0; y < h; y++) {
        const unsigned char *s = rgb + (size_t)y * stride;
        unsigned char *d = out + (size_t)y * out_stride;
        int pr = 0, pg = 0, pb = 0;
        for (x = 0; x < w; x++) {
            int r = s[x*3], g = s[x*3+1], b = s[x*3+2];
            int er = (r & lowmask) + absi(g-pg) + absi(b-pb);
            int eg = absi(r-pr) + (g & lowmask) + absi(b-pb);
            int eb = absi(r-pr) + absi(g-pg) + (b & lowmask);
            int es, idx, cr, cg, cb;
            if (bits >= 8) {
                int qr = (r*3+127)/255, qg = (g*3+127)/255, qb = (b*3+127)/255;
                cr=qr*85; cg=qg*85; cb=qb*85; idx=(qr<<4)|(qg<<2)|qb;
                es=absi(r-cr)+absi(g-cg)+absi(b-cb);
            } else {
                int q = (((r+g+b)/3)*15+127)/255;
                cr=cg=cb=q*17; idx=q;
                es=absi(r-cr)+absi(g-cg)+absi(b-cb);
            }
            if (es<=er && es<=eg && es<=eb) {
                d[x]=(unsigned char)idx; pr=cr; pg=cg; pb=cb;
            } else if (er<=eg && er<=eb) {
                int q=r>>mshift; d[x]=(unsigned char)((2<<cshift)|q); pr=q<<mshift;
            } else if (eg<=eb) {
                int q=g>>mshift; d[x]=(unsigned char)((3<<cshift)|q); pg=q<<mshift;
            } else {
                int q=b>>mshift; d[x]=(unsigned char)((1<<cshift)|q); pb=q<<mshift;
            }
        }
    }
}

int main(void)
{
    unsigned char rgb[37*3*9], expected[43*9], actual[43*9];
    int bits, iteration, i;
    for (bits = 6; bits <= 8; bits += 2) {
        for (iteration = 0; iteration < 500; iteration++) {
            for (i = 0; i < (int)sizeof rgb; i++) rgb[i] = random_byte();
            memset(expected, 0xa5, sizeof expected);
            memset(actual, 0xa5, sizeof actual);
            reference(rgb, 37, 9, 37*3, expected, 43, bits);
            mr_ham_encode(rgb, 37, 9, 37*3, actual, 43, bits);
            if (memcmp(expected, actual, sizeof actual)) {
                fprintf(stderr, "HAM%d mismatch at iteration %d\n", bits, iteration);
                return 1;
            }
        }
    }
    puts("HAM checks passed");
    return 0;
}
