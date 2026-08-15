/*
 * MintVID - direct YUV420P -> 8-bit indexed conversion, fused with an exact
 * integer vertical decimation.
 *
 * For AGA playback where the fitted display height is an exact integer
 * fraction of the source height (typical: a 640x360 H.264 frame on a
 * non-laced AGA screen needs 640x180, a clean 2:1 vertical fit with no
 * horizontal change - see display_aga.c's aga_open()), this replaces the
 * three-stage mr_yuv420_to_rgb24() -> mr_scale_resize_rgb24() ->
 * mr_dither_rgb8() pipeline with one pass over only the rows that survive
 * the downscale, entirely skipping the intermediate RGB24 buffers (both the
 * full-resolution one and the resized one).
 */
#ifndef MR_YUV_DITHER_H
#define MR_YUV_DITHER_H

#include "mr_types.h"

/*
 * Convert width x height YUV420P (2x2-subsampled Cb/Cr) directly to 8-bit
 * palette indices (mr_dither_rgb8()'s fixed 6x6x6 cube + 4x4 Bayer ordered
 * dither), selecting one source row out of every `vscale` via the same
 * nearest-neighbour rule mr_scale_resize_rgb24() uses for an exact-fit
 * downscale: source row = vscale*out_row + vscale/2 (destination pixel
 * centres). Pass vscale=1 for no vertical scaling (every row kept).
 *
 * Precondition (caller's responsibility - not checked here): this is only
 * bit-identical to the three-stage pipeline above when the destination
 * width equals `width` (no horizontal resize - mr_scale_resize_rgb24's own
 * DDA reduces to a 1:1 column copy in exactly that case, see mr_scale.c)
 * and `vscale` evenly divides `height`.
 *
 * out has out_stride bytes per row and (height / vscale) rows of `width`
 * indices each. y_base is the absolute output row of the first row written
 * (Bayer phase, matching mr_dither_rgb8()'s y_base) - pass 0 for a full
 * frame.
 */
void mr_yuv420_dither8(const uint8_t *y_plane, int y_stride,
                       const uint8_t *u_plane, int u_stride,
                       const uint8_t *v_plane, int v_stride,
                       int width, int height, int vscale,
                       uint8_t *out, int out_stride, int y_base);

#endif /* MR_YUV_DITHER_H */
