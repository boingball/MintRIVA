/*
 * MintVID - minimal QuickTime (MOV) demuxer.
 *
 * Reconstructs the video track's frames from the stbl sample tables
 * (stsd/stsc/stsz/stco) pointing into mdat, and surfaces audio track info for
 * the MintAMP tier. Reached through mr_demux; neutral structs live in
 * mr_demux.h.
 */
#ifndef MR_MOV_H
#define MR_MOV_H

#include "mr_demux.h"
#include "mr_source.h"

struct mov_sample;   /* opaque: {file offset, size} per video frame */

typedef struct {
    const uint8_t     *buf;
    size_t             len;
    mr_source         *source;     /* borrowed random-access compressed input */
    uint8_t           *metadata;   /* owned moov payload in file mode       */
    uint8_t           *packet_buf; /* reused by file-backed packet reads    */
    size_t             packet_cap;
    int                file_backed;
    struct mov_sample *samples;   /* interleaved video-frame + audio-chunk  */
    uint32_t           nsamples;  /* index, sorted by file offset           */
    uint32_t           cap;
    uint32_t           cursor;
    mr_video_info      video;
    mr_audio_info      audio;
    uint8_t            rawvideo_config[4]; /* LE source row stride         */
} mr_mov;

mr_status mr_mov_open(mr_mov *m, const uint8_t *buf, size_t len);
mr_status mr_mov_open_source(mr_mov *m, mr_source *source, size_t len);
mr_status mr_mov_next_packet(mr_mov *m, mr_packet *pkt);
void      mr_mov_rewind(mr_mov *m);
/* Seek to the nearest video keyframe at or before target_ms (clamped into
 * range at either end). *out_ms receives the keyframe actually reached. */
mr_status mr_mov_seek(mr_mov *m, uint64_t target_ms, uint64_t *out_ms);
void      mr_mov_close(mr_mov *m);

/* Read-only diagnostics over the interleaved sample index, for validating
 * mr_mov_seek() against ground truth without exposing struct mov_sample. */
uint32_t  mr_mov_sample_count(const mr_mov *m);
void      mr_mov_sample_info(const mr_mov *m, uint32_t index, uint64_t *t_ms,
                             int *is_video, int *is_keyframe);

#endif /* MR_MOV_H */
