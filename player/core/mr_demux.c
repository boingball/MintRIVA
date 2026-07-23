/*
 * MintRIVA - container auto-detect front end.
 *
 * Sniffs the buffer signature and delegates to the AVI or MOV backend, exposing
 * both through the neutral mr_demux interface so the player is container-blind.
 */
#include "mr_demux.h"
#include "mr_avi.h"
#include "mr_mov.h"
#include "mr_raw_mjpeg.h"
#include "mr_raw_mpeg4.h"
#include <stdlib.h>
#include <string.h>

struct mr_demux {
    mr_container kind;
    union {
        mr_avi avi;
        mr_mov mov;
        mr_raw_mjpeg raw_mjpeg;
        mr_raw_mpeg4 raw_mpeg4;
    } u;
};

/* AVI  = 'RIFF' .... 'AVI '   ;  MOV = an early 'ftyp'/'moov'/'mdat' atom;
 * raw MJPEG begins directly with a JPEG SOI marker. */
static mr_container sniff(const uint8_t *b, size_t len)
{
    if (len >= 12 &&
        mr_rl32(b) == MR_FOURCC('R','I','F','F') &&
        mr_rl32(b + 8) == MR_FOURCC('A','V','I',' '))
        return MR_CONTAINER_AVI;
    if (len >= 8) {
        /* atom types are stored big-endian, so pack the constants big-endian */
        #define BE4(a,b_,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b_)<<16)| \
                               ((uint32_t)(c)<<8)|(uint32_t)(d))
        uint32_t t = mr_rb32(b + 4);
        if (t == BE4('f','t','y','p') ||
            t == BE4('m','o','o','v') ||
            t == BE4('m','d','a','t') ||
            t == BE4('w','i','d','e') ||
            t == BE4('f','r','e','e') ||
            t == BE4('s','k','i','p'))
            return MR_CONTAINER_MOV;
        #undef BE4
    }
    if (len >= 2 && b[0] == 0xff && b[1] == 0xd8)
        return MR_CONTAINER_RAW_MJPEG;
    if (len >= 4 && b[0] == 0 && b[1] == 0 && b[2] == 1 &&
        (b[3] == 0xb0 || b[3] == 0xb5 || b[3] <= 0x2f))
        return MR_CONTAINER_RAW_MPEG4;
    return MR_CONTAINER_NONE;
}

mr_demux *mr_demux_open(const uint8_t *buf, size_t len)
{
    mr_container kind = sniff(buf, len);
    if (kind == MR_CONTAINER_NONE) return NULL;

    mr_demux *d = (mr_demux *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->kind = kind;

    mr_status st;
    if (kind == MR_CONTAINER_AVI)
        st = mr_avi_open(&d->u.avi, buf, len);
    else if (kind == MR_CONTAINER_MOV)
        st = mr_mov_open(&d->u.mov, buf, len);
    else if (kind == MR_CONTAINER_RAW_MJPEG)
        st = mr_raw_mjpeg_open(&d->u.raw_mjpeg, buf, len);
    else
        st = mr_raw_mpeg4_open(&d->u.raw_mpeg4, buf, len);
    if (st != MR_OK) { free(d); return NULL; }
    return d;
}

mr_status mr_demux_next_packet(mr_demux *d, mr_packet *pkt)
{
    if (d->kind == MR_CONTAINER_AVI)
        return mr_avi_next_packet(&d->u.avi, pkt);
    if (d->kind == MR_CONTAINER_MOV)
        return mr_mov_next_packet(&d->u.mov, pkt);
    if (d->kind == MR_CONTAINER_RAW_MJPEG)
        return mr_raw_mjpeg_next_packet(&d->u.raw_mjpeg, pkt);
    return mr_raw_mpeg4_next_packet(&d->u.raw_mpeg4, pkt);
}

void mr_demux_rewind(mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) mr_avi_rewind(&d->u.avi);
    else if (d->kind == MR_CONTAINER_MOV) mr_mov_rewind(&d->u.mov);
    else if (d->kind == MR_CONTAINER_RAW_MJPEG)
        mr_raw_mjpeg_rewind(&d->u.raw_mjpeg);
    else mr_raw_mpeg4_rewind(&d->u.raw_mpeg4);
}

void mr_demux_close(mr_demux *d)
{
    if (!d) return;
    if (d->kind == MR_CONTAINER_MOV) mr_mov_close(&d->u.mov);
    free(d);
}

const mr_video_info *mr_demux_video(const mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) return &d->u.avi.video;
    if (d->kind == MR_CONTAINER_MOV) return &d->u.mov.video;
    if (d->kind == MR_CONTAINER_RAW_MJPEG) return &d->u.raw_mjpeg.video;
    return &d->u.raw_mpeg4.video;
}

const mr_audio_info *mr_demux_audio(const mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) return &d->u.avi.audio;
    if (d->kind == MR_CONTAINER_MOV) return &d->u.mov.audio;
    if (d->kind == MR_CONTAINER_RAW_MJPEG) return &d->u.raw_mjpeg.audio;
    return &d->u.raw_mpeg4.audio;
}

const char *mr_demux_container_name(const mr_demux *d)
{
    return d->kind == MR_CONTAINER_AVI ? "AVI"
         : d->kind == MR_CONTAINER_MOV ? "MOV"
         : d->kind == MR_CONTAINER_RAW_MJPEG ? "raw MJPEG"
         : d->kind == MR_CONTAINER_RAW_MPEG4 ? "raw M4V" : "?";
}
