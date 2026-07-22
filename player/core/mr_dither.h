/*
 * MintRIVA - RGB24 -> 8-bit palette conversion with ordered dithering.
 *
 * For the AGA path: AGA is planar and tops out at a 256-colour palette, so
 * frames must be quantised to 8-bit indices. A fixed 6x6x6 RGB cube plus 4x4
 * Bayer ordered dithering trades per-pixel error for the absence of banding,
 * is deterministic and cheap (no error-diffusion state), and needs no per-frame
 * palette work. Portable and host-testable; the AGA backend feeds the result to
 * graphics.library WritePixelArray8 for the chunky->planar step.
 */
#ifndef MR_DITHER_H
#define MR_DITHER_H

#include "mr_types.h"

#define MR_PALETTE_COLORS 216   /* 6*6*6 RGB cube; indices 216..255 unused */

/* Fill pal (256*3 bytes) with the fixed palette as RGB triplets (0..255). */
void mr_dither_palette(uint8_t *pal);

/* Convert an RGB24 top-down frame to 8-bit palette indices with ordered
 * dithering. `out` has out_stride bytes per row. `y_base` is the absolute row
 * index of the first row (so the Bayer pattern stays aligned when encoding only
 * a sub-range of rows); pass 0 for a whole frame. */
void mr_dither_rgb8(const uint8_t *rgb, int w, int h, int rgb_stride,
                    uint8_t *out, int out_stride, int y_base);

#endif /* MR_DITHER_H */
