/*
 * MintVID - narrow retry shim for intermittent YouTube WEB_SAFARI replies.
 *
 * YouTube can return HTTP 200 from the WEB_SAFARI player endpoint while
 * intermittently omitting hlsManifestUrl for the same video/session.  Keep the
 * retry policy local to mr_youtube.c: mr_http.h includes this header only when
 * mr_youtube.h and mr_alloc.h were already included by that translation unit.
 * Generic HTTP callers are therefore unchanged.
 */
#ifndef MR_YOUTUBE_HTTP_RETRY_H
#define MR_YOUTUBE_HTTP_RETRY_H

#define MR_YOUTUBE_SAFARI_POST_ATTEMPTS 4

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

static inline int mr_http_post_json_youtube_retry(
    const char *url, const mr_http_options *options,
    const char *json, char **out, size_t *out_len, size_t max_size)
{
    int attempt;
    int safari_request;

    /* Match only MintVID's WEB_SAFARI Innertube request.  Android, VR,
     * embedded WEB and ordinary HTTP users retain the exact old behaviour. */
    safari_request =
        mr_youtube_http_contains(json, "\"clientName\":\"WEB\"") &&
        mr_youtube_http_contains(json, "Safari/605.1.15");
    if (!safari_request || !out || !out_len)
        return mr_http_post_json(url, options, json, out, out_len, max_size);

    *out = NULL;
    *out_len = 0;
    for (attempt = 0; attempt < MR_YOUTUBE_SAFARI_POST_ATTEMPTS; attempt++) {
        char *reply = NULL;
        size_t reply_len = 0;
        int ok = mr_http_post_json(url, options, json,
                                   &reply, &reply_len, max_size);

        if (ok) {
            int have_hls = mr_youtube_http_contains(
                reply, "\"hlsManifestUrl\"");
            int have_direct_adaptive =
                (mr_youtube_http_contains(reply, "\"itag\":160") ||
                 mr_youtube_http_contains(reply, "\"itag\":136")) &&
                mr_youtube_http_contains(reply, "\"url\":\"https://");

            /* A manifest is the preferred Safari result.  Preserve a directly
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

/* Defined after the wrapper so calls inside it still bind to the real HTTP
 * function.  Only the rest of mr_youtube.c sees the retrying name. */
#define mr_http_post_json mr_http_post_json_youtube_retry

#endif /* MR_YOUTUBE_HTTP_RETRY_H */
