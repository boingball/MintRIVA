/*
 * MintRIVA - decoder registry and lifecycle glue.
 */
#include "mr_codec.h"

static const mr_codec *const g_codecs[] = {
    &mr_codec_cinepak,
    &mr_codec_mjpeg,
    /* more decoders slot in here: mpeg1, ... */
};

const mr_codec *mr_codec_find(uint32_t fourcc)
{
    size_t i, j;
    for (i = 0; i < sizeof(g_codecs) / sizeof(g_codecs[0]); i++) {
        const mr_codec *c = g_codecs[i];
        for (j = 0; j < 4; j++) {
            if (c->fourcc[j] && c->fourcc[j] == fourcc)
                return c;
        }
    }
    return NULL;
}

mr_status mr_decoder_open(mr_decoder *dec, const mr_codec *codec,
                          int width, int height)
{
    if (!dec || !codec || width <= 0 || height <= 0)
        return MR_ERR;
    dec->codec  = codec;
    dec->width  = width;
    dec->height = height;
    dec->priv   = NULL;
    dec->frame.data = NULL;
    return codec->open(dec);
}

mr_status mr_decoder_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    if (!dec || !dec->codec)
        return MR_ERR;
    return dec->codec->decode(dec, data, len);
}

void mr_decoder_close(mr_decoder *dec)
{
    if (dec && dec->codec && dec->codec->close)
        dec->codec->close(dec);
}
