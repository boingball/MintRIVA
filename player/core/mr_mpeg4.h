/*
 * MintRIVA - MPEG-4 Part 2 (Visual) decoder, aka DivX/Xvid.
 *
 * A from-scratch, portable-C decoder (no libavcodec/xvid dependency) plugged in
 * behind mr_codec.h. Built and validated bottom-up against ffmpeg: I-VOP first,
 * then P-VOP (half-pel MC), then the Advanced Simple Profile tools (B-VOPs,
 * quarter-pel, GMC). Streams using tools not yet handled are rejected cleanly
 * (MR_EUNSUPPORTED) rather than decoded to garbage.
 *
 * One AVI/MP4 packet carries one coded VOP. State (reference frames, quant
 * matrices) persists across calls, like the other inter-frame decoders here.
 */
#ifndef MR_MPEG4_H
#define MR_MPEG4_H

#include "mr_codec.h"

extern const mr_codec mr_codec_mpeg4;

#endif /* MR_MPEG4_H */
