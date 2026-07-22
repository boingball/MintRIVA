/*
 * MintRIVA - container auto-detect front end.
 *
 * Sniffs the buffer signature and delegates to the AVI or MOV backend, exposing
 * both through the neutral mr_demux interface so the player is container-blind.
 */
#include "mr_demux.h"
#include "mr_avi.h"
#include "mr_mov.h"
#include <stdlib.h>
#include <string.h>

struct mr_demux {
    mr_container kind;
    union { mr_avi avi; mr_mov mov; } u;
};

/* AVI  = 'RIFF' .... 'AVI '   ;  MOV = an early 'ftyp'/'moov'/'mdat' atom. */
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
    return MR_CONTAINER_NONE;
}

mr_demux *mr_demux_open(const uint8_t *buf, size_t len)
{
    mr_container kind = sniff(buf, len);
    if (kind == MR_CONTAINER_NONE) return NULL;

    mr_demux *d = (mr_demux *)calloc(1, sizeof *d);
    if (!d) return NULL;
    d->kind = kind;

    mr_status st = (kind == MR_CONTAINER_AVI)
                 ? mr_avi_open(&d->u.avi, buf, len)
                 : mr_mov_open(&d->u.mov, buf, len);
    if (st != MR_OK) { free(d); return NULL; }
    return d;
}

mr_status mr_demux_next_packet(mr_demux *d, mr_packet *pkt)
{
    return d->kind == MR_CONTAINER_AVI
         ? mr_avi_next_packet(&d->u.avi, pkt)
         : mr_mov_next_packet(&d->u.mov, pkt);
}

void mr_demux_rewind(mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) mr_avi_rewind(&d->u.avi);
    else                             mr_mov_rewind(&d->u.mov);
}

void mr_demux_close(mr_demux *d)
{
    if (!d) return;
    if (d->kind == MR_CONTAINER_MOV) mr_mov_close(&d->u.mov);
    free(d);
}

const mr_video_info *mr_demux_video(const mr_demux *d)
{
    return d->kind == MR_CONTAINER_AVI ? &d->u.avi.video : &d->u.mov.video;
}

const mr_audio_info *mr_demux_audio(const mr_demux *d)
{
    return d->kind == MR_CONTAINER_AVI ? &d->u.avi.audio : &d->u.mov.audio;
}

const char *mr_demux_container_name(const mr_demux *d)
{
    return d->kind == MR_CONTAINER_AVI ? "AVI"
         : d->kind == MR_CONTAINER_MOV ? "MOV" : "?";
}
