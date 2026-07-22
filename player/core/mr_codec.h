/*
 * MintRIVA - video decoder plugin interface
 *
 * The whole point of this project is a *codec-agnostic* player: one demux +
 * sync + render skeleton, with decoders that plug in behind this vtable. That
 * is what lets the same player scale from Cinepak on a bare A600/AGA up to
 * heavier codecs on a fast PiStorm/RTG machine - you add a decoder, not a
 * player.
 */
#ifndef MR_CODEC_H
#define MR_CODEC_H

#include "mr_types.h"

typedef struct mr_decoder mr_decoder;

typedef struct mr_codec {
    const char *name;
    /* AVI fourcc(s) this decoder claims (BITMAPINFOHEADER biCompression). */
    uint32_t    fourcc[4];
    /* Allocate decoder state for a w*h stream. */
    mr_status (*open)(mr_decoder *dec);
    /* Decode one compressed frame into dec->frame. May reuse/patch the
     * previous frame buffer (inter frames), so the buffer persists across
     * calls until close(). */
    mr_status (*decode)(mr_decoder *dec, const uint8_t *data, uint32_t len);
    void      (*close)(mr_decoder *dec);
} mr_codec;

struct mr_decoder {
    const mr_codec *codec;
    int             width;
    int             height;
    mr_frame        frame;   /* filled in by decode()                       */
    void           *priv;    /* decoder-private state                       */
};

/* Registry: decoders self-select by fourcc. */
const mr_codec *mr_codec_find(uint32_t fourcc);

mr_status mr_decoder_open(mr_decoder *dec, const mr_codec *codec,
                          int width, int height);
mr_status mr_decoder_decode(mr_decoder *dec, const uint8_t *data, uint32_t len);
void      mr_decoder_close(mr_decoder *dec);

/* Individual codec descriptors (defined in their .c files). */
extern const mr_codec mr_codec_cinepak;

#endif /* MR_CODEC_H */
