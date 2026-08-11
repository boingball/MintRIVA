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

/* Emit concise playlist/segment startup diagnostics. Intended for mrplay's
 * --time mode so a slow CDN or incompatible rendition cannot look like a
 * silent player hang. */
void mr_hls_set_verbose(int enabled);

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
