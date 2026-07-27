/*
 * MintRIVA - random-access compressed-media source.
 *
 * Demuxers ask for exact byte ranges without caring whether the bytes come
 * from a local stdio file or a buffered HTTP/HTTPS response.
 */
#ifndef MR_SOURCE_H
#define MR_SOURCE_H

#include "mr_types.h"

typedef struct mr_source mr_source;

typedef struct mr_source_timing {
    unsigned long read_ms;          /* all source reads (disk or network) */
    unsigned long network_ms;       /* HTTP reads/open waits */
    unsigned long hls_segment_ms;   /* HLS segment open/read waits */
    unsigned long hls_playlist_ms;  /* HLS playlist fetch/refetch waits */
    unsigned long hls_segments;
    unsigned long net_buffer_used;
    unsigned long net_buffer_capacity;
    unsigned long net_buffer_high_water;
    unsigned long net_buffer_low_water;
    unsigned long producer_wait_ms;
    unsigned long consumer_starved;
    unsigned long starved_total_ms;
    unsigned long downloaded_bytes;
    unsigned long consumed_bytes;
} mr_source_timing;
struct mr_http_options;

/* Sentinel length for a forward-only (non-seekable) stream whose total size
 * is unknown up front — e.g. length-less chunked HTTP media. Callers must not
 * treat it as a real byte count; use mr_source_is_streaming() to detect it. */
#define MR_SOURCE_LEN_UNKNOWN ((size_t)-1)

int        mr_source_is_url(const char *path);
mr_source *mr_source_open(const char *path);
mr_source *mr_source_open_ex(const char *path,
                             const struct mr_http_options *options);
int        mr_source_read_at(mr_source *s, size_t off, void *dst, size_t len);
size_t     mr_source_length(const mr_source *s);
int        mr_source_is_streaming(const mr_source *s);
const char *mr_source_final_name(const mr_source *s);
void       mr_source_close(mr_source *s);
void       mr_source_abort(mr_source *s);

/* Process-local cumulative blocking-I/O counters.  They are intentionally
 * queried by the player only at rolling-report boundaries, never per frame. */
void       mr_source_timing_get(mr_source_timing *timing);
void       mr_source_timing_reset(void);
void       mr_source_timing_add_network(unsigned long ms);
void       mr_source_mark_network(mr_source *s);
/* Takes ownership of upstream. On Amiga this starts a bounded asynchronous
 * producer task; host validation builds retain the direct source. */
mr_source *mr_buffered_source_open(mr_source *upstream, size_t capacity);
void       mr_source_timing_add_hls_segment(unsigned long ms);
void       mr_source_timing_add_hls_playlist(unsigned long ms);

/* Last source-open diagnostic. Borrowed static text; intended for the
 * single-threaded CLI/player startup path. */
const char *mr_source_last_error(void);

/* HTTP backend constructor, used by mr_source_open(). */
mr_source *mr_http_source_open(const char *url);

/* Backend helper: allocate a source around callbacks and take ownership of
 * ctx. final_name is copied for diagnostics. */
mr_source *mr_source_create(void *ctx, size_t len,
                            int (*read_at)(void *, size_t, void *, size_t),
                            void (*close)(void *),
                            const char *final_name);
void       mr_source_set_error(const char *message);
void       mr_source_set_abort(mr_source *s, void (*abort_cb)(void *));

#endif /* MR_SOURCE_H */
