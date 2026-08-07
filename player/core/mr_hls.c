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
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef AMIGA_M68K
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#endif

#define HLS_PLAYLIST_MAX (8UL * 1024 * 1024)   /* sane cap on a playlist text  */
#define HLS_URL_MAX      1024

/* How many times playback may re-fetch a live playlist that reports no new
 * segment before giving up and reporting end of stream. Each re-fetch is a
 * full HTTP round-trip, which on real hardware paces the polling; this only
 * bounds a genuinely stalled or dead stream. */
#define HLS_LIVE_REFETCH_MAX 240
#define HLS_ASYNC_SLOTS 2
#define HLS_ASYNC_SLOT_MAX (4UL * 1024 * 1024)
#define HLS_ASYNC_TOTAL_MAX (HLS_ASYNC_SLOTS * HLS_ASYNC_SLOT_MAX)

typedef struct {
    size_t index;
    unsigned char *bytes;
    size_t length;
    size_t read_pos;
    volatile int state;             /* 0 free, 1 complete/published */
} hls_segment_buffer;

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
#ifdef AMIGA_M68K
    hls_segment_buffer slots[HLS_ASYNC_SLOTS];
    struct Task *parent_task, *worker_task;
    BYTE ready_sig, stopped_sig, data_sig, wake_sig;
    volatile int worker_ready, stop_worker, worker_done, worker_failed;
    volatile int worker_fallback;
    size_t worker_next;
    size_t buffered_bytes;
    unsigned payload_count;
    int async;
#endif
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

/* Choose a variant from a master playlist and resolve it against base_url into
 * `out`. Returns 1 if a variant was found.
 *
 * Selection: among the variants that fit the quality ceiling (hls_max_width/
 * height/fps, each 0 = don't care) pick the *highest* bandwidth, so a capable
 * machine (e.g. a PiStorm) uses its headroom instead of always the smallest
 * rendition. hls_low forces the lowest-bandwidth variant instead - the safe
 * choice for slower gear. A lowest-bandwidth fallback is always kept so a
 * master whose every rendition exceeds the ceiling still plays something. */
static int pick_variant(char *text, const char *base_url,
                        char *out, size_t out_size,
                        const mr_http_options *options)
{
    char line[HLS_URL_MAX];
    char chosen[HLS_URL_MAX];   /* best variant within the ceiling            */
    char fallback[HLS_URL_MAX]; /* lowest-bandwidth variant, ceiling ignored  */
    char *p = text;
    unsigned long chosen_bw = 0, fallback_bw = 0;
    int have_chosen = 0, have_fallback = 0, want_low, pending = 0;
    unsigned long pending_bw = 0;
    unsigned pending_width = 0, pending_height = 0, pending_fps = 0;
    want_low = options && options->hls_low;
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
            int fits;
            pending = 0;
            /* Always track the lowest-bandwidth variant as a safety net. */
            if (!have_fallback || pending_bw < fallback_bw) {
                memcpy(fallback, line, sizeof fallback);
                fallback_bw = pending_bw;
                have_fallback = 1;
            }
            fits = !options ||
                   ((!options->hls_max_width ||
                     pending_width <= options->hls_max_width) &&
                    (!options->hls_max_height ||
                     pending_height <= options->hls_max_height) &&
                    (!options->hls_max_fps ||
                     pending_fps <= options->hls_max_fps));
            if (fits &&
                (!have_chosen ||
                 (want_low ? pending_bw < chosen_bw : pending_bw > chosen_bw))) {
                memcpy(chosen, line, sizeof chosen);
                chosen_bw = pending_bw;
                have_chosen = 1;
            }
        }
    }
    if (have_chosen && mr_http_resolve_url(base_url, chosen, out, out_size))
        return 1;
    if (have_fallback && mr_http_resolve_url(base_url, fallback, out, out_size))
        return 1;
    return 0;
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
#ifdef AMIGA_M68K
        /* merge_playlist may grow/reallocate the arrays observed by the
         * consumer.  Amiga tasks share one address space, so prevent a task
         * switch only for this short publication transaction. */
        if (h->async) Forbid();
#endif
        st = merge_playlist(text, h->playlist_url, h, &added);
#ifdef AMIGA_M68K
        if (h->async) Permit();
#endif
        free(text);
        if (st != MR_OK) return 0;
        if (added > 0) return 1;                    /* fresh segments to play   */
        if (!h->live) return 0;                     /* ENDLIST arrived: done    */
    }
    mr_source_set_error("live HLS playlist stalled (no new segments)");
    return 0;
}

#ifdef AMIGA_M68K
/* Async allocation audit:
 *   shared hls_source: calloc(sizeof *h), opener owns it; mr_source owns it
 *     after mr_source_create(); hls_close() frees it after worker termination.
 *   process stack: NP_StackSize=32768, DOS owns it from CreateNewProcTags()
 *     until the process exits (signalled by stopped_sig).
 *   slot payload: malloc(exact HTTP Content-Length), worker owns it until
 *     slot_publish(); consumer then owns it until slot_release(); teardown
 *     releases either live slot only after the worker has stopped.
 *   playlist text: fetch_text() allocates Content-Length + 1; the worker owns
 *     refreshed text and hls_refetch_live() frees it after every merge.
 *   segment URLs/offset arrays and playlist_url are common synchronous HLS
 *     allocations and remain owned/freed by hls_source/hls_close(). */
static void slots_assert(const hls_source *h)
{
#ifndef NDEBUG
    size_t sum = 0;
    unsigned count = 0, i;
    for (i = 0; i < HLS_ASYNC_SLOTS; i++) {
        if (h->slots[i].bytes) {
            assert(h->slots[i].state);
            sum += h->slots[i].length;
            count++;
        } else {
            assert(!h->slots[i].state && h->slots[i].length == 0);
        }
    }
    assert(sum == h->buffered_bytes);
    assert(count == h->payload_count);
    assert(count <= HLS_ASYNC_SLOTS);
#else
    (void)h;
#endif
}

static void slot_release(hls_source *h, unsigned slot_no)
{
    hls_segment_buffer *slot = &h->slots[slot_no];
    size_t freed = slot->length;
    if (!slot->bytes) { slots_assert(h); return; }
    free(slot->bytes);
    slot->bytes = NULL;
    slot->length = slot->read_pos = 0;
    slot->state = 0;
    h->buffered_bytes -= freed;
    h->payload_count--;
    slots_assert(h);
    if (h->options.hls_timing)
        printf("HLS memory: segment %lu free=%lu slot=%u buffered=%lu\n",
               (unsigned long)slot->index, (unsigned long)freed, slot_no,
               (unsigned long)h->buffered_bytes);
}

static void slot_publish(hls_source *h, unsigned slot_no, size_t index,
                         unsigned char *bytes, size_t length)
{
    hls_segment_buffer *slot = &h->slots[slot_no];
    /* Reuse is legal only after the previous consumer-owned payload left. */
    assert(!slot->bytes && !slot->state && slot->length == 0);
    assert(h->payload_count < HLS_ASYNC_SLOTS);
    slot->index = index; slot->bytes = bytes; slot->length = length;
    slot->read_pos = 0; slot->state = 1;
    h->buffered_bytes += length;
    h->payload_count++;
    slots_assert(h);
}

/* The worker is the sole owner of every HTTP source after async is published.
 * A slot belongs to the worker while free and to the reader from publication
 * until the reader advances to the following segment.  State changes happen
 * under Forbid(), then a signal wakes the other side. */
static int fetch_segment_buffer(hls_source *h, size_t i,
                                unsigned char **bytes, size_t *length)
{
    mr_source *s;
    size_t len;
    unsigned char *p;
    int attempt;
    for (attempt = 0; attempt < 3 && !h->stop_worker; attempt++) {
        clock_t started = clock();
        if (h->options.hls_timing)
            printf("HLS prefetch: start segment %lu queue=%u\n",
                   (unsigned long)i, (unsigned)(h->slots[0].state + h->slots[1].state));
        s = mr_http_source_open_ex(h->segs[i],
                                   h->have_options ? &h->options : NULL);
        if (s) {
            len = mr_source_length(s);
            if (len > HLS_ASYNC_SLOT_MAX ||
                len > HLS_ASYNC_TOTAL_MAX - h->buffered_bytes) {
                if (h->options.hls_timing)
                    printf("HLS prefetch: segment %lu size=%lu exceeds "
                           "4 MB slot / 8 MB total cap; using synchronous mode\n",
                           (unsigned long)i, (unsigned long)len);
                mr_source_close(s);
                h->worker_fallback = 1;
                return 0;
            }
            if (len && len != MR_SOURCE_LEN_UNKNOWN &&
                (p = (unsigned char *)malloc(len)) != NULL) {
                size_t off = 0;
                while (off < len && !h->stop_worker) {
                    size_t chunk = len - off;
                    if (chunk > 64UL * 1024) chunk = 64UL * 1024;
                    if (!mr_source_read_at(s, off, p + off, chunk)) break;
                    off += chunk;
                }
                if (off == len && !h->stop_worker) {
                    unsigned long ms = (unsigned long)
                        ((clock() - started) * 1000UL / CLOCKS_PER_SEC);
                    mr_source_close(s);
                    mr_source_timing_add_hls_segment(ms);
                    if (h->options.hls_timing)
                        printf("HLS prefetch: end segment %lu %lu bytes %lu ms\n",
                               (unsigned long)i, (unsigned long)len, ms);
                    *bytes = p; *length = len;
                    return 1;
                }
                free(p);
            }
            mr_source_close(s);
        }
        if (!h->stop_worker) Delay(10); /* bounded reconnect, no spin */
    }
    return 0;
}

static void hls_worker_entry(void)
{
    struct Task *self = FindTask(NULL);
    hls_source *h;
    Wait(SIGBREAKF_CTRL_F); /* parent publishes tc_UserData after creation */
    h = (hls_source *)self->tc_UserData;
    if (!h) return;
    h->worker_task = self;
    h->wake_sig = AllocSignal(-1);
    h->worker_ready = h->wake_sig >= 0;
    Signal(h->parent_task, 1UL << h->ready_sig);
    if (!h->worker_ready) goto done;
    while (!h->stop_worker) {
        size_t i = h->worker_next;
        hls_segment_buffer *slot;
        unsigned char *bytes;
        size_t length;
        if (i >= h->nsegs) {
            if (!h->live || !hls_refetch_live(h)) break;
            continue;
        }
        slot = &h->slots[i % HLS_ASYNC_SLOTS];
        if (slot->state) {
            Wait((1UL << h->wake_sig) | SIGBREAKF_CTRL_C);
            continue;
        }
        if (!fetch_segment_buffer(h, i, &bytes, &length)) {
            if (!h->stop_worker) h->worker_failed = 1;
            break;
        }
        Forbid();
        slot_publish(h, (unsigned)(i % HLS_ASYNC_SLOTS), i, bytes, length);
        h->seg_start[i + 1] = h->seg_start[i] + length;
        if (i + 1 > h->discovered) h->discovered = i + 1;
        h->worker_next = i + 1;
        Permit();
        if (h->options.hls_timing)
            printf("HLS memory: segment %lu alloc=%lu slot=%lu buffered=%lu\n",
                   (unsigned long)i, (unsigned long)length,
                   (unsigned long)(i % HLS_ASYNC_SLOTS),
                   (unsigned long)h->buffered_bytes);
        Signal(h->parent_task, 1UL << h->data_sig);
    }
done:
    if (h->wake_sig >= 0) { FreeSignal(h->wake_sig); h->wake_sig = -1; }
    h->worker_done = 1;
    h->worker_task = NULL;
    Signal(h->parent_task, 1UL << h->stopped_sig);
    Signal(h->parent_task, 1UL << h->data_sig);
}

static int hls_async_start(hls_source *h)
{
    struct Process *p;
    h->ready_sig = AllocSignal(-1); h->stopped_sig = AllocSignal(-1);
    h->data_sig = AllocSignal(-1); h->wake_sig = -1;
    if (h->ready_sig < 0 || h->stopped_sig < 0 || h->data_sig < 0) goto fail;
    h->parent_task = FindTask(NULL);
    p = CreateNewProcTags(NP_Entry, (ULONG)hls_worker_entry,
                          NP_Name, (ULONG)"MintRIVA HLS prefetch",
                          NP_StackSize, 32768, TAG_DONE);
    if (!p) goto fail;
    p->pr_Task.tc_UserData = h;
    Signal(&p->pr_Task, SIGBREAKF_CTRL_F);
    Wait(1UL << h->ready_sig);
    if (!h->worker_ready) { Wait(1UL << h->stopped_sig); goto fail; }
    h->async = 1;
    return 1;
fail:
    if (h->ready_sig >= 0) FreeSignal(h->ready_sig);
    if (h->stopped_sig >= 0) FreeSignal(h->stopped_sig);
    if (h->data_sig >= 0) FreeSignal(h->data_sig);
    h->ready_sig = h->stopped_sig = h->data_sig = -1;
    return 0;
}

static void hls_async_stop(hls_source *h)
{
    size_t i;
    if (!h->async) return;
    h->stop_worker = 1;
    if (h->worker_task) {
        Signal(h->worker_task, (h->wake_sig >= 0 ? 1UL << h->wake_sig : 0) |
                               SIGBREAKF_CTRL_C);
        Wait(1UL << h->stopped_sig);
    }
    /* stopped_sig/worker_task proves the worker can no longer publish or use a
     * payload.  Only now may teardown reclaim consumer-visible slots. */
    for (i = 0; i < HLS_ASYNC_SLOTS; i++) slot_release(h, (unsigned)i);
    FreeSignal(h->ready_sig); FreeSignal(h->stopped_sig); FreeSignal(h->data_sig);
    h->async = 0;
}
#endif

static int hls_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    hls_source *h = (hls_source *)opaque;
    unsigned char *out = (unsigned char *)dst;
    if (!len) return 1;
#ifdef AMIGA_M68K
    if (h->async) {
        while (len) {
            size_t i, local, avail, take;
            hls_segment_buffer *slot;
            for (i = 0; i < h->discovered; i++)
                if (off < h->seg_start[i + 1]) break;
            if (i >= h->discovered) {
                if (h->worker_done) {
                    if (h->worker_fallback) {
                        hls_async_stop(h); /* HTTP ownership is released */
                        goto synchronous;
                    }
                    return 0;
                }
                if (h->options.hls_timing)
                    printf("HLS prefetch: buffer underrun at %lu, waiting queue=0\n",
                           (unsigned long)off);
                Wait((1UL << h->data_sig) | SIGBREAKF_CTRL_C);
                continue;
            }
            /* Crossing the boundary is the ownership hand-off: the consumer
             * can no longer reference the preceding complete buffer. */
            {
                hls_segment_buffer *old = &h->slots[(i + 1) % HLS_ASYNC_SLOTS];
                if (old->state && old->index < i) {
                    slot_release(h, (unsigned)((i + 1) % HLS_ASYNC_SLOTS));
                    if (h->worker_task) Signal(h->worker_task, 1UL << h->wake_sig);
                }
            }
            slot = &h->slots[i % HLS_ASYNC_SLOTS];
            if (!slot->state || slot->index != i) return 0;
            local = off - h->seg_start[i];
            avail = slot->length - local; take = len < avail ? len : avail;
            memcpy(out, slot->bytes + local, take); slot->read_pos = local + take;
            out += take; off += take; len -= take;
            if (slot->read_pos == slot->length) {
                slot_release(h, (unsigned)(i % HLS_ASYNC_SLOTS));
                if (h->worker_task) Signal(h->worker_task, 1UL << h->wake_sig);
            }
        }
        return 1;
    }
synchronous:
#endif
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

static void hls_close(void *opaque)
{
    hls_source *h = (hls_source *)opaque;
    size_t i;
    if (!h) return;
#ifdef AMIGA_M68K
    hls_async_stop(h);
#endif
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
        h->options.hls_timing = options->hls_timing;
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

#ifdef AMIGA_M68K
    /* Failure before worker_ready leaves HTTP ownership in this task and the
     * original synchronous source remains a safe, allocation-bounded fallback. */
    hls_async_start(h);
#endif

    /* Streaming (unknown total length): the demuxer reads forward and treats a
     * short read as end of stream. mr_source_create already closes the context
     * (hls_close) if it fails, so we must not close it again here. */
    return mr_source_create(h, MR_SOURCE_LEN_UNKNOWN, hls_read_at, hls_close, url);
}

mr_source *mr_hls_source_open(const char *url)
{
    return mr_hls_source_open_ex(url, NULL);
}
