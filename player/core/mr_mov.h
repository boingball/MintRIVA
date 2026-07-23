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
    void              *stream;     /* FILE *, opaque here for public header */
    uint8_t           *metadata;   /* owned moov payload in file mode       */
    uint8_t           *packet_buf; /* reused by file-backed packet reads    */
    size_t             packet_cap;
    size_t             stream_pos;
    int                stream_pos_valid;
    int                file_backed;
    struct mov_sample *samples;   /* interleaved video-frame + audio-chunk  */
    uint32_t           nsamples;  /* index, sorted by file offset           */
    uint32_t           cap;
    uint32_t           cursor;
    mr_video_info      video;
    mr_audio_info      audio;
} mr_mov;

mr_status mr_mov_open(mr_mov *m, const uint8_t *buf, size_t len);
mr_status mr_mov_open_file(mr_mov *m, void *stream, size_t len);
mr_status mr_mov_next_packet(mr_mov *m, mr_packet *pkt);
void      mr_mov_rewind(mr_mov *m);
void      mr_mov_close(mr_mov *m);

#endif /* MR_MOV_H */
