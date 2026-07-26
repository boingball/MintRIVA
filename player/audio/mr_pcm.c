#include "mr_pcm.h"

static unsigned bytes_per_sample(unsigned bits)
{
    return bits == 8 ? 1 : bits == 16 ? 2 : bits == 24 ? 3 :
           bits == 32 ? 4 : 0;
}

int mr_pcm_supported(const mr_audio_info *info)
{
    unsigned bytes;
    if (!info || !info->valid || info->format_tag != MR_AUDIO_FORMAT_PCM ||
        !info->channels || !info->sample_rate)
        return 0;
    bytes = bytes_per_sample(info->bits_per_sample);
    return bytes && info->block_align >= info->channels * bytes;
}

long mr_pcm_decode_s16(const mr_audio_info *info,
                       const uint8_t *src, size_t src_len,
                       int16_t *out, size_t out_frame_capacity)
{
    size_t frames, frame;
    unsigned channel, bytes;
    if (!mr_pcm_supported(info) || (!src && src_len) || !out)
        return -1;
    bytes = bytes_per_sample(info->bits_per_sample);
    frames = src_len / info->block_align;
    if (frames > out_frame_capacity) frames = out_frame_capacity;

    for (frame = 0; frame < frames; frame++) {
        const uint8_t *p = src + frame * info->block_align;
        for (channel = 0; channel < info->channels; channel++, p += bytes) {
            int16_t sample;
            if (bytes == 1) {
                sample = info->pcm_signed
                       ? (int16_t)((int16_t)(int8_t)p[0] << 8)
                       : (int16_t)(((int)p[0] - 128) << 8);
            } else if (bytes == 2) {
                sample = info->pcm_big_endian
                       ? (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1])
                       : (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
            } else if (bytes == 3) {
                /* Preserve the most significant 16 bits of signed S24. */
                sample = info->pcm_big_endian
                       ? (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1])
                       : (int16_t)((uint16_t)p[1] | ((uint16_t)p[2] << 8));
            } else {
                /* Preserve the most significant 16 bits of signed S32. */
                sample = info->pcm_big_endian
                       ? (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1])
                       : (int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8));
            }
            out[frame * info->channels + channel] = sample;
        }
    }
    return (long)frames;
}
