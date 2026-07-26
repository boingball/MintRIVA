#include <stdint.h>
#include <stdio.h>

#include "../core/mr_mpeg1.h"

static int check(const char *name, const uint8_t *data, size_t size,
                 int is_mpeg1, int is_mpeg2)
{
    int got_mpeg1 = mr_mpeg1_probe(data, size);
    int got_mpeg2 = mr_mpeg2_ps_probe(data, size);
    if (got_mpeg1 != is_mpeg1 || got_mpeg2 != is_mpeg2) {
        fprintf(stderr, "%s: MPEG-1=%d MPEG-2=%d, expected %d/%d\n",
                name, got_mpeg1, got_mpeg2, is_mpeg1, is_mpeg2);
        return 1;
    }
    return 0;
}

int main(void)
{
    static const uint8_t not_program_stream[] = {
        0x00, 0x00, 0x01, 0xB3, 0x11, 0x22, 0x33, 0x44
    };
    static const uint8_t mpeg1_program_stream[] = {
        0x00, 0x00, 0x01, 0xBA, 0x21, 0x00,
        0x00, 0x00, 0x01, 0xB3, 0x11, 0x22,
        0x00, 0x00, 0x01, 0x00, 0x00
    };
    static const uint8_t mpeg2_program_stream[] = {
        0x00, 0x00, 0x01, 0xBA, 0x44, 0x00,
        0x00, 0x00, 0x01, 0xB3, 0x11, 0x22,
        0x00, 0x00, 0x01, 0xB5, 0x10,
        0x00, 0x00, 0x01, 0x00, 0x00
    };

    return check("elementary stream", not_program_stream,
                 sizeof not_program_stream, 0, 0) ||
           check("MPEG-1 program stream", mpeg1_program_stream,
                 sizeof mpeg1_program_stream, 1, 0) ||
           check("MPEG-2 program stream", mpeg2_program_stream,
                 sizeof mpeg2_program_stream, 0, 1);
}
