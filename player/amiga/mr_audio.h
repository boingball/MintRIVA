/*
 * MintRIVA - Amiga audio output (abstract).
 *
 * A compact Paula (audio.device) PCM sink, modelled on MintAMP's proven
 * streaming/teardown patterns but self-contained. The player feeds it source
 * PCM (as the demuxer delivers it); the backend converts to Paula's signed
 * 8-bit mono and double-buffers it out. Its played-milliseconds counter is the
 * A/V master clock.
 *
 * Compressed audio (MP2/MP3) will decode via MintAMP/libhelix to PCM and feed
 * this same sink; today's files are PCM so no decode is needed.
 */
#ifndef MR_AUDIO_H
#define MR_AUDIO_H

typedef struct mr_audio mr_audio;

/* Open Paula output for source PCM of this rate/channels/bits (8 or 16).
 * Returns NULL if unsupported or on failure. */
mr_audio     *audio_open(unsigned rate, int channels, int bits);

/* Convert and enqueue a source-PCM buffer (a demuxer audio packet). */
void          audio_write(mr_audio *a, const unsigned char *pcm, unsigned bytes);

/* Pump the double buffer: reap finished writes, submit new ones from the FIFO.
 * Must be called frequently (the player calls it while pacing video). */
void          audio_service(mr_audio *a);

/* Master clock: milliseconds of audio actually played so far. */
unsigned long audio_elapsed_ms(mr_audio *a);

/* True when the FIFO is empty and nothing is in flight (underrun) - the player
 * uses this to avoid stalling video on an audio clock that has stopped. */
int           audio_starved(mr_audio *a);

void          audio_close(mr_audio *a);

#endif /* MR_AUDIO_H */
