/*
 * MintRIVA - HLS (.m3u8) playlist source (VOD).
 *
 * Fetches an HLS playlist, resolves a master playlist down to one media
 * variant, then presents that variant's segments - concatenated - as a single
 * forward-only MPEG-TS byte stream for the mr_ts demuxer. Segment sizes are
 * discovered lazily as they are opened, so the source still supports the
 * demuxer's probe-then-rewind access pattern (random access across already
 * seen segments; sequential fetch beyond them).
 */
#include "mr_hls.h"
#include "mr_http.h"

#include <stdlib.h>
#include <string.h>

#define HLS_PLAYLIST_MAX (8UL * 1024 * 1024)   /* sane cap on a playlist text  */
#define HLS_URL_MAX      1024

typedef struct {
    char   **segs;        /* resolved segment URLs                            */
    size_t   nsegs;
    size_t  *seg_start;   /* concatenated byte offset of each segment (+ end) */
    size_t   discovered;  /* segments whose size is known (seg_start[0..this])*/
    mr_source *cur;       /* currently open segment source                    */
    size_t   cur_seg;
} hls_source;

/* ---- URL detection ----------------------------------------------------- */

int mr_source_is_hls(const char *url)
{
    const char *q, *dot;
    size_t n;
    if (!mr_source_is_url(url)) return 0;
    q = strchr(url, '?');                       /* ignore any query string      */
    n = q ? (size_t)(q - url) : strlen(url);
    if (n < 5) return 0;
    dot = url + n - 5;
    return dot[0] == '.' && (dot[1] == 'm' || dot[1] == 'M') && dot[2] == '3' &&
           dot[3] == 'u' && (dot[4] == '8');
}

/* ---- small text helpers ------------------------------------------------ */

static char *fetch_text(const char *url)
{
    mr_source *s = mr_http_source_open(url);
    size_t len;
    char *buf;
    if (!s) return NULL;
    len = mr_source_length(s);
    if (!len || len == MR_SOURCE_LEN_UNKNOWN || len > HLS_PLAYLIST_MAX) {
        mr_source_set_error("HLS playlist is length-less or too large");
        mr_source_close(s);
        return NULL;
    }
    buf = (char *)malloc(len + 1);
    if (!buf || !mr_source_read_at(s, 0, buf, len)) {
        free(buf);
        mr_source_close(s);
        mr_source_set_error("cannot read HLS playlist");
        return NULL;
    }
    buf[len] = '\0';
    mr_source_close(s);
    return buf;
}

/* Copy the next line into `line` (without EOL); return the start of the line
 * after (or NULL at end). Trims trailing CR and leading/trailing spaces. */
static char *next_line(char *p, char *line, size_t cap)
{
    char *e;
    size_t n;
    if (!p || !*p) return NULL;
    e = p;
    while (*e && *e != '\n') e++;
    n = (size_t)(e - p);
    while (n && (p[n-1] == '\r' || p[n-1] == ' ' || p[n-1] == '\t')) n--;
    while (n && (*p == ' ' || *p == '\t')) { p++; n--; }
    if (n >= cap) n = cap - 1;
    memcpy(line, p, n);
    line[n] = '\0';
    return *e ? e + 1 : e;
}

static int starts(const char *s, const char *prefix)
{
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

/* ---- playlist parsing -------------------------------------------------- */

/* Pick the lowest-bandwidth variant from a master playlist; resolve it against
 * base_url into `out`. Returns 1 if a variant was found. */
static int pick_variant(char *text, const char *base_url,
                        char *out, size_t out_size)
{
    char line[HLS_URL_MAX];
    char *p = text;
    unsigned long best_bw = 0;
    int have = 0, pending = 0;
    unsigned long pending_bw = 0;
    while ((p = next_line(p, line, sizeof line)) != NULL) {
        if (starts(line, "#EXT-X-STREAM-INF")) {
            const char *bw = strstr(line, "BANDWIDTH=");
            pending = 1;
            pending_bw = bw ? strtoul(bw + 10, NULL, 10) : 0;
        } else if (line[0] && line[0] != '#' && pending) {
            pending = 0;
            if (!have || pending_bw < best_bw || best_bw == 0) {
                if (mr_http_resolve_url(base_url, line, out, out_size)) {
                    best_bw = pending_bw;
                    have = 1;
                }
            }
        }
    }
    return have;
}

/* Parse a media playlist: collect resolved segment URLs into the source.
 * Returns MR_OK, or an error status for encrypted / live / empty playlists. */
static mr_status parse_media(char *text, const char *base_url, hls_source *h)
{
    char line[HLS_URL_MAX], resolved[HLS_URL_MAX];
    char *p = text;
    size_t cap = 0;
    int endlist = 0;
    while ((p = next_line(p, line, sizeof line)) != NULL) {
        if (starts(line, "#EXT-X-KEY") && !strstr(line, "METHOD=NONE")) {
            mr_source_set_error("encrypted HLS (EXT-X-KEY) is not supported");
            return MR_EUNSUPPORTED;
        }
        if (starts(line, "#EXT-X-ENDLIST")) { endlist = 1; continue; }
        if (!line[0] || line[0] == '#') continue;      /* tag or blank         */
        if (!mr_http_resolve_url(base_url, line, resolved, sizeof resolved))
            continue;
        if (h->nsegs == cap) {
            size_t nc = cap ? cap * 2 : 64;
            char **ns = (char **)realloc(h->segs, nc * sizeof *ns);
            if (!ns) return MR_ENOMEM;
            h->segs = ns; cap = nc;
        }
        h->segs[h->nsegs] = (char *)malloc(strlen(resolved) + 1);
        if (!h->segs[h->nsegs]) return MR_ENOMEM;
        strcpy(h->segs[h->nsegs], resolved);
        h->nsegs++;
    }
    if (!h->nsegs) {
        mr_source_set_error("HLS playlist has no segments");
        return MR_EFORMAT;
    }
    if (!endlist) {
        mr_source_set_error("live HLS is not supported yet (no EXT-X-ENDLIST)");
        return MR_EUNSUPPORTED;
    }
    return MR_OK;
}

/* ---- segment stream ---------------------------------------------------- */

/* Open segment `i`, recording its length so seg_start stays contiguous. Must
 * be opened in order for its start offset to be known. */
static int open_seg(hls_source *h, size_t i)
{
    mr_source *s;
    size_t len;
    if (i >= h->nsegs) return 0;
    if (h->cur && h->cur_seg == i) return 1;
    if (i > h->discovered) return 0;               /* start offset unknown     */
    s = mr_http_source_open(h->segs[i]);
    if (!s) return 0;
    len = mr_source_length(s);
    if (!len || len == MR_SOURCE_LEN_UNKNOWN) { mr_source_close(s); return 0; }
    if (h->cur) mr_source_close(h->cur);
    h->cur = s;
    h->cur_seg = i;
    h->seg_start[i + 1] = h->seg_start[i] + len;
    if (i + 1 > h->discovered) h->discovered = i + 1;
    return 1;
}

/* Return the segment index whose byte range contains `off`, opening segments
 * forward as needed to discover it. Returns nsegs if `off` is at/after EOF. */
static size_t locate(hls_source *h, size_t off)
{
    size_t i;
    for (i = 0; i < h->discovered; i++)
        if (off < h->seg_start[i + 1]) return i;
    /* discover forward until off is covered or segments run out */
    while (h->discovered < h->nsegs) {
        if (!open_seg(h, h->discovered)) break;
        if (off < h->seg_start[h->cur_seg + 1]) return h->cur_seg;
    }
    return h->nsegs;
}

static int hls_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    hls_source *h = (hls_source *)opaque;
    unsigned char *out = (unsigned char *)dst;
    if (!len) return 1;
    while (len) {
        size_t i = locate(h, off);
        size_t local, avail, take;
        if (i >= h->nsegs) return 0;               /* past the last segment    */
        if (!open_seg(h, i)) return 0;
        local = off - h->seg_start[i];
        avail = h->seg_start[i + 1] - h->seg_start[i] - local;
        take  = len < avail ? len : avail;
        if (!mr_source_read_at(h->cur, local, out, take)) return 0;
        out += take; off += take; len -= take;
    }
    return 1;
}

static void hls_close(void *opaque)
{
    hls_source *h = (hls_source *)opaque;
    size_t i;
    if (!h) return;
    if (h->cur) mr_source_close(h->cur);
    for (i = 0; i < h->nsegs; i++) free(h->segs[i]);
    free(h->segs);
    free(h->seg_start);
    free(h);
}

/* ---- open -------------------------------------------------------------- */

mr_source *mr_hls_source_open(const char *url)
{
    char *text;
    char media_url[HLS_URL_MAX];
    const char *base;
    hls_source *h;
    mr_source *src;
    mr_status st;

    text = fetch_text(url);
    if (!text) return NULL;

    /* Master playlist? Resolve to one media variant and refetch. */
    base = url;
    if (strstr(text, "#EXT-X-STREAM-INF")) {
        if (!pick_variant(text, url, media_url, sizeof media_url)) {
            free(text);
            mr_source_set_error("no playable variant in HLS master playlist");
            return NULL;
        }
        free(text);
        text = fetch_text(media_url);
        if (!text) return NULL;
        base = media_url;
        if (strstr(text, "#EXT-X-STREAM-INF")) {
            free(text);
            mr_source_set_error("nested HLS master playlist is not supported");
            return NULL;
        }
    }

    h = (hls_source *)calloc(1, sizeof *h);
    if (!h) { free(text); mr_source_set_error("out of memory for HLS"); return NULL; }
    st = parse_media(text, base, h);
    free(text);
    if (st != MR_OK) { hls_close(h); return NULL; }

    h->seg_start = (size_t *)calloc(h->nsegs + 1, sizeof *h->seg_start);
    if (!h->seg_start) { hls_close(h); mr_source_set_error("out of memory for HLS"); return NULL; }

    /* Streaming (unknown total length): the demuxer reads forward and treats a
     * short read as end of stream. */
    src = mr_source_create(h, MR_SOURCE_LEN_UNKNOWN, hls_read_at, hls_close, url);
    if (!src) { hls_close(h); return NULL; }
    return src;
}
