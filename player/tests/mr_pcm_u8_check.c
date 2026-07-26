#include "../audio/mr_pcm.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static mr_audio_info pcm_info(unsigned bits, unsigned channels, unsigned align)
{
    mr_audio_info i;
    memset(&i, 0, sizeof i);
    i.valid = 1;
    i.format_tag = MR_AUDIO_FORMAT_PCM;
    i.sample_rate = 22050;
    i.channels = (uint16_t)channels;
    i.bits_per_sample = (uint16_t)bits;
    i.block_align = (uint16_t)align;
    return i;
}

static int expect(int condition, const char *message)
{
    if (!condition) fprintf(stderr, "PCM check failed: %s\n", message);
    return condition;
}

int main(void)
{
    static const uint8_t u8[] = { 0xa5, 0x00, 0x80, 0xff, 0x5a };
    static const uint8_t s16[] = {
        0xa5, 0x00, 0x80, 0x00, 0x00, 0xff, 0x7f, 0x5a
    };
    static const uint8_t stereo[] = {
        0x00, 0x80, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x80
    };
    static const uint8_t padded[] = {
        0x00, 0x80, 0xee, 0xee, 0xff, 0x7f, 0xdd, 0xdd, 0xcc
    };
    int16_t guarded[10];
    mr_audio_info i;
    long n;

    memset(guarded, 0x36, sizeof guarded);
    i = pcm_info(8, 1, 1);
    n = mr_pcm_decode_s16(&i, u8 + 1, 3, guarded + 1, 3);
    if (!expect(n == 3, "U8 frame count") ||
        !expect(guarded[1] == -32768 && guarded[2] == 0 &&
                guarded[3] == 32512, "U8 signed conversion") ||
        !expect(guarded[0] == 0x3636 && guarded[4] == 0x3636,
                "U8 output guards")) return 1;
    if (!expect(u8[0] == 0xa5 && u8[4] == 0x5a, "U8 input guards")) return 1;

    memset(guarded, 0x36, sizeof guarded);
    i = pcm_info(16, 1, 2);
    n = mr_pcm_decode_s16(&i, s16 + 1, 6, guarded + 1, 3);
    if (!expect(n == 3, "S16LE frame count") ||
        !expect(guarded[1] == -32768 && guarded[2] == 0 &&
                guarded[3] == 32767, "S16LE signed conversion") ||
        !expect(guarded[0] == 0x3636 && guarded[4] == 0x3636,
                "S16LE output guards") ||
        !expect(s16[0] == 0xa5 && s16[7] == 0x5a, "S16LE input guards"))
        return 1;

    i = pcm_info(16, 2, 4);
    n = mr_pcm_decode_s16(&i, stereo, sizeof stereo, guarded, 2);
    if (!expect(n == 2, "stereo frame count") ||
        !expect(guarded[0] == -32768 && guarded[1] == 32767 &&
                guarded[2] == 0 && guarded[3] == -32768,
                "stereo interleaving")) return 1;

    i = pcm_info(16, 1, 4);
    n = mr_pcm_decode_s16(&i, padded, sizeof padded, guarded, 3);
    if (!expect(n == 2, "truncated final frame ignored") ||
        !expect(guarded[0] == -32768 && guarded[1] == 32767,
                "block alignment/padding")) return 1;

    i = pcm_info(16, 2, 3);
    if (!expect(!mr_pcm_supported(&i), "undersized block alignment rejected"))
        return 1;
    i = pcm_info(12, 1, 2);
    if (!expect(!mr_pcm_supported(&i), "unsupported depth rejected")) return 1;

    puts("PCM U8/S16LE mono, stereo, alignment and guard checks passed");
    return 0;
}
