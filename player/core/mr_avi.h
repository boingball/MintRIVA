/*
 * MintRIVA - minimal AVI (RIFF) demuxer.
 *
 * Scope: enough to drive the player - identify the video stream and its codec
 * fourcc, expose geometry and frame rate, and iterate 'movi' packets. Audio
 * stream info is surfaced so the MintAMP audio tier can be wired in later.
 *
 * The host build parses an in-memory buffer; the Amiga build will feed the
 * same parser from a streaming/async read, so the packet iterator is kept
 * cursor-based rather than callback-based.
 */
#ifndef MR_AVI_H
#define MR_AVI_H

#include "mr_types.h"

typedef struct {
    uint32_t fourcc;      /* biCompression / handler                        */
    int      width;
    int      height;
    uint32_t rate;        /* dwRate / dwScale => fps = rate/scale           */
    uint32_t scale;
    int      valid;
} mr_avi_video_info;

typedef struct {
    uint16_t format_tag;  /* WAVE format tag (0x55 = MP3, 0x50 = MP2, ...)  */
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits;
    int      valid;
} mr_avi_audio_info;

typedef struct {
    const uint8_t *buf;
    size_t         len;

    size_t         movi_off;   /* start of 'movi' payload                   */
    size_t         movi_end;
    size_t         cursor;     /* current position within movi             */

    int            video_stream;   /* stream index of the video track      */
    int            audio_stream;

    mr_avi_video_info video;
    mr_avi_audio_info audio;
} mr_avi;

typedef struct {
    int            stream;     /* which stream this packet belongs to       */
    int            is_video;
    int            keyframe;
    const uint8_t *data;
    uint32_t       len;
} mr_avi_packet;

mr_status mr_avi_open(mr_avi *a, const uint8_t *buf, size_t len);

/* Returns MR_OK and fills pkt, MR_EAGAIN at end of stream. */
mr_status mr_avi_next_packet(mr_avi *a, mr_avi_packet *pkt);

/* Rewind the packet cursor to the start of 'movi' (for looping). */
void mr_avi_rewind(mr_avi *a);

#endif /* MR_AVI_H */
