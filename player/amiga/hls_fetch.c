/*
 * MintVID - background HLS fetch worker (Amiga). See hls_fetch.h for the
 * public contract. Design notes:
 *
 * - A single background process performs every playlist/segment fetch this
 *   session makes, installed as core/mr_http.c's fetch override/prefetch
 *   hint (mr_http_set_fetch_override()/mr_http_set_prefetch_hint()) so it
 *   is the *only* task that ever touches that file's socket/TLS state -
 *   see the design note above connect_socket() in mr_http.c for why that
 *   matters on Amiga. hls_fetch_start() is called before any other network
 *   call this session might make (mrplay.c calls it ahead of YouTube URL
 *   resolution), so the worker is always the session's first opener, never
 *   a second task adopting state another task already opened.
 *
 * - Exactly one request is ever in flight (one reusable struct Message,
 *   reused for every fetch and finally for the quit handshake). One
 *   segment of background lookahead rides alongside it: mr_http.c's
 *   prefetch hint fires for the next HLS segment while the current one
 *   plays, and is usually ready by the time it is needed. A miss (or the
 *   playlist, which is never hinted, only ever fetched on demand) falls
 *   back to a blocking round trip through the same worker - never worse
 *   than today's fully-synchronous behaviour, and unlike it, the main task
 *   stays responsive throughout (audio/video/UI keep moving, ESC/quit is
 *   honoured promptly) because every wait here is a serviced poll, never a
 *   raw blocking Wait()/WaitPort() (the one exception is the one-time
 *   worker-startup handshake in hls_fetch_start(), before any playback
 *   state exists to stay responsive for).
 *
 * - A caller-visible generation counter (hls_fetch_cancel(), bumped on
 *   stream switch/live-reconnect/stop) invalidates whatever the worker is
 *   holding without ever sending it a message: a reply reclaimed under a
 *   stale generation is simply freed instead of cached or handed to a
 *   caller. This can never race the single in-flight slot, because nothing
 *   is ever sent to the worker until the slot is confirmed free (reclaimed,
 *   generation-checked or not) - cancel() only changes what a *future*
 *   reclaim does with whatever comes back.
 *
 * - hls_fetch_stop() is the one place that waits unconditionally rather
 *   than aborting on the service callback's request (see
 *   hls_fetch_wait_busy_forever()): this task shares this process's own
 *   code segment (CreateNewProcTags() straight at a function here, not a
 *   separately LoadSeg'd program - same pattern as audio_paula.c's worker),
 *   so there is no safe way to "give up and proceed anyway" once this
 *   process's own exit would unload that segment out from under a task
 *   still running in it. Only actually waiting for the worker's quit
 *   acknowledgement is safe, however long that takes.
 *
 * - All allocation on the worker's fetch path already goes through
 *   core/mr_alloc.h (AllocVec/FreeVec on Amiga - task-safe, unlike libnix
 *   malloc/free) via mr_http_fetch_text_direct()/mr_http_post_json_direct().
 *   The result buffer's ownership passes to whichever caller consumes it
 *   (mr_free() required); anything reclaimed but not consumed (a stale
 *   generation, or a mismatched URL when a fresh request pre-empts a
 *   lingering hint result) is freed right there in hls_fetch_reclaim() -
 *   the one place either happens, so there is exactly one path that ever
 *   frees a reply buffer, not one per caller.
 */
#include "hls_fetch.h"

#include "../core/mr_http.h"
#include "../core/mr_source.h"
#include "../core/mr_alloc.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define HLS_FETCH_STACK       300000UL  /* HTTPS/AmiSSL needs a deep stack   */
#define HLS_FETCH_JSON_MAX    1024      /* matches mr_youtube.c's json[]     */
#define HLS_FETCH_ERROR_MAX   192       /* matches mr_source.c's error cap   */
#define HLS_FETCH_HINT_MAX    (24UL * 1024 * 1024) /* matches mr_hls.c's own
                                                     * HLS_SEGMENT_MAX - hints
                                                     * are only ever segments */
#define HLS_FETCH_MEM_FLOOR   (4UL * 1024 * 1024)  /* true-exhaustion guard,
                                                     * not a working-set cap */
#define HLS_FETCH_STOP_NOTICE_MS 5000UL /* re-print the "still waiting" note
                                         * at this cadence during teardown   */

/* main <-> worker request (reused; one in flight at a time) */
typedef struct {
    struct Message  msg;
    char            url[MR_HTTP_URL_MAX];
    mr_http_options opts;
    int             has_opts;
    char            post_json[HLS_FETCH_JSON_MAX];
    int             is_post;
    size_t          max_size;
    unsigned        generation;
    int             quit;
    /* result, written by the worker before ReplyMsg (untouched on quit) */
    unsigned char  *buf;
    size_t          len;
    int             ok;
    char            error[HLS_FETCH_ERROR_MAX];
} hls_fetch_request;

static struct Process *g_proc;
static struct MsgPort *g_worker_port;    /* worker's port (worker-owned)      */
static struct MsgPort *g_reply_port;     /* main's port (main-owned)          */
static struct Message  g_ready_msg;      /* worker -> main "I'm up"           */
static hls_fetch_request g_req;
static int              g_busy;          /* g_req sent, reply not reclaimed   */
static int              g_have_ready;    /* a completed reply is cached       */
static char             g_ready_url[MR_HTTP_URL_MAX];
static unsigned         g_ready_generation;
static unsigned char   *g_ready_buf;
static size_t           g_ready_len;
static int              g_ready_ok;
static char             g_ready_error[HLS_FETCH_ERROR_MAX];
static int              g_active;
static int              g_verbose;
static unsigned         g_generation;
static hls_fetch_service_fn g_service;
static void             *g_service_opaque;
static unsigned long    g_hits, g_misses, g_worst_wait_ms;

/* ---- worker task --------------------------------------------------------- */

static int hls_fetch_download(hls_fetch_request *r)
{
    char *buf = NULL;
    size_t len = 0;
    int ok;
    if (AvailMem(MEMF_ANY) <= HLS_FETCH_MEM_FLOOR) {
        strncpy(r->error, "not enough free memory to fetch",
               sizeof r->error - 1);
        r->error[sizeof r->error - 1] = 0;
        return 0;
    }
    if (r->is_post)
        ok = mr_http_post_json_direct(r->url, r->has_opts ? &r->opts : NULL,
                                      r->post_json, &buf, &len, r->max_size);
    else
        ok = mr_http_fetch_text_direct(r->url, r->has_opts ? &r->opts : NULL,
                                       &buf, &len, r->max_size);
    if (!ok) {
        const char *e = mr_source_last_error();
        strncpy(r->error, e ? e : "fetch failed", sizeof r->error - 1);
        r->error[sizeof r->error - 1] = 0;
        return 0;
    }
    r->buf = (unsigned char *)buf;
    r->len = len;
    return 1;
}

static void hls_fetch_worker(void)
{
    hls_fetch_request *req, *quit_req = NULL;
    /* Create the receive port in this task's context (its signal belongs to
     * us) and tell main we are up - whether or not the port was created. */
    g_worker_port = CreateMsgPort();
    PutMsg(g_reply_port, &g_ready_msg);
    if (!g_worker_port) return;            /* main sees NULL and gives up    */

    for (;;) {
        WaitPort(g_worker_port);
        while ((req = (hls_fetch_request *)GetMsg(g_worker_port)) != NULL) {
            if (req->quit) { quit_req = req; goto done; }
            req->buf = NULL; req->len = 0; req->error[0] = 0;
            req->ok = hls_fetch_download(req);
            ReplyMsg((struct Message *)req);
        }
    }
done:
    /* Release the socket/TLS state this task opened, from this task, before
     * acknowledging the quit - main proceeds to its own mr_http_net_shutdown()
     * as soon as it sees the reply, so ours must be fully done first. */
    mr_http_net_shutdown();
    DeleteMsgPort(g_worker_port);
    g_worker_port = NULL;
    /* Forbid() before the reply so main - woken by it - cannot start tearing
     * down further (freeing state this task's final RemTask still touches)
     * until dos has actually removed this task; only its last Permit-equivalent
     * switches back to main. */
    Forbid();
    ReplyMsg((struct Message *)quit_req);
}

/* ---- main-task helpers --------------------------------------------------- */

static void hls_fetch_free_ready(void)
{
    if (g_ready_buf) mr_free(g_ready_buf);
    g_ready_buf = NULL;
    g_have_ready = 0;
}

/* Non-blocking: pull a completed reply into the ready cache if one exists.
 * The one place a reply is ever interpreted - both the quit-ack skip and the
 * stale-generation discard live here, so every caller below (and
 * hls_fetch_stop()'s drain) shares exactly this logic. */
static void hls_fetch_reclaim(void)
{
    struct Message *m;
    if (!g_reply_port) return;
    while ((m = GetMsg(g_reply_port)) != NULL) {
        if (m != &g_req.msg) continue;     /* not our request (can't happen) */
        g_busy = 0;
        if (g_req.quit) continue;          /* quit ack: no payload           */
        if (g_req.generation != g_generation) {
            if (g_req.buf) mr_free(g_req.buf);
            continue;
        }
        hls_fetch_free_ready();
        g_have_ready = 1;
        strncpy(g_ready_url, g_req.url, sizeof g_ready_url - 1);
        g_ready_url[sizeof g_ready_url - 1] = 0;
        g_ready_generation = g_req.generation;
        g_ready_buf = g_req.buf;
        g_ready_len = g_req.len;
        g_ready_ok = g_req.ok;
        strncpy(g_ready_error, g_req.error, sizeof g_ready_error - 1);
        g_ready_error[sizeof g_ready_error - 1] = 0;
    }
}

/* If the ready cache holds `url` at the current generation, consume it
 * (transferring buffer ownership to the caller) and return 1/0 for
 * ok/failed. Returns -1 (cache left untouched) when there is no match. */
static int hls_fetch_take_ready(const char *url, unsigned char **out,
                                size_t *out_len)
{
    int ok;
    if (!g_have_ready || g_ready_generation != g_generation ||
        strcmp(g_ready_url, url) != 0)
        return -1;
    ok = g_ready_ok;
    if (ok) { *out = g_ready_buf; *out_len = g_ready_len; }
    else mr_source_set_error(g_ready_error);
    g_ready_buf = NULL;
    g_have_ready = 0;
    return ok;
}

/* Poll until the outstanding request completes or the service callback asks
 * to abort. Used for a normal (non-teardown) wait: returning early with
 * g_busy still true just means this one call reports failure - the slot
 * stays legitimately busy for whoever asks next. */
static int hls_fetch_wait_busy(void)
{
    clock_t started = g_verbose ? clock() : 0;
    for (;;) {
        hls_fetch_reclaim();
        if (!g_busy) {
            if (g_verbose) {
                unsigned long ms = (unsigned long)
                    ((clock() - started) * 1000UL / CLOCKS_PER_SEC);
                if (ms > g_worst_wait_ms) g_worst_wait_ms = ms;
            }
            return 1;
        }
        if (g_service && g_service(g_service_opaque)) return 0;
        Delay(1);                          /* ~20 ms; stay responsive to ESC */
    }
}

/* Poll until the outstanding request completes, servicing playback but
 * IGNORING the service callback's abort request - and never giving up.
 * Used only by hls_fetch_stop(). Two things make "give up after N seconds
 * and proceed anyway" (an earlier version of this function) unsafe here,
 * not just impolite:
 *
 *   1. Re-sending into g_req while the worker might still own the previous
 *      message (between its GetMsg and ReplyMsg) is genuinely unsafe, so
 *      teardown cannot take "the caller wants to abort" as licence to skip
 *      the wait.
 *   2. This worker task was created with CreateNewProcTags() pointing
 *      straight at a function in this process's own already-loaded code -
 *      it is not a separately LoadSeg'd program with its own seglist
 *      reference count (see audio_paula.c's audio_worker_entry for the same
 *      pattern). If this process's own segment is ever unloaded - which
 *      happens once mrplay_exit() lets main() return - while this task is
 *      still alive, that task's own program counter is pointing into memory
 *      that no longer belongs to it. "Abandon it and free what we can"
 *      (an earlier version's approach: DeleteMsgPort() the reply port,
 *      mark it inactive, return) does not defuse that: the task is still
 *      running, will still try to ReplyMsg() into a port that may since
 *      have been freed and reused, and remains a landmine for whatever runs
 *      next regardless. There is no safe partial teardown - only actually
 *      waiting for the worker's own acknowledgement is safe, matching
 *      audio_paula.c's audio_close(), which never gives up on its worker
 *      either.
 *
 * In practice this is bounded by the underlying stack's own timeouts even
 * though nothing here enforces one: connect_with_timeout() bounds connect()
 * itself (core/mr_http.c), and SO_RCVTIMEO/SO_SNDTIMEO bound recv()/send().
 * The one true residual is DNS (gethostbyname()), which this project cannot
 * bound - see connect_socket()'s note. A genuinely wedged DNS lookup means
 * "Stop" waits it out rather than hangs the emulator/machine; there is no
 * safe alternative on AmigaOS. */
static void hls_fetch_wait_busy_forever(void)
{
    unsigned long waited = 0, last_notice = 0;
    for (;;) {
        hls_fetch_reclaim();
        if (!g_busy) return;
        if (g_service) g_service(g_service_opaque);
        Delay(1);
        waited += 20;
        if (waited - last_notice >= HLS_FETCH_STOP_NOTICE_MS) {
            last_notice = waited;
            printf("hls-fetch: still waiting for the worker to finish a "
                   "network operation (%lu ms) - not abandoning it, see "
                   "hls_fetch_wait_busy_forever()\n", waited);
        }
    }
}

/* Send `url` to the worker (non-blocking). Assumes the slot is already free
 * (g_busy and g_have_ready both false) - every caller below reclaims and
 * consumes-or-discards first. post_json NULL sends a GET-shaped fetch. */
static void hls_fetch_send(const char *url, const mr_http_options *opts,
                           const char *post_json, size_t max_size)
{
    strcpy(g_req.url, url);
    if (opts) { g_req.opts = *opts; g_req.has_opts = 1; }
    else g_req.has_opts = 0;
    if (post_json) {
        strncpy(g_req.post_json, post_json, sizeof g_req.post_json - 1);
        g_req.post_json[sizeof g_req.post_json - 1] = 0;
        g_req.is_post = 1;
    } else {
        g_req.post_json[0] = 0;
        g_req.is_post = 0;
    }
    g_req.max_size = max_size;
    g_req.generation = g_generation;
    g_req.quit = 0;
    g_busy = 1;
    PutMsg(g_worker_port, &g_req.msg);
}

/* ---- mr_http backend callbacks (main task) ------------------------------- */

static int hls_fetch_override(const char *url, const mr_http_options *options,
                              const char *post_json, unsigned char **out,
                              size_t *out_len, size_t max_size)
{
    int r;
    if (out) *out = NULL;
    if (out_len) *out_len = 0;
    if (!g_active || !url) return 0;

    hls_fetch_reclaim();
    r = hls_fetch_take_ready(url, out, out_len);
    if (r >= 0) { g_hits++; return r; }
    hls_fetch_free_ready();                /* stale or mismatched: drop it  */

    if (g_busy) {
        if (!hls_fetch_wait_busy()) return 0;      /* abort requested        */
        r = hls_fetch_take_ready(url, out, out_len);
        if (r >= 0) { g_hits++; return r; }        /* it was already fetching
                                                     * exactly this - a hit  */
        hls_fetch_free_ready();
    }

    g_misses++;
    if (strlen(url) >= sizeof g_req.url) {
        mr_source_set_error("HLS URL too long for the fetch worker");
        return 0;
    }
    if (!max_size || max_size == (size_t)-1) max_size = HLS_FETCH_HINT_MAX;
    hls_fetch_send(url, options, post_json, max_size);
    if (!hls_fetch_wait_busy()) return 0;
    r = hls_fetch_take_ready(url, out, out_len);
    if (r >= 0) return r;
    hls_fetch_free_ready();                /* can't happen, but stay safe   */
    return 0;
}

static void hls_fetch_hint(const char *url, const mr_http_options *options)
{
    if (!g_active || !url) return;
    hls_fetch_reclaim();
    if (g_busy || g_have_ready) return;    /* one segment of lookahead only */
    if (strlen(url) >= sizeof g_req.url) return;
    hls_fetch_send(url, options, NULL, HLS_FETCH_HINT_MAX);
}

/* ---- lifecycle (main task) ------------------------------------------------ */

int hls_fetch_active(void)
{
    return g_active;
}

void hls_fetch_set_service(hls_fetch_service_fn fn, void *opaque)
{
    g_service = fn;
    g_service_opaque = opaque;
}

void hls_fetch_cancel(void)
{
    g_generation++;
}

void hls_fetch_stats(unsigned long *hits, unsigned long *misses,
                     unsigned long *worst_wait_ms)
{
    if (hits) *hits = g_hits;
    if (misses) *misses = g_misses;
    if (worst_wait_ms) *worst_wait_ms = g_worst_wait_ms;
}

int hls_fetch_start(int verbose)
{
    if (g_active) return 1;
    g_verbose = verbose;
    g_generation = 0;
    g_busy = 0; g_have_ready = 0; g_ready_buf = NULL;
    g_hits = g_misses = g_worst_wait_ms = 0;
    g_worker_port = NULL;

    g_reply_port = CreateMsgPort();
    if (!g_reply_port) return 0;
    g_ready_msg.mn_Node.ln_Type = NT_MESSAGE;
    g_ready_msg.mn_ReplyPort = NULL;
    g_ready_msg.mn_Length = sizeof g_ready_msg;
    g_req.msg.mn_Node.ln_Type = NT_MESSAGE;
    g_req.msg.mn_ReplyPort = g_reply_port;
    g_req.msg.mn_Length = sizeof g_req;

    g_proc = CreateNewProcTags(
        NP_Entry, (ULONG)hls_fetch_worker,
        NP_Name, (ULONG)"MintVID HLS fetch",
        NP_StackSize, HLS_FETCH_STACK,
        NP_Priority, 0,
        TAG_END);
    if (!g_proc) {
        DeleteMsgPort(g_reply_port);
        g_reply_port = NULL;
        return 0;
    }
    /* One-time startup handshake: safe to block here (raw WaitPort, the one
     * place in this file that isn't a serviced poll) because no playback
     * state exists yet for anything to stay responsive for, and the
     * worker's first action is just CreateMsgPort()+PutMsg - no network I/O
     * is possible before this returns. */
    WaitPort(g_reply_port);
    GetMsg(g_reply_port);
    if (!g_worker_port) {                  /* worker failed and exited       */
        DeleteMsgPort(g_reply_port);
        g_reply_port = NULL;
        g_proc = NULL;
        return 0;
    }
    g_active = 1;
    mr_http_set_fetch_override(hls_fetch_override);
    mr_http_set_prefetch_hint(hls_fetch_hint);
    return 1;
}

void hls_fetch_stop(void)
{
    hls_fetch_cancel();
    if (!g_active) {
        if (g_reply_port) { DeleteMsgPort(g_reply_port); g_reply_port = NULL; }
        return;
    }
    mr_http_set_fetch_override(NULL);
    mr_http_set_prefetch_hint(NULL);

    hls_fetch_reclaim();
    if (g_busy) hls_fetch_wait_busy_forever();
    hls_fetch_free_ready();

    g_req.quit = 1;
    g_busy = 1;
    PutMsg(g_worker_port, &g_req.msg);
    hls_fetch_wait_busy_forever();

    g_active = 0;
    g_proc = NULL;
    hls_fetch_free_ready();
    DeleteMsgPort(g_reply_port);
    g_reply_port = NULL;
}
