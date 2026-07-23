/*
 * MintRIVA - raw MPEG-4 Part 2 Visual elementary-stream demuxer.
 */
#ifndef MR_RAW_MPEG4_H
#define MR_RAW_MPEG4_H

#include "mr_demux.h"

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         cursor;
    mr_video_info  video;
    mr_audio_info  audio;
} mr_raw_mpeg4;

mr_status mr_raw_mpeg4_open(mr_raw_mpeg4 *m, const uint8_t *buf, size_t len);
mr_status mr_raw_mpeg4_next_packet(mr_raw_mpeg4 *m, mr_packet *pkt);
void      mr_raw_mpeg4_rewind(mr_raw_mpeg4 *m);

#endif /* MR_RAW_MPEG4_H */
