/*
 * MintRIVA - raw MPEG-4 Part 2 Visual elementary-stream demuxer.
 *
 * The decoder expects one VOP per packet. Preserve any VOS/VO/VOL/GOV headers
 * immediately preceding a VOP so repeated configuration reaches the decoder.
 */
#include "mr_raw_mpeg4.h"
#include "mr_mpeg4.h"

#include <string.h>

static size_t next_code(const uint8_t *buf, size_t len, size_t from, int *id)
{
    size_t p;
    for (p = from; p + 3 < len; p++) {
        if (buf[p] == 0 && buf[p + 1] == 0 && buf[p + 2] == 1) {
            *id = buf[p + 3];
            return p;
        }
    }
    return len;
}

mr_status mr_raw_mpeg4_open(mr_raw_mpeg4 *m, const uint8_t *buf, size_t len)
{
    int w, h;
    uint32_t rate, scale;

    memset(m, 0, sizeof *m);
    if (!buf || len < 8 ||
        buf[0] != 0 || buf[1] != 0 || buf[2] != 1)
        return MR_EFORMAT;
    if (!mr_mpeg4_probe(buf, len, &w, &h, &rate, &scale))
        return MR_EUNSUPPORTED;

    m->buf = buf;
    m->len = len;
    m->video.fourcc = MR_FOURCC('m','p','4','v');
    m->video.width = w;
    m->video.height = h;
    m->video.rate = rate;
    m->video.scale = scale;
    m->video.valid = 1;
    return MR_OK;
}

mr_status mr_raw_mpeg4_next_packet(mr_raw_mpeg4 *m, mr_packet *pkt)
{
    size_t start = m->cursor;
    size_t vop, p, prefix = m->len, end;
    int id;

    vop = next_code(m->buf, m->len, start, &id);
    while (vop < m->len && id != 0xb6) {
        vop = next_code(m->buf, m->len, vop + 4, &id);
    }
    if (vop >= m->len) return MR_EAGAIN;

    /* The first later start code begins the header sequence for the following
     * VOP. Keep scanning until that VOP establishes this packet's boundary. */
    p = next_code(m->buf, m->len, vop + 4, &id);
    while (p < m->len) {
        if (id == 0xb6) break;
        if (prefix == m->len) prefix = p;
        p = next_code(m->buf, m->len, p + 4, &id);
    }
    end = (p < m->len) ? (prefix < p ? prefix : p) : m->len;
    if (end <= start || end - start > UINT32_MAX) return MR_EFORMAT;

    pkt->is_video = 1;
    pkt->data = m->buf + start;
    pkt->len = (uint32_t)(end - start);
    m->cursor = end;
    return MR_OK;
}

void mr_raw_mpeg4_rewind(mr_raw_mpeg4 *m)
{
    m->cursor = 0;
}
