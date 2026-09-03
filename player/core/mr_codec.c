/*
 * MintVID - decoder registry and lifecycle glue.
 */
#include "mr_codec.h"

#include <stdlib.h>
#include <string.h>

static const mr_codec *const g_codecs[] = {
    &mr_codec_cinepak,
    &mr_codec_mjpeg,
    &mr_codec_mpeg2,
    &mr_codec_mpeg4,
    &mr_codec_msmpeg4v2,
    &mr_codec_wmv1,
    &mr_codec_wmv2,
    &mr_codec_h263,
    &mr_codec_msvideo1,
    &mr_codec_rle,
    &mr_codec_rawvideo,
#ifdef MR_HAVE_H264
    &mr_codec_h264,
#endif
};

const mr_codec *mr_codec_find(uint32_t fourcc)
{
    size_t i, j;
    for (i = 0; i < sizeof(g_codecs) / sizeof(g_codecs[0]); i++) {
        const mr_codec *c = g_codecs[i];
        for (j = 0; j < sizeof(c->fourcc) / sizeof(c->fourcc[0]); j++) {
            if (c->fourcc[j] && c->fourcc[j] == fourcc)
                return c;
        }
    }
    return NULL;
}

mr_status mr_decoder_open(mr_decoder *dec, const mr_codec *codec,
                          int width, int height)
{
    return mr_decoder_open_config(dec, codec, width, height, NULL, 0);
}

mr_status mr_decoder_open_config(mr_decoder *dec, const mr_codec *codec,
                                 int width, int height,
                                 const uint8_t *config, uint32_t config_len)
{
    if (!dec || !codec || width <= 0 || height <= 0)
        return MR_ERR;
    dec->codec  = codec;
    dec->width  = width;
    dec->height = height;
    dec->config = config;
    dec->config_len = config_len;
    dec->priv   = NULL;
    dec->frame.data = NULL;
    return codec->open(dec);
}

#ifdef MR_HAVE_H264
static int h264_packet_is_annexb(const uint8_t *data, uint32_t len)
{
    if (!data || len < 3) return 0;
    if (data[0] || data[1]) return 0;
    if (data[2] == 1) return 1;
    return len >= 4 && data[2] == 0 && data[3] == 1;
}

static uint32_t avcc_read_nal_size(const uint8_t *p, unsigned bytes)
{
    uint32_t n = 0;
    unsigned i;
    for (i = 0; i < bytes; i++) n = (n << 8) | p[i];
    return n;
}

/*
 * An avc1 sample entry carries its sequence/picture parameter sets in avcC.
 * They are deliberately fed to libavc once, during h264_open(), before its
 * shared display buffers are allocated.  Some hardware encoders nevertheless
 * repeat SPS/PPS NALs inside the first MP4 sample.  AMD AMF is one real-world
 * example, and may repeat an SPS with extra VUI colour/timing fields even
 * though the coded geometry is unchanged.
 *
 * Passing that second SPS to libavc after setup can make it treat the first
 * access unit as a sequence reconfiguration while MintVID still owns buffers
 * sized from the avcC configuration; on Amiga this has been observed as a
 * black window stuck at "buffering first frame".  For avc1 the out-of-band
 * avcC configuration is authoritative (avc3 is the variant intended for
 * in-band parameter-set changes), so strip type 7/8 NALs from length-prefixed
 * avc1 samples before handing them to the existing H.264 adapter.
 *
 * Annex-B input (MPEG-TS/HLS) is detected and left byte-for-byte untouched.
 * The common MP4 path also stays allocation-free: we scan first and allocate
 * a temporary packet only when a stray SPS/PPS is actually present.
 */
static mr_status decode_avc1_sample(mr_decoder *dec,
                                    const uint8_t *data, uint32_t len)
{
    const uint8_t *cfg = dec->config;
    unsigned nls;
    uint32_t p;
    int has_parameter_set = 0;
    uint8_t *filtered;
    uint32_t out = 0;
    mr_status st;

    if (dec->codec != &mr_codec_h264 || !cfg || dec->config_len < 7 ||
        cfg[0] != 1 || h264_packet_is_annexb(data, len))
        return dec->codec->decode(dec, data, len);

    nls = (unsigned)(cfg[4] & 3u) + 1u;
    p = 0;
    while (p < len) {
        uint32_t n;
        unsigned type;
        if (len - p < nls)
            return dec->codec->decode(dec, data, len);
        n = avcc_read_nal_size(data + p, nls);
        p += nls;
        if (!n || n > len - p)
            return dec->codec->decode(dec, data, len);
        type = data[p] & 0x1fu;
        if (type == 7u || type == 8u)
            has_parameter_set = 1;
        p += n;
    }

    if (!has_parameter_set)
        return dec->codec->decode(dec, data, len);

    filtered = (uint8_t *)malloc(len);
    if (!filtered) return MR_ENOMEM;

    p = 0;
    while (p < len) {
        uint32_t start = p;
        uint32_t n = avcc_read_nal_size(data + p, nls);
        unsigned type;
        p += nls;
        type = data[p] & 0x1fu;
        p += n;
        if (type != 7u && type != 8u) {
            uint32_t bytes = nls + n;
            memcpy(filtered + out, data + start, bytes);
            out += bytes;
        }
    }

    /* A parameter-set-only sample is malformed for avc1.  Let libavc consume
     * it rather than returning early here, so its pending input timestamp and
     * normal error/flow-control bookkeeping remain consistent. */
    if (!out) {
        free(filtered);
        return dec->codec->decode(dec, data, len);
    }

    st = dec->codec->decode(dec, filtered, out);
    free(filtered);
    return st;
}
#endif

mr_status mr_decoder_decode(mr_decoder *dec, const uint8_t *data, uint32_t len)
{
    if (!dec || !dec->codec)
        return MR_ERR;
#ifdef MR_HAVE_H264
    if (dec->codec == &mr_codec_h264)
        return decode_avc1_sample(dec, data, len);
#endif
    return dec->codec->decode(dec, data, len);
}

mr_status mr_decoder_flush(mr_decoder *dec)
{
    if (!dec || !dec->codec || !dec->codec->flush)
        return MR_EAGAIN;
    return dec->codec->flush(dec);
}

mr_status mr_decoder_reset(mr_decoder *dec)
{
    const mr_codec *codec;
    int width, height;
    const uint8_t *config;
    uint32_t config_len;
    if (!dec || !dec->codec) return MR_ERR;
    codec = dec->codec; width = dec->width; height = dec->height;
    config = dec->config; config_len = dec->config_len;
    if (codec->close) codec->close(dec);
    return mr_decoder_open_config(dec, codec, width, height, config, config_len);
}

void mr_decoder_close(mr_decoder *dec)
{
    if (dec && dec->codec && dec->codec->close)
        dec->codec->close(dec);
}