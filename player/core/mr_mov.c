/*
 * MintRIVA - minimal QuickTime (MOV) demuxer implementation.
 *
 * MOV is a tree of atoms [size:4][type:4][payload]. We locate the video track's
 * sample tables and flatten them into a per-frame (offset,size) index into the
 * file, so mr_mov_next_packet just walks that index. Only what the player needs
 * is parsed - no edit lists, no fragmented MP4.
 */
#include "mr_mov.h"
#include <stdlib.h>
#include <string.h>

struct mov_sample { uint32_t off; uint32_t size; uint8_t is_video; };

static uint32_t rb32(const uint8_t *p){ return mr_rb32(p); }
static uint16_t rb16(const uint8_t *p){ return mr_rb16(p); }
static uint64_t rb64(const uint8_t *p){
    return ((uint64_t)mr_rb32(p) << 32) | mr_rb32(p + 4);
}
/* atom types compared as big-endian 4CC values */
#define T(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

/* Find the first child atom of `type` within [p,end); returns payload pointer
 * and sets *size to the payload length. */
static const uint8_t *find_atom(const uint8_t *p, const uint8_t *end,
                                uint32_t type, uint32_t *size)
{
    while (p + 8 <= end) {
        uint64_t asz = rb32(p);
        uint32_t t   = rb32(p + 4);
        int hdr = 8;
        if (asz == 1) {                 /* 64-bit extended size */
            if (p + 16 > end) break;
            asz = rb64(p + 8);
            hdr = 16;
        } else if (asz == 0) {          /* extends to end */
            asz = (uint64_t)(end - p);
        }
        if (asz < (uint64_t)hdr) break;
        if (p + asz > end) asz = (uint64_t)(end - p);
        if (t == type) { *size = (uint32_t)(asz - hdr); return p + hdr; }
        p += asz;
    }
    return NULL;
}

static mr_status read_stbl_segments(mr_mov *m, const uint8_t *stbl,
                                    const uint8_t *end, int is_video,
                                    int coalesce_chunks);

/* ISO/IEC 14496 descriptor lengths use up to four base-128 bytes. */
static int descriptor_header(const uint8_t *p, const uint8_t *end,
                             uint8_t *tag, const uint8_t **body,
                             const uint8_t **body_end)
{
    uint32_t n = 0;
    int i;
    if (p >= end) return 0;
    *tag = *p++;
    for (i = 0; i < 4; i++) {
        uint8_t b;
        if (p >= end) return 0;
        b = *p++;
        n = (n << 7) | (uint32_t)(b & 0x7f);
        if (!(b & 0x80)) {
            if ((uint64_t)(end - p) < n) return 0;
            *body = p;
            *body_end = p + n;
            return 1;
        }
    }
    return 0;
}

/* Find DecoderSpecificInfo (tag 0x05) inside an ESDS descriptor tree. */
static int find_decoder_config(const uint8_t *p, const uint8_t *end,
                               const uint8_t **cfg, uint32_t *cfg_len,
                               int depth)
{
    while (p < end && depth < 4) {
        uint8_t tag;
        const uint8_t *body, *body_end, *child;
        if (!descriptor_header(p, end, &tag, &body, &body_end)) return 0;
        if (tag == 0x05) {
            *cfg = body;
            *cfg_len = (uint32_t)(body_end - body);
            return 1;
        }

        child = body;
        if (tag == 0x03) {              /* ES_Descriptor */
            uint8_t flags;
            if (child + 3 > body_end) goto next;
            child += 2; flags = *child++;
            if (flags & 0x80) {                 /* dependsOn_ES_ID */
                if (body_end - child < 2) goto next;
                child += 2;
            }
            if (flags & 0x40) {                 /* URL */
                uint8_t url_len;
                if (child >= body_end) goto next;
                url_len = *child++;
                if (body_end - child < url_len) goto next;
                child += url_len;
            }
            if (flags & 0x20) {                 /* OCR_ES_ID */
                if (body_end - child < 2) goto next;
                child += 2;
            }
        } else if (tag == 0x04) {       /* DecoderConfigDescriptor */
            if (body_end - child < 13) goto next;
            child += 13;
        }
        if (child <= body_end &&
            find_decoder_config(child, body_end, cfg, cfg_len, depth + 1))
            return 1;
next:
        p = body_end;
    }
    return 0;
}

/* sort interleaved segments by ascending file offset */
static int cmp_off(const void *a, const void *b)
{
    uint32_t oa = ((const struct mov_sample *)a)->off;
    uint32_t ob = ((const struct mov_sample *)b)->off;
    return (oa > ob) - (oa < ob);
}

/* mdia/hdlr -> handler type ('vide' / 'soun'). */
static uint32_t track_handler(const uint8_t *mdia, uint32_t mdia_sz)
{
    uint32_t sz;
    const uint8_t *h = find_atom(mdia, mdia + mdia_sz, T('h','d','l','r'), &sz);
    if (!h || sz < 12) return 0;
    return rb32(h + 8);
}

/* Parse the video stbl into m->samples + m->video geometry/fourcc. */
static mr_status parse_video(mr_mov *m, const uint8_t *stbl, uint32_t stbl_sz,
                             const uint8_t *mdia, uint32_t mdia_sz)
{
    const uint8_t *end = stbl + stbl_sz;
    uint32_t sz;
    const uint8_t *stsd = find_atom(stbl, end, T('s','t','s','d'), &sz);
    if (stsd && sz >= 16) {
        const uint8_t *e = stsd + 8;            /* skip ver/flags + count   */
        uint32_t entry_sz = rb32(e);
        /* codec 4CC is stored big-endian at entry+4; pack it the same way the
         * registry does (MR_FOURCC), so 'cvid' matches mr_codec_cinepak. */
        m->video.fourcc = MR_FOURCC(e[4], e[5], e[6], e[7]);
        m->video.width  = rb16(e + 32);
        m->video.height = rb16(e + 34);
        m->video.valid  = 1;
        /* VisualSampleEntry is 86 bytes including size/type.  H.264 stores
         * DecoderConfigurationRecord as a child avcC atom after it. */
        if (m->video.fourcc == MR_FOURCC('a','v','c','1') &&
            entry_sz >= 94 && entry_sz <= sz - 8) {
            uint32_t avcc_sz;
            const uint8_t *avcc = find_atom(e + 86, e + entry_sz,
                                            T('a','v','c','C'), &avcc_sz);
            if (avcc && avcc_sz >= 7) {
                m->video.config = avcc;
                m->video.config_len = avcc_sz;
            }
        }
    }
    /* frame rate: mdhd timescale over first stts delta */
    {
        uint32_t s2;
        const uint8_t *mdhd = find_atom(mdia, mdia + mdia_sz,
                                        T('m','d','h','d'), &s2);
        if (mdhd && s2 >= 20) m->video.rate = rb32(mdhd + 12); /* timescale */
        const uint8_t *stts = find_atom(stbl, end, T('s','t','t','s'), &s2);
        if (stts && s2 >= 16) m->video.scale = rb32(stts + 12); /* 1st delta */
        if (!m->video.scale) m->video.scale = m->video.rate ? m->video.rate : 1;
    }

    return read_stbl_segments(m, stbl, end, 1 /*video*/, 0 /*per-sample*/);
}

/* Append a segment to the growing interleaved index. */
static mr_status push_seg(mr_mov *m, uint32_t off, uint32_t size, int is_video)
{
    if (!size) return MR_OK;
    if (m->nsamples >= m->cap) {
        uint32_t nc = m->cap ? m->cap * 2 : 256;
        struct mov_sample *ns = (struct mov_sample *)
            realloc(m->samples, (size_t)nc * sizeof *ns);
        if (!ns) return MR_ENOMEM;
        m->samples = ns; m->cap = nc;
    }
    m->samples[m->nsamples].off      = off;
    m->samples[m->nsamples].size     = size;
    m->samples[m->nsamples].is_video = (uint8_t)is_video;
    m->nsamples++;
    return MR_OK;
}

/* Flatten a track's stbl into the shared index. Video emits one segment per
 * sample (frame); audio coalesces each chunk into a single PCM segment (far
 * fewer, larger packets). */
static mr_status read_stbl_segments(mr_mov *m, const uint8_t *stbl,
                                    const uint8_t *end, int is_video,
                                    int coalesce_chunks)
{
    uint32_t stsz_sz, stsc_sz, stco_sz;
    const uint8_t *stsz = find_atom(stbl, end, T('s','t','s','z'), &stsz_sz);
    const uint8_t *stsc = find_atom(stbl, end, T('s','t','s','c'), &stsc_sz);
    const uint8_t *stco = find_atom(stbl, end, T('s','t','c','o'), &stco_sz);
    int co64 = 0;
    if (!stco) { stco = find_atom(stbl, end, T('c','o','6','4'), &stco_sz); co64 = 1; }
    if (!stsz || !stsc || !stco || stsz_sz < 12 || stsc_sz < 8 || stco_sz < 8)
        return MR_EFORMAT;

    uint32_t uniform  = rb32(stsz + 4);
    uint32_t nsamp    = rb32(stsz + 8);
    uint32_t stsc_cnt = rb32(stsc + 4);
    uint32_t nchunks  = rb32(stco + 4);
    if (!nsamp || !nchunks) return MR_EFORMAT;

    const uint8_t *sizes = stsz + 12;
    const uint8_t *sc    = stsc + 8;
    const uint8_t *co    = stco + 8;

    uint32_t si = 0, e;
    for (e = 0; e < stsc_cnt && si < nsamp; e++) {
        uint32_t first = rb32(sc + e * 12);
        uint32_t spc   = rb32(sc + e * 12 + 4);
        uint32_t last  = (e + 1 < stsc_cnt) ? rb32(sc + (e + 1) * 12) - 1
                                            : nchunks;
        uint32_t chunk;
        for (chunk = first; chunk <= last && chunk <= nchunks && si < nsamp;
             chunk++) {
            uint64_t off = co64 ? rb64(co + (uint64_t)(chunk - 1) * 8)
                                : rb32(co + (uint64_t)(chunk - 1) * 4);
            uint32_t k;
            if (coalesce_chunks) {
                uint32_t start = (uint32_t)off, total = 0;
                for (k = 0; k < spc && si < nsamp; k++) {
                    total += uniform ? uniform : rb32(sizes + (uint64_t)si * 4);
                    si++;
                }
                if (push_seg(m, start, total, is_video) != MR_OK)
                    return MR_ENOMEM;
            } else {
                for (k = 0; k < spc && si < nsamp; k++) {
                    uint32_t ssz = uniform ? uniform
                                           : rb32(sizes + (uint64_t)si * 4);
                    if (push_seg(m, (uint32_t)off, ssz, is_video) != MR_OK)
                        return MR_ENOMEM;
                    off += ssz;
                    si++;
                }
            }
        }
    }
    return MR_OK;
}

static void parse_audio(mr_mov *m, const uint8_t *stbl, uint32_t stbl_sz)
{
    uint32_t sz;
    const uint8_t *stsd = find_atom(stbl, stbl + stbl_sz,
                                    T('s','t','s','d'), &sz);
    if (!stsd || sz < 44) return;
    const uint8_t *e = stsd + 8;                /* audio sample entry       */
    uint32_t entry_sz = rb32(e);
    uint32_t fmt = rb32(e + 4);
    uint16_t version = rb16(e + 16);
    if (entry_sz < 36 || entry_sz > sz - 8) return;
    m->audio.channels    = rb16(e + 24);
    m->audio.bits        = rb16(e + 26);
    m->audio.sample_rate = rb32(e + 32) >> 16;  /* 16.16 fixed              */
    /* map common uncompressed PCM 4CCs to the WAVE PCM tag */
    if (fmt == T('s','o','w','t') || fmt == T('t','w','o','s') ||
        fmt == T('r','a','w',' ') || fmt == T('l','p','c','m') ||
        fmt == T('i','n','2','4') || fmt == T('i','n','3','2'))
        m->audio.format_tag = MR_AUDIO_FORMAT_PCM;
    else if (fmt == T('.','m','p','3'))
        m->audio.format_tag = MR_AUDIO_FORMAT_MP3;
    else if (fmt == T('m','p','4','a')) {
        const uint8_t *entry_end;
        const uint8_t *child;
        const uint8_t *esds, *cfg;
        uint32_t esds_sz, cfg_len;
        m->audio.format_tag = MR_AUDIO_FORMAT_AAC;

        entry_end = e + entry_sz;
        child = e + 36;
        if (version == 1) child += 16;
        else if (version == 2) child += 36;
        esds = child <= entry_end
             ? find_atom(child, entry_end, T('e','s','d','s'), &esds_sz)
             : NULL;
        if (!esds && child <= entry_end) {
            uint32_t wave_sz;
            const uint8_t *wave = find_atom(child, entry_end,
                                            T('w','a','v','e'), &wave_sz);
            if (wave)
                esds = find_atom(wave, wave + wave_sz,
                                 T('e','s','d','s'), &esds_sz);
        }
        /* esds starts with version/flags, followed by MPEG-4 descriptors. */
        if (esds && esds_sz >= 4 &&
            find_decoder_config(esds + 4, esds + esds_sz,
                                &cfg, &cfg_len, 0)) {
            if (cfg_len > MR_AUDIO_CONFIG_MAX) cfg_len = MR_AUDIO_CONFIG_MAX;
            memcpy(m->audio.config, cfg, cfg_len);
            m->audio.config_len = (uint8_t)cfg_len;
        }
    }
    m->audio.valid = 1;

    /* Compressed access units must keep their sample boundaries.  PCM remains
     * coalesced per chunk to avoid flooding the player with tiny packets. */
    read_stbl_segments(m, stbl, stbl + stbl_sz, 0 /*audio*/,
                       m->audio.format_tag == MR_AUDIO_FORMAT_PCM);
}

static void parse_trak(mr_mov *m, const uint8_t *trak, uint32_t trak_sz)
{
    uint32_t sz;
    const uint8_t *mdia = find_atom(trak, trak + trak_sz,
                                    T('m','d','i','a'), &sz);
    if (!mdia) return;
    uint32_t mdia_sz = sz, minf_sz, stbl_sz;
    uint32_t htype = track_handler(mdia, mdia_sz);
    const uint8_t *minf = find_atom(mdia, mdia + mdia_sz,
                                    T('m','i','n','f'), &minf_sz);
    if (!minf) return;
    const uint8_t *stbl = find_atom(minf, minf + minf_sz,
                                    T('s','t','b','l'), &stbl_sz);
    if (!stbl) return;

    if (htype == T('v','i','d','e') && !m->video.valid)
        parse_video(m, stbl, stbl_sz, mdia, mdia_sz);
    else if (htype == T('s','o','u','n') && !m->audio.valid)
        parse_audio(m, stbl, stbl_sz);
}

mr_status mr_mov_open(mr_mov *m, const uint8_t *buf, size_t len)
{
    memset(m, 0, sizeof *m);
    m->buf = buf;
    m->len = len;

    uint32_t sz;
    const uint8_t *moov = find_atom(buf, buf + len, T('m','o','o','v'), &sz);
    if (!moov) return MR_EFORMAT;

    /* iterate every trak in moov */
    const uint8_t *p = moov, *end = moov + sz;
    for (;;) {
        uint32_t tsz;
        const uint8_t *trak = find_atom(p, end, T('t','r','a','k'), &tsz);
        if (!trak) break;
        parse_trak(m, trak, tsz);
        p = trak + tsz;                         /* advance past this trak   */
    }

    if (!m->video.valid || !m->samples) return MR_EFORMAT;

    /* Interleave audio and video by file offset so packets arrive in the order
     * they sit in mdat - the natural order for streaming and A/V sync. */
    qsort(m->samples, m->nsamples, sizeof *m->samples, cmp_off);
    return MR_OK;
}

mr_status mr_mov_next_packet(mr_mov *m, mr_packet *pkt)
{
    while (m->cursor < m->nsamples) {
        struct mov_sample *s = &m->samples[m->cursor++];
        if ((size_t)s->off + s->size > m->len) continue;   /* guard */
        pkt->is_video = s->is_video;
        pkt->data     = m->buf + s->off;
        pkt->len      = s->size;
        return MR_OK;
    }
    return MR_EAGAIN;
}

void mr_mov_rewind(mr_mov *m) { m->cursor = 0; }

void mr_mov_close(mr_mov *m)
{
    if (m && m->samples) { free(m->samples); m->samples = NULL; }
}
