/*
 * MintRIVA - minimal AVI (RIFF) demuxer implementation.
 */
#include "mr_avi.h"

/* RIFF/AVI chunk & list fourccs (little-endian on disk; compare as LE u32). */
#define CC(a,b,c,d) MR_FOURCC(a,b,c,d)
#define CC_RIFF CC('R','I','F','F')
#define CC_AVI  CC('A','V','I',' ')
#define CC_LIST CC('L','I','S','T')
#define CC_hdrl CC('h','d','r','l')
#define CC_strl CC('s','t','r','l')
#define CC_strh CC('s','t','r','h')
#define CC_strf CC('s','t','r','f')
#define CC_movi CC('m','o','v','i')
#define CC_rec  CC('r','e','c',' ')
#define CC_vids CC('v','i','d','s')
#define CC_auds CC('a','u','d','s')

/* Parse one strl LIST (the strh + strf pair for a single stream) and record
 * it against stream index `idx` - the index by which 'NNxx' movi chunk ids
 * reference this stream. */
static void parse_strl(mr_avi *a, int idx, const uint8_t *p, const uint8_t *end)
{
    uint32_t cur_type = 0;   /* 'vids' / 'auds' from this strl's strh       */

    while (p + 8 <= end) {
        uint32_t id   = mr_rl32(p);
        uint32_t size = mr_rl32(p + 4);
        const uint8_t *body = p + 8;
        if (body + size > end) break;

        if (id == CC_strh) {
            cur_type = mr_rl32(body);           /* fccType                  */
            if (cur_type == CC_vids) {
                a->video_stream = idx;
                a->video.fourcc = mr_rl32(body + 4); /* fccHandler          */
                a->video.scale  = mr_rl32(body + 20);
                a->video.rate   = mr_rl32(body + 24);
            } else if (cur_type == CC_auds) {
                a->audio_stream = idx;
            }
        } else if (id == CC_strf) {
            if (cur_type == CC_vids && size >= 40) {
                /* BITMAPINFOHEADER */
                a->video.width  = (int)mr_rl32(body + 4);
                a->video.height = (int)mr_rl32(body + 8);
                /* biCompression at offset 16 overrides handler if present. */
                {
                    uint32_t comp = mr_rl32(body + 16);
                    if (comp) a->video.fourcc = comp;
                }
                a->video.valid = 1;
            } else if (cur_type == CC_auds && size >= 16) {
                /* WAVEFORMATEX */
                a->audio.format_tag  = mr_rl16(body + 0);
                a->audio.channels    = mr_rl16(body + 2);
                a->audio.sample_rate = mr_rl32(body + 4);
                a->audio.bits        = mr_rl16(body + 14);
                a->audio.valid = 1;
            }
        }

        size += (size & 1);           /* chunks are word-aligned            */
        p = body + size;
    }
}

/* Walk the hdrl LIST, assigning an incrementing stream index to each strl. */
static void parse_hdrl(mr_avi *a, const uint8_t *p, const uint8_t *end)
{
    int stream_index = -1;

    while (p + 8 <= end) {
        uint32_t id   = mr_rl32(p);
        uint32_t size = mr_rl32(p + 4);
        const uint8_t *body = p + 8;
        if (body + size > end) break;

        if (id == CC_LIST && mr_rl32(body) == CC_strl) {
            stream_index++;
            parse_strl(a, stream_index, body + 4, body + size);
        }

        size += (size & 1);
        p = body + size;
    }
}

/* Locate the top-level 'movi' LIST and the 'hdrl' LIST. */
static mr_status scan_top(mr_avi *a)
{
    const uint8_t *p   = a->buf + 12;         /* skip RIFF/size/'AVI '      */
    const uint8_t *end = a->buf + a->len;

    while (p + 8 <= end) {
        uint32_t id   = mr_rl32(p);
        uint32_t size = mr_rl32(p + 4);
        const uint8_t *body = p + 8;
        if (body > end) break;

        if (id == CC_LIST) {
            uint32_t ltype = mr_rl32(body);
            const uint8_t *lend = body + size;
            if (lend > end) lend = end;
            if (ltype == CC_hdrl) {
                parse_hdrl(a, body + 4, lend);
            } else if (ltype == CC_movi) {
                a->movi_off = (size_t)((body + 4) - a->buf);
                a->movi_end = (size_t)(lend - a->buf);
            }
        }
        size += (size & 1);
        p = body + size;
    }
    if (!a->video.valid) return MR_EFORMAT;
    if (a->movi_off == 0) return MR_EFORMAT;
    return MR_OK;
}

mr_status mr_avi_open(mr_avi *a, const uint8_t *buf, size_t len)
{
    size_t i;
    for (i = 0; i < sizeof(*a); i++) ((uint8_t *)a)[i] = 0;
    a->video_stream = -1;
    a->audio_stream = -1;

    if (!buf || len < 12) return MR_EFORMAT;
    if (mr_rl32(buf) != CC_RIFF)      return MR_EFORMAT;
    if (mr_rl32(buf + 8) != CC_AVI)   return MR_EFORMAT;

    a->buf = buf;
    a->len = len;

    {
        mr_status st = scan_top(a);
        if (st != MR_OK) return st;
    }
    a->cursor = a->movi_off;
    return MR_OK;
}

/* Decode a 'NNxx' movi chunk id: two ASCII digits give the stream number,
 * the trailing two chars give the type ('dc'/'db' video, 'wb' audio, ...). */
static int chunk_stream_index(uint32_t id, int *is_video)
{
    int d0 = (int)((id) & 0xff) - '0';
    int d1 = (int)((id >> 8) & 0xff) - '0';
    int c2 = (int)((id >> 16) & 0xff);
    int c3 = (int)((id >> 24) & 0xff);
    if (d0 < 0 || d0 > 9 || d1 < 0 || d1 > 9) return -1;
    *is_video = (c2 == 'd' && (c3 == 'c' || c3 == 'b'));
    return d0 * 10 + d1;
}

mr_status mr_avi_next_packet(mr_avi *a, mr_packet *pkt)
{
    const uint8_t *base = a->buf;
    while (a->cursor + 8 <= a->movi_end) {
        const uint8_t *p = base + a->cursor;
        uint32_t id   = mr_rl32(p);
        uint32_t size = mr_rl32(p + 4);
        size_t   body = a->cursor + 8;

        if (id == CC_LIST) {
            /* 'rec ' grouping: descend into it. */
            a->cursor = body + 4;
            continue;
        }

        {
            size_t adv = size + (size & 1);
            if (body + size > a->movi_end) return MR_EAGAIN; /* truncated   */

            int is_video = 0;
            int idx = chunk_stream_index(id, &is_video);
            a->cursor = body + adv;

            if (idx < 0) continue;              /* skip idx1/junk/unknown   */

            pkt->is_video = (idx == a->video_stream);
            pkt->data     = base + body;
            pkt->len      = size;
            return MR_OK;
        }
    }
    return MR_EAGAIN;
}

void mr_avi_rewind(mr_avi *a)
{
    a->cursor = a->movi_off;
}
