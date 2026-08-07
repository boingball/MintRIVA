/*
 * MintRIVA - HLS segment prefetch worker (Amiga).
 *
 * See hls_prefetch.h for the contract. Design notes:
 *
 * - A single background process (the worker) performs *all* HTTP: playlists and
 *   segments. It processes one request at a time, so only ever one connection
 *   is live - which is what the socket/TLS layer requires. The main task never
 *   opens a connection; it only reads already-downloaded bytes from RAM, so a
 *   blocking fetch cannot stall presentation.
 *
 * - One request is ever in flight (pf_req, reused). One segment of lookahead is
 *   kept: opening segment i hints the next segment i+1, which downloads while i
 *   plays and is usually ready by the time i+1 is needed. A miss (or a playlist,
 *   which is never prefetched) falls back to a blocking priority download - i.e.
 *   today's behaviour for that one fetch, never worse.
 *
 * - All allocation on the worker's path uses exec AllocVec/FreeVec (task-safe);
 *   the download buffer's ownership passes to a memory-backed mr_source that the
 *   main task frees on close. bsdsocket/AmiSSL are opened lazily by the worker
 *   in its own task and released by it (mr_http_net_shutdown) before it exits.
 *
 * - All backend callbacks (pf_backend_*) run on the MAIN task; the worker only
 *   ever writes pf_req's result fields, and only between GetMsg and ReplyMsg, so
 *   the message hand-off is the whole synchronisation - no shared field is
 *   touched by both tasks at once.
 */
#include "hls_prefetch.h"

#include "../core/mr_hls.h"
#include "../core/mr_http.h"
#include "../core/mr_source.h"

#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

#define PF_URL_MAX 1300           /* >= the longest HLS URL we resolve        */
#define PF_STACK   300000UL       /* HTTPS/AmiSSL needs a deep stack           */
#define PF_SEGMENT_MAX (24UL * 1024 * 1024) /* refuse absurd segment lengths   */
#define PF_MEM_FLOOR   (12UL * 1024 * 1024) /* keep this much fast RAM spare    */

/* main <-> worker request (reused; one in flight at a time) */
typedef struct {
    struct Message  msg;
    char            url[PF_URL_MAX];
    mr_http_options opts;
    int             has_opts;
    int             quit;
    /* result, written by the worker before ReplyMsg */
    void           *buf;          /* AllocVec'd; ownership passes to the caller */
    size_t          len;
    int             ok;
} pf_request;

static struct Process *pf_proc;
static struct MsgPort *pf_worker_port;    /* worker waits here (worker owns)   */
static struct MsgPort *pf_reply_port;     /* main waits here (main owns)       */
static struct Message  pf_ready_msg;      /* worker -> main "I'm up"           */
static pf_request      pf_req;
static int             pf_busy;           /* pf_req sent, reply not reclaimed  */
static char            pf_busy_url[PF_URL_MAX];
static int             pf_ready;          /* a completed download is cached    */
static char            pf_ready_url[PF_URL_MAX];
static void           *pf_ready_buf;
static size_t          pf_ready_len;
static int             pf_ready_ok;
static int             pf_active;
static int             pf_verbose;      /* log segment sizes / free RAM         */

/* ---- memory-backed source over a downloaded segment (main task) --------- */

typedef struct { unsigned char *buf; size_t len; } pf_mem;

static int pf_mem_read(void *ctx, size_t off, void *dst, size_t n)
{
    pf_mem *m = (pf_mem *)ctx;
    if (off > m->len || n > m->len - off) return 0;
    memcpy(dst, m->buf + off, n);
    return 1;
}

static void pf_mem_close(void *ctx)
{
    pf_mem *m = (pf_mem *)ctx;
    if (!m) return;
    if (m->buf) FreeVec(m->buf);
    FreeVec(m);
}

/* Wrap a downloaded RAM buffer as an mr_source, taking ownership of `buf`. */
static mr_source *pf_mem_source(void *buf, size_t len, const char *url)
{
    pf_mem *m;
    mr_source *s;
    if (!buf || !len) { if (buf) FreeVec(buf); return NULL; }
    m = (pf_mem *)AllocVec(sizeof *m, MEMF_ANY);
    if (!m) { FreeVec(buf); return NULL; }
    m->buf = (unsigned char *)buf;
    m->len = len;
    s = mr_source_create(m, len, pf_mem_read, pf_mem_close, url);
    /* On failure mr_source_create already called pf_mem_close(m), which frees
     * both m and buf - nothing more to release here. */
    return s;
}

/* ---- worker task -------------------------------------------------------- */

/* Download the whole resource at `url` into a fresh AllocVec buffer. Runs on
 * the worker task; returns 1 with *out_buf/*out_len set on success. */
static int pf_download(const char *url, const mr_http_options *opts,
                       void **out_buf, size_t *out_len)
{
    mr_source *s;
    size_t len;
    unsigned char *buf;
    *out_buf = NULL;
    *out_len = 0;
    s = mr_http_source_open_ex(url, opts);
    if (!s) return 0;
    len = mr_source_length(s);
    if (!len || len == MR_SOURCE_LEN_UNKNOWN) { mr_source_close(s); return 0; }
    /* Refuse a segment that is implausibly large (a mis-parsed length or a live
     * endpoint that never ends), and never eat the last of fast RAM - either
     * would lock the machine. A refused prefetch just means no lookahead; a
     * refused priority open makes mr_hls treat the segment as unavailable. */
    if (len > PF_SEGMENT_MAX ||
        AvailMem(MEMF_ANY) < (ULONG)len + PF_MEM_FLOOR) {
        mr_source_close(s);
        return 0;
    }
    buf = (unsigned char *)AllocVec(len, MEMF_ANY);
    if (!buf) { mr_source_close(s); return 0; }
    if (!mr_source_read_at(s, 0, buf, len)) {
        FreeVec(buf);
        mr_source_close(s);
        return 0;
    }
    mr_source_close(s);
    *out_buf = buf;
    *out_len = len;
    return 1;
}

static void pf_worker(void)
{
    pf_request *r, *quit_req = NULL;
    /* Create the receive port in this task's context (its signal belongs to us)
     * and tell main we are up - whether or not the port was created. */
    pf_worker_port = CreateMsgPort();
    PutMsg(pf_reply_port, &pf_ready_msg);
    if (!pf_worker_port) return;              /* main sees NULL and gives up   */

    for (;;) {
        WaitPort(pf_worker_port);
        while ((r = (pf_request *)GetMsg(pf_worker_port)) != NULL) {
            if (r->quit) { quit_req = r; goto done; }
            r->buf = NULL; r->len = 0;
            r->ok = pf_download(r->url, r->has_opts ? &r->opts : NULL,
                                &r->buf, &r->len);
            ReplyMsg((struct Message *)r);
        }
    }
done:
    /* Do ALL teardown before acknowledging the quit: once main sees the reply it
     * proceeds to exit and frees this program's code segment, so after the reply
     * we must not run any more of it. Release the socket/TLS state we opened
     * (from this task, as bsdsocket/AmiSSL require) and delete our port. */
    mr_http_net_shutdown();
    DeleteMsgPort(pf_worker_port);
    pf_worker_port = NULL;
    /* Forbid() before the reply so main - woken by it - cannot run (and free the
     * segment) until dos has fully removed this task: the process exit runs under
     * this Forbid, and only its final RemTask/Permit switches back to main, by
     * which point this task no longer touches the segment. */
    Forbid();
    ReplyMsg((struct Message *)quit_req);
}

/* ---- main-task helpers -------------------------------------------------- */

/* Collect any completed reply into the ready cache. Non-blocking. */
static void pf_reclaim(void)
{
    struct Message *m;
    while ((m = GetMsg(pf_reply_port)) != NULL) {
        if (m != &pf_req.msg) continue;                /* not a download reply */
        pf_busy = 0;
        if (pf_ready && pf_ready_buf) FreeVec(pf_ready_buf);
        pf_ready = 1;
        strcpy(pf_ready_url, pf_busy_url);
        pf_ready_buf = pf_req.buf;
        pf_ready_len = pf_req.len;
        pf_ready_ok  = pf_req.ok;
        if (pf_verbose)
            printf("prefetch: seg %s %lu KB, fast RAM %lu KB free\n",
                   pf_ready_ok ? "ready" : "failed",
                   (unsigned long)(pf_ready_len / 1024),
                   (unsigned long)(AvailMem(MEMF_FAST) / 1024));
    }
}

/* Wait for and reclaim the in-flight request, then drop any cached buffer. */
static void pf_discard_pending(void)
{
    pf_reclaim();
    if (pf_busy) { WaitPort(pf_reply_port); pf_reclaim(); }
    if (pf_ready) {
        if (pf_ready_buf) FreeVec(pf_ready_buf);
        pf_ready_buf = NULL;
        pf_ready = 0;
    }
}

/* Send `url` to the worker (non-blocking); one request in flight at a time. */
static void pf_send(const char *url, const mr_http_options *opts)
{
    size_t n = strlen(url);
    if (n >= sizeof pf_req.url) return;        /* URL too long: skip (falls to  */
                                               /* a later blocking open)        */
    memcpy(pf_req.url, url, n + 1);
    if (opts) { pf_req.opts = *opts; pf_req.has_opts = 1; }
    else pf_req.has_opts = 0;
    pf_req.quit = 0;
    memcpy(pf_busy_url, pf_req.url, n + 1);
    pf_busy = 1;
    PutMsg(pf_worker_port, &pf_req.msg);
}

/* If the ready cache holds `url`, hand it out as a source and clear the cache. */
static mr_source *pf_take_ready(const char *url)
{
    mr_source *s;
    if (!(pf_ready && pf_ready_ok && !strcmp(pf_ready_url, url))) return NULL;
    s = pf_mem_source(pf_ready_buf, pf_ready_len, url);
    pf_ready = 0;
    pf_ready_buf = NULL;                        /* ownership moved (or freed)    */
    return s;
}

/* ---- mr_hls backend callbacks (main task) ------------------------------- */

static mr_source *pf_backend_open(const char *url, const mr_http_options *opts,
                                  int is_segment)
{
    mr_source *s;
    pf_reclaim();
    /* 1. Already prefetched? */
    if (is_segment) { s = pf_take_ready(url); if (s) return s; }
    /* 2. In flight for exactly this URL: wait for it. */
    if (pf_busy && !strcmp(pf_busy_url, url)) {
        WaitPort(pf_reply_port);
        pf_reclaim();
        s = pf_take_ready(url);
        if (s) return s;                       /* else it failed: retry below   */
    }
    /* 3. Nothing usable pending: blocking priority download. */
    pf_discard_pending();
    pf_send(url, opts);
    WaitPort(pf_reply_port);
    pf_reclaim();
    s = pf_take_ready(url);
    if (s) return s;
    if (pf_ready) {                            /* failed reply left cached      */
        if (pf_ready_buf) FreeVec(pf_ready_buf);
        pf_ready_buf = NULL;
        pf_ready = 0;
    }
    return NULL;
}

static void pf_backend_prefetch(const char *url, const mr_http_options *opts)
{
    pf_reclaim();
    if (pf_busy || pf_ready) return;           /* one segment of lookahead      */
    if (strlen(url) >= sizeof pf_req.url) return;
    pf_send(url, opts);
}

/* ---- lifecycle (main task) ---------------------------------------------- */

int hls_prefetch_start(int verbose)
{
    if (pf_active) return 1;
    pf_verbose = verbose;
    pf_reply_port = CreateMsgPort();
    if (!pf_reply_port) return 0;
    pf_ready_msg.mn_Node.ln_Type = NT_MESSAGE;
    pf_ready_msg.mn_ReplyPort = NULL;
    pf_ready_msg.mn_Length = sizeof pf_ready_msg;
    pf_req.msg.mn_Node.ln_Type = NT_MESSAGE;
    pf_req.msg.mn_ReplyPort = pf_reply_port;
    pf_req.msg.mn_Length = sizeof pf_req;
    pf_busy = pf_ready = 0;
    pf_ready_buf = NULL;
    pf_worker_port = NULL;
    pf_proc = CreateNewProcTags(
        NP_Entry, (ULONG)pf_worker,
        NP_Name, (ULONG) "MintRIVA prefetch",
        NP_StackSize, PF_STACK,
        NP_Priority, 0,
        TAG_END);
    if (!pf_proc) {
        DeleteMsgPort(pf_reply_port);
        pf_reply_port = NULL;
        return 0;
    }
    /* Wait for the worker's ready message, then confirm its port came up. */
    WaitPort(pf_reply_port);
    GetMsg(pf_reply_port);
    if (!pf_worker_port) {                      /* worker failed and exited      */
        DeleteMsgPort(pf_reply_port);
        pf_reply_port = NULL;
        pf_proc = NULL;
        return 0;
    }
    pf_active = 1;
    mr_hls_set_backend(pf_backend_open, pf_backend_prefetch);
    return 1;
}

void hls_prefetch_stop(void)
{
    if (!pf_active) {
        if (pf_reply_port) { DeleteMsgPort(pf_reply_port); pf_reply_port = NULL; }
        return;
    }
    /* Stop new requests reaching us, drain what is pending, then quit the
     * worker and wait for its acknowledgement. */
    mr_hls_set_backend(NULL, NULL);
    pf_discard_pending();
    pf_req.quit = 1;
    pf_busy_url[0] = 0;
    pf_busy = 1;                                /* quit reply reclaimed below    */
    PutMsg(pf_worker_port, &pf_req.msg);
    WaitPort(pf_reply_port);
    GetMsg(pf_reply_port);                      /* quit reply                    */
    pf_req.quit = 0;
    pf_busy = 0;
    pf_active = 0;
    pf_proc = NULL;                             /* worker tears itself down      */
    if (pf_ready) {
        if (pf_ready_buf) FreeVec(pf_ready_buf);
        pf_ready_buf = NULL;
        pf_ready = 0;
    }
    DeleteMsgPort(pf_reply_port);
    pf_reply_port = NULL;
}
