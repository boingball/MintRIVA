#include "../amiga/mr_audio.h"
#include <stdio.h>

int main(void)
{
    if (audio_pcm_u8_to_s16(0x00) != (short)-32768) return 1;
    if (audio_pcm_u8_to_s16(0x80) != 0) return 1;
    if (audio_pcm_u8_to_s16(0xff) != (short)32512) return 1;
    puts("PCM U8 conversion checks passed");
    return 0;
}
