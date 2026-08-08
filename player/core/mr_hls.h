/*
 * MintRIVA - HLS (.m3u8) playlist source.
 *
 * Presents an HLS media playlist as one forward-only MPEG-TS byte stream:
 * the segments, concatenated, are exactly the transport stream the existing
 * mr_ts demuxer already consumes. Master playlists are resolved to a single
 * variant. Both VOD and live playlists play (live re-fetches the playlist to
 * follow new segments); encrypted (EXT-X-KEY) playlists are rejected cleanly.
 */
#ifndef MR_HLS_H
#define MR_HLS_H

#include "mr_source.h"
struct mr_http_options;

/* True if the URL looks like an HLS playlist (…\.m3u8[?…]). */
int        mr_source_is_hls(const char *url);

/* Open an HLS playlist URL as a streaming MPEG-TS source. */
mr_source *mr_hls_source_open(const char *url);
mr_source *mr_hls_source_open_ex(const char *url,
                                 const struct mr_http_options *options);

/* Optional pluggable network backend. When installed, mr_hls routes every
 * playlist/segment open through `open_fn` (is_segment distinguishes them) and
 * calls `prefetch_fn` to hint the next segment. A platform uses this to funnel
 * all HTTP through one background worker (single live connection) and prefetch
 * segments into RAM ahead of the reader. Pass NULLs to restore the default
 * synchronous behaviour. Host builds never install one. */
typedef mr_source *(*mr_hls_open_fn)(const char *url,
                                     const struct mr_http_options *options,
                                     int is_segment);
typedef void (*mr_hls_prefetch_fn)(const char *url,
                                   const struct mr_http_options *options);
void mr_hls_set_backend(mr_hls_open_fn open_fn, mr_hls_prefetch_fn prefetch_fn);

/* Optional wait hook, called between live-playlist re-fetches. When playback
 * reaches the live edge the reader must poll the playlist for new segments;
 * without a wait it hammers the server and (single-threaded) freezes the caller
 * for the whole poll. The hook should pause about `wait_ms` while keeping the
 * caller responsive - servicing audio/video/UI - and return nonzero to abort
 * (the user asked to quit). Pass NULL to restore the plain uninterruptible
 * behaviour. Host builds install none. */
typedef int (*mr_hls_wait_fn)(void *opaque, unsigned wait_ms);
void mr_hls_set_wait(mr_hls_wait_fn fn, void *opaque);

#endif /* MR_HLS_H */
