/*
 * MintRIVA - MPEG transport stream demuxer.
 *
 * Supports 188-byte TS and 192-byte M2TS carrying MPEG-1/2 or H.264/AVC video,
 * plus MPEG Layer II or ADTS AAC audio. PES payloads are assembled into the
 * packet interface used by the existing codec/audio layers.
 */
#ifndef MR_TS_H
#define MR_TS_H

#include "mr_demux.h"
#include "mr_source.h"

typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
    size_t   expected;            /* ES bytes from bounded PES, 0 = unknown */
    uint64_t pts;                 /* 90 kHz, valid when has_pts is set       */
    int      has_pts;
    int      active;
    int      drained;
} mr_ts_pes;

typedef struct {
    const uint8_t *buf;
    size_t         len;
    mr_source     *source;      /* borrowed random-access compressed input  */
    int            file_backed;
    int            streaming;   /* forward-only source with unknown length   */
    int            packet_size; /* 188 for TS, 192 for M2TS                 */
    int            sync_off;    /* 0 for TS, 4 for M2TS                     */
    size_t         cursor;

    uint16_t       pmt_pid;
    uint16_t       video_pid;
    uint16_t       audio_pid;
    uint8_t        video_type;
    uint8_t        audio_type;
    uint8_t        unsupported_video_type; /* first video codec in the PMT we
                                            * cannot decode, 0 if none: lets the
                                            * caller name what it can't play  */

    mr_ts_pes      video_pes;
    mr_ts_pes      audio_pes;
    uint8_t       *packet_buf;  /* Annex-B -> AVCC output                   */
    size_t         packet_cap;
    uint8_t       *config;      /* generated avcC SPS/PPS                   */

    mr_video_info  video;
    mr_audio_info  audio;
    mr_demux_service_fn service;
    void          *service_opaque;
    mr_demux_timing timing;
} mr_ts;

mr_status mr_ts_open(mr_ts *t, const uint8_t *buf, size_t len);
mr_status mr_ts_open_source(mr_ts *t, mr_source *source, size_t len);
/* Human-readable name for a PMT video stream_type we don't decode (e.g. HEVC),
 * or NULL if the type is not a recognised video codec. */
const char *mr_ts_video_type_name(unsigned stream_type);
/* Human-readable PMT audio stream_type, including codecs not decoded yet. */
const char *mr_ts_audio_type_name(unsigned stream_type);
mr_status mr_ts_next_packet(mr_ts *t, mr_packet *pkt);
void      mr_ts_rewind(mr_ts *t);
void      mr_ts_close(mr_ts *t);

#endif /* MR_TS_H */
