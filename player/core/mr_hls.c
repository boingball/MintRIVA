/*
 * MintRIVA - HLS (.m3u8) playlist source (VOD and live).
 *
 * Fetches an HLS playlist, resolves a master playlist down to one media
 * variant, then presents that variant's segments - concatenated - as a single
 * forward-only MPEG-TS byte stream for the mr_ts demuxer. Segment sizes are
 * discovered lazily as they are opened, so the source still supports the
 * demuxer's probe-then-rewind access pattern (random access across already
 * seen segments; sequential fetch beyond them).
 *
 * A VOD playlist (EXT-X-ENDLIST, or EXT-X-PLAYLIST-TYPE:VOD) is a fixed list.
 * A live playlist has neither: it is a sliding window of segments that the
 * server keeps extending. We track EXT-X-MEDIA-SEQUENCE so a re-fetch appends
 * only genuinely new segments, and when playback catches up to the last known
 * segment we re-fetch the playlist to discover more - until an EXT-X-ENDLIST
 * finally turns the stream into a bounded one and it ends naturally.
 */
#include "mr_hls.h"
#include "mr_http.h"
#include "mr_types.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define HLS_PLAYLIST_MAX (8UL * 1024 * 1024)   /* sane cap on a playlist text  */
#define HLS_URL_MAX      1024

/* How many times playback may re-fetch a live playlist that reports no new
 * segment before giving up and reporting end of stream. Each re-fetch is a
 * full HTTP round-trip, which on real hardware paces the polling; this only
 * bounds a genuinely stalled or dead stream. */
#define HLS_LIVE_REFETCH_MAX 240

typedef struct {
    char   **segs;        /* resolved segment URLs                            */
    size_t   nsegs;
    size_t   cap;         /* allocated slots in segs (seg_start holds cap + 1)*/
    size_t  *seg_start;   /* concatenated byte offset of each segment (+ end) */
    size_t   discovered;  /* segments whose size is known (seg_start[0..this])*/
    mr_source *cur;       /* currently open segment source                    */
    size_t   cur_seg;
    /* Live streaming state (unused for VOD). */
    int      live;        /* playlist has no ENDLIST: keep re-fetching        */
    char    *playlist_url;/* media playlist URL to re-fetch for new segments  */
    unsigned long next_seq; /* media-sequence of the next not-yet-queued seg  */
    mr_http_options options; /* inherited by playlists and segments            */
    int      have_options;
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

static char *fetch_text(const char *url, const mr_http_options *options)
{
    clock_t started = clock();
    mr_source *s = mr_http_source_open_ex(url, options);
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
    mr_source_timing_add_hls_playlist((unsigned long)
        ((clock() - started) * 1000UL / CLOCKS_PER_SEC));
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

/* ---- segment list ------------------------------------------------------ */

/* Ensure segs[] has room for one more entry; seg_start[] tracks nsegs + 1
 * offsets, with seg_start[0] anchored at 0 on the first allocation. */
static int seg_reserve(hls_source *h)
{
    size_t nc;
    char **ns;
    size_t *nss;
    if (h->nsegs < h->cap) return 1;
    nc = h->cap ? h->cap * 2 : 64;
    ns = (char **)realloc(h->segs, nc * sizeof *ns);
    if (!ns) return 0;
    h->segs = ns;
    nss = (size_t *)realloc(h->seg_start, (nc + 1) * sizeof *nss);
    if (!nss) return 0;
    if (!h->cap) nss[0] = 0;
    h->seg_start = nss;
    h->cap = nc;
    return 1;
}

static int append_seg(hls_source *h, const char *url)
{
    if (!seg_reserve(h)) return 0;
    h->segs[h->nsegs] = (char *)malloc(strlen(url) + 1);
    if (!h->segs[h->nsegs]) return 0;
    strcpy(h->segs[h->nsegs], url);
    h->nsegs++;
    return 1;
}

/* ---- playlist parsing -------------------------------------------------- */

/* Pick the lowest-bandwidth variant from a master playlist; resolve it against
 * base_url into `out`. Returns 1 if a variant was found. */
static int pick_variant(char *text, const char *base_url,
                        char *out, size_t out_size,
                        const mr_http_options *options)
{
    char line[HLS_URL_MAX];
    char *p = text;
    unsigned long best_bw = 0;
    int have = 0, pending = 0;
    unsigned long pending_bw = 0;
    unsigned pending_width = 0, pending_height = 0, pending_fps = 0;
    while ((p = next_line(p, line, sizeof line)) != NULL) {
        if (starts(line, "#EXT-X-STREAM-INF")) {
            const char *bw = strstr(line, "BANDWIDTH=");
            const char *resolution = strstr(line, "RESOLUTION=");
            const char *fps = strstr(line, "FRAME-RATE=");
            pending = 1;
            pending_bw = bw ? strtoul(bw + 10, NULL, 10) : 0;
            pending_width = pending_height = pending_fps = 0;
            if (resolution)
                sscanf(resolution + 11, "%ux%u", &pending_width,
                       &pending_height);
            if (fps)
                pending_fps = (unsigned)strtoul(fps + 11, NULL, 10);
        } else if (line[0] && line[0] != '#' && pending) {
            pending = 0;
            if (options &&
                ((options->hls_max_width && pending_width > options->hls_max_width) ||
                 (options->hls_max_height && pending_height > options->hls_max_height) ||
                 (options->hls_max_fps && pending_fps > options->hls_max_fps)))
                continue;
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

/* Merge a (possibly refreshed) media playlist into the segment list. Segments
 * are numbered by EXT-X-MEDIA-SEQUENCE + position, so only those at or beyond
 * next_seq are appended - a re-fetch of a sliding window skips the ones we
 * already hold. Sets h->live from the ENDLIST / PLAYLIST-TYPE tags. Returns
 * MR_OK (with *added = how many new segments were queued), or an error for an
 * encrypted playlist. */
static mr_status merge_playlist(char *text, const char *base_url,
                                hls_source *h, int *added)
{
    char line[HLS_URL_MAX], resolved[HLS_URL_MAX];
    char *p = text;
    unsigned long media_seq = 0, idx = 0;
    int endlist = 0, vod = 0;
    *added = 0;
    while ((p = next_line(p, line, sizeof line)) != NULL) {
        if (starts(line, "#EXT-X-KEY") && !strstr(line, "METHOD=NONE")) {
            mr_source_set_error("encrypted HLS (EXT-X-KEY) is not supported");
            return MR_EUNSUPPORTED;
        }
        if (starts(line, "#EXT-X-MEDIA-SEQUENCE:")) {
            media_seq = strtoul(line + strlen("#EXT-X-MEDIA-SEQUENCE:"),
                                NULL, 10);
            continue;
        }
        if (starts(line, "#EXT-X-PLAYLIST-TYPE:") && strstr(line, "VOD"))
            vod = 1;
        if (starts(line, "#EXT-X-ENDLIST")) { endlist = 1; continue; }
        if (!line[0] || line[0] == '#') continue;      /* tag or blank         */
        {
            unsigned long seq = media_seq + idx++;
            if (seq < h->next_seq) continue;           /* already queued       */
            if (!mr_http_resolve_url(base_url, line, resolved, sizeof resolved))
                continue;
            if (!append_seg(h, resolved)) return MR_ENOMEM;
            h->next_seq = seq + 1;
            (*added)++;
        }
    }
    h->live = !(endlist || vod);
    return MR_OK;
}

/* ---- segment stream ---------------------------------------------------- */

/* Open segment `i`, recording its length so seg_start stays contiguous. Must
 * be opened in order for its start offset to be known. */
static int open_seg(hls_source *h, size_t i)
{
    clock_t started = clock();
    mr_source *s;
    size_t len;
    if (i >= h->nsegs) return 0;
    if (h->cur && h->cur_seg == i) return 1;
    if (i > h->discovered) return 0;               /* start offset unknown     */
    /* Close the previous segment's source *before* opening the next one. The
     * platform TLS layer (AmiSSL) keeps global session state, so two HTTP/S
     * connections must never be open at the same time - overlapping them makes
     * the second close tear the first's state down twice. Closing first also
     * keeps only one segment's read-ahead buffer resident at a time. */
    if (h->cur) { mr_source_close(h->cur); h->cur = NULL; }
    s = mr_http_source_open_ex(h->segs[i],
                               h->have_options ? &h->options : NULL);
    if (!s) return 0;
    len = mr_source_length(s);
    if (!len || len == MR_SOURCE_LEN_UNKNOWN) { mr_source_close(s); return 0; }
    h->cur = s;
    h->cur_seg = i;
    h->seg_start[i + 1] = h->seg_start[i] + len;
    if (i + 1 > h->discovered) h->discovered = i + 1;
    mr_source_timing_add_hls_segment((unsigned long)
        ((clock() - started) * 1000UL / CLOCKS_PER_SEC));
    return 1;
}

/* Return the segment index whose byte range contains `off`, opening segments
 * forward as needed to discover it. Returns nsegs if `off` is at/after the end
 * of the currently known segments. */
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

/* Playback has caught up to the last known segment of a live stream: re-fetch
 * the playlist until it grows (new segments) or ends (ENDLIST). Returns 1 if
 * at least one new segment was appended, 0 if the stream ended or stalled. */
static int hls_refetch_live(hls_source *h)
{
    int tries;
    /* Playback has consumed the last known segment, so its source is done with:
     * close it before fetching the playlist so only one HTTP/S connection is
     * ever open at once (see open_seg). */
    if (h->cur) { mr_source_close(h->cur); h->cur = NULL; }
    for (tries = 0; tries < HLS_LIVE_REFETCH_MAX; tries++) {
        char *text = fetch_text(h->playlist_url,
                                h->have_options ? &h->options : NULL);
        int added;
        mr_status st;
        if (!text) return 0;                       /* playlist gone / error    */
        st = merge_playlist(text, h->playlist_url, h, &added);
        free(text);
        if (st != MR_OK) return 0;
        if (added > 0) return 1;                    /* fresh segments to play   */
        if (!h->live) return 0;                     /* ENDLIST arrived: done    */
    }
    mr_source_set_error("live HLS playlist stalled (no new segments)");
    return 0;
}

static int hls_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    hls_source *h = (hls_source *)opaque;
    unsigned char *out = (unsigned char *)dst;
    if (!len) return 1;
    while (len) {
        size_t i = locate(h, off);
        size_t local, avail, take;
        if (i >= h->nsegs) {
            /* Ran off the end of the known segments. For live, pull more from
             * the playlist and retry; for VOD/ended, this is end of stream. */
            if (h->live && hls_refetch_live(h)) continue;
            return 0;
        }
        if (!open_seg(h, i)) return 0;
        local = off - h->seg_start[i];
        avail = h->seg_start[i + 1] - h->seg_start[i] - local;
        take  = len < avail ? len : avail;
        if (!mr_source_read_at(h->cur, local, out, take)) return 0;
        out += take; off += take; len -= take;
    }
    return 1;
}

static void hls_abort(void *opaque)
{
    hls_source *h = (hls_source *)opaque;
    if (h && h->cur) mr_source_abort(h->cur);
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
    free(h->playlist_url);
    free(h);
}

/* ---- open -------------------------------------------------------------- */

mr_source *mr_hls_source_open_ex(const char *url,
                                 const mr_http_options *options)
{
    char *text;
    char media_url[HLS_URL_MAX];
    const char *base;
    hls_source *h;
    mr_status st;
    int added;

    text = fetch_text(url, options);
    if (!text) return NULL;

    /* Master playlist? Resolve to one media variant and refetch. */
    base = url;
    if (strstr(text, "#EXT-X-STREAM-INF")) {
        if (!pick_variant(text, url, media_url, sizeof media_url, options)) {
            free(text);
            mr_source_set_error("no playable variant in HLS master playlist");
            return NULL;
        }
        free(text);
        text = fetch_text(media_url, options);
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
    if (options) {
        if (!mr_http_options_init(&h->options, options->user_agent,
                                  options->referer)) {
            free(text);
            hls_close(h);
            return NULL;
        }
        h->have_options = 1;
        h->options.hls_low = options->hls_low;
        h->options.hls_max_width = options->hls_max_width;
        h->options.hls_max_height = options->hls_max_height;
        h->options.hls_max_fps = options->hls_max_fps;
    }
    st = merge_playlist(text, base, h, &added);
    free(text);
    if (st != MR_OK) { hls_close(h); return NULL; }
    if (!h->nsegs) {
        hls_close(h);
        mr_source_set_error("HLS playlist has no segments");
        return NULL;
    }

    /* A live playlist (no ENDLIST) keeps growing: remember its URL so playback
     * can re-fetch it to discover new segments as the stream advances. */
    if (h->live) {
        h->playlist_url = (char *)malloc(strlen(base) + 1);
        if (!h->playlist_url) {
            hls_close(h);
            mr_source_set_error("out of memory for HLS");
            return NULL;
        }
        strcpy(h->playlist_url, base);
    }

    /* Streaming (unknown total length): the demuxer reads forward and treats a
     * short read as end of stream. mr_source_create already closes the context
     * (hls_close) if it fails, so we must not close it again here. */
    {
        mr_source *source = mr_source_create(h, MR_SOURCE_LEN_UNKNOWN,
                                             hls_read_at, hls_close, url);
        mr_source_set_abort(source, hls_abort);
        return source;
    }
}

mr_source *mr_hls_source_open(const char *url)
{
    return mr_hls_source_open_ex(url, NULL);
}
