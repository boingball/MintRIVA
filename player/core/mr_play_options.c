#include "mr_play_options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mr_play_options_default(mr_play_options *o)
{
    if (!o) return;
    memset(o, 0, sizeof(*o));
    o->display = MR_DISPLAY_AGA;
    o->c2p = MR_C2P_STANDARD;
    /* Default to the smallest HLS rendition: it is the one most likely to play
     * on any machine, and picking a bigger one automatically can break a channel
     * that worked (a 720p variant may be a codec we can't decode, or just too
     * heavy). Higher quality is opt-in via --hls-max-height / clearing --hls-low.
     * The picker still selects the *best* variant within the ceiling once low is
     * off, so a caller that raises the ceiling gets the best stream that fits. */
    o->hls_low = 1;
    o->hls_max_width = 640;
    /* On by default for GUI-launched playback (IPTV streams are always live);
     * a direct "mrplay <url>" invocation keeps its own conservative default of
     * off. Disable with --no-live-resync. */
    o->live_resync = 1;
}

static int append_text(char *out, size_t cap, const char *text)
{
    size_t used = strlen(out), length = strlen(text);
    if (used >= cap || length >= cap - used) return 0;
    memcpy(out + used, text, length + 1);
    return 1;
}

static int append_quoted(char *out, size_t cap, const char *value)
{
    size_t used = strlen(out), i;
    if (used && !append_text(out, cap, " ")) return 0;
    if (!append_text(out, cap, "\"")) return 0;
    for (i = 0; value && value[i]; i++) {
        char one[3];
        if (value[i] == '\r' || value[i] == '\n') return 0;
        one[0] = 0;
        if (value[i] == '"' || value[i] == '*') {
            one[0] = '*'; one[1] = value[i]; one[2] = 0;
        } else {
            one[0] = value[i]; one[1] = 0;
        }
        if (!append_text(out, cap, one)) return 0;
    }
    return append_text(out, cap, "\"");
}

static int append_option(char *out, size_t cap, const char *option)
{
    return (!out[0] || append_text(out, cap, " ")) &&
           append_text(out, cap, option);
}

static const char *display_name(mr_display_mode display)
{
    switch (display) {
    case MR_DISPLAY_HAM6: return "ham6";
    case MR_DISPLAY_HAM8: return "ham8";
    case MR_DISPLAY_CGX: return "cgx";
    default: return "aga";
    }
}

static const char *c2p_name(mr_c2p_mode c2p)
{
    switch (c2p) {
    case MR_C2P_AKIKO: return "akiko";
    case MR_C2P_KALMS: return "kalms";
    case MR_C2P_RIVA: return "riva";
    case MR_C2P_WPA: return "wpa";
    default: return "standard";
    }
}

static int append_playback_flags(char *out, size_t cap,
                                 const mr_play_options *o, int explicit)
{
    char number[32];
    if (explicit) {
        if (!append_option(out, cap, "--display") ||
            !append_option(out, cap, display_name(o->display))) return 0;
        if (o->display != MR_DISPLAY_CGX) {
            if (!append_option(out, cap, "--c2p") ||
                !append_option(out, cap, c2p_name(o->c2p)) ||
                !append_option(out, cap, o->laced ? "--laced" : "--no-laced") ||
                !append_option(out, cap, o->scale_2x ? "--scale-2x" :
                                                       "--no-scale-2x")) return 0;
        }
    } else {
        if (o->display == MR_DISPLAY_AGA && !append_option(out, cap, "--aga")) return 0;
        if (o->display == MR_DISPLAY_HAM6 &&
            (!append_option(out, cap, "--aga") || !append_option(out, cap, "--ham6"))) return 0;
        if (o->display == MR_DISPLAY_HAM8 &&
            (!append_option(out, cap, "--aga") || !append_option(out, cap, "--ham"))) return 0;
        if (o->display != MR_DISPLAY_CGX) {
            const char *flag = o->c2p == MR_C2P_AKIKO ? "--cd32" :
                               o->c2p == MR_C2P_KALMS ? "--kalms-c2p" :
                               o->c2p == MR_C2P_RIVA ? "--riva-c2p" :
                               o->c2p == MR_C2P_STANDARD ? "--wpa" : "--c2p";
            if (!append_option(out, cap, flag)) return 0;
            if (o->laced && !append_option(out, cap, "--lace")) return 0;
            if (o->scale_2x && !append_option(out, cap, "--2x")) return 0;
        }
    }
    if (o->hls_low && !append_option(out, cap, "--hls-low")) return 0;
    if (o->hls_max_width) {
        snprintf(number, sizeof(number), "--hls-max-width=%u", o->hls_max_width);
        if (!append_option(out, cap, number)) return 0;
    }
    if (o->hls_max_height) {
        snprintf(number, sizeof(number), "--hls-max-height=%u", o->hls_max_height);
        if (!append_option(out, cap, number)) return 0;
    }
    if (o->hls_max_fps) {
        snprintf(number, sizeof(number), "--hls-max-fps=%u", o->hls_max_fps);
        if (!append_option(out, cap, number)) return 0;
    }
    if (o->live_resync && !append_option(out, cap, "--live-resync")) return 0;
    return 1;
}

int mr_build_player_arguments(char *out, size_t cap,
                              const mr_play_options *o, const char *url,
                              const char *ua, const char *referer)
{
    mr_play_options defaults;
    if (!out || !cap || !url || !*url) return 0;
    if (!o) { mr_play_options_default(&defaults); o = &defaults; }
    out[0] = 0;
    if (!append_playback_flags(out, cap, o, 0)) return 0;
    if (ua && *ua &&
        (!append_option(out, cap, "--user-agent") ||
         !append_quoted(out, cap, ua))) return 0;
    if (referer && *referer &&
        (!append_option(out, cap, "--referer") ||
         !append_quoted(out, cap, referer))) return 0;
    if (!append_quoted(out, cap, url) || !append_text(out, cap, "\n")) return 0;
    return 1;
}

int mr_build_iptv_arguments(char *out, size_t cap, const mr_play_options *o)
{
    mr_play_options defaults;
    if (!out || !cap) return 0;
    if (!o) { mr_play_options_default(&defaults); o = &defaults; }
    out[0] = 0;
    return append_playback_flags(out, cap, o, 1) &&
           append_text(out, cap, "\n");
}

static int parse_uint(const char *text, unsigned *value)
{
    char *end;
    unsigned long parsed;
    if (!text || !*text) return 0;
    parsed = strtoul(text, &end, 10);
    if (*end || parsed > 65535) return 0;
    *value = (unsigned)parsed;
    return 1;
}

int mr_play_options_parse(mr_play_options *o, int argc, char **argv,
                          char *error, size_t error_size)
{
    int i;
    if (!o) return 0;
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i], *value;
        if (!strcmp(arg, "--display")) {
            if (i + 1 >= argc) goto bad;
            value = argv[++i];
            if (!strcmp(value, "aga")) o->display = MR_DISPLAY_AGA;
            else if (!strcmp(value, "ham6")) o->display = MR_DISPLAY_HAM6;
            else if (!strcmp(value, "ham8")) o->display = MR_DISPLAY_HAM8;
            else if (!strcmp(value, "cgx") || !strcmp(value, "rtg")) o->display = MR_DISPLAY_CGX;
            else goto bad;
        } else if (!strcmp(arg, "--c2p")) {
            if (i + 1 >= argc) goto bad;
            value = argv[++i];
            if (!strcmp(value, "standard")) o->c2p = MR_C2P_STANDARD;
            else if (!strcmp(value, "akiko")) o->c2p = MR_C2P_AKIKO;
            else if (!strcmp(value, "kalms")) o->c2p = MR_C2P_KALMS;
            else if (!strcmp(value, "riva")) o->c2p = MR_C2P_RIVA;
            else if (!strcmp(value, "wpa")) o->c2p = MR_C2P_WPA;
            else goto bad;
        } else if (!strcmp(arg, "--laced")) o->laced = 1;
        else if (!strcmp(arg, "--no-laced")) o->laced = 0;
        else if (!strcmp(arg, "--scale-2x")) o->scale_2x = 1;
        else if (!strcmp(arg, "--no-scale-2x")) o->scale_2x = 0;
        else if (!strcmp(arg, "--hls-low")) o->hls_low = 1;
        else if (!strcmp(arg, "--live-resync")) o->live_resync = 1;
        else if (!strcmp(arg, "--no-live-resync")) o->live_resync = 0;
        else if (!strncmp(arg, "--hls-max-width=", 16)) {
            if (!parse_uint(arg + 16, &o->hls_max_width)) goto bad;
        } else if (!strncmp(arg, "--hls-max-height=", 17)) {
            if (!parse_uint(arg + 17, &o->hls_max_height)) goto bad;
        } else if (!strncmp(arg, "--hls-max-fps=", 14)) {
            if (!parse_uint(arg + 14, &o->hls_max_fps)) goto bad;
        } else goto bad;
    }
    return 1;
bad:
    if (error && error_size) snprintf(error, error_size, "invalid playback option near %s", argv[i]);
    return 0;
}

/* Short description of the current HLS rendition policy for the status line. */
static void hls_policy_text(const mr_play_options *o, char *out, size_t cap)
{
    if (o->hls_low)
        snprintf(out, cap, "HLS low");
    else if (o->hls_max_height)
        snprintf(out, cap, "HLS <=%up", o->hls_max_height);
    else
        snprintf(out, cap, "HLS best");
}

void mr_play_options_summary(const mr_play_options *o, char *out, size_t cap)
{
    char hls[24];
    if (!out || !cap || !o) return;
    hls_policy_text(o, hls, sizeof hls);
    if (o->display == MR_DISPLAY_CGX)
        snprintf(out, cap, "Playback: RTG / %s%s", hls,
                 o->live_resync ? " / Live-resync" : "");
    else
        snprintf(out, cap, "Playback: %s / %s / Lace %s / 2x %s / %s%s",
                 o->display == MR_DISPLAY_HAM6 ? "HAM6" :
                 o->display == MR_DISPLAY_HAM8 ? "HAM8" : "AGA",
                 c2p_name(o->c2p), o->laced ? "on" : "off",
                 o->scale_2x ? "on" : "off", hls,
                 o->live_resync ? " / Live-resync" : "");
}
