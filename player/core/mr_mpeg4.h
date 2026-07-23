/*
 * MintRIVA - MPEG-4 Part 2 (Visual) decoder, aka DivX/Xvid.
 *
 * A from-scratch, portable-C decoder (no libavcodec/xvid dependency) plugged in
 * behind mr_codec.h. Built and validated bottom-up against ffmpeg.
 *
 * Status:
 *   done  - I-VOP (intra), P-VOP (half-pel MC, 1MV/4MV), quarter-pel MC,
 *           B-VOPs (display-order reorder + direct/forward/backward/interp
 *           prediction + co-located MV scaling + skipped-MB handling),
 *           video-packet resync. All validated in `make check`.
 *   todo  - GMC (sprite). S(GMC)-VOPs return MR_EUNSUPPORTED rather than
 *           decode garbage.
 *
 * One AVI/MP4 packet carries one coded VOP. State (reference frames, quant
 * matrices) persists across calls, like the other inter-frame decoders here.
 */
#ifndef MR_MPEG4_H
#define MR_MPEG4_H

#include "mr_codec.h"

extern const mr_codec mr_codec_mpeg4;

#endif /* MR_MPEG4_H */
