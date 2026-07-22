/*
 * MintRIVA - MPEG-1 program-stream source (wraps pl_mpeg).
 *
 * MPEG-1 .mpg is a self-contained stream (its own demux + video + MP2 audio),
 * so it does not fit the mr_demux -> mr_codec split; it gets this small source
 * wrapper instead. pl_mpeg (Dominic Szablewski, MIT) does the heavy lifting;
 * this exposes just what the player/harness need: open a memory buffer, pull
 * decoded RGB frames, rewind, close. (MP2 audio comes later.)
 */
#ifndef MR_MPEG1_H
#define MR_MPEG1_H

#include "mr_types.h"

typedef struct mr_mpeg1 mr_mpeg1;

/* True if the buffer looks like an MPEG-1 program stream (pack header). */
int        mr_mpeg1_probe(const uint8_t *buf, size_t len);

/* Open over a borrowed buffer (must outlive the source). NULL on failure. */
mr_mpeg1  *mr_mpeg1_open(const uint8_t *buf, size_t len);

int        mr_mpeg1_width(mr_mpeg1 *m);
int        mr_mpeg1_height(mr_mpeg1 *m);
double     mr_mpeg1_framerate(mr_mpeg1 *m);

/* Audio: the effective output sample rate (0 = no audio track). MP2 is decoded
 * as stereo; the rate is halved internally if the stream is above Paula's reach
 * (~28 kHz), so this is the rate to open the audio backend with. */
unsigned   mr_mpeg1_samplerate(mr_mpeg1 *m);

/* Decode the next video frame into `out` (RGB24, owned by the source); *pts (if
 * non-NULL) gets its presentation time in seconds. Returns 1 on a frame, 0 at
 * end of stream. */
int        mr_mpeg1_next(mr_mpeg1 *m, mr_frame *out, double *pts);

/* Decode one audio frame into `dst` as little-endian signed-16 stereo
 * interleaved bytes (room for 1152*4 bytes needed; explicit LE so it is correct
 * on the big-endian 68k). Returns the output sample-frame count, or 0 if none
 * is available right now. */
int        mr_mpeg1_audio(mr_mpeg1 *m, unsigned char *dst);

void       mr_mpeg1_rewind(mr_mpeg1 *m);
void       mr_mpeg1_close(mr_mpeg1 *m);

#endif /* MR_MPEG1_H */
