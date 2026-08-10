/*
 * MintRIVA - native YouTube Live watch-page resolver.
 *
 * No JavaScript engine, Innertube client or signature decipher is attempted.
 * A public live watch page may contain streamingData.hlsManifestUrl; this
 * module downloads the bounded page, decodes that JSON string, and accepts it
 * only when it is an HTTPS manifest.googlevideo.com M3U8 URL.
 */
#include "mr_youtube.h"

#include "mr_alloc.h"
#include "mr_http.h"
#include "mr_source.h"

#include <stdio.h>
#include <string.h>

#define YOUTUBE_PAGE_MAX (4UL * 1024 * 1024)
#define YOUTUBE_BROWSER_UA \
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 " \
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"
#define YOUTUBE_REFERER "https://www.youtube.com/"
#define MINT_RIVA_DEFAULT_UA "MintRIVA/0.1 AmigaOS"
#define YOUTUBE_LIVE_START_SEGMENTS 2
#define YOUTUBE_ANDROID_VERSION "21.08.266"

static const char *g_last_client = "";

static int ascii_tolower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

static int ascii_ncasecmp(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = ascii_tolower((unsigned char)a[i]);
        int cb = ascii_tolower((unsigned char)b[i]);
        if (ca != cb || !ca || !cb) return ca - cb;
    }
    return 0;
}

static int host_equal(const char *host, size_t len, const char *expected)
{
    size_t i, n = strlen(expected);
    if (len != n) return 0;
    for (i = 0; i < n; i++)
        if (ascii_tolower((unsigned char)host[i]) !=
            ascii_tolower((unsigned char)expected[i])) return 0;
    return 1;
}

int mr_youtube_is_url(const char *url)
{
    const char *host, *end, *colon;
    size_t len;
    if (!url) return 0;
    if (!ascii_ncasecmp(url, "http://", 7)) host = url + 7;
    else if (!ascii_ncasecmp(url, "https://", 8)) host = url + 8;
    else return 0;
    end = host + strcspn(host, "/?#");
    colon = (const char *)memchr(host, ':', (size_t)(end - host));
    if (colon) end = colon;
    len = (size_t)(end - host);
    return host_equal(host, len, "youtube.com") ||
           host_equal(host, len, "www.youtube.com") ||
           host_equal(host, len, "m.youtube.com") ||
           host_equal(host, len, "music.youtube.com") ||
           host_equal(host, len, "youtu.be") ||
           host_equal(host, len, "www.youtube-nocookie.com");
}

static int video_id_char(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

int mr_youtube_extract_video_id(const char *url, char out[12])
{
    const char *p = NULL, *v;
    size_t i;
    if (!mr_youtube_is_url(url) || !out) return 0;
    v = strstr(url, "v=");
    while (v) {
        if (v == url || v[-1] == '?' || v[-1] == '&') { p = v + 2; break; }
        v = strstr(v + 2, "v=");
    }
    if (!p) {
        static const char *prefixes[] = { "/live/", "/embed/", "/shorts/" };
        for (i = 0; i < sizeof prefixes / sizeof prefixes[0]; i++) {
            p = strstr(url, prefixes[i]);
            if (p) { p += strlen(prefixes[i]); break; }
        }
    }
    if (!p && strstr(url, "youtu.be/"))
        p = strstr(url, "youtu.be/") + strlen("youtu.be/");
    if (!p) return 0;
    for (i = 0; i < 11; i++) {
        if (!p[i] || !video_id_char((unsigned char)p[i])) return 0;
        out[i] = p[i];
    }
    if (video_id_char((unsigned char)p[11])) return 0;
    out[11] = '\0';
    return 1;
}

int mr_youtube_http_options_init(mr_http_options *out,
                                 const mr_http_options *base)
{
    const char *ua = YOUTUBE_BROWSER_UA;
    const char *referer = YOUTUBE_REFERER;
    if (!out) {
        mr_source_set_error("invalid YouTube HTTP options");
        return 0;
    }
    if (base && base->user_agent[0] &&
        strcmp(base->user_agent, MINT_RIVA_DEFAULT_UA))
        ua = base->user_agent;
    if (base && base->referer[0]) referer = base->referer;
    if (!mr_http_options_init(out, ua, referer)) return 0;
    /* Resolving YouTube costs several HTTPS round trips. Its six-segment live
     * window can advance while that happens, so starting with its oldest item
     * commonly produces a Google Video 403 before playback even begins. */
    out->hls_live_start_segments = YOUTUBE_LIVE_START_SEGMENTS;
    out->hls_buffer_segments = 1;
    if (base) {
        out->hls_low = base->hls_low;
        out->hls_max_width = base->hls_max_width;
        out->hls_max_height = base->hls_max_height;
        out->hls_max_fps = base->hls_max_fps;
    }
    return 1;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode one JSON string. YouTube manifest URLs are ASCII; accepting only
 * ASCII \u escapes avoids needing a Unicode implementation in this tiny path. */
static int decode_json_string(const char *p, char *out, size_t out_size)
{
    size_t used = 0;
    if (!p || *p != '"' || !out || out_size < 2) return 0;
    p++;
    while (*p && *p != '"') {
        unsigned value = (unsigned char)*p++;
        if (value == '\\') {
            int a, b, c, d;
            value = (unsigned char)*p++;
            if (!value) return 0;
            if (value == 'u') {
                if (!p[0] || !p[1] || !p[2] || !p[3]) return 0;
                a = hex_value((unsigned char)p[0]);
                b = hex_value((unsigned char)p[1]);
                c = hex_value((unsigned char)p[2]);
                d = hex_value((unsigned char)p[3]);
                if (a < 0 || b < 0 || c < 0 || d < 0) return 0;
                value = (unsigned)((a << 12) | (b << 8) | (c << 4) | d);
                p += 4;
                if (!value || value > 0x7f) return 0;
            } else if (value == 'b') value = '\b';
            else if (value == 'f') value = '\f';
            else if (value == 'n') value = '\n';
            else if (value == 'r') value = '\r';
            else if (value == 't') value = '\t';
            else if (value != '"' && value != '\\' && value != '/') return 0;
        }
        if (value < 0x20 || value == 0x7f) return 0;
        if (used + 1 >= out_size) return 0;
        out[used++] = (char)value;
    }
    if (*p != '"') return 0;
    out[used] = '\0';
    return 1;
}

static int is_google_hls(const char *url)
{
    static const char prefix[] = "https://manifest.googlevideo.com/";
    return !strncmp(url, prefix, sizeof prefix - 1) && strstr(url, ".m3u8");
}

/* YouTube's /n/<value> path component is a throttling challenge, unrelated to
 * a PO token. The value must be transformed by code extracted from the current
 * player JavaScript before GVS will serve the media. This native resolver has
 * no JavaScript engine, so never hand an unsolved URL to HLS. */
static int manifest_needs_n_transform(const char *url)
{
    return url && strstr(url, "/n/") != NULL;
}

const char *mr_youtube_last_client(void)
{
    return g_last_client;
}

static int extract_config_string(const char *html, const char *name,
                                 char *out, size_t out_size)
{
    char key[80];
    const char *p;
    int n = snprintf(key, sizeof key, "\"%s\"", name);
    if (!html || !name || n <= 0 || (size_t)n >= sizeof key) return 0;
    p = strstr(html, key);
    if (!p) return 0;
    p += (size_t)n;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (*p++ != ':') return 0;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return decode_json_string(p, out, out_size);
}

int mr_youtube_extract_live_manifest(const char *html, char *out,
                                     size_t out_size)
{
    static const char key[] = "\"hlsManifestUrl\"";
    const char *p;
    if (!html || !out || !out_size) return 0;
    p = html;
    while ((p = strstr(p, key)) != NULL) {
        p += sizeof key - 1;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p != ':') {
            if (!*p) break;
            p++;
            continue;
        }
        p++;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (decode_json_string(p, out, out_size) && is_google_hls(out))
            return 1;
    }
    out[0] = '\0';
    return 0;
}

/* Returns 1 for an immediately usable HLS URL, -1 when an HLS URL was present
 * but carried an unsolved n challenge, and 0 for a failed/no-manifest reply. */
static int try_player_manifest(const char *api_url,
                               const mr_http_options *options,
                               const char *json, const char *client,
                               char *out, size_t out_size)
{
    char *reply = NULL;
    size_t reply_len = 0;
    int ok;
    if (!mr_http_post_json(api_url, options, json, &reply, &reply_len,
                           YOUTUBE_PAGE_MAX))
        return 0;
    (void)reply_len;
    ok = mr_youtube_extract_live_manifest(reply, out, out_size);
    mr_free(reply);
    if (!ok) return 0;
    if (manifest_needs_n_transform(out)) return -1;
    g_last_client = client;
    return 1;
}

int mr_youtube_resolve_live(const char *url,
                            const mr_http_options *options,
                            char *out, size_t out_size)
{
    mr_http_options fetch_options;
    char *html = NULL;
    size_t html_len = 0;
    char video_id[12], api_key[80], client_version[64];
    char api_url[256], json[1024];
    int n, ok, result, saw_n_challenge = 0;
    g_last_client = "";
    if (!mr_youtube_is_url(url) || !out || out_size < 2) {
        mr_source_set_error("invalid YouTube URL or output buffer");
        return 0;
    }
    if (!mr_youtube_http_options_init(&fetch_options, options))
        return 0;
    if (!mr_http_fetch_text(url, &fetch_options, &html, &html_len,
                            YOUTUBE_PAGE_MAX))
        return 0;
    (void)html_len;
    ok = mr_youtube_extract_live_manifest(html, out, out_size);
    if (ok && !manifest_needs_n_transform(out)) {
        g_last_client = "watch page";
        mr_free(html);
        return 1;
    }
    if (ok) saw_n_challenge = 1;

    /* Modern watch pages often omit streamingData even for a public live
     * stream. The same page still publishes its Innertube API key and current
     * WEB client version; use those to ask the lightweight player endpoint for
     * the authoritative streamingData object. */
    if (!mr_youtube_extract_video_id(url, video_id) ||
        !extract_config_string(html, "INNERTUBE_API_KEY",
                               api_key, sizeof api_key) ||
        (!extract_config_string(html, "INNERTUBE_CLIENT_VERSION",
                                client_version, sizeof client_version) &&
         !extract_config_string(html, "INNERTUBE_CONTEXT_CLIENT_VERSION",
                                client_version, sizeof client_version))) {
        mr_free(html);
        mr_source_set_error("YouTube page omitted player API configuration");
        return 0;
    }
    mr_free(html);
    html = NULL;
    n = snprintf(api_url, sizeof api_url,
                 "https://www.youtube.com/youtubei/v1/player?key=%s&prettyPrint=false",
                 api_key);
    if (n <= 0 || (size_t)n >= sizeof api_url) {
        mr_source_set_error("YouTube player API URL is too long");
        return 0;
    }
    /* Streamlink's current live-only YouTube implementation uses this Android
     * profile without a JavaScript challenge solver. Prefer it because its HLS
     * response can avoid the WEB client's /n/ path challenge entirely. */
    n = snprintf(json, sizeof json,
                 "{\"videoId\":\"%s\",\"contentCheckOk\":true,"
                 "\"racyCheckOk\":true,\"context\":{\"client\":{"
                 "\"clientName\":\"ANDROID\",\"clientVersion\":\"%s\","
                 "\"platform\":\"DESKTOP\",\"clientScreen\":\"EMBED\","
                 "\"clientFormFactor\":\"UNKNOWN_FORM_FACTOR\","
                 "\"browserName\":\"Chrome\"},\"user\":{"
                 "\"lockedSafetyMode\":\"false\"},\"request\":{"
                 "\"useSsl\":\"true\"}}}",
                 video_id, YOUTUBE_ANDROID_VERSION);
    if (n <= 0 || (size_t)n >= sizeof json) {
        mr_source_set_error("YouTube Android player request is too large");
        return 0;
    }
    result = try_player_manifest(api_url, &fetch_options, json, "ANDROID",
                                 out, out_size);
    if (result > 0) return 1;
    if (result < 0) saw_n_challenge = 1;

    /* Prefer the embedded WEB client next for embeddable public streams; its
     * thirdParty context identifies a genuine external embed origin. Live HLS
     * currently does not require a GVS Proof-of-Origin token, so a later media
     * 403 must not automatically be diagnosed as a missing token. */
    n = snprintf(json, sizeof json,
                 "{\"context\":{\"client\":{"
                 "\"clientName\":\"WEB_EMBEDDED_PLAYER\","
                 "\"clientVersion\":\"%s\",\"hl\":\"en\",\"gl\":\"GB\"},"
                 "\"thirdParty\":{\"embedUrl\":\"https://www.reddit.com/\"}},"
                 "\"videoId\":\"%s\",\"contentCheckOk\":true,"
                 "\"racyCheckOk\":true}", client_version, video_id);
    if (n <= 0 || (size_t)n >= sizeof json) {
        mr_source_set_error("YouTube player API request is too large");
        return 0;
    }
    result = try_player_manifest(api_url, &fetch_options, json,
                                 "WEB_EMBEDDED_PLAYER", out, out_size);
    if (result > 0) return 1;
    if (result < 0) saw_n_challenge = 1;

    /* Non-embeddable streams may reject WEB_EMBEDDED_PLAYER. Keep the normal
     * WEB request as a compatibility fallback, although deployments enforcing
     * a GVS PO token can still reject its eventual media segments. */
    n = snprintf(json, sizeof json,
                 "{\"context\":{\"client\":{\"clientName\":\"WEB\","
                 "\"clientVersion\":\"%s\",\"hl\":\"en\",\"gl\":\"GB\"}},"
                 "\"videoId\":\"%s\",\"contentCheckOk\":true,"
                 "\"racyCheckOk\":true}", client_version, video_id);
    if (n <= 0 || (size_t)n >= sizeof json)
        return 0;
    result = try_player_manifest(api_url, &fetch_options, json, "WEB",
                                 out, out_size);
    if (result > 0) return 1;
    if (result < 0) saw_n_challenge = 1;
    mr_source_set_error(saw_n_challenge
        ? "YouTube HLS manifest requires a player n challenge"
        : "YouTube player APIs returned no live HLS manifest");
    return 0;
}
