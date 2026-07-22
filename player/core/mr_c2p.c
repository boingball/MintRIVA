/*
 * MintRIVA - chunky-to-planar via 8x8 bit transpose.
 */
#include "mr_c2p.h"

/* Transpose 8 chunky bytes (pixels, pixel 0 -> MSB of each plane byte) into 8
 * plane bytes. Host-verified identical to the naive per-bit reference. */
static void transpose8(const uint8_t *in, uint8_t out[8])
{
    uint64_t x = 0;
    int i;
    for (i = 0; i < 8; i++) x = (x << 8) | in[i];
    x = (x & 0xAA55AA55AA55AA55ULL)
      | ((x & 0x00AA00AA00AA00AAULL) << 7)
      | ((x >> 7) & 0x00AA00AA00AA00AAULL);
    x = (x & 0xCCCC3333CCCC3333ULL)
      | ((x & 0x0000CCCC0000CCCCULL) << 14)
      | ((x >> 14) & 0x0000CCCC0000CCCCULL);
    x = (x & 0xF0F0F0F00F0F0F0FULL)
      | ((x & 0x00000000F0F0F0F0ULL) << 28)
      | ((x >> 28) & 0x00000000F0F0F0F0ULL);
    for (i = 0; i < 8; i++) { out[i] = (uint8_t)x; x >>= 8; }
}

void mr_c2p8(const uint8_t *chunky, int pw, int h, int chunky_stride,
             int nplanes, uint8_t *const planes[], int bpr,
             int x0byte, int y0)
{
    int groups = pw >> 3;                 /* pw is a multiple of 8          */
    int y, g, k;
    for (y = 0; y < h; y++) {
        const uint8_t *row = chunky + (size_t)y * chunky_stride;
        size_t base = (size_t)(y0 + y) * bpr + x0byte;
        for (g = 0; g < groups; g++) {
            uint8_t pl[8];
            transpose8(row + (g << 3), pl);
            for (k = 0; k < nplanes; k++)
                planes[k][base + g] = pl[k];
        }
    }
}
