/*
 * MintVID - minimal YouTube media resolver.
 *
 * This is deliberately not a general YouTube extractor. It accepts public
 * live HLS, directly usable muxed MP4s and selected H.264/AAC adaptive pairs
 * for compatible uploads.
 */
#ifndef MR_YOUTUBE_H
#define MR_YOUTUBE_H

#include <stddef.h>

struct mr_http_options;

/* Optional native n-challenge callback. mrplay installs the embedded QuickJS
 * implementation; lightweight GUI/test users of mr_youtube.c remain free of
 * the interpreter. The callback receives the downloaded current player and a
 * media URL, and must return the same URL with its n value transformed. */
typedef int (*mr_youtube_nsig_solver_fn)(const char *player_js,
                                         size_t player_js_len,
                                         const char *url,
                                         char *out, size_t out_size,
                                         void *opaque);

void mr_youtube_set_nsig_solver(mr_youtube_nsig_solver_fn solver,
                                void *opaque);

/* True only for HTTP(S) URLs on YouTube's own watch/share hosts. */
int mr_youtube_is_url(const char *url);
int mr_youtube_extract_video_id(const char *url, char out[12]);

/* Build the HTTP identity used throughout a YouTube request chain. Browser
 * defaults are supplied when the caller left MintVID's generic UA/referer in
 * place; explicit caller values and all HLS quality limits are preserved. */
int mr_youtube_http_options_init(struct mr_http_options *out,
                                 const struct mr_http_options *base);

/* Extract a YouTube HLS manifest from already-downloaded watch-page HTML.
 * Exposed for deterministic host tests. */
int mr_youtube_extract_live_manifest(const char *html, char *out,
                                     size_t out_size);

typedef enum mr_youtube_media_kind {
    MR_YOUTUBE_MEDIA_NONE = 0,
    MR_YOUTUBE_MEDIA_HLS,
    MR_YOUTUBE_MEDIA_HLS_VOD,
    MR_YOUTUBE_MEDIA_PROGRESSIVE_360P,
    MR_YOUTUBE_MEDIA_PROGRESSIVE_720P,
    MR_YOUTUBE_MEDIA_ADAPTIVE_144P,
    MR_YOUTUBE_MEDIA_ADAPTIVE_720P
} mr_youtube_media_kind;

/* Name and kind used by the most recent successful resolution. */
const char *mr_youtube_last_client(void);
mr_youtube_media_kind mr_youtube_last_media_kind(void);

/* Extract a directly playable, muxed 360p MP4 (itag 18) from a player JSON
 * response. Exposed for deterministic host tests. */
int mr_youtube_extract_progressive_360p(const char *json, char *out,
                                        size_t out_size);

/* Prefer the muxed 720p MP4 (itag 22) when requested, with an automatic
 * fallback to itag 18. Returns the selected media kind through kind. */
int mr_youtube_extract_progressive(const char *json, int prefer_720p,
                                   char *out, size_t out_size,
                                   mr_youtube_media_kind *kind);

/* Extract directly usable adaptive MP4 tracks from a player response. Low
 * selects H.264 itag 160 plus AAC itag 139/140; high selects H.264 itag 136
 * plus AAC itag 140/139. Both URLs must be signed Google Video URLs without
 * an unresolved n transform. */
int mr_youtube_extract_adaptive(const char *json, int prefer_720p,
                                char *video_out, size_t video_out_size,
                                char *audio_out, size_t audio_out_size,
                                mr_youtube_media_kind *kind);

/* Apply the media client's HTTP identity after a successful resolution. */
int mr_youtube_media_http_options_init(struct mr_http_options *out,
                                       const struct mr_http_options *base);

/* Resolve live HLS or a muxed 360p/optional 720p recorded-video URL. */
int mr_youtube_resolve_media(const char *url,
                             const struct mr_http_options *options,
                             char *out, size_t out_size,
                             mr_youtube_media_kind *kind);

/* Resolve a recorded video to either one muxed URL or separate adaptive video
 * and audio URLs. audio_out remains empty for live HLS and muxed MP4 results.
 * When a requested adaptive pair is unavailable the resolver retains the
 * established muxed 360p fallback. */
int mr_youtube_resolve_media_pair(const char *url,
                                  const struct mr_http_options *options,
                                  char *video_out, size_t video_out_size,
                                  char *audio_out, size_t audio_out_size,
                                  mr_youtube_media_kind *kind);

/* Download a public YouTube watch page and resolve its signed live manifest. */
int mr_youtube_resolve_live(const char *url,
                            const struct mr_http_options *options,
                            char *out, size_t out_size);

#endif /* MR_YOUTUBE_H */
