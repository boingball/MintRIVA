/*
 * MintVID - narrow retry shim for intermittent YouTube WEB_SAFARI replies.
 *
 * YouTube can return HTTP 200 from the WEB_SAFARI player endpoint while
 * intermittently omitting hlsManifestUrl for the same video/session. Keep the
 * retry policy local to mr_youtube.c: mr_http.h includes this header only when
 * mr_youtube.h and mr_alloc.h were already included by that translation unit.
 * Generic HTTP callers are therefore unchanged.
 *
 * A useful detail of the otherwise-unusable player reply is responseContext's
 * visitorData. Real watch pages do not always expose that identity, but the
 * player endpoint commonly does. Learn it from a failed Safari response and
 * feed it back into the next POST so retries are no longer four identical,
 * completely anonymous requests. The value is kept only in this translation
 * unit and is never logged or sent to googlevideo.com.
 */
#ifndef MR_YOUTUBE_HTTP_RETRY_H
#define MR_YOUTUBE_HTTP_RETRY_H

#include <stdio.h>
#include <string.h>

#define MR_YOUTUBE_SAFARI_POST_ATTEMPTS 4
#define MR_YOUTUBE_SAFARI_VISITOR_MAX 256

static char g_mr_youtube_safari_visitor[MR_YOUTUBE_SAFARI_VISITOR_MAX];

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

/* Extract the simple ASCII visitorData strings YouTube currently returns.
 * Refuse escapes/control characters instead of trying to be a general JSON
 * parser: a rejected identity merely leaves the established fallback path. */
static inline int mr_youtube_http_extract_visitor(const char *text,
                                                  char *out, size_t out_size)
{
    static const char marker[] = "\"visitorData\":\"";
    const char *p;
    size_t used = 0;

    if (!text || !out || out_size < 2) return 0;
    p = strstr(text, marker);
    if (!p) return 0;
    p += sizeof marker - 1;
    while (*p && *p != '"') {
        unsigned char c = (unsigned char)*p++;
        if (c < 0x21 || c == 0x7f || c == '\\' || used + 1 >= out_size)
            return 0;
        out[used++] = (char)c;
    }
    if (*p != '"' || !used) return 0;
    out[used] = '\0';
    return 1;
}

static inline void mr_youtube_http_remember_visitor(const char *visitor)
{
    size_t n;
    if (!visitor || !visitor[0]) return;
    n = strlen(visitor);
    if (n >= sizeof g_mr_youtube_safari_visitor) return;
    memcpy(g_mr_youtube_safari_visitor, visitor, n + 1);
}

/* Return an allocated Safari request containing visitorData when the original
 * watch-page request did not have one. NULL means use the original JSON. */
static inline char *mr_youtube_http_json_with_visitor(const char *json,
                                                      const char *visitor)
{
    static const char marker[] = "\"clientName\":\"WEB\"";
    static const char prefix[] = "\"visitorData\":\"";
    const char *at;
    size_t before, visitor_len, json_len, add, total;
    char *out;

    if (!json || !visitor || !visitor[0] ||
        mr_youtube_http_contains(json, "\"visitorData\":"))
        return NULL;
    at = strstr(json, marker);
    if (!at) return NULL;
    before = (size_t)(at - json);
    visitor_len = strlen(visitor);
    json_len = strlen(json);
    add = (sizeof prefix - 1) + visitor_len + 2; /* closing quote + comma */
    if (json_len > 1023 || add > 1023 || json_len + add > 1023)
        return NULL; /* mr_http_post_json's own request-body ceiling */
    total = json_len + add;
    out = (char *)mr_alloc(total + 1);
    if (!out) return NULL;

    memcpy(out, json, before);
    memcpy(out + before, prefix, sizeof prefix - 1);
    memcpy(out + before + sizeof prefix - 1, visitor, visitor_len);
    out[before + sizeof prefix - 1 + visitor_len] = '"';
    out[before + sizeof prefix + visitor_len] = ',';
    memcpy(out + before + add, at, json_len - before + 1);
    return out;
}

static inline int mr_http_post_json_youtube_retry(
    const char *url, const mr_http_options *options,
    const char *json, char **out, size_t *out_len, size_t max_size)
{
    int attempt;
    int safari_request;
    char initial_visitor[MR_YOUTUBE_SAFARI_VISITOR_MAX];

    /* Match only MintVID's WEB_SAFARI Innertube request. Android, VR,
     * embedded WEB and ordinary HTTP users retain the exact old behaviour. */
    safari_request =
        mr_youtube_http_contains(json, "\"clientName\":\"WEB\"") &&
        mr_youtube_http_contains(json, "Safari/605.1.15");
    if (!safari_request || !out || !out_len)
        return mr_http_post_json(url, options, json, out, out_len, max_size);

    if (mr_youtube_http_extract_visitor(json, initial_visitor,
                                        sizeof initial_visitor))
        mr_youtube_http_remember_visitor(initial_visitor);

    *out = NULL;
    *out_len = 0;
    for (attempt = 0; attempt < MR_YOUTUBE_SAFARI_POST_ATTEMPTS; attempt++) {
        char *reply = NULL;
        char *owned_json = NULL;
        char learned_visitor[MR_YOUTUBE_SAFARI_VISITOR_MAX];
        const char *request_json = json;
        size_t reply_len = 0;
        int ok;

        if (g_mr_youtube_safari_visitor[0]) {
            owned_json = mr_youtube_http_json_with_visitor(
                json, g_mr_youtube_safari_visitor);
            if (owned_json) request_json = owned_json;
        }

        ok = mr_http_post_json(url, options, request_json,
                               &reply, &reply_len, max_size);
        if (owned_json) mr_free(owned_json);

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

            if (mr_youtube_http_extract_visitor(reply, learned_visitor,
                                                sizeof learned_visitor) &&
                strcmp(learned_visitor, g_mr_youtube_safari_visitor)) {
                mr_youtube_http_remember_visitor(learned_visitor);
                printf("YouTube: WEB_SAFARI learned player visitor identity; "
                       "retrying with session context\n");
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
 * function. Only the rest of mr_youtube.c sees the retrying name. */
#define mr_http_post_json mr_http_post_json_youtube_retry

#endif /* MR_YOUTUBE_HTTP_RETRY_H */
