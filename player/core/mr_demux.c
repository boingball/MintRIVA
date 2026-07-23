/*
 * MintRIVA - container auto-detect front end.
 *
 * Sniffs the buffer signature and delegates to the AVI or MOV backend, exposing
 * both through the neutral mr_demux interface so the player is container-blind.
 */
#include "mr_demux.h"
#include "mr_avi.h"
#include "mr_mov.h"
#include "mr_ts.h"
#include "mr_source.h"
#include "mr_raw_mjpeg.h"
#include "mr_raw_mpeg4.h"
#include <stdlib.h>
#include <string.h>

struct mr_demux {
    mr_container kind;
    union {
        mr_avi avi;
        mr_mov mov;
        mr_ts ts;
        mr_raw_mjpeg raw_mjpeg;
        mr_raw_mpeg4 raw_mpeg4;
    } u;
    mr_source *owned_source;
};

/* AVI  = 'RIFF' .... 'AVI '   ;  MOV = an early 'ftyp'/'moov'/'mdat' atom;
 * TS has 0x47 sync bytes every 188 bytes (or at +4 in 192-byte M2TS packets);
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
    if (len >= 377 && b[0] == 0x47 && b[188] == 0x47 && b[376] == 0x47)
        return MR_CONTAINER_TS;
    if (len >= 389 && b[4] == 0x47 && b[196] == 0x47 && b[388] == 0x47)
        return MR_CONTAINER_TS;
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
    else if (kind == MR_CONTAINER_TS)
        st = mr_ts_open(&d->u.ts, buf, len);
    else if (kind == MR_CONTAINER_RAW_MJPEG)
        st = mr_raw_mjpeg_open(&d->u.raw_mjpeg, buf, len);
    else
        st = mr_raw_mpeg4_open(&d->u.raw_mpeg4, buf, len);
    if (st != MR_OK) {
        if (kind == MR_CONTAINER_AVI) mr_avi_close(&d->u.avi);
        else if (kind == MR_CONTAINER_MOV) mr_mov_close(&d->u.mov);
        else if (kind == MR_CONTAINER_TS) mr_ts_close(&d->u.ts);
        free(d);
        return NULL;
    }
    return d;
}

mr_demux *mr_demux_open_file(const char *path)
{
    uint8_t head[512];
    size_t got;
    size_t end;
    mr_container kind;
    mr_demux *d;
    mr_status st;
    mr_source *source;

    if (!path) return NULL;
    source = mr_source_open(path);
    if (!source) return NULL;
    end = mr_source_length(source);
    got = end < sizeof head ? end : sizeof head;
    if (!mr_source_read_at(source, 0, head, got)) {
        mr_source_set_error("cannot read media response body");
        mr_source_close(source);
        return NULL;
    }
    kind = sniff(head, got);
    if (kind != MR_CONTAINER_AVI && kind != MR_CONTAINER_MOV &&
        kind != MR_CONTAINER_TS) {
        mr_source_set_error(
            "network/file source is not a supported AVI, MOV/MP4 or MPEG-TS");
        mr_source_close(source);
        return NULL;
    }
    /* AVI/MOV rely on seeking a sample index, which needs a known length. Only
     * MPEG-TS plays forward from a length-less stream. */
    if (mr_source_is_streaming(source) && kind != MR_CONTAINER_TS) {
        mr_source_set_error(
            "streamed AVI/MOV needs a seekable server (Content-Length)");
        mr_source_close(source);
        return NULL;
    }

    d = (mr_demux *)calloc(1, sizeof *d);
    if (!d) {
        mr_source_set_error("not enough memory for demuxer");
        mr_source_close(source);
        return NULL;
    }
    d->kind = kind;
    d->owned_source = source;

    if (kind == MR_CONTAINER_AVI)
        st = mr_avi_open_source(&d->u.avi, source, end);
    else if (kind == MR_CONTAINER_MOV)
        st = mr_mov_open_source(&d->u.mov, source, end);
    else
        st = mr_ts_open_source(&d->u.ts, source, end);
    if (st != MR_OK) {
        if (kind == MR_CONTAINER_AVI) mr_avi_close(&d->u.avi);
        else if (kind == MR_CONTAINER_MOV) mr_mov_close(&d->u.mov);
        else mr_ts_close(&d->u.ts);
        mr_source_set_error("unsupported or malformed streamed container");
        mr_source_close(source);
        free(d);
        return NULL;
    }
    return d;
}

int mr_demux_is_file_backed_container(const char *path)
{
    uint8_t head[512];
    size_t got;
    mr_container kind;
    mr_source *source;
    if (!path) return 0;
    if (mr_source_is_url(path)) return 1;
    source = mr_source_open(path);
    if (!source) return 0;
    got = mr_source_length(source) < sizeof head
        ? mr_source_length(source) : sizeof head;
    if (!mr_source_read_at(source, 0, head, got)) got = 0;
    mr_source_close(source);
    kind = sniff(head, got);
    return kind == MR_CONTAINER_AVI || kind == MR_CONTAINER_MOV ||
           kind == MR_CONTAINER_TS;
}

const char *mr_demux_last_open_error(void)
{
    return mr_source_last_error();
}

mr_status mr_demux_next_packet(mr_demux *d, mr_packet *pkt)
{
    if (d->kind == MR_CONTAINER_AVI)
        return mr_avi_next_packet(&d->u.avi, pkt);
    if (d->kind == MR_CONTAINER_MOV)
        return mr_mov_next_packet(&d->u.mov, pkt);
    if (d->kind == MR_CONTAINER_TS)
        return mr_ts_next_packet(&d->u.ts, pkt);
    if (d->kind == MR_CONTAINER_RAW_MJPEG)
        return mr_raw_mjpeg_next_packet(&d->u.raw_mjpeg, pkt);
    return mr_raw_mpeg4_next_packet(&d->u.raw_mpeg4, pkt);
}

void mr_demux_rewind(mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) mr_avi_rewind(&d->u.avi);
    else if (d->kind == MR_CONTAINER_MOV) mr_mov_rewind(&d->u.mov);
    else if (d->kind == MR_CONTAINER_TS) mr_ts_rewind(&d->u.ts);
    else if (d->kind == MR_CONTAINER_RAW_MJPEG)
        mr_raw_mjpeg_rewind(&d->u.raw_mjpeg);
    else mr_raw_mpeg4_rewind(&d->u.raw_mpeg4);
}

void mr_demux_close(mr_demux *d)
{
    if (!d) return;
    if (d->kind == MR_CONTAINER_AVI) mr_avi_close(&d->u.avi);
    else if (d->kind == MR_CONTAINER_MOV) mr_mov_close(&d->u.mov);
    else if (d->kind == MR_CONTAINER_TS) mr_ts_close(&d->u.ts);
    if (d->owned_source) mr_source_close(d->owned_source);
    free(d);
}

const mr_video_info *mr_demux_video(const mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) return &d->u.avi.video;
    if (d->kind == MR_CONTAINER_MOV) return &d->u.mov.video;
    if (d->kind == MR_CONTAINER_TS) return &d->u.ts.video;
    if (d->kind == MR_CONTAINER_RAW_MJPEG) return &d->u.raw_mjpeg.video;
    return &d->u.raw_mpeg4.video;
}

const mr_audio_info *mr_demux_audio(const mr_demux *d)
{
    if (d->kind == MR_CONTAINER_AVI) return &d->u.avi.audio;
    if (d->kind == MR_CONTAINER_MOV) return &d->u.mov.audio;
    if (d->kind == MR_CONTAINER_TS) return &d->u.ts.audio;
    if (d->kind == MR_CONTAINER_RAW_MJPEG) return &d->u.raw_mjpeg.audio;
    return &d->u.raw_mpeg4.audio;
}

const char *mr_demux_container_name(const mr_demux *d)
{
    return d->kind == MR_CONTAINER_AVI ? "AVI"
         : d->kind == MR_CONTAINER_MOV ? "MOV"
         : d->kind == MR_CONTAINER_TS ? "MPEG-TS"
         : d->kind == MR_CONTAINER_RAW_MJPEG ? "raw MJPEG"
         : d->kind == MR_CONTAINER_RAW_MPEG4 ? "raw M4V" : "?";
}
