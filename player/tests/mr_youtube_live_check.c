/*
 * Host-only live YouTube resolver probe.
 *
 * A normal invocation runs one cold resolver attempt.  --repeat runs several
 * resolves inside one process so the process-local anonymous YouTube cookie
 * jar survives between attempts.  --safari-page makes the initial watch-page
 * request use the same Safari identity as the WEB_SAFARI player request, so we
 * can A/B YouTube session identity without changing MintVID's product code.
 */
#include "../core/mr_youtube.h"
#include "../core/mr_youtube_nsig.h"
#include "../core/mr_http.h"
#include "../core/mr_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_YOUTUBE_WEB_SAFARI_UA \
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) " \
    "AppleWebKit/605.1.15 (KHTML, like Gecko) Version/15.5 " \
    "Safari/605.1.15,gzip(gfe)"

static const char *kind_name(mr_youtube_media_kind kind)
{
    switch (kind) {
    case MR_YOUTUBE_MEDIA_HLS: return "HLS";
    case MR_YOUTUBE_MEDIA_HLS_VOD: return "SAFARI_HLS";
    case MR_YOUTUBE_MEDIA_PROGRESSIVE_360P: return "FALLBACK_360";
    case MR_YOUTUBE_MEDIA_PROGRESSIVE_720P: return "PROGRESSIVE_720";
    case MR_YOUTUBE_MEDIA_ADAPTIVE_144P: return "ADAPTIVE_144";
    case MR_YOUTUBE_MEDIA_ADAPTIVE_720P: return "ADAPTIVE_720";
    default: return "NONE";
    }
}

static int run_one(const char *url, const mr_http_options *options, int attempt)
{
    mr_youtube_media_kind kind = MR_YOUTUBE_MEDIA_NONE;
    char video_url[MR_HTTP_URL_MAX];
    char audio_url[MR_HTTP_URL_MAX];
    const char *client;

    video_url[0] = '\0';
    audio_url[0] = '\0';

    if (!mr_youtube_resolve_media_pair(url, options,
                                       video_url, sizeof video_url,
                                       audio_url, sizeof audio_url,
                                       &kind)) {
        fprintf(stderr, "RESULT ERROR attempt=%d client=%s error=%s\n",
                attempt, mr_youtube_last_client(), mr_source_last_error());
        return 0;
    }

    client = mr_youtube_last_client();
    printf("RESULT %s attempt=%d client=%s audio_pair=%s\n",
           kind_name(kind), attempt,
           client && *client ? client : "unknown",
           audio_url[0] ? "yes" : "no");
    return 1;
}

int main(int argc, char **argv)
{
    mr_http_options options;
    const char *url;
    const char *initial_ua = NULL;
    int count = 1;
    int safari_page = 0;
    int i;
    int failures = 0;

    if (argc == 2) {
        url = argv[1];
    } else if (argc == 3 && !strcmp(argv[1], "--safari-page")) {
        safari_page = 1;
        url = argv[2];
    } else if (argc == 4 && !strcmp(argv[1], "--repeat")) {
        char *end = NULL;
        long value = strtol(argv[2], &end, 10);
        if (!end || *end || value < 1 || value > 10000) {
            fprintf(stderr, "repeat count must be 1..10000\n");
            return 2;
        }
        count = (int)value;
        url = argv[3];
    } else {
        fprintf(stderr,
                "usage: %s [--safari-page | --repeat count] <youtube-url>\n",
                argv[0]);
        return 2;
    }

    if (safari_page) initial_ua = TEST_YOUTUBE_WEB_SAFARI_UA;
    if (!mr_http_options_init(&options, initial_ua, NULL)) {
        fprintf(stderr, "HTTP options failed: %s\n", mr_source_last_error());
        return 2;
    }

    /* Match MintVID Low: Safari HLS/adaptive 144p, muxed 360p fallback. */
    options.hls_low = 1;
    options.hls_max_width = 640;

    if (safari_page)
        printf("TEST MODE Safari identity from watch page onward\n");

    mr_youtube_set_nsig_solver(mr_youtube_nsig_transform_url, NULL);

    for (i = 1; i <= count; i++) {
        if (count > 1)
            printf("===== warm attempt %d/%d =====\n", i, count);
        if (!run_one(url, &options, i)) failures++;
    }

    mr_http_net_shutdown();
    return failures ? 1 : 0;
}
