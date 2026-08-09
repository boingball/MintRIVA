/*
 * MintRIVA - H.264/AVC decoder plugin backed by Ittiam libavc.
 *
 * The MOV demuxer supplies the avcC decoder configuration and one AVCC
 * (length-prefixed) access unit per packet.  The adapter converts both to
 * Annex B and lets libavc handle High Profile tools and display reordering.
 */
#ifndef MR_H264_H
#define MR_H264_H

#include "mr_codec.h"

extern const mr_codec mr_codec_h264;
typedef void (*mr_h264_service_fn)(void *opaque);
typedef struct mr_h264_timing {
    unsigned long input_us, core_us, output_us;
} mr_h264_timing;
typedef struct mr_h264_diag {
    unsigned long decoded;   /* libavc frames with u4_output_present set     */
    unsigned long emitted;   /* frames that completed RGB conversion          */
    unsigned long skipped;   /* frames skipped via skip_output               */
    unsigned long eagain;    /* frames consumed with no output yet            */
} mr_h264_diag;
void mr_h264_set_service(mr_decoder *dec, mr_h264_service_fn fn, void *opaque);
void mr_h264_frame_timing(mr_decoder *dec, mr_h264_timing *timing);
void mr_h264_set_skip_output(mr_decoder *dec, int skip);
/* Set rate-limit for MR_H264_DEBUG output: log every N frames (0 = always). */
void mr_h264_set_diag_log_rate(mr_decoder *dec, unsigned long every);
void mr_h264_get_diag(mr_decoder *dec, mr_h264_diag *out);

#endif /* MR_H264_H */
