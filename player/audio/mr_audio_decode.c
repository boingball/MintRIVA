/*
 * MintRIVA - MintAMP/Helix MP3 and AAC packet adapter.
 *
 * No codec implementation lives here.  The build supplies MintAMP's public
 * mp3dec/aacdec APIs; this file handles AVI packet joins, MP4 AAC raw-block
 * setup and Paula-friendly 2:1 output decimation above its ~28 kHz ceiling.
 */
#include "mr_audio_decode.h"

#include "mp3dec.h"
#include "aacdec.h"

#include <stdlib.h>
#include <string.h>

#define PCM_SHORTS_MAX 4096
#define PAULA_RATE_MAX 28000U

enum audio_kind {
    AUDIO_KIND_MP3,
    AUDIO_KIND_AAC_RAW,
    AUDIO_KIND_AAC_ADTS
};

struct mr_audio_decoder {
    enum audio_kind kind;
    HMP3Decoder mp3;
    HAACDecoder aac;
    unsigned source_rate;
    unsigned output_rate;
    unsigned channels;
    unsigned stride;
    unsigned char *pending;
    size_t pending_len;
    size_t pending_cap;
    short pcm[PCM_SHORTS_MAX];
};

static int reserve_pending(mr_audio_decoder *d, size_t add)
{
    size_t need = d->pending_len + add;
    unsigned char *p;
    size_t cap;
    if (need <= d->pending_cap) return 1;
    cap = d->pending_cap ? d->pending_cap : 4096;
    while (cap < need) {
        if (cap > 1024U * 1024U) return 0;
        cap *= 2;
    }
    p = (unsigned char *)realloc(d->pending, cap);
    if (!p) return 0;
    d->pending = p;
    d->pending_cap = cap;
    return 1;
}

static void consume_pending(mr_audio_decoder *d, size_t n)
{
    if (n >= d->pending_len) {
        d->pending_len = 0;
        return;
    }
    memmove(d->pending, d->pending + n, d->pending_len - n);
    d->pending_len -= n;
}

/* Return a complete Layer III frame length, 0 for an invalid header. */
static unsigned mp3_frame_bytes(const unsigned char *p)
{
    static const unsigned br_mpeg1_l3[16] = {
        0, 32, 40, 48, 56, 64, 80, 96, 112,
        128, 160, 192, 224, 256, 320, 0
    };
    static const unsigned br_mpeg2_l3[16] = {
        0, 8, 16, 24, 32, 40, 48, 56, 64,
        80, 96, 112, 128, 144, 160, 0
    };
    static const unsigned sr_base[3] = { 44100, 48000, 32000 };
    unsigned version, layer, bri, sri, rate, br, pad;

    if (p[0] != 0xff || (p[1] & 0xe0) != 0xe0) return 0;
    version = (p[1] >> 3) & 3;
    layer = (p[1] >> 1) & 3;
    bri = p[2] >> 4;
    sri = (p[2] >> 2) & 3;
    pad = (p[2] >> 1) & 1;
    if (version == 1 || layer != 1 || bri == 0 || bri == 15 || sri == 3)
        return 0;                       /* reserved / not Layer III */

    rate = sr_base[sri];
    if (version == 2) rate /= 2;        /* MPEG-2 */
    else if (version == 0) rate /= 4;   /* MPEG-2.5 */
    br = (version == 3 ? br_mpeg1_l3[bri] : br_mpeg2_l3[bri]) * 1000U;
    return ((version == 3 ? 144U : 72U) * br) / rate + pad;
}

static unsigned aac_adts_frame_bytes(const unsigned char *p)
{
    if (p[0] != 0xff || (p[1] & 0xf6) != 0xf0) return 0;
    return ((unsigned)(p[3] & 3) << 11) |
           ((unsigned)p[4] << 3) | ((unsigned)p[5] >> 5);
}

static long emit_pcm(mr_audio_decoder *d, unsigned total_shorts,
                     unsigned rate, unsigned channels,
                     mr_audio_pcm_sink sink, void *user)
{
    unsigned frames, out, i;
    if (!channels || channels > 2 || total_shorts > PCM_SHORTS_MAX)
        return -1;
    frames = total_shorts / channels;
    d->channels = channels;
    if (rate) d->source_rate = rate;

    if (d->stride == 1) {
        if (sink && frames) sink(user, d->pcm, frames, channels);
        return (long)frames;
    }

    /* Compact in-place, preserving interleaving. Frame counts are even for
     * normal MP3/AAC blocks, so phase cannot drift between packets. */
    out = 0;
    for (i = 0; i < frames; i += d->stride) {
        unsigned ch;
        for (ch = 0; ch < channels; ch++)
            d->pcm[out * channels + ch] = d->pcm[i * channels + ch];
        out++;
    }
    if (sink && out) sink(user, d->pcm, out, channels);
    return (long)out;
}

static int parse_aac_asc(const mr_audio_info *info,
                         unsigned *object_type, unsigned *sample_rate,
                         unsigned *channels)
{
    static const unsigned rates[13] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350
    };
    uint32_t bits;
    unsigned sf_index;
    if (info->config_len < 2) return 0;
    bits = ((uint32_t)info->config[0] << 16) |
           ((uint32_t)info->config[1] << 8) |
           (info->config_len >= 3 ? info->config[2] : 0);
    *object_type = (bits >> 19) & 0x1f;
    sf_index = (bits >> 15) & 0x0f;
    *channels = (bits >> 11) & 0x0f;
    if (*object_type == 31 || sf_index == 15) {
        /* Extended object types / explicit-frequency ASC are valid MPEG-4,
         * but this fixed-point LC path intentionally rejects them cleanly. */
        return 0;
    }
    if (sf_index >= 13 || *channels < 1 || *channels > 2) return 0;
    *sample_rate = rates[sf_index];
    return 1;
}

mr_audio_decoder *mr_audio_decoder_open(const mr_audio_info *info)
{
    mr_audio_decoder *d;
    if (!info || !info->valid) return NULL;
    if (info->format_tag != MR_AUDIO_FORMAT_MP3 &&
        info->format_tag != MR_AUDIO_FORMAT_AAC)
        return NULL;

    d = (mr_audio_decoder *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->source_rate = info->sample_rate;
    d->channels = info->channels;
    d->stride = info->sample_rate > PAULA_RATE_MAX ? 2 : 1;
    d->output_rate = info->sample_rate / d->stride;

    if (info->format_tag == MR_AUDIO_FORMAT_MP3) {
        d->kind = AUDIO_KIND_MP3;
        d->mp3 = MP3InitDecoder();
        if (!d->mp3) goto fail;
    } else {
        d->aac = AACInitDecoder();
        if (!d->aac) goto fail;
        if (info->config_len) {
            AACFrameInfo fi;
            unsigned object_type, rate, channels;
            memset(&fi, 0, sizeof fi);
            if (!parse_aac_asc(info, &object_type, &rate, &channels) ||
                object_type != 2)             /* AAC-LC */
                goto fail;
            fi.nChans = (int)channels;
            fi.sampRateCore = (int)rate;
            fi.profile = (int)object_type - 1;
            if (AACSetRawBlockParams(d->aac, 0, &fi) != 0) goto fail;
            d->kind = AUDIO_KIND_AAC_RAW;
            d->source_rate = rate;
            d->channels = channels;
            d->stride = rate > PAULA_RATE_MAX ? 2 : 1;
            d->output_rate = rate / d->stride;
        } else {
            d->kind = AUDIO_KIND_AAC_ADTS;
        }
    }
    return d;

fail:
    mr_audio_decoder_close(d);
    return NULL;
}

static long feed_mp3(mr_audio_decoder *d, const uint8_t *data, uint32_t len,
                     mr_audio_pcm_sink sink, void *user)
{
    long produced = 0;
    if (!reserve_pending(d, len)) return -1;
    memcpy(d->pending + d->pending_len, data, len);
    d->pending_len += len;

    while (d->pending_len >= 4) {
        int off = MP3FindSyncWord(d->pending, (int)d->pending_len);
        unsigned frame_len;
        unsigned char *in;
        int left, err;
        MP3FrameInfo fi;
        long got;
        if (off < 0) {
            if (d->pending_len > 3)
                consume_pending(d, d->pending_len - 3);
            break;
        }
        if (off) consume_pending(d, (size_t)off);
        if (d->pending_len < 4) break;
        frame_len = mp3_frame_bytes(d->pending);
        if (!frame_len) { consume_pending(d, 1); continue; }
        if (d->pending_len < frame_len) break;

        in = d->pending;
        left = (int)frame_len;
        err = MP3Decode(d->mp3, &in, &left, d->pcm, 0);
        consume_pending(d, frame_len);
        if (err == ERR_MP3_MAINDATA_UNDERFLOW) continue;
        if (err != ERR_MP3_NONE) continue;      /* resync at next frame */
        MP3GetLastFrameInfo(d->mp3, &fi);
        got = emit_pcm(d, (unsigned)fi.outputSamps, (unsigned)fi.samprate,
                       (unsigned)fi.nChans, sink, user);
        if (got < 0) return -1;
        produced += got;
    }
    return produced;
}

static long feed_aac_raw(mr_audio_decoder *d, const uint8_t *data, uint32_t len,
                         mr_audio_pcm_sink sink, void *user)
{
    unsigned char *in = (unsigned char *)(uintptr_t)data;
    int left = (int)len;
    AACFrameInfo fi;
    int err = AACDecode(d->aac, &in, &left, d->pcm);
    if (err != ERR_AAC_NONE) return 0;          /* bad AU: skip, keep playing */
    AACGetLastFrameInfo(d->aac, &fi);
    return emit_pcm(d, (unsigned)fi.outputSamps,
                    (unsigned)fi.sampRateOut, (unsigned)fi.nChans,
                    sink, user);
}

static long feed_aac_adts(mr_audio_decoder *d, const uint8_t *data, uint32_t len,
                          mr_audio_pcm_sink sink, void *user)
{
    long produced = 0;
    if (!reserve_pending(d, len)) return -1;
    memcpy(d->pending + d->pending_len, data, len);
    d->pending_len += len;

    while (d->pending_len >= 7) {
        int off = AACFindSyncWord(d->pending, (int)d->pending_len);
        unsigned frame_len;
        unsigned char *in;
        int left, err;
        AACFrameInfo fi;
        long got;
        if (off < 0) {
            if (d->pending_len > 6)
                consume_pending(d, d->pending_len - 6);
            break;
        }
        if (off) consume_pending(d, (size_t)off);
        if (d->pending_len < 7) break;
        frame_len = aac_adts_frame_bytes(d->pending);
        if (frame_len < 7) { consume_pending(d, 1); continue; }
        if (d->pending_len < frame_len) break;
        in = d->pending;
        left = (int)frame_len;
        err = AACDecode(d->aac, &in, &left, d->pcm);
        consume_pending(d, frame_len);
        if (err != ERR_AAC_NONE) continue;
        AACGetLastFrameInfo(d->aac, &fi);
        got = emit_pcm(d, (unsigned)fi.outputSamps,
                       (unsigned)fi.sampRateOut, (unsigned)fi.nChans,
                       sink, user);
        if (got < 0) return -1;
        produced += got;
    }
    return produced;
}

long mr_audio_decoder_feed(mr_audio_decoder *d,
                           const uint8_t *data, uint32_t len,
                           mr_audio_pcm_sink sink, void *sink_user)
{
    if (!d || (!data && len)) return -1;
    if (!len) return 0;
    if (d->kind == AUDIO_KIND_MP3)
        return feed_mp3(d, data, len, sink, sink_user);
    if (d->kind == AUDIO_KIND_AAC_RAW)
        return feed_aac_raw(d, data, len, sink, sink_user);
    return feed_aac_adts(d, data, len, sink, sink_user);
}

int mr_audio_decoder_reset(mr_audio_decoder *d)
{
    if (!d) return 0;
    d->pending_len = 0;
    if (d->kind == AUDIO_KIND_MP3) {
        MP3FreeDecoder(d->mp3);
        d->mp3 = MP3InitDecoder();
        return d->mp3 != NULL;
    }
    return AACFlushCodec(d->aac) == 0;
}

unsigned mr_audio_decoder_rate(const mr_audio_decoder *d)
{
    return d ? d->output_rate : 0;
}

unsigned mr_audio_decoder_channels(const mr_audio_decoder *d)
{
    return d ? d->channels : 0;
}

const char *mr_audio_decoder_name(const mr_audio_decoder *d)
{
    if (!d) return "none";
    return d->kind == AUDIO_KIND_MP3 ? "MP3"
         : d->kind == AUDIO_KIND_AAC_RAW ? "AAC-LC/mp4a"
         : "AAC-LC/ADTS";
}

void mr_audio_decoder_close(mr_audio_decoder *d)
{
    if (!d) return;
    if (d->mp3) MP3FreeDecoder(d->mp3);
    if (d->aac) AACFreeDecoder(d->aac);
    free(d->pending);
    free(d);
}
