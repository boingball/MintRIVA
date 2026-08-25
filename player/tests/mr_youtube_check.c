#include "../core/mr_youtube.h"
#include "../core/mr_alloc.h"
#include "../core/mr_http.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int nsig_fetches;
static int nsig_calls;

static void expect(int condition, const char *name)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static int return_fixture(const char *text, unsigned char **out,
                          size_t *out_len, size_t max_size)
{
    size_t len = strlen(text);
    if (len > max_size) return 0;
    *out = (unsigned char *)mr_alloc(len + 1);
    if (!*out) return 0;
    memcpy(*out, text, len + 1);
    *out_len = len;
    return 1;
}

static int nsig_fetch_override(const char *url,
                               const mr_http_options *options,
                               const char *post_json,
                               unsigned char **out, size_t *out_len,
                               size_t max_size)
{
    static const char watch[] =
        "{\"INNERTUBE_API_KEY\":\"test-key\","
        "\"INNERTUBE_CLIENT_VERSION\":\"1.2.3\",\"STS\":12345,"
        "\"jsUrl\":\"\\/s\\/player\\/test\\/base.js\"}";
    static const char safari[] =
        "{\"streamingData\":{\"hlsManifestUrl\":\""
        "https://manifest.googlevideo.com/api/manifest/"
        "hls_variant/n/abc123/file/index.m3u8\"}}";
    (void)options;
    nsig_fetches++;
    if (!post_json && strstr(url, "/watch?v=EvsLqQS_80E"))
        return return_fixture(watch, out, out_len, max_size);
    if (!post_json && !strcmp(url,
                              "https://www.youtube.com/s/player/test/base.js"))
        return return_fixture("synthetic current player", out, out_len,
                              max_size);
    if (post_json && strstr(url, "/youtubei/v1/player?key=test-key"))
        return return_fixture(safari, out, out_len, max_size);
    return 0;
}

static int nsig_solver(const char *player_js, size_t player_js_len,
                       const char *url, char *out, size_t out_size,
                       void *opaque)
{
    static const char solved[] =
        "https://manifest.googlevideo.com/api/manifest/"
        "hls_variant/n/solved/file/index.m3u8";
    (void)opaque;
    nsig_calls++;
    if (player_js_len != strlen("synthetic current player") ||
        memcmp(player_js, "synthetic current player", player_js_len) ||
        !strstr(url, "/n/abc123/") || sizeof solved > out_size)
        return 0;
    memcpy(out, solved, sizeof solved);
    return 1;
}

int main(int argc, char **argv)
{
    char out[1024], audio_out[1024];
    char video_id[12];
    mr_youtube_media_kind media_kind;
    mr_http_options base_options, youtube_options;
    const char *raw =
        "before \"hlsManifestUrl\":\"https://manifest.googlevideo.com/"
        "api/manifest/hls_variant/file/index.m3u8\" after";
    const char *escaped =
        "{\n \"hlsManifestUrl\" : \"https:\\/\\/manifest.googlevideo.com/"
        "api\\/manifest\\/hls_variant\\/index.m3u8?x=1\\u0026y=2\"}";
    const char *foreign =
        "\"hlsManifestUrl\":\"https://evil.example/live/index.m3u8\"";
    const char *progressive =
        "{\"streamingData\":{\"formats\":["
        "{\"itag\":17,\"mimeType\":\"video/3gpp; codecs=\\\"mp4v.20.3, "
        "mp4a.40.2\\\"\",\"url\":\"https://r1.googlevideo.com/low\"},"
        "{\"itag\":18,\"mimeType\":\"video/mp4; codecs=\\\"avc1.42001E, "
        "mp4a.40.2\\\"\",\"width\":640,\"height\":360,"
        "\"url\":\"https://r2---sn-test.googlevideo.com/videoplayback?"
        "expire=1\\u0026sig=ok\"}]}}";
    const char *progressive_hd =
        "{\"streamingData\":{\"formats\":["
        "{\"itag\":18,\"mimeType\":\"video/mp4; codecs=\\\"avc1.42001E, "
        "mp4a.40.2\\\"\",\"url\":\"https://r1.googlevideo.com/360\"},"
        "{\"itag\":22,\"mimeType\":\"video/mp4; codecs=\\\"avc1.64001F, "
        "mp4a.40.2\\\"\",\"width\":1280,\"height\":720,"
        "\"url\":\"https://r2.googlevideo.com/720\"}]}}";
    const char *adaptive =
        "{\"streamingData\":{\"adaptiveFormats\":["
        "{\"itag\":160,\"mimeType\":\"video/mp4; "
        "codecs=\\\"avc1.4d400c\\\"\","
        "\"url\":\"https://r1.googlevideo.com/144-video\"},"
        "{\"itag\":136,\"mimeType\":\"video/mp4; "
        "codecs=\\\"avc1.4d401f\\\"\","
        "\"url\":\"https://r1.googlevideo.com/720-video\"},"
        "{\"itag\":139,\"mimeType\":\"audio/mp4; "
        "codecs=\\\"mp4a.40.5\\\"\","
        "\"url\":\"https://r1.googlevideo.com/low-audio\"},"
        "{\"itag\":140,\"mimeType\":\"audio/mp4; "
        "codecs=\\\"mp4a.40.2\\\"\","
        "\"url\":\"https://r1.googlevideo.com/audio\"}]}}";

    if (argc == 2 || (argc == 3 && !strcmp(argv[1], "--post"))) {
        char *html = NULL;
        size_t html_len = 0;
        int fetched = argc == 2
                    ? mr_http_fetch_text(argv[1], NULL, &html, &html_len, 65536)
                    : mr_http_post_json(argv[2], NULL, "{}", &html,
                                        &html_len, 65536);
        if (!fetched ||
            !html_len || !mr_youtube_extract_live_manifest(html, out,
                                                            sizeof out)) {
            fprintf(stderr, "YouTube HTTP fixture failed\n");
            mr_free(html);
            return 1;
        }
        mr_free(html);
        puts("YouTube HTTP fixture passed");
        return 0;
    }

    expect(mr_youtube_is_url("https://www.youtube.com/watch?v=abc"),
           "www.youtube.com accepted");
    expect(mr_youtube_is_url("https://youtu.be/abc"), "youtu.be accepted");
    expect(mr_youtube_is_url("http://m.youtube.com/live/abc"),
           "mobile YouTube accepted");
    expect(!mr_youtube_is_url("https://notyoutube.com/watch?v=abc"),
           "lookalike host rejected");
    expect(!mr_youtube_is_url("https://youtube.com.evil.test/watch?v=abc"),
           "host suffix attack rejected");
    expect(mr_http_options_init(&base_options, NULL, NULL),
           "generic HTTP options initialised");
    base_options.hls_max_width = 640;
    base_options.hls_max_height = 360;
    expect(mr_youtube_http_options_init(&youtube_options, &base_options) &&
           strstr(youtube_options.user_agent, "Mozilla/5.0") &&
           !strcmp(youtube_options.referer, "https://www.youtube.com/") &&
           youtube_options.hls_max_width == 640 &&
           youtube_options.hls_max_height == 360 &&
           youtube_options.hls_live_start_segments == 2 &&
           youtube_options.hls_buffer_segments,
           "YouTube browser defaults, live edge and HLS limits applied");
    expect(mr_http_options_init(&base_options, "Custom Agent",
                                "https://custom.example/") &&
           mr_youtube_http_options_init(&youtube_options, &base_options) &&
           !strcmp(youtube_options.user_agent, "Custom Agent") &&
           !strcmp(youtube_options.referer, "https://custom.example/"),
           "explicit YouTube headers preserved");
    expect(mr_youtube_extract_video_id(
               "https://www.youtube.com/watch?v=EvsLqQS_80E", video_id) &&
           !strcmp(video_id, "EvsLqQS_80E"), "watch video ID extracted");
    expect(mr_youtube_extract_video_id(
               "https://www.youtube.com/live/EvsLqQS_80E?si=test", video_id) &&
           !strcmp(video_id, "EvsLqQS_80E"), "live video ID extracted");
    expect(mr_youtube_extract_video_id(
               "https://youtu.be/EvsLqQS_80E", video_id) &&
           !strcmp(video_id, "EvsLqQS_80E"), "share video ID extracted");

    expect(mr_youtube_extract_live_manifest(raw, out, sizeof out) &&
           !strcmp(out, "https://manifest.googlevideo.com/api/manifest/"
                        "hls_variant/file/index.m3u8"),
           "plain manifest extracted");
    expect(mr_youtube_extract_live_manifest(escaped, out, sizeof out) &&
           !strcmp(out, "https://manifest.googlevideo.com/api/manifest/"
                        "hls_variant/index.m3u8?x=1&y=2"),
           "JSON escapes decoded");
    expect(!mr_youtube_extract_live_manifest(foreign, out, sizeof out),
           "foreign manifest host rejected");
    expect(!mr_youtube_extract_live_manifest(
               "\"hlsManifestUrl\":\"https://manifest.googlevideo.com/"
               "live/index.m3u8\\nInjected: yes\"", out, sizeof out),
           "escaped control character rejected");
    expect(!mr_youtube_extract_live_manifest("<html>ordinary video</html>",
                                             out, sizeof out),
           "non-live page rejected");
    expect(!mr_youtube_extract_live_manifest(raw, out, 24),
           "truncated output rejected");
    expect(mr_youtube_extract_progressive_360p(progressive, out,
                                                sizeof out) &&
           !strcmp(out, "https://r2---sn-test.googlevideo.com/videoplayback?"
                        "expire=1&sig=ok"),
           "muxed progressive 360p MP4 extracted");
    expect(mr_youtube_extract_progressive(progressive_hd, 1, out,
                                           sizeof out, &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_PROGRESSIVE_720P &&
           !strcmp(out, "https://r2.googlevideo.com/720"),
           "muxed progressive 720p MP4 preferred");
    expect(mr_youtube_extract_progressive(progressive_hd, 0, out,
                                           sizeof out, &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_PROGRESSIVE_360P &&
           !strcmp(out, "https://r1.googlevideo.com/360"),
           "360p retained for lower quality setting");
    expect(mr_youtube_extract_progressive(progressive, 1, out,
                                           sizeof out, &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_PROGRESSIVE_360P,
           "missing 720p automatically falls back to 360p");
    expect(!mr_youtube_extract_progressive_360p(
               "{\"formats\":[{\"itag\":18,\"mimeType\":\"video/mp4; "
               "codecs=\\\"avc1.42001E, mp4a.40.2\\\"\","
               "\"url\":\"https://evil.example/videoplayback\"}]}",
               out, sizeof out), "foreign progressive host rejected");
    expect(!mr_youtube_extract_progressive_360p(
               "{\"formats\":[{\"itag\":18,\"mimeType\":\"video/mp4; "
               "codecs=\\\"avc1.42001E, mp4a.40.2\\\"\","
               "\"url\":\"https://r1.googlevideo.com/videoplayback?x=1"
               "\\u0026n=unsolved\"}]}", out, sizeof out),
           "progressive n challenge rejected");
    expect(!mr_youtube_extract_progressive_360p(
               "{\"formats\":[{\"itag\":18,\"mimeType\":\"video/mp4; "
               "codecs=\\\"avc1.42001E, mp4a.40.2\\\"\","
               "\"signatureCipher\":\"url=hidden\"}]}", out, sizeof out),
           "cipher-only progressive format rejected");
    expect(!mr_youtube_extract_progressive_360p(
               "{\"adaptiveFormats\":[{\"itag\":134,"
               "\"mimeType\":\"video/mp4; codecs=\\\"avc1.4d401e\\\"\","
               "\"url\":\"https://r1.googlevideo.com/video-only\"}]}",
               out, sizeof out), "adaptive video-only format rejected");
    expect(mr_youtube_extract_adaptive(adaptive, 0,
                                       out, sizeof out,
                                       audio_out, sizeof audio_out,
                                       &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_ADAPTIVE_144P &&
           !strcmp(out, "https://r1.googlevideo.com/144-video") &&
           !strcmp(audio_out, "https://r1.googlevideo.com/low-audio"),
           "adaptive 144p H.264 plus low AAC pair extracted");
    expect(mr_youtube_extract_adaptive(adaptive, 1,
                                       out, sizeof out,
                                       audio_out, sizeof audio_out,
                                       &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_ADAPTIVE_720P &&
           !strcmp(out, "https://r1.googlevideo.com/720-video") &&
           !strcmp(audio_out, "https://r1.googlevideo.com/audio"),
           "adaptive 720p H.264 plus AAC pair extracted");
    expect(!mr_youtube_extract_adaptive(
               "{\"adaptiveFormats\":[{\"itag\":160,"
               "\"mimeType\":\"video/mp4; codecs=\\\"avc1.4d400c\\\"\","
               "\"url\":\"https://r1.googlevideo.com/v?n=unsolved\"},"
               "{\"itag\":139,\"mimeType\":\"audio/mp4; "
               "codecs=\\\"mp4a.40.5\\\"\","
               "\"url\":\"https://r1.googlevideo.com/a\"}]}", 0,
               out, sizeof out, audio_out, sizeof audio_out, &media_kind),
           "adaptive pair with unresolved video n challenge rejected");
    expect(!mr_youtube_extract_progressive_360p(progressive, out, 24),
           "truncated progressive output rejected");
    expect(!strcmp(mr_youtube_last_client(), ""),
           "client diagnostic empty before a successful resolution");

    expect(mr_http_options_init(&base_options, NULL, NULL),
           "native n integration options initialised");
    base_options.hls_low = 1;
    nsig_fetches = nsig_calls = 0;
    mr_http_set_fetch_override(nsig_fetch_override);
    mr_youtube_set_nsig_solver(nsig_solver, NULL);
    expect(mr_youtube_resolve_media_pair(
               "https://www.youtube.com/watch?v=EvsLqQS_80E",
               &base_options, out, sizeof out,
               audio_out, sizeof audio_out, &media_kind) &&
           media_kind == MR_YOUTUBE_MEDIA_HLS_VOD &&
           !strcmp(out,
                   "https://manifest.googlevideo.com/api/manifest/"
                   "hls_variant/n/solved/file/index.m3u8") &&
           !audio_out[0] && nsig_fetches == 3 && nsig_calls == 1 &&
           !strcmp(mr_youtube_last_client(), "WEB_SAFARI"),
           "Safari HLS n challenge uses current player and native solver");
    mr_youtube_set_nsig_solver(NULL, NULL);
    mr_http_set_fetch_override(NULL);

    if (failures) return 1;
    puts("YouTube resolver checks passed");
    return 0;
}
