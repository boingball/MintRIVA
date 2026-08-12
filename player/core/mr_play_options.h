#ifndef MR_PLAY_OPTIONS_H
#define MR_PLAY_OPTIONS_H

#include <stddef.h>

/* AmigaDOS requester pattern for the local containers MintRIVA can probe.
 * Playback still sniffs the container; this is only to keep audio-only files
 * out of the video picker. */
#define MR_VIDEO_FILE_PATTERN \
    "#?.(avi|divx|mov|mp4|m4v|mkv|mpg|mpeg|mpe|vob|ts|m2ts|mts|m3u8)"

typedef enum {
    MR_DISPLAY_AGA = 0,
    MR_DISPLAY_HAM6,
    MR_DISPLAY_HAM8,
    MR_DISPLAY_CGX
} mr_display_mode;

typedef enum {
    MR_C2P_STANDARD = 0,
    MR_C2P_AKIKO,
    MR_C2P_KALMS,
    MR_C2P_RIVA,
    MR_C2P_WPA
} mr_c2p_mode;

typedef enum {
    MR_H264_PERF_AUTO = 0,
    MR_H264_PERF_QUALITY,
    MR_H264_PERF_BALANCED,
    MR_H264_PERF_FAST
} mr_h264_performance;

typedef struct mr_play_options {
    mr_display_mode display;
    mr_c2p_mode c2p;
    int laced;
    int scale_2x;
    int hls_low;
    unsigned hls_max_width;
    unsigned hls_max_height;
    unsigned hls_max_fps;
    int live_resync;   /* pass --live-resync: catch up / reconnect live streams */
    mr_h264_performance h264_performance;
} mr_play_options;

void mr_play_options_default(mr_play_options *options);
int mr_play_options_parse(mr_play_options *options, int argc, char **argv,
                          char *error, size_t error_size);
int mr_build_player_arguments(char *output, size_t output_size,
                              const mr_play_options *options, const char *url,
                              const char *user_agent, const char *referer);
int mr_build_iptv_arguments(char *output, size_t output_size,
                            const mr_play_options *options);
void mr_play_options_summary(const mr_play_options *options, char *output,
                             size_t output_size);
int mr_path_is_audio_only(const char *path);

#endif
