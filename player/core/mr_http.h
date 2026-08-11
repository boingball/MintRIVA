/*
 * MintRIVA - HTTP/HTTPS media source internals.
 */
#ifndef MR_HTTP_H
#define MR_HTTP_H

#if (((defined(AMIGA_M68K) && !defined(MR_HOST_BUILD)) || \
      defined(__amigaos__) || defined(__AMIGA__))) && \
    defined(__GNUC__)
/*
 * The classic Amiga SDK inline stubs use unsigned-char CONST_STRPTR names and
 * non-const STRPTR/APTR parameters for several read-only socket arguments.
 * GCC consequently reports pointer-sign and discarded-qualifier warnings at
 * otherwise valid OpenLibrary(), gethostbyname(), setsockopt() and send()
 * calls. Keep those SDK-only diagnostics scoped to this HTTP translation unit
 * rather than weakening the warning policy for the complete Amiga build.
 */
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#endif

#include "mr_source.h"
#include <stddef.h>

#define MR_HTTP_URL_MAX  4096
#define MR_HTTP_PATH_MAX 3840
#define MR_HTTP_USER_AGENT_MAX 256
#define MR_HTTP_REFERER_MAX 1024

typedef struct mr_http_options {
    char user_agent[MR_HTTP_USER_AGENT_MAX];
    char referer[MR_HTTP_REFERER_MAX];
    int hls_low;
    /* For slow live-start resolvers, retain only this many newest segments
     * from the first sliding playlist. Zero preserves the complete window. */
    unsigned hls_live_start_segments;
    /* Download each HLS segment to a bounded RAM buffer before exposing it to
     * the demuxer. Required by CDNs that use chunked transfer without length. */
    int hls_buffer_segments;
    unsigned hls_max_width;
    unsigned hls_max_height;
    unsigned hls_max_fps;
} mr_http_options;

int mr_http_options_init(mr_http_options *options, const char *user_agent,
                         const char *referer);

mr_source *mr_http_source_open(const char *url);
mr_source *mr_http_source_open_ex(const char *url,
                                  const mr_http_options *options);

/* Download a complete text response into a task-safe allocated buffer. The
 * caller owns *out and releases it with mr_free(). One NUL byte is appended but
 * is not included in *out_len. Both fixed-length and chunked bodies work. */
int mr_http_fetch_text(const char *url, const mr_http_options *options,
                       char **out, size_t *out_len, size_t max_size);

/* Binary-safe complete-response variant. The returned task-safe buffer belongs
 * to the caller and must be released with mr_free(). */
int mr_http_fetch_buffer(const char *url, const mr_http_options *options,
                         unsigned char **out, size_t *out_len,
                         size_t max_size);

/* POST a small JSON document and return the complete text response. Used by
 * lightweight resolver APIs that cannot be represented as a media GET. */
int mr_http_post_json(const char *url, const mr_http_options *options,
                      const char *json, char **out, size_t *out_len,
                      size_t max_size);

/* Download a complete response to a file using the shared redirect, TLS,
 * timeout and chunk decoder. The destination is removed on failure. */
int mr_http_download_file(const char *url, const char *path, size_t max_size);

/* Resolve a possibly-relative URL (an HLS variant/segment) against a base URL.
 * Handles absolute, scheme-relative (//host), root-relative (/path) and
 * directory-relative forms. Returns 1 on success. */
int mr_http_resolve_url(const char *base_url, const char *rel,
                        char *out, size_t out_size);

/* Release the process-wide socket/TLS state from the CURRENT task. bsdsocket
 * must be closed by the task that opened it, so a background task that performed
 * all the networking calls this before exiting; the atexit shutdown then no-ops.
 * Idempotent. */
void mr_http_net_shutdown(void);

/* Non-zero once an unhealthy TLS drop has disabled HTTPS for the rest of this
 * process (see mr_http.c). A live player uses this to stop retrying a reconnect
 * that can never succeed and end cleanly instead. Always zero on non-TLS
 * builds. */
int mr_http_tls_disabled(void);

/* Cooperative service hook, called between the individual socket reads that make
 * up a body fetch. A single-threaded player installs its audio/video service
 * pump here so it keeps presenting already-decoded frames and feeding audio
 * while an HLS segment downloads, instead of freezing for the whole (blocking)
 * read. Pass NULL to disable (the default; host builds install none). */
/* Return non-zero to interrupt the current body read.  This lets a foreground
 * player honour ESC/Close while the socket itself is idle, instead of waiting
 * for the network timeout before it can begin normal teardown. */
typedef int (*mr_http_service_fn)(void *opaque);
void mr_http_set_service(mr_http_service_fn fn, void *opaque);

#endif /* MR_HTTP_H */
