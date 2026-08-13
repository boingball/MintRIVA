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
    /* Sub-stages of core_us, broken out via wrapped libavc function pointers
     * (vendor/libavc_port/ih264d_stage_profile.c): motion compensation,
     * deblocking, and IDCT/reconstruction. core_us minus these three is
     * everything else - bitstream/CABAC/CAVLC parsing, intra prediction,
     * MV prediction, and per-MB bookkeeping - which has no single function
     * pointer to wrap so is not broken out further. */
    unsigned long mc_us, deblock_us, recon_us;
} mr_h264_timing;
typedef enum mr_h264_speed_mode {
    MR_H264_SPEED_QUALITY = 0,
    MR_H264_SPEED_BALANCED,
    MR_H264_SPEED_FAST
} mr_h264_speed_mode;
void mr_h264_set_service(mr_decoder *dec, mr_h264_service_fn fn, void *opaque);
void mr_h264_set_quit(mr_decoder *dec, mr_h264_quit_fn fn, void *opaque);
void mr_h264_set_diag(mr_decoder *dec, const char *path, int width, int height);
void mr_h264_frame_timing(mr_decoder *dec, mr_h264_timing *timing);
void mr_h264_set_skip_output(mr_decoder *dec, int skip);
/* Associate the next compressed access unit with its container PTS.  Libavc
 * may emit an older access unit after display reordering; output_pts() returns
 * the PTS belonging to that emitted frame rather than the current input. */
void mr_h264_set_input_pts(mr_decoder *dec, int has_pts, uint64_t pts_us);
int mr_h264_output_pts(mr_decoder *dec, uint64_t *pts_us);
/* Select libavc's quality/performance trade-off. Balanced only degrades
 * non-reference pictures; Fast applies cheaper filtering to all non-key
 * pictures. Returns non-zero when the decoder accepted the control call. */
int mr_h264_set_speed_mode(mr_decoder *dec, mr_h264_speed_mode mode);

#endif /* MR_H264_H */
