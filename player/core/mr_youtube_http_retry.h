/*
 * MintVID - narrow YouTube request shim for browser-session matching and
 * intermittent WEB_SAFARI replies.
 *
 * YouTube can return HTTP 200 from the WEB_SAFARI player endpoint while
 * intermittently omitting hlsManifestUrl for the same video/session. Keep the
 * retry policy local to mr_youtube.c: mr_http.h includes this header only when
 * mr_youtube.h and mr_alloc.h were already included by that translation unit.
 * Generic HTTP callers are therefore unchanged.
 *
 * HTTP itself keeps the anonymous youtube.com cookie session alive. The watch
 * fetch below also matches the current desktop Chrome identity used by yt-dlp,
 * and VISIONOS player POSTs use yt-dlp's keyless player endpoint. No visitorData
 * synthesis is done here: when the watch page supplies a visitor identity,
 * mr_youtube.c carries it in the request JSON and mr_http.c forwards the matching
 * X-Goog-Visitor-Id header.
 */
#ifndef MR_YOUTUBE_HTTP_RETRY_H
#define MR_YOUTUBE_HTTP_RETRY_H

#define MR_YOUTUBE_SAFARI_POST_ATTEMPTS 4
#define MR_YOUTUBE_WATCH_UA \
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 " \
    "(KHTML, like Gecko) Chrome/149.0.0.0 Safari/537.36"
#define MR_YOUTUBE_VISIONOS_PLAYER_URL \
    "https://www.youtube.com/youtubei/v1/player?prettyPrint=false"

static inline int mr_youtube_http_contains(const char *text,
                                           const char *needle)
{
    if (!text || !needle || !*needle) return 0;
    while (*text) {
        const char *h = text;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return 1;
        text++;
    }
    return 0;
}

static inline int mr_youtube_http_copy_text(char *out, size_t out_size,
                                            const char *text)
{
    size_t used = 0;
    if (!out || !out_size || !text) return 0;
    while (text[used]) {
        if (used + 1 >= out_size) {
            out[0] = '\0';
            return 0;
        }
        out[used] = text[used];
        used++;
    }
    out[used] = '\0';
    return 1;
}

static inline int mr_youtube_http_is_watch_fetch(const char *url)
{
    if (!url) return 0;
    if (!mr_youtube_http_contains(url, "youtube.com/") &&
        !mr_youtube_http_contains(url, "youtu.be/"))
        return 0;
    if (mr_youtube_http_contains(url, "/youtubei/") ||
        mr_youtube_http_contains(url, "/s/player/"))
        return 0;
    return 1;
}

static inline int mr_http_fetch_text_youtube_session(
    const char *url, const mr_http_options *options,
    char **out, size_t *out_len, size_t max_size)
{
    mr_http_options watch_options;

    /* Only replace MintVID's old default Chrome 120 identity. A caller that
     * deliberately supplied its own YouTube UA keeps it unchanged. */
    if (!options || !mr_youtube_http_is_watch_fetch(url) ||
        !mr_youtube_http_contains(options->user_agent, "Chrome/120.0"))
        return mr_http_fetch_text(url, options, out, out_len, max_size);

    watch_options = *options;
    if (!mr_youtube_http_copy_text(watch_options.user_agent,
                                   sizeof watch_options.user_agent,
                                   MR_YOUTUBE_WATCH_UA))
        return mr_http_fetch_text(url, options, out, out_len, max_size);

    return mr_http_fetch_text(url, &watch_options, out, out_len, max_size);
}

static inline int mr_http_post_json_youtube_retry(
    const char *url, const mr_http_options *options,
    const char *json, char **out, size_t *out_len, size_t max_size)
{
    int attempt;
    int safari_request;
    int visionos_request;
    const char *post_url = url;

    visionos_request =
        mr_youtube_http_contains(json, "\"clientName\":\"VISIONOS\"");
    if (visionos_request && url &&
        mr_youtube_http_contains(url, "youtube.com/youtubei/v1/player"))
        post_url = MR_YOUTUBE_VISIONOS_PLAYER_URL;

    /* Match only MintVID's WEB_SAFARI Innertube request for retries. Android,
     * VR, VISIONOS, embedded WEB and ordinary HTTP users retain one attempt. */
    safari_request =
        mr_youtube_http_contains(json, "\"clientName\":\"WEB\"") &&
        mr_youtube_http_contains(json, "Safari/605.1.15");
    if (!safari_request || !out || !out_len)
        return mr_http_post_json(post_url, options, json,
                                 out, out_len, max_size);

    *out = NULL;
    *out_len = 0;
    for (attempt = 0; attempt < MR_YOUTUBE_SAFARI_POST_ATTEMPTS; attempt++) {
        char *reply = NULL;
        size_t reply_len = 0;
        int ok = mr_http_post_json(post_url, options, json,
                                   &reply, &reply_len, max_size);

        if (ok) {
            int have_hls = mr_youtube_http_contains(
                reply, "\"hlsManifestUrl\"");
            int have_direct_adaptive =
                (mr_youtube_http_contains(reply, "\"itag\":160") ||
                 mr_youtube_http_contains(reply, "\"itag\":136")) &&
                mr_youtube_http_contains(reply, "\"url\":\"https://");

            /* A manifest is the preferred Safari result. Preserve a directly
             * usable adaptive response too instead of throwing away something
             * the normal resolver can already consume. */
            if (have_hls || have_direct_adaptive ||
                attempt + 1 == MR_YOUTUBE_SAFARI_POST_ATTEMPTS) {
                *out = reply;
                *out_len = reply_len;
                return 1;
            }
            mr_free(reply);
        } else if (attempt + 1 == MR_YOUTUBE_SAFARI_POST_ATTEMPTS) {
            if (reply) mr_free(reply);
            return 0;
        } else if (reply) {
            mr_free(reply);
        }
    }

    return 0;
}

/* Defined after the wrappers so calls inside them still bind to the real HTTP
 * functions. Only the rest of mr_youtube.c sees the adjusted names. */
#define mr_http_fetch_text mr_http_fetch_text_youtube_session
#define mr_http_post_json mr_http_post_json_youtube_retry

#endif /* MR_YOUTUBE_HTTP_RETRY_H */
