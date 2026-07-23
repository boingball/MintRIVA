/*
 * MintRIVA - raw Motion-JPEG demuxer.
 *
 * A raw .mjpeg stream is simply a sequence of complete JPEG images with no
 * container metadata. Frame rate is therefore unknown; we use 25 fps.
 */
#ifndef MR_RAW_MJPEG_H
#define MR_RAW_MJPEG_H

#include "mr_demux.h"

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         cursor;
    mr_video_info  video;
    mr_audio_info  audio;
} mr_raw_mjpeg;

mr_status mr_raw_mjpeg_open(mr_raw_mjpeg *m, const uint8_t *buf, size_t len);
mr_status mr_raw_mjpeg_next_packet(mr_raw_mjpeg *m, mr_packet *pkt);
void      mr_raw_mjpeg_rewind(mr_raw_mjpeg *m);

#endif /* MR_RAW_MJPEG_H */
