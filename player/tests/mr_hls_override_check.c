/*
 * MintVID - host check for the mr_http fetch-override seam (core/mr_http.c)
 * and core/mr_hls.c's use of it.
 *
 * amiga/hls_fetch.c's background worker depends on this seam routing every
 * playlist and segment fetch through a caller-supplied function instead of
 * mr_hls.c ever touching mr_http's real socket/TLS code directly - that is
 * what lets one dedicated task own this session's whole HTTP/S connection.
 * This test cannot exercise the Amiga worker itself (no AmigaOS toolchain on
 * this host), but it can and does prove the portable half of that contract:
 * once an override is installed, mr_hls_source_open_ex() never reaches the
 * real network at all - if it did, resolving "test.invalid" would fail (or
 * hang) instead of the fake fetch below being called.
 */
#include "../core/mr_hls.h"
#include "../core/mr_http.h"
#include "../core/mr_alloc.h"

#include <stdio.h>
#include <string.h>

static const char *g_playlist =
    "#EXTM3U\n"
    "#EXT-X-VERSION:3\n"
    "#EXT-X-TARGETDURATION:2\n"
    "#EXT-X-MEDIA-SEQUENCE:0\n"
    "#EXTINF:2.0,\n"
    "seg0.ts\n"
    "#EXTINF:2.0,\n"
    "seg1.ts\n"
    "#EXT-X-ENDLIST\n";

static int  g_calls;
static int  g_hint_calls;
static char g_last_hint_url[256];

static int fake_override(const char *url, const mr_http_options *options,
                         const char *post_json,
                         unsigned char **out, size_t *out_len,
                         size_t max_size)
{
    const char *body;
    size_t len;
    (void)options; (void)post_json; (void)max_size;
    g_calls++;
    if (strstr(url, "playlist.m3u8")) { body = g_playlist; len = strlen(body); }
    else if (strstr(url, "seg0.ts"))  { body = "AAAA"; len = 4; }
    else if (strstr(url, "seg1.ts"))  { body = "BBBB"; len = 4; }
    else return 0;
    *out = (unsigned char *)mr_alloc(len + 1);
    if (!*out) return 0;
    memcpy(*out, body, len);
    (*out)[len] = 0;
    *out_len = len;
    return 1;
}

static void fake_hint(const char *url, const mr_http_options *options)
{
    (void)options;
    g_hint_calls++;
    strncpy(g_last_hint_url, url, sizeof g_last_hint_url - 1);
    g_last_hint_url[sizeof g_last_hint_url - 1] = 0;
}

int main(void)
{
    mr_source *s;
    unsigned char buf[8];
    int fails = 0;

    mr_http_set_fetch_override(fake_override);
    mr_http_set_prefetch_hint(fake_hint);

    if (!mr_http_fetch_override_active()) {
        printf("FAIL: override not reported active\n");
        fails++;
    }

    s = mr_hls_source_open_ex("http://test.invalid/playlist.m3u8", NULL);
    if (!s) {
        printf("FAIL: mr_hls_source_open_ex returned NULL: %s\n",
               mr_source_last_error());
        mr_http_set_fetch_override(NULL);
        mr_http_set_prefetch_hint(NULL);
        return 1;
    }

    if (!mr_source_read_at(s, 0, buf, 4) || memcmp(buf, "AAAA", 4) != 0) {
        printf("FAIL: segment 0 content mismatch\n");
        fails++;
    }
    /* Opening segment 0 should have hinted segment 1's URL for background
     * lookahead (open_seg()'s mr_http_prefetch_hint() call). */
    if (!g_hint_calls || !strstr(g_last_hint_url, "seg1.ts")) {
        printf("FAIL: expected a prefetch hint for seg1.ts, got %d hint(s) "
               "(last=%s)\n", g_hint_calls, g_last_hint_url);
        fails++;
    }
    if (!mr_source_read_at(s, 4, buf, 4) || memcmp(buf, "BBBB", 4) != 0) {
        printf("FAIL: segment 1 content mismatch\n");
        fails++;
    }
    /* Exactly 3 fetches expected: the playlist plus the two segments. If the
     * override weren't honoured, mr_hls_source_open_ex() would have tried to
     * resolve test.invalid via the real network instead. */
    if (g_calls != 3) {
        printf("FAIL: expected 3 override calls (playlist+2 segments), "
               "got %d\n", g_calls);
        fails++;
    }

    mr_source_close(s);
    mr_http_set_fetch_override(NULL);
    mr_http_set_prefetch_hint(NULL);

    if (fails) {
        printf("mr_hls fetch-override checks FAILED: %d mismatch(es)\n",
               fails);
        return 1;
    }
    printf("mr_hls fetch-override checks passed\n");
    return 0;
}
