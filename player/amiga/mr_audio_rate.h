/*
 * MintVID - allocation-free streaming PCM rate conversion helper.
 *
 * Paula selects its sample rate with an integer hardware period, so the rate
 * it can actually play is normally a little different from the source rate.
 * This phase accumulator returns how many output samples (usually zero or
 * one, occasionally two) should be emitted for each input sample.  Repeating
 * or dropping the nearest sample is deliberately simple: Paula is 8-bit mono,
 * and keeping the duration exact matters much more than interpolation here.
 */
#ifndef MR_AUDIO_RATE_H
#define MR_AUDIO_RATE_H

#include <stdint.h>

typedef struct mr_audio_rate_state {
    uint32_t phase;
    uint32_t input_rate;
    uint32_t output_rate;
} mr_audio_rate_state;

static inline void mr_audio_rate_init(mr_audio_rate_state *s,
                                      unsigned input_rate,
                                      unsigned output_rate)
{
    s->phase = 0;
    s->input_rate = input_rate ? input_rate : 1;
    s->output_rate = output_rate;
}

static inline unsigned mr_audio_rate_outputs(mr_audio_rate_state *s)
{
    unsigned outputs = 0;
    s->phase += s->output_rate;
    while (s->phase >= s->input_rate) {
        s->phase -= s->input_rate;
        outputs++;
    }
    return outputs;
}

#endif /* MR_AUDIO_RATE_H */
