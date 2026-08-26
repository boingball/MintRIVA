/*
 * VisionOS HLS audio pairing shim.
 *
 * The base resolver already gets us a reliable anonymous VisionOS recorded
 * HLS master for Low. VisionOS exposes that ladder as separate video/audio
 * media playlists, while mr_hls.c intentionally resolves one variant only.
 * Split the master here into the existing dual-stream contract so mrplay can
 * keep its proven scheduler/fallback path and every fetch stays on the HLS
 * worker. No DASH/fragmented-MP4 path is introduced.
 */
#define mr_youtube_resolve_media_pair mr_youtube_resolve_media_pair_base
#include "mr_youtube_base.c"
#undef mr_youtube_resolve_media_pair

#define YT_HLS_MASTER_MAX (2UL * 1024UL * 1024UL)
#define YT_HLS_LINE_MAX   (MR_HTTP_URL_MAX + 2048)

static const char *yt_hls_next_line(const char *p, char *out, size_t out_size)
{
    const char *end;
    size_t n;
    if (!p || !*p || !out || out_size < 2) return NULL;
    end = strchr(p, '\n');
    n = end ? (size_t)(end - p) : strlen(p);
    if (n && p[n - 1] == '\r') n--;
    if (n >= out_size) {
        out[0] = '\0';
    } else {
        memcpy(out, p, n);
        out[n] = '\0';
    }
    return end ? end + 1 : p + strlen(p);
}

static int yt_hls_attr(const char *line, const char *name,
                       char *out, size_t out_size)
{
    const char *p;
    size_t wanted;
    if (!line || !name || !out || out_size < 2) return 0;
    out[0] = '\0';
    wanted = strlen(name);
    p = strchr(line, ':');
    if (!p) return 0;
    p++;
    while (*p) {
        const char *key, *key_end, *value;
        size_t key_len, used = 0;
        int quoted = 0;
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (!*p) break;
        key = p;
        while (*p && *p != '=' && *p != ',') p++;
        key_end = p;
        while (key_end > key &&
               (key_end[-1] == ' ' || key_end[-1] == '\t'))
            key_end--;
        key_len = (size_t)(key_end - key);
        if (*p != '=') {
            if (*p == ',') p++;
            continue;
        }
        p++;
        while (*p == ' ' || *p == '\t') p++;
        value = p;
        if (*p == '"') {
            quoted = 1;
            value = ++p;
            while (*p) {
                if (*p == '\\' && p[1]) {
                    p += 2;
                    continue;
                }
                if (*p == '"') break;
                p++;
            }
        } else {
            while (*p && *p != ',') p++;
        }
        if (key_len == wanted && !strncmp(key, name, wanted)) {
            const char *q = value;
            const char *value_end = p;
            while (q < value_end && used + 1 < out_size) {
                if (quoted && *q == '\\' && q + 1 < value_end) q++;
                out[used++] = *q++;
            }
            if (q != value_end) {
                out[0] = '\0';
                return 0;
            }
            out[used] = '\0';
            return 1;
        }
        if (quoted && *p == '"') p++;
        while (*p && *p != ',') p++;
        if (*p == ',') p++;
    }
    return 0;
}

static int yt_hls_yes(const char *s)
{
    return s && (!ascii_ncasecmp(s, "YES", 3) ||
                 !ascii_ncasecmp(s, "TRUE", 4));
}

static size_t yt_hls_lang_base_len(const char *s)
{
    size_t n = 0;
    if (!s) return 0;
    while (s[n] && s[n] != '-' && s[n] != '_' && s[n] != '.') n++;
    return n;
}

static int yt_hls_language_rank(const char *wanted, const char *candidate)
{
    size_t wn, cn;
    char normalized[MR_HTTP_HLS_LANGUAGE_MAX * 2];
    char *dot;
    if (!wanted || !wanted[0] || !candidate || !candidate[0]) return 0;
    strncpy(normalized, candidate, sizeof normalized - 1);
    normalized[sizeof normalized - 1] = '\0';
    dot = strchr(normalized, '.');
    if (dot) *dot = '\0';
    if (strlen(wanted) == strlen(normalized) &&
        !ascii_ncasecmp(wanted, normalized, strlen(wanted)))
        return 4;
    wn = yt_hls_lang_base_len(wanted);
    cn = yt_hls_lang_base_len(normalized);
    if (wn && wn == cn && !ascii_ncasecmp(wanted, normalized, wn))
        return 3;
    return 0;
}

static int yt_hls_split_visionos_master(
    const char *master, const char *master_url, const char *wanted_language,
    char *video_out, size_t video_out_size,
    char *audio_out, size_t audio_out_size)
{
    const char *p;
    char line[YT_HLS_LINE_MAX], pending[YT_HLS_LINE_MAX];
    char best_video[MR_HTTP_URL_MAX], best_group[256];
    unsigned long best_bw = 0;
    int have_best = 0, best_lang_rank = -1;
    int pending_variant = 0;

    if (!master || !master_url || !video_out || video_out_size < 2 ||
        !audio_out || audio_out_size < 2)
        return 0;
    best_video[0] = '\0';
    best_group[0] = '\0';

    p = master;
    while (p && *p) {
        char resolution[64], codecs[256], group[256], language[128];
        char bandwidth[64];
        unsigned w = 0, h = 0;
        unsigned long bw = 0;
        int lang_rank = 1;

        p = yt_hls_next_line(p, line, sizeof line);
        if (!line[0]) continue;
        if (!strncmp(line, "#EXT-X-STREAM-INF:", 18)) {
            strncpy(pending, line, sizeof pending - 1);
            pending[sizeof pending - 1] = '\0';
            pending_variant = 1;
            continue;
        }
        if (line[0] == '#') continue;
        if (!pending_variant) continue;
        pending_variant = 0;

        resolution[0] = codecs[0] = group[0] = language[0] = bandwidth[0] = '\0';
        if (!yt_hls_attr(pending, "RESOLUTION", resolution,
                         sizeof resolution) ||
            sscanf(resolution, "%ux%u", &w, &h) != 2 ||
            w != 256 || h != 144)
            continue;
        if (!yt_hls_attr(pending, "CODECS", codecs, sizeof codecs) ||
            (!text_contains_nocase(codecs, "avc1") &&
             !text_contains_nocase(codecs, "avc3")))
            continue;
        if (!yt_hls_attr(pending, "AUDIO", group, sizeof group) || !group[0])
            continue;
        if (yt_hls_attr(pending, "BANDWIDTH", bandwidth, sizeof bandwidth))
            bw = strtoul(bandwidth, NULL, 10);
        if (wanted_language && wanted_language[0] &&
            yt_hls_attr(pending, "YT-EXT-AUDIO-CONTENT-ID",
                        language, sizeof language) && language[0]) {
            lang_rank = yt_hls_language_rank(wanted_language, language);
            if (!lang_rank) continue;
        }

        if (!have_best || lang_rank > best_lang_rank ||
            (lang_rank == best_lang_rank &&
             (!best_bw || (bw && bw < best_bw)))) {
            if (strlen(line) + 1 > sizeof best_video ||
                strlen(group) + 1 > sizeof best_group)
                continue;
            strcpy(best_video, line);
            strcpy(best_group, group);
            best_bw = bw;
            best_lang_rank = lang_rank;
            have_best = 1;
        }
    }
    if (!have_best ||
        !mr_http_resolve_url(master_url, best_video,
                             video_out, video_out_size))
        return 0;

    {
        char best_audio[MR_HTTP_URL_MAX];
        int have_audio = 0, best_score = -1;
        best_audio[0] = '\0';
        p = master;
        while (p && *p) {
            char type[32], group[256], uri[MR_HTTP_URL_MAX];
            char language[128], content_id[128], name[256];
            char def[32], channels[64];
            int language_rank = 0, has_language = 0;
            int original, is_default, stereo, score;

            p = yt_hls_next_line(p, line, sizeof line);
            if (strncmp(line, "#EXT-X-MEDIA:", 13)) continue;
            type[0] = group[0] = uri[0] = language[0] = content_id[0] = '\0';
            name[0] = def[0] = channels[0] = '\0';
            if (!yt_hls_attr(line, "TYPE", type, sizeof type) ||
                ascii_ncasecmp(type, "AUDIO", 5) ||
                !yt_hls_attr(line, "GROUP-ID", group, sizeof group) ||
                strcmp(group, best_group) ||
                !yt_hls_attr(line, "URI", uri, sizeof uri) || !uri[0])
                continue;

            if (yt_hls_attr(line, "LANGUAGE", language, sizeof language) &&
                language[0]) {
                has_language = 1;
                language_rank = yt_hls_language_rank(
                    wanted_language, language);
            }
            if (yt_hls_attr(line, "YT-EXT-AUDIO-CONTENT-ID",
                            content_id, sizeof content_id) && content_id[0]) {
                int r = yt_hls_language_rank(wanted_language, content_id);
                has_language = 1;
                if (r > language_rank) language_rank = r;
            }
            (void)yt_hls_attr(line, "NAME", name, sizeof name);
            (void)yt_hls_attr(line, "DEFAULT", def, sizeof def);
            (void)yt_hls_attr(line, "CHANNELS", channels, sizeof channels);
            original = text_contains_nocase(name, "original");
            is_default = yt_hls_yes(def);
            stereo = channels[0] == '2';

            if (wanted_language && wanted_language[0]) {
                if (has_language && !language_rank) continue;
                if (!has_language && !original && !is_default) continue;
            } else if (!original && !is_default) {
                continue;
            }

            score = language_rank * 100 +
                    (original ? 20 : 0) +
                    (is_default ? 10 : 0) +
                    (stereo ? 1 : 0);
            if (!have_audio || score > best_score) {
                if (strlen(uri) + 1 > sizeof best_audio) continue;
                strcpy(best_audio, uri);
                best_score = score;
                have_audio = 1;
            }
        }
        if (!have_audio ||
            !mr_http_resolve_url(master_url, best_audio,
                                 audio_out, audio_out_size)) {
            video_out[0] = '\0';
            audio_out[0] = '\0';
            return 0;
        }
    }
    return 1;
}

int mr_youtube_resolve_media_pair(const char *url,
                                  const mr_http_options *options,
                                  char *video_out, size_t video_out_size,
                                  char *audio_out, size_t audio_out_size,
                                  mr_youtube_media_kind *kind)
{
    int ok = mr_youtube_resolve_media_pair_base(
        url, options, video_out, video_out_size,
        audio_out, audio_out_size, kind);

    if (ok && kind && *kind == MR_YOUTUBE_MEDIA_HLS_VOD &&
        !strcmp(g_last_client, "VISIONOS") &&
        audio_out && audio_out_size >= 2 && !audio_out[0]) {
        mr_http_options media_options;
        char *master = NULL;
        size_t master_len = 0;
        char split_video[MR_HTTP_URL_MAX], split_audio[MR_HTTP_URL_MAX];

        split_video[0] = split_audio[0] = '\0';
        if (mr_youtube_media_http_options_init(&media_options, options) &&
            mr_http_fetch_text(video_out, &media_options,
                               &master, &master_len, YT_HLS_MASTER_MAX)) {
            (void)master_len;
            if (yt_hls_split_visionos_master(
                    master, video_out, g_last_hls_audio_language,
                    split_video, sizeof split_video,
                    split_audio, sizeof split_audio) &&
                strlen(split_video) + 1 <= video_out_size &&
                strlen(split_audio) + 1 <= audio_out_size) {
                strcpy(video_out, split_video);
                strcpy(audio_out, split_audio);
                g_last_kind = MR_YOUTUBE_MEDIA_ADAPTIVE_144P;
                *kind = g_last_kind;
                /* The Amiga HLS worker has one speculative lookahead slot for
                 * the whole session. With two independent HLS media sources,
                 * a video lookahead can occupy that slot while mrplay is
                 * synchronously opening the audio playlist, which is exactly
                 * the startup stall seen on hardware. Keep the single network
                 * worker/owner, but turn off speculative hints for this paired
                 * session so video and audio issue only demanded requests and
                 * are naturally serialized by the existing worker. */
                mr_http_set_prefetch_hint(NULL);
                printf("YouTube: dual HLS: prefetch disabled for paired "
                       "VisionOS streams\n");
                printf("YouTube: VISIONOS HLS split: 144p AVC + %s audio\n",
                       g_last_hls_audio_language[0]
                           ? g_last_hls_audio_language
                           : "original/default");
            } else {
                printf("YouTube: VISIONOS HLS master had no safe "
                       "144p/original-audio pair; keeping muxed fallback\n");
            }
            mr_free(master);
        } else {
            printf("YouTube: VISIONOS HLS master could not be inspected; "
                   "keeping muxed fallback\n");
        }
    }
    return ok;
}
