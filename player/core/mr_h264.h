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
/* Returns non-zero to abort the current decode (called between NAL sub-calls). */
typedef int  (*mr_h264_quit_fn)(void *opaque);
typedef struct mr_h264_timing {
    unsigned long input_us, core_us, output_us;
} mr_h264_timing;
void mr_h264_set_service(mr_decoder *dec, mr_h264_service_fn fn, void *opaque);
void mr_h264_set_quit(mr_decoder *dec, mr_h264_quit_fn fn, void *opaque);
void mr_h264_set_diag(mr_decoder *dec, const char *path, int width, int height);
void mr_h264_frame_timing(mr_decoder *dec, mr_h264_timing *timing);
void mr_h264_set_skip_output(mr_decoder *dec, int skip);

#endif /* MR_H264_H */
