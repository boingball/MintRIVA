/*
 * MintRIVA - minimal QuickTime (MOV) demuxer.
 *
 * Reconstructs the video track's frames from the stbl sample tables
 * (stsd/stsc/stsz/stco) pointing into mdat, and surfaces audio track info for
 * the MintAMP tier. Reached through mr_demux; neutral structs live in
 * mr_demux.h.
 */
#ifndef MR_MOV_H
#define MR_MOV_H

#include "mr_demux.h"

struct mov_sample;   /* opaque: {file offset, size} per video frame */

typedef struct {
    const uint8_t     *buf;
    size_t             len;
    struct mov_sample *samples;   /* flat video-frame index                */
    uint32_t           nsamples;
    uint32_t           cursor;
    mr_video_info      video;
    mr_audio_info      audio;
} mr_mov;

mr_status mr_mov_open(mr_mov *m, const uint8_t *buf, size_t len);
mr_status mr_mov_next_packet(mr_mov *m, mr_packet *pkt);
void      mr_mov_rewind(mr_mov *m);
void      mr_mov_close(mr_mov *m);

#endif /* MR_MOV_H */
