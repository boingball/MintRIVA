/*
 * MintRIVA - raw Motion-JPEG demuxer implementation.
 */
#include "mr_raw_mjpeg.h"

#include <string.h>

/* Read geometry from a baseline JPEG SOF0 marker. */
static int jpeg_dimensions(const uint8_t *buf, size_t len, int *w, int *h)
{
    size_t p = 2;                    /* skip SOI */

    while (p + 1 < len) {
        uint8_t marker;
        uint16_t seglen;

        while (p < len && buf[p] != 0xff) p++;
        while (p < len && buf[p] == 0xff) p++;
        if (p >= len) break;
        marker = buf[p++];

        if (marker == 0xd9 || marker == 0xda) break;  /* EOI / SOS */
        if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7))
            continue;                                /* no length field */
        if (p + 2 > len) break;

        seglen = mr_rb16(buf + p);
        if (seglen < 2 || p + seglen > len) break;
        if (marker == 0xc0 && seglen >= 8) {          /* baseline SOF */
            *h = (int)mr_rb16(buf + p + 3);
            *w = (int)mr_rb16(buf + p + 5);
            return *w > 0 && *h > 0;
        }
        p += seglen;
    }
    return 0;
}

mr_status mr_raw_mjpeg_open(mr_raw_mjpeg *m, const uint8_t *buf, size_t len)
{
    int w, h;

    memset(m, 0, sizeof *m);
    if (!buf || len < 4 || buf[0] != 0xff || buf[1] != 0xd8)
        return MR_EFORMAT;
    if (!jpeg_dimensions(buf, len, &w, &h))
        return MR_EUNSUPPORTED;       /* picojpeg supports baseline JPEG */

    m->buf = buf;
    m->len = len;
    m->video.fourcc = MR_FOURCC('M','J','P','G');
    m->video.width = w;
    m->video.height = h;
    m->video.rate = 25;               /* raw MJPEG carries no timing data */
    m->video.scale = 1;
    m->video.valid = 1;
    return MR_OK;
}

mr_status mr_raw_mjpeg_next_packet(mr_raw_mjpeg *m, mr_packet *pkt)
{
    size_t start = m->cursor;
    size_t p;

    /* Tolerate padding/junk between frames, but require a complete SOI..EOI
     * JPEG before returning a packet. Entropy-coded 0xff bytes are escaped,
     * so an unescaped EOI marker is an unambiguous frame boundary. */
    while (start + 1 < m->len &&
           !(m->buf[start] == 0xff && m->buf[start + 1] == 0xd8))
        start++;
    if (start + 1 >= m->len) return MR_EAGAIN;

    for (p = start + 2; p + 1 < m->len; p++) {
        if (m->buf[p] == 0xff && m->buf[p + 1] == 0xd9) {
            size_t frame_len = p + 2 - start;
            if (frame_len > UINT32_MAX) return MR_EFORMAT;
            pkt->is_video = 1;
            pkt->data = m->buf + start;
            pkt->len = (uint32_t)frame_len;
            m->cursor = p + 2;
            return MR_OK;
        }
    }
    return MR_EAGAIN;
}

void mr_raw_mjpeg_rewind(mr_raw_mjpeg *m)
{
    m->cursor = 0;
}
