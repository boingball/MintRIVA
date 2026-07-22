/*
 * MintRIVA - minimal AVI (RIFF) demuxer.
 *
 * Identifies the video stream + codec fourcc, geometry and frame rate, and
 * iterates 'movi' packets. Audio stream info is surfaced for the MintAMP audio
 * tier. Reached through mr_demux (mr_demux.h); the neutral info/packet structs
 * live there.
 */
#ifndef MR_AVI_H
#define MR_AVI_H

#include "mr_demux.h"

typedef struct {
    const uint8_t *buf;
    size_t         len;

    size_t         movi_off;   /* start of 'movi' payload                   */
    size_t         movi_end;
    size_t         cursor;

    int            video_stream;
    int            audio_stream;

    mr_video_info  video;
    mr_audio_info  audio;
} mr_avi;

mr_status mr_avi_open(mr_avi *a, const uint8_t *buf, size_t len);
mr_status mr_avi_next_packet(mr_avi *a, mr_packet *pkt);
void      mr_avi_rewind(mr_avi *a);

#endif /* MR_AVI_H */
