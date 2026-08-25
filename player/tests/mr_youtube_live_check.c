/*
 * Host-only live YouTube resolver probe.
 *
 * Runs one cold resolver attempt using the same portable resolver, HTTP cookie
 * layer and QuickJS n solver as mrplay.  The companion shell script launches
 * this executable repeatedly so every sample starts with a fresh process-local
 * cookie jar, matching separate Amiga mrplay launches.
 */
#include "../core/mr_youtube.h"
#include "../core/mr_youtube_nsig.h"
#include "../core/mr_http.h"
#include "../core/mr_source.h"

#include <stdio.h>

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

int main(int argc, char **argv)
{
    mr_http_options options;
    mr_youtube_media_kind kind = MR_YOUTUBE_MEDIA_NONE;
    char video_url[MR_HTTP_URL_MAX];
    char audio_url[MR_HTTP_URL_MAX];
    const char *client;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <youtube-url>\n", argv[0]);
        return 2;
    }

    if (!mr_http_options_init(&options, NULL, NULL)) {
        fprintf(stderr, "HTTP options failed: %s\n", mr_source_last_error());
        return 2;
    }

    /* Match MintVID Low: Safari HLS/adaptive 144p, muxed 360p fallback. */
    options.hls_low = 1;
    options.hls_max_width = 640;

    mr_youtube_set_nsig_solver(mr_youtube_nsig_transform_url, NULL);
    video_url[0] = '\0';
    audio_url[0] = '\0';

    if (!mr_youtube_resolve_media_pair(argv[1], &options,
                                       video_url, sizeof video_url,
                                       audio_url, sizeof audio_url,
                                       &kind)) {
        fprintf(stderr, "RESULT ERROR client=%s error=%s\n",
                mr_youtube_last_client(), mr_source_last_error());
        return 1;
    }

    client = mr_youtube_last_client();
    printf("RESULT %s client=%s audio_pair=%s\n",
           kind_name(kind), client && *client ? client : "unknown",
           audio_url[0] ? "yes" : "no");
    return 0;
}
