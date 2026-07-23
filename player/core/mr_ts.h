/*
 * MintRIVA - MPEG transport stream demuxer.
 *
 * Supports 188-byte TS and 192-byte M2TS carrying H.264/AVC video and ADTS
 * AAC audio. PES payloads are assembled into the packet interface used by the
 * existing codec/audio layers.
 */
#ifndef MR_TS_H
#define MR_TS_H

#include "mr_demux.h"

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    size_t   expected;            /* ES bytes from bounded PES, 0 = unknown */
    int      active;
    int      drained;
} mr_ts_pes;

typedef struct {
    const uint8_t *buf;
    size_t         len;
    void          *stream;      /* FILE *, opaque in the public header      */
    size_t         stream_pos;
    int            stream_pos_valid;
    int            file_backed;
    int            packet_size; /* 188 for TS, 192 for M2TS                 */
    int            sync_off;    /* 0 for TS, 4 for M2TS                     */
    size_t         cursor;

    uint16_t       pmt_pid;
    uint16_t       video_pid;
    uint16_t       audio_pid;
    uint8_t        video_type;
    uint8_t        audio_type;

    mr_ts_pes      video_pes;
    mr_ts_pes      audio_pes;
    uint8_t       *packet_buf;  /* Annex-B -> AVCC output                   */
    size_t         packet_cap;
    uint8_t       *config;      /* generated avcC SPS/PPS                   */

    mr_video_info  video;
    mr_audio_info  audio;
} mr_ts;

mr_status mr_ts_open(mr_ts *t, const uint8_t *buf, size_t len);
mr_status mr_ts_open_file(mr_ts *t, void *stream, size_t len);
mr_status mr_ts_next_packet(mr_ts *t, mr_packet *pkt);
void      mr_ts_rewind(mr_ts *t);
void      mr_ts_close(mr_ts *t);

#endif /* MR_TS_H */
