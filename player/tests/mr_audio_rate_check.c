#include "../amiga/mr_audio_rate.h"

#include <stdint.h>
#include <stdio.h>

static int check_duration(unsigned input_rate, unsigned output_rate,
                          unsigned seconds)
{
    mr_audio_rate_state state;
    uint64_t input_samples = (uint64_t)input_rate * seconds;
    uint64_t expected = (uint64_t)output_rate * seconds;
    uint64_t actual = 0, i;

    mr_audio_rate_init(&state, input_rate, output_rate);
    for (i = 0; i < input_samples; i++)
        actual += mr_audio_rate_outputs(&state);

    if (actual != expected || state.phase != 0) {
        fprintf(stderr,
                "%u -> %u Hz over %u s: expected %llu, got %llu, phase %lu\n",
                input_rate, output_rate, seconds,
                (unsigned long long)expected, (unsigned long long)actual,
                (unsigned long)state.phase);
        return 0;
    }
    return 1;
}

int main(void)
{
    /* 22050 -> PAL period 160 is the YouTube case which exhausted the audio
     * cushion after roughly fifteen minutes without conversion. */
    if (!check_duration(22050, 3546895U / 160U, 15U * 60U)) return 1;
    /* Exercise the downsampling path used when the source exceeds Paula's
     * minimum hardware period. */
    if (!check_duration(48000, 3546895U / 124U, 15U * 60U)) return 1;
    /* And the corresponding NTSC clock selection. */
    if (!check_duration(22050, 3579545U / 162U, 15U * 60U)) return 1;
    puts("Paula rate conversion preserves 15-minute PCM duration");
    return 0;
}
