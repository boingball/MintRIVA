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

int        mr_source_is_url(const char *path);
mr_source *mr_source_open(const char *path);
int        mr_source_read_at(mr_source *s, size_t off, void *dst, size_t len);
size_t     mr_source_length(const mr_source *s);
const char *mr_source_final_name(const mr_source *s);
void       mr_source_close(mr_source *s);

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

#endif /* MR_SOURCE_H */
