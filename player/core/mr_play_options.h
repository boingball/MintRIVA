#ifndef MR_PLAY_OPTIONS_H
#define MR_PLAY_OPTIONS_H

#include <stddef.h>

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

#endif
