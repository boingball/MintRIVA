#include "mr_latm.h"

#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t bits, pos;
    int bad;
} latm_bits;

static unsigned get_bits(latm_bits *b, unsigned n)
{
    unsigned value = 0, i;
    if (n > 32 || b->pos + n > b->bits) { b->bad = 1; return 0; }
    for (i = 0; i < n; i++) {
        value = (value << 1) |
                ((b->data[b->pos >> 3] >> (7 - (b->pos & 7))) & 1);
        b->pos++;
    }
    return value;
}

static int parse_asc(latm_bits *b, mr_latm_config *cfg)
{
    static const unsigned rates[13] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350
    };
    unsigned object = get_bits(b, 5);
    unsigned sri, channels, depends, extension;
    if (object == 31) object = 32 + get_bits(b, 6);
    sri = get_bits(b, 4);
    if (sri == 15) { get_bits(b, 24); return 0; }
    channels = get_bits(b, 4);
    /* MintRIVA's fixed Helix path currently accepts AAC-LC mono/stereo. */
    if (b->bad || object != 2 || sri >= 13 || channels < 1 || channels > 2)
        return 0;
    get_bits(b, 1);                       /* frameLengthFlag              */
    depends = get_bits(b, 1);
    if (depends) get_bits(b, 14);         /* coreCoderDelay               */
    extension = get_bits(b, 1);
    if (b->bad || extension) return 0;    /* LC has no extension payload  */
    memset(cfg, 0, sizeof *cfg);
    cfg->object_type = object;
    cfg->sample_rate = rates[sri];
    cfg->channels = channels;
    cfg->asc[0] = (uint8_t)((object << 3) | (sri >> 1));
    cfg->asc[1] = (uint8_t)((sri << 7) | (channels << 3));
    cfg->asc_len = 2;
    cfg->valid = 1;
    return 1;
}

int mr_latm_payload(const uint8_t *data, size_t len, mr_latm_config *cfg,
                    size_t *payload_bit, size_t *payload_len)
{
    latm_bits b;
    unsigned same, audio_mux_version, frame_length_type, n;
    size_t bytes = 0;
    if (!data || !cfg || !payload_bit || !payload_len) return 0;
    b.data = data; b.bits = len * 8; b.pos = 0; b.bad = 0;
    same = get_bits(&b, 1);
    if (!same) {
        audio_mux_version = get_bits(&b, 1);
        if (audio_mux_version) return 0;  /* version A/tara not supported */
        if (!get_bits(&b, 1)) return 0;   /* allStreamsSameTimeFraming    */
        if (get_bits(&b, 6) != 0 ||      /* numSubFrames                 */
            get_bits(&b, 4) != 0 ||      /* numProgram                   */
            get_bits(&b, 3) != 0)        /* numLayer                     */
            return 0;
        if (!parse_asc(&b, cfg)) return 0;
        frame_length_type = get_bits(&b, 3);
        if (frame_length_type != 0) return 0;
        get_bits(&b, 8);                  /* latmBufferFullness            */
        if (get_bits(&b, 1)) {            /* otherDataPresent             */
            unsigned esc;
            do { esc = get_bits(&b, 1); get_bits(&b, 8); } while (esc && !b.bad);
        }
        if (get_bits(&b, 1)) get_bits(&b, 8); /* crcCheckPresent/value      */
    } else if (!cfg->valid) {
        return 0;
    }
    if (b.bad) return 0;
    do {
        n = get_bits(&b, 8);
        if (b.bad || bytes > 1024U * 1024U - n) return 0;
        bytes += n;
    } while (n == 255);
    if (b.pos + bytes * 8 > b.bits) return 0;
    *payload_bit = b.pos;
    *payload_len = bytes;
    return bytes != 0;
}

int mr_latm_copy_payload(const uint8_t *data, size_t len, size_t bit,
                         uint8_t *out, size_t bytes)
{
    size_t i;
    if (!data || !out || bit + bytes * 8 > len * 8) return 0;
    if (!(bit & 7)) {
        memcpy(out, data + (bit >> 3), bytes);
        return 1;
    }
    for (i = 0; i < bytes; i++) {
        unsigned shift = (unsigned)(bit & 7);
        size_t p = (bit >> 3) + i;
        unsigned value = (unsigned)data[p] << 8;
        if (p + 1 < len) value |= data[p + 1];
        out[i] = (uint8_t)(value >> (8 - shift));
    }
    return 1;
}
