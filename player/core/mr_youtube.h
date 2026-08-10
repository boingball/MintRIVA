/*
 * MintRIVA - minimal YouTube Live resolver.
 *
 * This is deliberately not a general YouTube extractor.  Public live watch
 * pages commonly expose a signed HLS master URL in hlsManifestUrl; resolving
 * that one field is enough to hand the stream to MintRIVA's existing HLS path.
 */
#ifndef MR_YOUTUBE_H
#define MR_YOUTUBE_H

#include <stddef.h>

struct mr_http_options;

/* True only for HTTP(S) URLs on YouTube's own watch/share hosts. */
int mr_youtube_is_url(const char *url);
int mr_youtube_extract_video_id(const char *url, char out[12]);

/* Build the HTTP identity used throughout a YouTube request chain. Browser
 * defaults are supplied when the caller left MintRIVA's generic UA/referer in
 * place; explicit caller values and all HLS quality limits are preserved. */
int mr_youtube_http_options_init(struct mr_http_options *out,
                                 const struct mr_http_options *base);

/* Extract a YouTube HLS manifest from already-downloaded watch-page HTML.
 * Exposed for deterministic host tests. */
int mr_youtube_extract_live_manifest(const char *html, char *out,
                                     size_t out_size);

/* Name of the source/client used by the most recent successful resolution.
 * Empty when no manifest has been accepted. */
const char *mr_youtube_last_client(void);

/* Download a public YouTube watch page and resolve its signed live manifest. */
int mr_youtube_resolve_live(const char *url,
                            const struct mr_http_options *options,
                            char *out, size_t out_size);

#endif /* MR_YOUTUBE_H */
