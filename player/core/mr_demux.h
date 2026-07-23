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
    /* Borrowed container decoder setup.  For avc1 this is the avcC payload,
     * including SPS/PPS and the AVCC NAL length size. */
    const uint8_t *config;
    uint32_t config_len;
    int      valid;
} mr_video_info;

#define MR_AUDIO_CONFIG_MAX 16
#define MR_AUDIO_FORMAT_PCM 0x0001
#define MR_AUDIO_FORMAT_MP3 0x0055
#define MR_AUDIO_FORMAT_AAC 0x00ff

typedef struct {
    uint16_t format_tag;  /* WAVE tag (AVI) or mapped from MOV codec        */
    uint16_t channels;
    uint32_t sample_rate;
    uint16_t bits;
    /* Container codec setup bytes.  MP4 AAC stores its AudioSpecificConfig
     * here; packet decoders may ignore this for self-describing formats such
     * as PCM, MP3 and ADTS AAC. */
    uint8_t  config[MR_AUDIO_CONFIG_MAX];
    uint8_t  config_len;
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
    MR_CONTAINER_MOV,
    MR_CONTAINER_RAW_MJPEG,
    MR_CONTAINER_RAW_MPEG4
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
