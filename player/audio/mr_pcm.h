/* RIFF/WAVE PCM byte conversion to native signed 16-bit interleaved PCM. */
#ifndef MR_PCM_H
#define MR_PCM_H

#include "../core/mr_demux.h"
#include <stddef.h>
#include <stdint.h>

/* Validate the PCM layout.  PCM depths supported here all decode to S16. */
int mr_pcm_supported(const mr_audio_info *info);

/* Decode at most out_frame_capacity complete frames.  A trailing incomplete
 * frame is deliberately ignored.  Returns frames decoded, or -1 for an
 * unsupported/malformed layout. */
long mr_pcm_decode_s16(const mr_audio_info *info,
                       const uint8_t *src, size_t src_len,
                       int16_t *out, size_t out_frame_capacity);

#endif
