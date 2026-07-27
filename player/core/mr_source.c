/*
 * MintRIVA - local-file and generic source ownership.
 */
#include "mr_source.h"
#include "mr_hls.h"
#include "mr_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef AMIGA_M68K
#include <exec/tasks.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>
#endif

#define MR_SOURCE_NAME_MAX 1024
#define MR_SOURCE_ERROR_MAX 192

struct mr_source {
    void   *ctx;
    size_t  len;
    int   (*read_at)(void *, size_t, void *, size_t);
    void  (*close)(void *);
    void  (*abort_cb)(void *);
    char    final_name[MR_SOURCE_NAME_MAX];
    int     network;
};

typedef struct {
    FILE   *file;
    size_t  pos;
    int     pos_valid;
} file_source;

static char g_source_error[MR_SOURCE_ERROR_MAX];
static mr_source_timing g_timing;

static unsigned long elapsed_ms(clock_t start)
{
    clock_t elapsed = clock() - start;
    return (unsigned long)((elapsed * 1000UL) / CLOCKS_PER_SEC);
}

void mr_source_timing_get(mr_source_timing *timing)
{
    if (timing) *timing = g_timing;
}
void mr_source_timing_reset(void) { memset(&g_timing, 0, sizeof g_timing); }
void mr_source_timing_add_network(unsigned long ms) { g_timing.network_ms += ms; }
void mr_source_mark_network(mr_source *s) { if (s) s->network = 1; }
void mr_source_timing_add_hls_segment(unsigned long ms)
{
    g_timing.hls_segment_ms += ms;
    g_timing.hls_segments++;
}
void mr_source_timing_add_hls_playlist(unsigned long ms) { g_timing.hls_playlist_ms += ms; }


void mr_source_set_error(const char *message)
{
    size_t n;
    if (!message) message = "source open failed";
    n = strlen(message);
    if (n >= sizeof g_source_error) n = sizeof g_source_error - 1;
    memcpy(g_source_error, message, n);
    g_source_error[n] = '\0';
}

const char *mr_source_last_error(void)
{
    return g_source_error[0] ? g_source_error : "source open failed";
}

static int starts_nocase(const char *s, const char *prefix)
{
    while (*prefix) {
        int a = (unsigned char)*s++;
        int b = (unsigned char)*prefix++;
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

int mr_source_is_url(const char *path)
{
    return path && (starts_nocase(path, "http://") ||
                    starts_nocase(path, "https://"));
}

mr_source *mr_source_create(void *ctx, size_t len,
                            int (*read_at)(void *, size_t, void *, size_t),
                            void (*close)(void *),
                            const char *final_name)
{
    mr_source *s;
    size_t n;
    /* len == MR_SOURCE_LEN_UNKNOWN marks a forward-only stream; only a truly
     * zero-length source is rejected. */
    if (!ctx || !read_at || !close || !len) return NULL;
    s = (mr_source *)calloc(1, sizeof *s);
    if (!s) {
        close(ctx);
        mr_source_set_error("not enough memory for media source");
        return NULL;
    }
    s->ctx = ctx;
    s->len = len;
    s->read_at = read_at;
    s->close = close;
    if (!final_name) final_name = "";
    n = strlen(final_name);
    if (n >= sizeof s->final_name) n = sizeof s->final_name - 1;
    memcpy(s->final_name, final_name, n);
    s->final_name[n] = '\0';
    return s;
}


void mr_source_set_abort(mr_source *s, void (*abort_cb)(void *))
{
    if (s) s->abort_cb = abort_cb;
}

void mr_source_abort(mr_source *s)
{
    if (s && s->abort_cb) s->abort_cb(s->ctx);
}

static int file_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    file_source *f = (file_source *)opaque;
    if (!f || !f->file || (!dst && len)) return 0;
    if (!f->pos_valid || f->pos != off) {
        if (off > 0x7fffffffUL || fseek(f->file, (long)off, SEEK_SET) != 0) {
            f->pos_valid = 0;
            return 0;
        }
    }
    if (len && fread(dst, 1, len, f->file) != len) {
        f->pos_valid = 0;
        return 0;
    }
    f->pos = off + len;
    f->pos_valid = 1;
    return 1;
}

static void file_close(void *opaque)
{
    file_source *f = (file_source *)opaque;
    if (!f) return;
    if (f->file) fclose(f->file);
    free(f);
}

static mr_source *open_local_file(const char *path)
{
    file_source *ctx;
    mr_source *source;
    long end;
    FILE *file = fopen(path, "rb");
    if (!file) {
        mr_source_set_error("cannot open local file");
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        mr_source_set_error("cannot determine local file size");
        return NULL;
    }
    ctx = (file_source *)calloc(1, sizeof *ctx);
    if (!ctx) {
        fclose(file);
        mr_source_set_error("not enough memory for local file");
        return NULL;
    }
    ctx->file = file;
    ctx->pos = 0;
    ctx->pos_valid = 1;
    source = mr_source_create(ctx, (size_t)end, file_read_at, file_close, path);
    return source;
}

mr_source *mr_source_open_ex(const char *path,
                             const struct mr_http_options *options)
{
    g_source_error[0] = '\0';
    if (!path || !*path) {
        mr_source_set_error("empty media path");
        return NULL;
    }
    if (mr_source_is_hls(path)) {
        mr_source *upstream = mr_hls_source_open_ex(path, options);
        return upstream ? mr_buffered_source_open(upstream, 1024UL * 1024) : NULL;
    }
    if (mr_source_is_url(path)) {
        mr_source *upstream = mr_http_source_open_ex(path, options);
        return upstream ? mr_buffered_source_open(upstream, 1024UL * 1024) : NULL;
    }
    return open_local_file(path);
}


#ifdef AMIGA_M68K
#define MR_BUFFER_MIN       (256UL * 1024)
#define MR_BUFFER_MAX       (4UL * 1024 * 1024)
#define MR_BUFFER_START     (256UL * 1024)
#define MR_BUFFER_RESUME    (128UL * 1024)
#define MR_BUFFER_CHUNK     (188UL * 64)
#define MR_BUFFER_PREFIX    (256UL * 1024)
#define MR_BUFFER_HIGH      (768UL * 1024)

typedef struct buffered_source {
    mr_source *upstream;
    unsigned char *ring, *prefix;
    size_t capacity, prefix_len, base, produced, consumer;
    size_t high_water, low_water, seek_off;
    int seek_requested;
    int eof, error, abort, starving, task_done;
    struct Task *producer, *consumer_task;
    ULONG consumer_mask, producer_mask;
    BYTE consumer_sig, producer_sig;
    uint64_t producer_wait_us, starved_us;
    uint64_t published_wait_us, published_starved_us;
    unsigned long starved_count, downloaded, consumed;
    unsigned long published_starved_count, published_downloaded,
                  published_consumed;
} buffered_source;

static size_t buffered_used(const buffered_source *b)
{
    return b->produced - b->base;
}

static void buffered_publish_stats(buffered_source *b)
{
    Forbid();
    g_timing.net_buffer_used = (unsigned long)buffered_used(b);
    g_timing.net_buffer_capacity = (unsigned long)b->capacity;
    g_timing.net_buffer_high_water = (unsigned long)b->high_water;
    g_timing.net_buffer_low_water = (unsigned long)b->low_water;
    g_timing.producer_wait_ms += (unsigned long)
        ((b->producer_wait_us - b->published_wait_us) / 1000);
    g_timing.consumer_starved += b->starved_count - b->published_starved_count;
    g_timing.starved_total_ms += (unsigned long)
        ((b->starved_us - b->published_starved_us) / 1000);
    g_timing.downloaded_bytes += b->downloaded - b->published_downloaded;
    g_timing.consumed_bytes += b->consumed - b->published_consumed;
    b->published_wait_us = b->producer_wait_us;
    b->published_starved_us = b->starved_us;
    b->published_starved_count = b->starved_count;
    b->published_downloaded = b->downloaded;
    b->published_consumed = b->consumed;
    Permit();
}

static void buffered_producer_main(void)
{
    struct Task *self = FindTask(NULL);
    buffered_source *b = (buffered_source *)self->tc_UserData;
    unsigned char chunk[MR_BUFFER_CHUNK];
    b->producer = self;
    b->producer_sig = AllocSignal(-1);
    if (b->producer_sig < 0) {
        b->error = 1; b->task_done = 1;
        Signal(b->consumer_task, b->consumer_mask);
        return;
    }
    b->producer_mask = 1UL << b->producer_sig;
    Signal(b->consumer_task, b->consumer_mask);
    while (!b->abort) {
        size_t used, free_bytes, take, pos, first;
        Forbid();
        if (b->seek_requested) {
            b->base = b->produced = b->consumer = b->seek_off;
            b->seek_requested = 0;
        }
        used = buffered_used(b);
        if (b->consumer > b->base)
            b->base = b->consumer;
        used = buffered_used(b);
        free_bytes = b->capacity - used;
        if (used >= MR_BUFFER_HIGH && b->capacity > MR_BUFFER_HIGH)
            free_bytes = 0;
        Permit();
        if (!free_bytes) {
            uint64_t began = (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC;
            Wait(b->producer_mask);
            b->producer_wait_us +=
                (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC - began;
            continue;
        }
        take = free_bytes < sizeof chunk ? free_bytes : sizeof chunk;
        if (mr_source_length(b->upstream) != MR_SOURCE_LEN_UNKNOWN) {
            size_t remain = mr_source_length(b->upstream) - b->produced;
            if (!remain) { b->eof = 1; break; }
            if (take > remain) take = remain;
        }
        if (!mr_source_read_at(b->upstream, b->produced, chunk, take)) {
            /* HLS transport streams end on 188-byte boundaries. Retry the tail
             * in packet units so an exact-read source never discards it. */
            if (take > 188 && mr_source_length(b->upstream) == MR_SOURCE_LEN_UNKNOWN) {
                take = 188;
                if (!mr_source_read_at(b->upstream, b->produced, chunk, take)) {
                    b->eof = 1; break;
                }
            } else {
                if (mr_source_length(b->upstream) == MR_SOURCE_LEN_UNKNOWN)
                    b->eof = 1;
                else
                    b->error = 1;
                break;
            }
        }
        pos = b->produced % b->capacity;
        first = take < b->capacity - pos ? take : b->capacity - pos;
        memcpy(b->ring + pos, chunk, first);
        if (take > first) memcpy(b->ring, chunk + first, take - first);
        if (b->prefix_len < MR_BUFFER_PREFIX) {
            size_t keep = take;
            if (keep > MR_BUFFER_PREFIX - b->prefix_len)
                keep = MR_BUFFER_PREFIX - b->prefix_len;
            memcpy(b->prefix + b->prefix_len, chunk, keep);
            b->prefix_len += keep;
        }
        Forbid();
        b->produced += take; b->downloaded += (unsigned long)take;
        used = buffered_used(b);
        if (used > b->high_water) b->high_water = used;
        Permit();
        Signal(b->consumer_task, b->consumer_mask);
    }
    buffered_publish_stats(b);
    FreeSignal(b->producer_sig);
    b->producer_mask = 0;
    b->task_done = 1;
    Signal(b->consumer_task, b->consumer_mask);
}

static int buffered_read_at(void *opaque, size_t off, void *dst, size_t len)
{
    buffered_source *b = (buffered_source *)opaque;
    unsigned char *out = (unsigned char *)dst;
    uint64_t starved_at = 0;
    while (len && !b->abort) {
        size_t available, take, pos, first;
        if ((off < b->base && off >= b->prefix_len) ||
            (off > b->produced &&
             mr_source_length(b->upstream) != MR_SOURCE_LEN_UNKNOWN)) {
            Forbid();
            b->seek_off = off; b->seek_requested = 1;
            Permit();
            if (b->producer_mask) Signal(b->producer, b->producer_mask);
            Wait(b->consumer_mask);
            continue;
        }
        if (off < b->base) {
            if (off >= b->prefix_len) return 0;
            take = len < b->prefix_len - off ? len : b->prefix_len - off;
            memcpy(out, b->prefix + off, take);
            out += take; off += take; len -= take; b->consumed += (unsigned long)take;
            continue;
        }
        Forbid();
        available = off < b->produced ? b->produced - off : 0;
        Permit();
        if (b->starving && available < MR_BUFFER_RESUME && !b->eof &&
            !b->error && !b->task_done) {
            Wait(b->consumer_mask);
            continue;
        }
        if (b->starving && (available >= MR_BUFFER_RESUME || b->eof ||
                            b->error || b->task_done)) {
            b->starved_us += (uint64_t)clock() * 1000000ULL /
                              CLOCKS_PER_SEC - starved_at;
            b->starving = 0;
        }
        if (!available) {
            if (b->eof || b->error || b->task_done) break;
            if (!b->starving) {
                b->starving = 1; b->starved_count++;
                starved_at = (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC;
            }
            if (b->producer_mask) Signal(b->producer, b->producer_mask);
            Wait(b->consumer_mask);
            continue;
        }
        take = len < available ? len : available;
        pos = off % b->capacity;
        first = take < b->capacity - pos ? take : b->capacity - pos;
        memcpy(out, b->ring + pos, first);
        if (take > first) memcpy(out + first, b->ring, take - first);
        out += take; off += take; len -= take;
        Forbid();
        if (off > b->consumer) b->consumer = off;
        b->consumed += (unsigned long)take;
        if (b->produced - off < b->low_water) b->low_water = b->produced - off;
        Permit();
        if (b->producer_mask) Signal(b->producer, b->producer_mask);
    }
    buffered_publish_stats(b);
    return len == 0;
}

static void buffered_close(void *opaque)
{
    buffered_source *b = (buffered_source *)opaque;
    if (!b) return;
    b->abort = 1;
    mr_source_abort(b->upstream);
    if (b->producer && !b->task_done && b->producer_mask)
        Signal(b->producer, b->producer_mask);
    while (!b->task_done) Wait(b->consumer_mask);
    mr_source_close(b->upstream);
    if (b->consumer_sig >= 0) FreeSignal(b->consumer_sig);
    free(b->prefix); free(b->ring); free(b);
}

mr_source *mr_buffered_source_open(mr_source *upstream, size_t capacity)
{
    buffered_source *b;
    mr_source *result;
    size_t length;
    const char *name;
    if (!upstream) return NULL;
    if (capacity < MR_BUFFER_MIN) capacity = MR_BUFFER_MIN;
    if (capacity > MR_BUFFER_MAX) capacity = MR_BUFFER_MAX;
    b = (buffered_source *)calloc(1, sizeof *b);
    if (!b) return upstream;
    while (capacity >= MR_BUFFER_MIN && !(b->ring = malloc(capacity)))
        capacity /= 2;
    b->prefix = (unsigned char *)malloc(MR_BUFFER_PREFIX);
    if (!b->ring || !b->prefix) {
        free(b->ring); free(b->prefix); free(b); return upstream;
    }
    b->capacity = capacity; b->low_water = capacity; b->upstream = upstream;
    b->consumer_task = FindTask(NULL);
    b->consumer_sig = AllocSignal(-1);
    if (b->consumer_sig < 0) {
        free(b->prefix); free(b->ring); free(b); return upstream;
    }
    b->consumer_mask = 1UL << b->consumer_sig;
    name = mr_source_final_name(upstream);
    length = mr_source_length(upstream);
    Forbid();
    b->producer = CreateTask((STRPTR)"MintRIVA net reader", 1,
                             (APTR)buffered_producer_main, 65536);
    if (b->producer) b->producer->tc_UserData = b;
    Permit();
    if (!b->producer) {
        FreeSignal(b->consumer_sig); free(b->prefix); free(b->ring); free(b); return upstream;
    }
    while (!b->producer_mask && !b->error) Wait(b->consumer_mask);
    if (b->error) {
        while (!b->task_done) Wait(b->consumer_mask);
        mr_source_close(upstream); FreeSignal(b->consumer_sig);
        free(b->prefix); free(b->ring); free(b); return NULL;
    }
    /* Useful startup prebuffer, bounded by a short/finite stream. */
    while (b->produced < MR_BUFFER_START && !b->eof && !b->error)
        Wait(b->consumer_mask);
    result = mr_source_create(b, length, buffered_read_at, buffered_close, name);
    if (!result) return NULL;
    mr_source_mark_network(result);
    return result;
}
#else
mr_source *mr_buffered_source_open(mr_source *upstream, size_t capacity)
{
    (void)capacity;
    return upstream;
}
#endif

mr_source *mr_source_open(const char *path)
{
    return mr_source_open_ex(path, NULL);
}

int mr_source_read_at(mr_source *s, size_t off, void *dst, size_t len)
{
    if (!s || (!dst && len)) return 0;
    /* Seekable sources are bounds-checked against their known size. A streaming
     * source has no known end, so the backend reports EOF via a short read. */
    if (s->len != MR_SOURCE_LEN_UNKNOWN &&
        (off > s->len || len > s->len - off))
        return 0;
    {
        clock_t started = clock();
        int ok = s->read_at(s->ctx, off, dst, len);
        unsigned long ms = elapsed_ms(started);
        g_timing.read_ms += ms;
        if (s->network) g_timing.network_ms += ms;
        return ok;
    }
}

size_t mr_source_length(const mr_source *s)
{
    return s ? s->len : 0;
}

int mr_source_is_streaming(const mr_source *s)
{
    return s && s->len == MR_SOURCE_LEN_UNKNOWN;
}

const char *mr_source_final_name(const mr_source *s)
{
    return s ? s->final_name : "";
}

void mr_source_close(mr_source *s)
{
    if (!s) return;
    s->close(s->ctx);
    free(s);
}
