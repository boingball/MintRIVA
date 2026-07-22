/*
 * MintRIVA - simple integer upscaling (portable).
 *
 * Nearest-neighbour 2x pixel doubling, so small clips fill more of an AGA
 * screen. Cheap and deterministic; the AGA backend scales RGB before dither/HAM
 * encoding.
 */
#ifndef MR_SCALE_H
#define MR_SCALE_H

#include "mr_types.h"

/* Double an RGB24 top-down frame into a 2w x 2h destination. */
void mr_scale2x_rgb24(const uint8_t *src, int w, int h, int src_stride,
                      uint8_t *dst, int dst_stride);

/* Double an 8-bit (1 byte/pixel) frame into 2w x 2h. Used to scale the encoded
 * chunky buffer - far cheaper than scaling RGB and re-encoding. Correct for HAM
 * because a modify sets an absolute channel value and scanlines are
 * independent, so a duplicated pixel/row reconstructs the same colour. */
void mr_scale2x_u8(const uint8_t *src, int w, int h, int src_stride,
                   uint8_t *dst, int dst_stride);

/* Integer downscale an RGB24 frame by `factor` (>=1) with a box average, so an
 * oversized clip fits a small (AGA) screen. Output is (w/factor)x(h/factor). */
void mr_scale_down_rgb24(const uint8_t *src, int w, int h, int src_stride,
                         uint8_t *dst, int dst_stride, int factor);

#endif /* MR_SCALE_H */
