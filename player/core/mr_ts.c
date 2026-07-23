/*
 * MintRIVA - MPEG-TS/M2TS demuxer.
 */
#include "mr_ts.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TS_SYNC             0x47
#define TS_PID_NONE         0x1fff
#define TS_PROBE_LIMIT      (8UL * 1024 * 1024)
#define TS_PROBE_VIDEO_MAX  (1024UL * 1024)
#define TS_PES_MAX          (16UL * 1024 * 1024)

typedef struct {
    const uint8_t *p;
    size_t         bits;
    size_t         pos;
    int            bad;
} ts_bits;

static int reserve(uint8_t **buf, size_t *cap, size_t need, size_t limit)
{
    uint8_t *p;
    size_t n;
    if (need <= *cap) return 1;
    n = *cap ? *cap : 4096;
    while (n < need) {
        if (n >= limit) return 0;
        n *= 2;
        if (n > limit) n = limit;
    }
    p = (uint8_t *)realloc(*buf, n);
    if (!p) return 0;
    *buf = p;
    *cap = n;
    return 1;
}

static int ts_read_at(mr_ts *t, size_t off, void *dst, size_t len)
{
    if (!t->file_backed) {
        if (off > t->len || len > t->len - off) return 0;
        memcpy(dst, t->buf + off, len);
        return 1;
    } else {
        FILE *f = (FILE *)t->stream;
        if (!t->stream_pos_valid || t->stream_pos != off) {
            if (off > 0x7fffffffUL || fseek(f, (long)off, SEEK_SET) != 0) {
                t->stream_pos_valid = 0;
                return 0;
            }
        }
        if (fread(dst, 1, len, f) != len) {
            t->stream_pos_valid = 0;
            return 0;
        }
        t->stream_pos = off + len;
        t->stream_pos_valid = 1;
        return 1;
    }
}

static int detect_layout(const uint8_t *b, size_t len,
                         int *packet_size, int *sync_off)
{
    if (len >= 377 && b[0] == TS_SYNC &&
        b[188] == TS_SYNC && b[376] == TS_SYNC) {
        *packet_size = 188;
        *sync_off = 0;
        return 1;
    }
    if (len >= 389 && b[4] == TS_SYNC &&
        b[196] == TS_SYNC && b[388] == TS_SYNC) {
        *packet_size = 192;
        *sync_off = 4;
        return 1;
    }
    return 0;
}

/* Return payload and length for one 188-byte packet, or NULL if absent/bad. */
static const uint8_t *payload(const uint8_t *p, size_t *len, int *pusi,
                              uint16_t *pid)
{
    int afc, off = 4;
    if (p[0] != TS_SYNC || (p[1] & 0x80)) return NULL;
    *pusi = (p[1] & 0x40) != 0;
    *pid = (uint16_t)(((p[1] & 0x1f) << 8) | p[2]);
    afc = (p[3] >> 4) & 3;
    if (afc == 0 || afc == 2) return NULL;
    if (afc == 3) {
        off += 1 + p[4];
        if (off > 188) return NULL;
    }
    *len = (size_t)(188 - off);
    return p + off;
}

static void parse_pat(mr_ts *t, const uint8_t *p, size_t len, int pusi)
{
    size_t section_len, end, pos, skip;
    if (!pusi || !len) return;
    skip = 1 + (size_t)p[0];
    if (skip > len) return;
    p += skip;
    len -= skip;
    if (len < 12 || p[0] != 0x00) return;
    section_len = (size_t)(((p[1] & 0x0f) << 8) | p[2]);
    if (section_len + 3 > len || section_len < 9) return;
    end = 3 + section_len - 4;                   /* exclude CRC              */
    for (pos = 8; pos + 4 <= end; pos += 4) {
        uint16_t program = mr_rb16(p + pos);
        uint16_t pid = (uint16_t)(((p[pos + 2] & 0x1f) << 8) | p[pos + 3]);
        if (program) {
            t->pmt_pid = pid;
            return;
        }
    }
}

static void parse_pmt(mr_ts *t, const uint8_t *p, size_t len, int pusi)
{
    size_t section_len, end, pos, program_info_len, skip;
    if (!pusi || !len) return;
    skip = 1 + (size_t)p[0];
    if (skip > len) return;
    p += skip;
    len -= skip;
    if (len < 16 || p[0] != 0x02) return;
    section_len = (size_t)(((p[1] & 0x0f) << 8) | p[2]);
    if (section_len + 3 > len || section_len < 13) return;
    end = 3 + section_len - 4;
    program_info_len = (size_t)(((p[10] & 0x0f) << 8) | p[11]);
    pos = 12 + program_info_len;
    while (pos + 5 <= end) {
        uint8_t type = p[pos];
        uint16_t pid =
            (uint16_t)(((p[pos + 1] & 0x1f) << 8) | p[pos + 2]);
        size_t es_info_len =
            (size_t)(((p[pos + 3] & 0x0f) << 8) | p[pos + 4]);
        if (pos + 5 + es_info_len > end) break;
        if (type == 0x1b && t->video_pid == TS_PID_NONE) {
            t->video_pid = pid;                  /* AVC/H.264                */
            t->video_type = type;
        } else if ((type == 0x0f || type == 0x06) &&
                   t->audio_pid == TS_PID_NONE) {
            t->audio_pid = pid;                  /* AAC with ADTS            */
            t->audio_type = type;
        }
        pos += 5 + es_info_len;
    }
}

/* Strip the PES header from the first TS payload of a PES packet. */
static const uint8_t *pes_payload(const uint8_t *p, size_t *len,
                                  uint64_t *pts, int *has_pts,
                                  size_t *expected)
{
    size_t hdr, packet_len;
    *has_pts = 0;
    *expected = 0;
    if (*len < 9 || p[0] != 0 || p[1] != 0 || p[2] != 1) return NULL;
    packet_len = mr_rb16(p + 4);
    hdr = 9 + p[8];
    if (hdr > *len) return NULL;
    if (packet_len) {
        size_t pes_header_after_length = 3 + (size_t)p[8];
        if (packet_len < pes_header_after_length) return NULL;
        *expected = packet_len - pes_header_after_length;
    }
    if ((p[7] & 0x80) && p[8] >= 5) {
        const uint8_t *q = p + 9;
        *pts = ((uint64_t)(q[0] & 0x0e) << 29) |
               ((uint64_t)q[1] << 22) |
               ((uint64_t)(q[2] & 0xfe) << 14) |
               ((uint64_t)q[3] << 7) |
               ((uint64_t)(q[4] & 0xfe) >> 1);
        *has_pts = 1;
    }
    *len -= hdr;
    return p + hdr;
}

static unsigned bits_get(ts_bits *b, unsigned n)
{
    unsigned v = 0, i;
    if (n > 32 || b->pos + n > b->bits) {
        b->bad = 1;
        return 0;
    }
    for (i = 0; i < n; i++) {
        v = (v << 1) | ((b->p[b->pos >> 3] >> (7 - (b->pos & 7))) & 1);
        b->pos++;
    }
    return v;
}

static unsigned bits_ue(ts_bits *b)
{
    unsigned zeros = 0;
    while (!b->bad && b->pos < b->bits && bits_get(b, 1) == 0) {
        if (++zeros > 30) {
            b->bad = 1;
            return 0;
        }
    }
    return zeros ? ((1u << zeros) - 1u + bits_get(b, zeros)) : 0;
}

static int bits_se(ts_bits *b)
{
    unsigned v = bits_ue(b);
    return (v & 1) ? (int)((v + 1) >> 1) : -(int)(v >> 1);
}

static void skip_scaling_list(ts_bits *b, int count)
{
    int last = 8, next = 8, j;
    for (j = 0; j < count; j++) {
        if (next) next = (last + bits_se(b) + 256) & 255;
        last = next ? next : last;
    }
}

static uint8_t *nal_rbsp(const uint8_t *nal, size_t len, size_t *out_len)
{
    uint8_t *r;
    size_t i, n = 0;
    int zeros = 0;
    if (len < 2) return NULL;
    r = (uint8_t *)malloc(len - 1);
    if (!r) return NULL;
    for (i = 1; i < len; i++) {                 /* skip NAL header          */
        uint8_t v = nal[i];
        if (zeros >= 2 && v == 3) {
            zeros = 0;
            continue;
        }
        r[n++] = v;
        zeros = v == 0 ? zeros + 1 : 0;
    }
    *out_len = n;
    return r;
}

static int parse_sps_geometry(const uint8_t *nal, size_t len,
                              int *width, int *height)
{
    uint8_t *rbsp;
    size_t rbsp_len;
    ts_bits b;
    unsigned profile, chroma = 1, frame_only;
    unsigned width_mbs, height_map, crop = 0;
    unsigned crop_l = 0, crop_r = 0, crop_t = 0, crop_b = 0;

    rbsp = nal_rbsp(nal, len, &rbsp_len);
    if (!rbsp) return 0;
    b.p = rbsp; b.bits = rbsp_len * 8; b.pos = 0; b.bad = 0;
    profile = bits_get(&b, 8);
    bits_get(&b, 8);                             /* constraints              */
    bits_get(&b, 8);                             /* level                    */
    bits_ue(&b);                                 /* sps id                   */
    if (profile == 100 || profile == 110 || profile == 122 ||
        profile == 244 || profile == 44 || profile == 83 ||
        profile == 86 || profile == 118 || profile == 128 ||
        profile == 138 || profile == 139 || profile == 134) {
        unsigned i, scaling;
        chroma = bits_ue(&b);
        if (chroma == 3) bits_get(&b, 1);
        bits_ue(&b);
        bits_ue(&b);
        bits_get(&b, 1);
        scaling = bits_get(&b, 1);
        if (scaling) {
            unsigned count = chroma == 3 ? 12 : 8;
            for (i = 0; i < count; i++)
                if (bits_get(&b, 1)) skip_scaling_list(&b, i < 6 ? 16 : 64);
        }
    }
    bits_ue(&b);                                 /* log2_max_frame_num       */
    {
        unsigned poc = bits_ue(&b);
        if (poc == 0) bits_ue(&b);
        else if (poc == 1) {
            unsigned i, n;
            bits_get(&b, 1);
            bits_se(&b);
            bits_se(&b);
            n = bits_ue(&b);
            for (i = 0; i < n; i++) bits_se(&b);
        }
    }
    bits_ue(&b);                                 /* max refs                 */
    bits_get(&b, 1);
    width_mbs = bits_ue(&b) + 1;
    height_map = bits_ue(&b) + 1;
    frame_only = bits_get(&b, 1);
    if (!frame_only) bits_get(&b, 1);
    bits_get(&b, 1);
    crop = bits_get(&b, 1);
    if (crop) {
        crop_l = bits_ue(&b); crop_r = bits_ue(&b);
        crop_t = bits_ue(&b); crop_b = bits_ue(&b);
    }
    if (!b.bad && width_mbs && height_map) {
        unsigned sub_w = (chroma == 1 || chroma == 2) ? 2 : 1;
        unsigned sub_h = chroma == 1 ? 2 : 1;
        unsigned unit_x = chroma ? sub_w : 1;
        unsigned unit_y = chroma ? sub_h * (2 - frame_only)
                                 : (2 - frame_only);
        unsigned w = width_mbs * 16;
        unsigned h = (2 - frame_only) * height_map * 16;
        unsigned cx = (crop_l + crop_r) * unit_x;
        unsigned cy = (crop_t + crop_b) * unit_y;
        if (cx < w && cy < h) {
            *width = (int)(w - cx);
            *height = (int)(h - cy);
            free(rbsp);
            return 1;
        }
    }
    free(rbsp);
    return 0;
}

/* Find the next Annex-B start code. Returns len when none remains. */
static size_t start_code(const uint8_t *p, size_t len, size_t from,
                         size_t *prefix)
{
    size_t i;
    for (i = from; i + 3 <= len; i++) {
        if (p[i] == 0 && p[i + 1] == 0) {
            if (p[i + 2] == 1) {
                *prefix = 3;
                return i;
            }
            if (i + 4 <= len && p[i + 2] == 0 && p[i + 3] == 1) {
                *prefix = 4;
                return i;
            }
        }
    }
    return len;
}

static int make_avcc_config(mr_ts *t, const uint8_t *p, size_t len)
{
    const uint8_t *sps = NULL, *pps = NULL;
    size_t sps_len = 0, pps_len = 0, pos = 0, prefix;
    while ((pos = start_code(p, len, pos, &prefix)) < len) {
        size_t begin = pos + prefix, next_prefix, end;
        size_t next = start_code(p, len, begin, &next_prefix);
        end = next;
        while (end > begin && p[end - 1] == 0) end--;
        if (end > begin) {
            unsigned type = p[begin] & 0x1f;
            if (type == 7 && !sps) { sps = p + begin; sps_len = end - begin; }
            if (type == 8 && !pps) { pps = p + begin; pps_len = end - begin; }
        }
        if (sps && pps) break;
        pos = next;
    }
    if (!sps || !pps || sps_len > 65535 || pps_len > 65535 ||
        sps_len < 4 || !parse_sps_geometry(sps, sps_len,
                                           &t->video.width,
                                           &t->video.height))
        return 0;
    t->config = (uint8_t *)malloc(11 + sps_len + pps_len);
    if (!t->config) return 0;
    t->config[0] = 1;
    t->config[1] = sps[1];
    t->config[2] = sps[2];
    t->config[3] = sps[3];
    t->config[4] = 0xff;                         /* four-byte NAL lengths    */
    t->config[5] = 0xe1;
    t->config[6] = (uint8_t)(sps_len >> 8);
    t->config[7] = (uint8_t)sps_len;
    memcpy(t->config + 8, sps, sps_len);
    t->config[8 + sps_len] = 1;
    t->config[9 + sps_len] = (uint8_t)(pps_len >> 8);
    t->config[10 + sps_len] = (uint8_t)pps_len;
    memcpy(t->config + 11 + sps_len, pps, pps_len);
    t->video.config = t->config;
    t->video.config_len = (uint32_t)(11 + sps_len + pps_len);
    return 1;
}

static void parse_adts_info(mr_ts *t, const uint8_t *p, size_t len)
{
    static const uint32_t rates[13] = {
        96000, 88200, 64000, 48000, 44100, 32000, 24000,
        22050, 16000, 12000, 11025, 8000, 7350
    };
    size_t i;
    if (t->audio.valid) return;
    for (i = 0; i + 7 <= len; i++) {
        if (p[i] == 0xff && (p[i + 1] & 0xf6) == 0xf0) {
            unsigned sri = (p[i + 2] >> 2) & 0x0f;
            unsigned ch = ((p[i + 2] & 1) << 2) | (p[i + 3] >> 6);
            if (sri < 13 && ch >= 1 && ch <= 2) {
                t->audio.format_tag = MR_AUDIO_FORMAT_AAC;
                t->audio.sample_rate = rates[sri];
                t->audio.channels = (uint16_t)ch;
                t->audio.bits = 16;
                t->audio.valid = 1;
                return;
            }
        }
    }
}

static uint32_t gcd32(uint32_t a, uint32_t b)
{
    while (b) {
        uint32_t r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static mr_status probe_stream(mr_ts *t)
{
    uint8_t packet[192];
    uint8_t *video_probe = NULL;
    size_t video_len = 0, video_cap = 0;
    size_t pos, limit = t->len < TS_PROBE_LIMIT ? t->len : TS_PROBE_LIMIT;
    uint64_t last_pts = 0;
    int have_last_pts = 0;
    uint32_t pts_step = 0;

    for (pos = 0; pos + (size_t)t->packet_size <= limit;
         pos += (size_t)t->packet_size) {
        const uint8_t *p, *es;
        size_t n, es_len;
        int pusi, has_pts = 0;
        uint16_t pid;
        uint64_t pts = 0;
        size_t expected;
        if (!ts_read_at(t, pos, packet, (size_t)t->packet_size)) break;
        p = payload(packet + t->sync_off, &n, &pusi, &pid);
        if (!p) continue;
        if (pid == 0) parse_pat(t, p, n, pusi);
        else if (pid == t->pmt_pid) parse_pmt(t, p, n, pusi);
        else if (pid == t->video_pid) {
            es = p; es_len = n;
            if (pusi) {
                es = pes_payload(p, &es_len, &pts, &has_pts, &expected);
                if (!es) continue;
                if (has_pts) {
                    if (have_last_pts) {
                        uint32_t d = pts > last_pts
                                   ? (uint32_t)(pts - last_pts)
                                   : (uint32_t)(last_pts - pts);
                        if (d) pts_step = pts_step ? gcd32(pts_step, d) : d;
                    }
                    last_pts = pts;
                    have_last_pts = 1;
                }
            }
            if (video_len < TS_PROBE_VIDEO_MAX) {
                size_t add = es_len;
                if (add > TS_PROBE_VIDEO_MAX - video_len)
                    add = TS_PROBE_VIDEO_MAX - video_len;
                if (!reserve(&video_probe, &video_cap, video_len + add,
                             TS_PROBE_VIDEO_MAX)) {
                    free(video_probe);
                    return MR_ENOMEM;
                }
                memcpy(video_probe + video_len, es, add);
                video_len += add;
            }
        } else if (pid == t->audio_pid) {
            es = p; es_len = n;
            if (pusi) {
                es = pes_payload(p, &es_len, &pts, &has_pts, &expected);
                if (!es) continue;
            }
            parse_adts_info(t, es, es_len);
        }
        if (t->video_pid != TS_PID_NONE && video_len &&
            t->audio_pid != TS_PID_NONE && t->audio.valid &&
            make_avcc_config(t, video_probe, video_len))
            break;
    }
    if (!t->config && video_len)
        make_avcc_config(t, video_probe, video_len);
    free(video_probe);

    if (t->video_pid == TS_PID_NONE || t->video_type != 0x1b ||
        !t->config || t->video.width <= 0 || t->video.height <= 0)
        return MR_EUNSUPPORTED;
    t->video.fourcc = MR_FOURCC('a','v','c','1');
    t->video.rate = 90000;
    t->video.scale = pts_step >= 300 ? pts_step : 3600; /* default 25 fps */
    t->video.valid = 1;
    return MR_OK;
}

static void ts_init(mr_ts *t)
{
    memset(t, 0, sizeof *t);
    t->pmt_pid = TS_PID_NONE;
    t->video_pid = TS_PID_NONE;
    t->audio_pid = TS_PID_NONE;
}

static mr_status ts_open_common(mr_ts *t)
{
    uint8_t head[512];
    mr_status st;
    size_t n = t->len < sizeof head ? t->len : sizeof head;
    if (n < 389 || !ts_read_at(t, 0, head, n) ||
        !detect_layout(head, n, &t->packet_size, &t->sync_off))
        return MR_EFORMAT;
    st = probe_stream(t);
    if (st != MR_OK) return st;
    mr_ts_rewind(t);
    return MR_OK;
}

mr_status mr_ts_open(mr_ts *t, const uint8_t *buf, size_t len)
{
    ts_init(t);
    t->buf = buf;
    t->len = len;
    return ts_open_common(t);
}

mr_status mr_ts_open_file(mr_ts *t, void *stream, size_t len)
{
    ts_init(t);
    t->stream = stream;
    t->len = len;
    t->file_backed = 1;
    return ts_open_common(t);
}

static int pes_append(mr_ts_pes *p, const uint8_t *data, size_t len)
{
    if (!len) return 1;
    if (p->len > TS_PES_MAX - len ||
        !reserve(&p->data, &p->cap, p->len + len, TS_PES_MAX))
        return 0;
    memcpy(p->data + p->len, data, len);
    p->len += len;
    return 1;
}

static mr_status annexb_to_avcc(mr_ts *t, const uint8_t *p, size_t len,
                                mr_packet *pkt)
{
    size_t pos = 0, used = 0, prefix;
    while ((pos = start_code(p, len, pos, &prefix)) < len) {
        size_t begin = pos + prefix, next_prefix, end;
        size_t next = start_code(p, len, begin, &next_prefix);
        size_t n;
        end = next;
        while (end > begin && p[end - 1] == 0) end--;
        n = end - begin;
        if (n) {
            if (n > 0xffffffffUL || used > 0xffffffffUL - n - 4 ||
                !reserve(&t->packet_buf, &t->packet_cap, used + n + 4,
                         TS_PES_MAX))
                return MR_ENOMEM;
            t->packet_buf[used + 0] = (uint8_t)(n >> 24);
            t->packet_buf[used + 1] = (uint8_t)(n >> 16);
            t->packet_buf[used + 2] = (uint8_t)(n >> 8);
            t->packet_buf[used + 3] = (uint8_t)n;
            memcpy(t->packet_buf + used + 4, p + begin, n);
            used += n + 4;
        }
        pos = next;
    }
    if (!used) return MR_EFORMAT;
    pkt->is_video = 1;
    pkt->data = t->packet_buf;
    pkt->len = (uint32_t)used;
    return MR_OK;
}

static mr_status emit_pes(mr_ts *t, mr_ts_pes *p, int video, mr_packet *pkt)
{
    mr_status st;
    if (video) {
        st = annexb_to_avcc(t, p->data, p->len, pkt);
    } else {
        pkt->is_video = 0;
        pkt->data = p->data;
        pkt->len = (uint32_t)p->len;
        st = p->len ? MR_OK : MR_EAGAIN;
    }
    p->len = 0;
    p->expected = 0;
    p->active = 0;
    return st;
}

mr_status mr_ts_next_packet(mr_ts *t, mr_packet *pkt)
{
    uint8_t packet[192];
    while (t->cursor + (size_t)t->packet_size <= t->len) {
        const uint8_t *p, *es;
        size_t n, es_len;
        int pusi, has_pts;
        uint16_t pid;
        uint64_t pts;
        size_t expected;
        mr_ts_pes *a;
        int video;

        if (!ts_read_at(t, t->cursor, packet, (size_t)t->packet_size))
            return MR_EFORMAT;
        p = payload(packet + t->sync_off, &n, &pusi, &pid);
        if (!p || (pid != t->video_pid && pid != t->audio_pid)) {
            t->cursor += (size_t)t->packet_size;
            continue;
        }
        video = pid == t->video_pid;
        a = video ? &t->video_pes : &t->audio_pes;

        /* Return the completed old PES first. Leave this PUSI packet at the
         * cursor so the next call starts the new PES without losing bytes. */
        if (pusi && a->active && a->len)
            return emit_pes(t, a, video, pkt);

        t->cursor += (size_t)t->packet_size;
        es = p;
        es_len = n;
        if (pusi) {
            es = pes_payload(p, &es_len, &pts, &has_pts, &expected);
            if (!es) {
                a->active = 0;
                a->len = 0;
                continue;
            }
            a->active = 1;
            a->expected = expected;
        } else if (!a->active) {
            continue;
        }
        if (a->expected) {
            size_t remain = a->expected > a->len ? a->expected - a->len : 0;
            if (es_len > remain) es_len = remain;
        }
        if (!pes_append(a, es, es_len)) return MR_ENOMEM;
        if (a->expected && a->len >= a->expected)
            return emit_pes(t, a, video, pkt);
    }

    if (!t->video_pes.drained && t->video_pes.len) {
        t->video_pes.drained = 1;
        return emit_pes(t, &t->video_pes, 1, pkt);
    }
    if (!t->audio_pes.drained && t->audio_pes.len) {
        t->audio_pes.drained = 1;
        return emit_pes(t, &t->audio_pes, 0, pkt);
    }
    return MR_EAGAIN;
}

void mr_ts_rewind(mr_ts *t)
{
    t->cursor = 0;
    t->video_pes.len = t->audio_pes.len = 0;
    t->video_pes.expected = t->audio_pes.expected = 0;
    t->video_pes.active = t->audio_pes.active = 0;
    t->video_pes.drained = t->audio_pes.drained = 0;
}

void mr_ts_close(mr_ts *t)
{
    if (!t) return;
    free(t->video_pes.data);
    free(t->audio_pes.data);
    free(t->packet_buf);
    free(t->config);
    t->video_pes.data = t->audio_pes.data = NULL;
    t->packet_buf = t->config = NULL;
}
