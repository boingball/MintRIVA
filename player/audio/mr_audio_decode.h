/*
 * MintRIVA - packet audio decode adapter.
 *
 * This is glue only: the actual MP3/AAC codecs come from MintAMP's fixed-point
 * Helix sources.  The demuxer feeds compressed packets here and receives
 * native-endian signed 16-bit PCM through the sink callback.
 */
#ifndef MR_AUDIO_DECODE_H
#define MR_AUDIO_DECODE_H

#include "../core/mr_demux.h"
#include <stdint.h>

typedef struct mr_audio_decoder mr_audio_decoder;

typedef void (*mr_audio_pcm_sink)(void *user, const int16_t *pcm,
                                  unsigned frames, unsigned channels);

mr_audio_decoder *mr_audio_decoder_open(const mr_audio_info *info);
void              mr_audio_decoder_close(mr_audio_decoder *dec);

/* Feed one demuxed packet. Returns PCM sample frames produced, zero when the
 * codec needs more compressed input, or a negative value on a fatal error. */
long mr_audio_decoder_feed(mr_audio_decoder *dec,
                           const uint8_t *data, uint32_t len,
                           mr_audio_pcm_sink sink, void *sink_user);
int  mr_audio_decoder_reset(mr_audio_decoder *dec);

unsigned    mr_audio_decoder_rate(const mr_audio_decoder *dec);
unsigned    mr_audio_decoder_channels(const mr_audio_decoder *dec);
const char *mr_audio_decoder_name(const mr_audio_decoder *dec);

#endif /* MR_AUDIO_DECODE_H */
