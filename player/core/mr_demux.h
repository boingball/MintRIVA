/*
 * MintRIVA - container-agnostic demux interface.
 *
 * The player shouldn't care whether frames come from AVI or QuickTime MOV, so
 * both parsers fill these neutral info/packet structs and are reached through
 * one auto-detecting front end (mr_demux_open sniffs the signature). Adding a
 * container = adding a backend here, exactly like adding a codec behind
 * mr_codec.h.
 */
#ifndef MR_DEMUX_H
#define MR_DEMUX_H

#include "mr_types.h"

typedef struct {
    uint32_t fourcc;      /* video codec (e.g. 'cvid')                      */
    int      width;
    int      height;
    uint32_t rate;        /* fps = rate / scale                             */
    uint32_t scale;
    int      valid;
} mr_video_info;

typedef struct {
    uint16_t format_tag;  /* WAVE tag (AVI) or mapped from MOV codec        */
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits;
    int      valid;
} mr_audio_info;

typedef struct {
    int            is_video;
    const uint8_t *data;
    uint32_t       len;
} mr_packet;

typedef enum {
    MR_CONTAINER_NONE = 0,
    MR_CONTAINER_AVI,
    MR_CONTAINER_MOV
} mr_container;

typedef struct mr_demux mr_demux;

/* Auto-detect container and open over an in-memory buffer (borrowed, must
 * outlive the demux). Returns NULL if unrecognised/malformed. */
mr_demux    *mr_demux_open(const uint8_t *buf, size_t len);
mr_status    mr_demux_next_packet(mr_demux *d, mr_packet *pkt);
void         mr_demux_rewind(mr_demux *d);
void         mr_demux_close(mr_demux *d);

const mr_video_info *mr_demux_video(const mr_demux *d);
const mr_audio_info *mr_demux_audio(const mr_demux *d);
const char          *mr_demux_container_name(const mr_demux *d);

#endif /* MR_DEMUX_H */
