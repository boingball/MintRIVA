/*
 * MintRIVA - Amiga player.
 *
 * Ties the proven portable core (demux + decoder) to the Amiga display + audio
 * backends: load file -> auto-detect container -> decode frames / enqueue audio
 * -> blit, with audio as the A/V master clock (video frames are held until
 * Paula playback reaches their timestamp). Falls back to frame-rate pacing when
 * there is no audio. ESC or the close gadget quits.
 *
 * AVI, MOV/MP4 and MPEG-TS containers are file-backed: only metadata and the
 * current compressed packet live in RAM. Raw elementary streams and MPEG-1
 * retain the original whole-file fallback.
 *
 *   mrplay <file.avi|file.mov>
 */
#include "../core/mr_demux.h"
#include "../core/mr_hls.h"
#include "../core/mr_http.h"
#include "../core/mr_youtube.h"
#include "../core/mr_codec.h"
#include "../core/mr_rawvideo.h"
#include "../core/mr_mpeg1.h"
#include "../core/mr_h264.h"
#include "../audio/mr_audio_decode.h"
#include "../iptv/mr_iptv.h"
#include "amiga_display.h"
#include "mr_audio.h"
#include "mr_player_status.h"
#include "hls_prefetch.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <clib/alib_protos.h>
#include <devices/timer.h>
#include <exec/memory.h>
#include <exec/nodes.h>
#include <exec/ports.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * libavc's P/B-slice reference-list setup has stack frames above 20 KiB
 * before its callers and the AmigaOS libraries are accounted for.  Classic
 * Shells commonly provide only 4 KiB, which corrupts memory during H.264
 * playback and makes the eventual EOF/ESC teardown appear to crash.  AmigaOS
 * versions with stack-cookie support raise the process stack to this minimum;
 * older systems can use "Stack 320000" before launching mrplay.
 */
static const char mr_min_stack[] __attribute__((used)) = "$STACK:320000";

/*
 * mrplay handles Ctrl-C / Ctrl-F itself: the IPTV controller signals them to
 * ask the player to stop, and the main loop (control_signal_event) turns them
 * into a clean quit that closes the display and Paula. Left to libnix, a
 * pending Ctrl-C during any stdio call - frequent when --time logging is on -
 * fires its "***Break" handler and exit()s the process mid-printf, skipping
 * CloseWindow() and leaving an orphaned RTG window stuck on screen. An empty
 * __chkabort() disables that automatic abort; the break signals still arrive as
 * exec signals and are serviced by our own event loop.
 */
void __chkabort(void) { }

/* Upper ceiling on the decoded-frame ring - the size of the vq[] array only.
 * The ACTIVE ring is video_cap (chosen at runtime from the audio cushion and
 * free RAM, see main), and video_cap is the modulus for all qhead arithmetic,
 * so only video_cap slots are ever touched and the RGB footprint is exactly
 * video_cap * frame_bytes. (Modulo the whole array instead and qhead would cycle
 * every slot, allocating an RGB buffer in each - the footprint would be this
 * ceiling * frame_bytes regardless of video_cap, which once ate all of Fast RAM.)
 * The array structs are tiny; only the lazily-allocated RGB buffers cost. */
#define VIDEO_QUEUE_CAP 48
/* Default decoded-frame depths, clamped to free RAM and raisable by --net-queue.
 * In the video-ahead regime the presented rate is about depth / cushion_seconds
 * (topping up the audio cushion discards frames decoded past the cap), so a
 * deeper queue presents more before each gap: measured ~5 fps at depth 14 vs
 * ~2 fps at depth 4 against the 2.5 s cushion. Keep it as deep as RAM allows. */
#define VIDEO_QUEUE_NET_DEPTH  16
#define VIDEO_QUEUE_DISK_DEPTH 16
/* Never let the decoded queue eat RAM below this: it must leave room for the
 * segment fetch, decoder and TLS. An over-deep queue once filled Fast RAM to
 * the safety floor and starved playback into an endless reconnect (see the
 * runtime sizing in main). */
#define VIDEO_QUEUE_MEM_FLOOR (8UL * 1024 * 1024)
#define STATS_INTERVAL_US 3000000ULL
#define PRESENTATION_GUARD_US 4000ULL
#define AUDIO_REFILL_WARNING_MS 120UL
#define AUDIO_RESCUE_ENTRY_MS 100UL
#define AUDIO_RESCUE_TARGET_MS 200UL
#define AUDIO_RESCUE_FIFO_NEAR_EMPTY_MS 20UL
#define AUDIO_RESCUE_ONE_REQUEST_MS 100UL
#define AUDIO_STARTUP_TARGET_MS 400UL
#define AUDIO_CUSHION_MIN_MS 400UL
/* 2.5 s is the legacy/direct-path ceiling: enough to ride the ~1.7 s stalls
 * seen in hardware logs, but a decoded video ring shallower than the
 * corresponding number of frames cannot stay smooth while filling that much
 * audio. For synchronous network playback the active cushion is therefore
 * reduced after video_cap is known to roughly (video_cap - 2) frame periods,
 * clamped between AUDIO_CUSHION_MIN_MS and this ceiling. Local files keep the
 * ceiling, and the prefetch worker has its own smaller target below. */
#define AUDIO_CUSHION_TARGET_MS 2500UL
/* Cushion when the prefetch worker is on: it absorbs segment stalls at the
 * (cheap) compressed level, so the decoder need only reach a little ahead. */
#define AUDIO_CUSHION_PREFETCH_MS 600UL
/* Live-resync (opt-in, --live-resync, network sources only). A multi-second
 * network stall can leave a live stream many seconds behind the wall clock with
 * the audio clock unable to climb back; these bound the catch-up-to-live burst.
 * The trigger is far above any jitter normal playback produces. */
#define LIVE_RESYNC_BEHIND_US 4000000ULL  /* >4 s behind wall clock: catch up  */
#define LIVE_RESYNC_TARGET_US 1000000ULL  /* aim to land ~1 s behind the edge   */
#define LIVE_RESYNC_MAX_US    3000000ULL  /* hard cap on one catch-up burst     */
#define LIVE_RESYNC_EDGE_US   2000000ULL  /* a read slower than any normal fetch
                                           * means we have caught the frontier   */
#define LIVE_RECONNECT_TRIES  200         /* reopen attempts before giving up;
                                           * with backoff this is ~15 min, enough
                                           * to ride out a flaky mobile link      */
#define LIVE_RECONNECT_STALL_LIMIT 3      /* consecutive reopens with no playback
                                           * before giving up (avoids a spin)     */
#define AUDIO_RESCUE_MAX_PACKETS 16U
#define AUDIO_RESCUE_MAX_US 100000ULL

static struct MsgPort *timer_port;
static struct timerequest *timer_request;
struct Device *TimerBase;

/* Control port: published on exec's public port list so the IPTV controller can
 * discover this player and signal it (Ctrl-F clean stop, Ctrl-C forced stop)
 * before launching another stream. We never receive messages through it, only
 * signals aimed at mp_SigTask, so it is created PA_IGNORE. The port carries an
 * embedded status block (see mr_player_status.h) so the controller can read
 * back the codec that is playing, or why a stream refused to play. */
static mr_player_control_port *control_block;
static mr_audio *control_audio;
static int control_volume = 64;
static int deferred_player_event;
static ULONG status_file_seq;
static int playing_status_published;
static char playing_status_codec[MR_PLAYER_CODEC_MAX];
static char playing_status_text[MR_PLAYER_STATUS_TEXT_MAX];

/* One-shot high-resolution pipeline breadcrumb. It advances only when a new
 * stage is reached, so diagnostics cannot turn every 1080p frame into DOS I/O. */
static int h264_pipeline_diag_enabled;
static int h264_pipeline_stage;
static int h264_pipeline_width, h264_pipeline_height;

static void h264_pipeline_checkpoint_player(const char *stage, int qcount,
                                            int playback_started)
{
    BPTR fh;
    char buf[256];
    int n;
    ULONG fast_total, fast_largest;

    if (!h264_pipeline_diag_enabled || !stage) return;
    fh = Open((CONST_STRPTR)"RAM:MintRIVA-H264.pipeline", MODE_NEWFILE);
    if (!fh) return;
    fast_total = AvailMem(MEMF_FAST);
    fast_largest = AvailMem(MEMF_FAST | MEMF_LARGEST);
    n = snprintf(buf, sizeof buf,
                 "stage=%s res=%dx%d qcount=%d playback=%d fast=%lu "
                 "fast_largest=%lu\n",
                 stage, h264_pipeline_width, h264_pipeline_height, qcount,
                 playback_started, (unsigned long)fast_total,
                 (unsigned long)fast_largest);
    if (n > 0) {
        LONG bytes = n < (int)sizeof buf ? (LONG)n : (LONG)(sizeof buf - 1);
        Write(fh, (APTR)buf, bytes);
    }
    Close(fh);
}

static void control_port_close(void)
{
    if (control_block) {
        RemPort(&control_block->port);
        FreeMem(control_block, sizeof *control_block);
        control_block = NULL;
    }
}

static void control_port_open(void)
{
    mr_player_control_port *block;
    /* Only one player should ever be live. If a port already exists another
     * player is still shutting down; skip rather than publish a duplicate. */
    Forbid();
    if (FindPort((CONST_STRPTR)MR_IPTV_PLAYER_PORT)) { Permit(); return; }
    Permit();
    block = (mr_player_control_port *)AllocMem(sizeof *block,
                                               MEMF_PUBLIC | MEMF_CLEAR);
    if (!block) return;
    block->port.mp_Node.ln_Type = NT_MSGPORT;
    block->port.mp_Node.ln_Name = (char *)MR_IPTV_PLAYER_PORT;
    block->port.mp_Flags = PA_IGNORE;
    block->port.mp_SigTask = FindTask(NULL);
    NewList(&block->port.mp_MsgList);
    block->status.state = MR_PLAYER_STATE_STARTING;
    block->status.magic = MR_PLAYER_STATUS_MAGIC;
    AddPort(&block->port);
    control_block = block;
    atexit(control_port_close);
}

/* Publish a status update for the IPTV controller (no-op if the port was not
 * created). Thin wrapper so call sites read cleanly. */
static void player_status(LONG state, const char *codec, const char *text)
{
    mr_player_status_snapshot snapshot;
    BPTR file;
    mr_player_status_set(control_block, state, codec, text);
    memset(&snapshot, 0, sizeof snapshot);
    snapshot.status.magic = MR_PLAYER_STATUS_MAGIC;
    snapshot.status.seq = ++status_file_seq;
    snapshot.status.state = state;
    mr_player_status_copy(snapshot.status.codec,
                          sizeof snapshot.status.codec, codec ? codec : "");
    mr_player_status_copy(snapshot.status.text,
                          sizeof snapshot.status.text, text ? text : "");
    snapshot.trailer_magic = MR_PLAYER_STATUS_MAGIC;
    file = Open((CONST_STRPTR)MR_PLAYER_STATUS_TMP, MODE_NEWFILE);
    if (file) {
        LONG written = Write(file, &snapshot, (LONG)sizeof snapshot);
        Close(file);
        if (written == (LONG)sizeof snapshot) {
            DeleteFile((CONST_STRPTR)MR_PLAYER_STATUS_FILE);
            if (!Rename((CONST_STRPTR)MR_PLAYER_STATUS_TMP,
                        (CONST_STRPTR)MR_PLAYER_STATUS_FILE))
                DeleteFile((CONST_STRPTR)MR_PLAYER_STATUS_TMP);
        } else {
            DeleteFile((CONST_STRPTR)MR_PLAYER_STATUS_TMP);
        }
    }
}

static void player_prepare_playing_status(const char *codec, const char *text)
{
    mr_player_status_copy(playing_status_codec,
                          sizeof playing_status_codec, codec ? codec : "");
    mr_player_status_copy(playing_status_text,
                          sizeof playing_status_text, text ? text : "");
    playing_status_published = 0;
}

static void player_first_frame_presented(void)
{
    if (playing_status_published) return;
    playing_status_published = 1;
    player_status(MR_PLAYER_STATE_PLAYING, playing_status_codec,
                  playing_status_text);
}

/* Turn a video fourcc into its four printable characters (non-printables shown
 * as '?'), for status messages about codecs we cannot decode. */
static void fourcc_to_tag(uint32_t fourcc, char tag[5])
{
    int i;
    tag[0] = (char)(fourcc & 255);
    tag[1] = (char)((fourcc >> 8) & 255);
    tag[2] = (char)((fourcc >> 16) & 255);
    tag[3] = (char)((fourcc >> 24) & 255);
    tag[4] = 0;
    for (i = 0; i < 4; i++)
        if (tag[i] < 32 || tag[i] > 126) tag[i] = '?';
}

/* Friendly name for a video codec we have no decoder for, so the IPTV status
 * line names the missing codec (and we know what to port next). NULL when the
 * fourcc is not one of the common streaming codecs. */
static const char *unsupported_codec_name(uint32_t fourcc)
{
    char tag[5];
    int i;
    fourcc_to_tag(fourcc, tag);
    for (i = 0; i < 4; i++)
        if (tag[i] >= 'a' && tag[i] <= 'z') tag[i] = (char)(tag[i] - 32);
    if (!strcmp(tag, "H264") || !strcmp(tag, "AVC1") || !strcmp(tag, "X264") ||
        !strcmp(tag, "DAVC")) return "H.264/AVC";
    if (!strcmp(tag, "HEVC") || !strcmp(tag, "HVC1") || !strcmp(tag, "HEV1") ||
        !strcmp(tag, "H265") || !strcmp(tag, "DHEV")) return "H.265/HEVC";
    if (!strcmp(tag, "AV01")) return "AV1";
    if (!strcmp(tag, "VP80")) return "VP8";
    if (!strcmp(tag, "VP90")) return "VP9";
    if (!strcmp(tag, "VP60") || !strcmp(tag, "VP61") || !strcmp(tag, "VP62"))
        return "VP6";
    if (!strcmp(tag, "WMV1") || !strcmp(tag, "WMV2")) return "Windows Media Video";
    if (!strcmp(tag, "WMV3")) return "Windows Media Video 9";
    if (!strcmp(tag, "VC1") || !strcmp(tag, "WVC1")) return "VC-1";
    if (!strcmp(tag, "THEO")) return "Theora";
    if (!strcmp(tag, "FLV1")) return "Sorenson Spark";
    if (!strcmp(tag, "SVQ1") || !strcmp(tag, "SVQ3")) return "Sorenson Video";
    return NULL;
}

/* Build the "reason" line for a video codec we cannot decode. */
static void describe_unsupported_video(uint32_t fourcc, char *buf, size_t n)
{
    const char *name = unsupported_codec_name(fourcc);
    char tag[5];
    fourcc_to_tag(fourcc, tag);
    if (name)
        snprintf(buf, n, "%s video (fourcc '%s') has no decoder", name, tag);
    else
        snprintf(buf, n, "video codec '%s' has no decoder", tag);
}

/* Keep a failure status readable by the IPTV controller for a moment before we
 * exit, but bail out promptly if the controller asks us to stop. The controller
 * polls a few times a second, so a short hold is enough to be seen. */
static void status_hold(void)
{
    int i;
    for (i = 0; i < 20; i++) { /* up to ~2 s at 1/10 s ticks */
        if (SetSignal(0, 0) & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F))
            break;
        Delay(5);
    }
}

static void playback_timer_close(void);

static int playback_timer_open(void)
{
    timer_port = CreateMsgPort();
    if (!timer_port) return 0;
    timer_request = (struct timerequest *)CreateIORequest(timer_port,
                                                          sizeof *timer_request);
    if (!timer_request) {
        playback_timer_close();
        return 0;
    }
    if (OpenDevice((CONST_STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)timer_request, 0) != 0) {
        playback_timer_close();
        return 0;
    }
    TimerBase = timer_request->tr_node.io_Device;
    return 1;
}

static void playback_timer_close(void)
{
    if (TimerBase && timer_request)
        CloseDevice((struct IORequest *)timer_request);
    TimerBase = NULL;
    if (timer_request) DeleteIORequest((struct IORequest *)timer_request);
    if (timer_port) DeleteMsgPort(timer_port);
    timer_request = NULL; timer_port = NULL;
}

/* Every main() exit, including resolver/stream failures, must release Amiga
 * device and library state explicitly. Repeated failed launches otherwise
 * leave timer.device requests and, on some libnix setups, socket/AmiSSL state
 * behind until reboot. All three shutdowns are idempotent. */
static int mrplay_exit(int code)
{
    playback_timer_close();
    mr_http_net_shutdown();
    control_port_close();
    return code;
}

/* ---- Media-clock source tags ------------------------------------------- */
#define MCLOCK_MONO     0   /* no audio: caller uses raw monotonic elapsed   */
#define MCLOCK_AUDIO    1   /* audio healthy: signed offset from audio raw    */
#define MCLOCK_HOLDOVER 2   /* audio starved: advance from frozen media time  */

/*
 * Consecutive non-starved scheduler iterations required before holdover exit
 * may be attempted.  This stability condition is necessary but not sufficient:
 * the audio raw counter must also have reached or passed last_output_us before
 * the signed offset can represent the media/audio relationship without clamping
 * the mapped candidate behind the continuous media timeline.
 */
#define MCLOCK_RESUME_STABLE_ITERS 3U

/*
 * Stateful media-clock controller.
 *
 * Bridges audio underruns with a monotonic holdover so that the media timeline
 * never jumps forward to raw wall-clock time when Paula is starved.
 *
 * On resume, audio_to_media_offset_us maps the raw audio counter back onto the
 * continuous holdover position regardless of whether raw audio is ahead of or
 * behind the frozen media time.  This signed representation replaces the old
 * unsigned clock_base_us which could only subtract from the audio counter and
 * therefore had to reset to zero whenever media was ahead of raw audio.
 */
typedef struct {
    uint64_t last_output_us;           /* last returned value (monotonic gate)*/
    uint64_t holdover_media_us;        /* media time captured on starvation   */
    uint64_t holdover_started_mono_us; /* monotonic time at holdover entry    */
    int64_t  audio_to_media_offset_us; /* media = audio_raw + this (signed)   */
    unsigned healthy_audio_samples;    /* non-starved iters since holdover     */
    int      holdover_active;
    int      source;                   /* MCLOCK_MONO / AUDIO / HOLDOVER       */
} media_clock;

/*
 * Rebase the media clock to a new (audio_raw, target_media_us) correspondence.
 * Call on playback start, pause/resume, or loop restart.
 */
static void media_clock_rebase(media_clock *mc, uint64_t audio_raw,
                               uint64_t target_media_us)
{
    mc->audio_to_media_offset_us = (int64_t)target_media_us - (int64_t)audio_raw;
    mc->holdover_active  = 0;
    mc->healthy_audio_samples = 0;
    mc->last_output_us   = target_media_us;
    mc->source           = MCLOCK_AUDIO;
}

/*
 * Estimate current media time without advancing mc->last_output_us.
 * Used for post-rescue frame-drop evaluation before the main clock tick.
 */

/* Clamp a signed microsecond value to a non-negative uint64_t. */
static uint64_t s64_to_us(int64_t v) { return v > 0 ? (uint64_t)v : 0; }

static uint64_t media_clock_rescue_estimate(const media_clock *mc,
                                            uint64_t audio_raw, uint64_t now)
{
    uint64_t candidate;
    if (mc->holdover_active) {
        candidate = mc->holdover_media_us +
                    (now - mc->holdover_started_mono_us);
    } else {
        candidate = s64_to_us((int64_t)audio_raw + mc->audio_to_media_offset_us);
    }
    return candidate < mc->last_output_us ? mc->last_output_us : candidate;
}

/*
 * Advance the stateful media clock by one scheduler iteration.
 *
 * - Healthy audio   → MCLOCK_AUDIO: candidate = audio_raw + offset (signed).
 * - Starvation      → MCLOCK_HOLDOVER: advance from frozen media time using
 *                     monotonic elapsed since starvation began.
 * - Audio resumes   → require MCLOCK_RESUME_STABLE_ITERS non-starved iters
 *                     AND audio_raw >= last_output_us before exiting holdover.
 *                     Both conditions are necessary; stability alone is not
 *                     sufficient.  While waiting, remain in MCLOCK_HOLDOVER.
 *
 * The returned value is monotonically non-decreasing (never < last_output_us).
 * When !have_audio, sets source=MCLOCK_MONO and returns last_output_us — the
 * caller should substitute its own mono_media_clock_us in that case.
 */
static uint64_t current_media_clock_us(media_clock *mc, int have_audio,
                                       int starved, uint64_t audio_raw,
                                       uint64_t now, int want_time)
{
    uint64_t candidate;

    if (!have_audio) {
        mc->source = MCLOCK_MONO;
        return mc->last_output_us;
    }

    if (starved) {
        if (!mc->holdover_active) {
            mc->holdover_media_us         = mc->last_output_us;
            mc->holdover_started_mono_us  = now;
            mc->holdover_active           = 1;
            mc->healthy_audio_samples     = 0;
            if (want_time)
                printf("clock-holdover entered media=%lu us\n",
                       (unsigned long)mc->holdover_media_us);
        }
        /* Reset stability counter on every starved iteration so that partial
         * recovery attempts that re-stall require a fresh run of stable iters. */
        mc->healthy_audio_samples = 0;
        mc->source = MCLOCK_HOLDOVER;
        candidate = mc->holdover_media_us +
                    (now - mc->holdover_started_mono_us);
        if (candidate < mc->last_output_us) candidate = mc->last_output_us;
        mc->last_output_us = candidate;
        return candidate;
    }

    /* Audio not starved. */
    if (mc->holdover_active) {
        mc->healthy_audio_samples++;

        /* Stability gate: need MCLOCK_RESUME_STABLE_ITERS good iterations. */
        if (mc->healthy_audio_samples < MCLOCK_RESUME_STABLE_ITERS) {
            mc->source = MCLOCK_HOLDOVER;
            candidate = mc->holdover_media_us +
                        (now - mc->holdover_started_mono_us);
            if (candidate < mc->last_output_us) candidate = mc->last_output_us;
            mc->last_output_us = candidate;
            return candidate;
        }

        if (audio_raw < mc->last_output_us) {
            /*
             * Audio has resumed but has not yet caught up to the holdover
             * media position.  Stay in holdover — do not reset the base or
             * change source.  Exiting now would leave the mapped audio
             * candidate behind last_output_us, causing the monotonic clamp
             * to freeze the clock until audio catches up.
             */
            if (want_time)
                printf("clock-holdover audio-raw=%lu us < resume-at=%lu us, "
                       "remaining in holdover\n",
                       (unsigned long)audio_raw,
                       (unsigned long)mc->last_output_us);
            mc->source = MCLOCK_HOLDOVER;
            candidate = mc->holdover_media_us +
                        (now - mc->holdover_started_mono_us);
            if (candidate < mc->last_output_us) candidate = mc->last_output_us;
            mc->last_output_us = candidate;
            return candidate;
        }

        /*
         * Both gates passed: exit holdover.  Compute a signed offset so that
         * the audio counter maps exactly onto the current continuous media
         * position, supporting both "media behind audio" (negative) and
         * "media ahead of audio" (positive) cases.
         */
        mc->audio_to_media_offset_us = (int64_t)mc->last_output_us -
                                       (int64_t)audio_raw;
        mc->holdover_active = 0;
        if (want_time)
            printf("clock-holdover exit audio-raw=%lu us resume-at=%lu us "
                   "offset=%ld us\n",
                   (unsigned long)audio_raw,
                   (unsigned long)mc->last_output_us,
                   (long)mc->audio_to_media_offset_us);
    }

    /* Normal audio mode: audio_raw mapped through signed offset. */
    mc->source = MCLOCK_AUDIO;
    candidate = s64_to_us((int64_t)audio_raw + mc->audio_to_media_offset_us);
    if (candidate < mc->last_output_us) candidate = mc->last_output_us;
    mc->last_output_us = candidate;
    return candidate;
}

/* ---- end media-clock controller ---------------------------------------- */

typedef struct queued_video {
    unsigned char *rgb;
    size_t capacity;
    int width, height, stride, dirty_y0, dirty_y1;
    uint64_t pts_us;
    uint64_t decoded_at_us;
    unsigned long decode_us;
} queued_video;

typedef struct playback_stats {
    uint64_t since_us, network_us, demux_us, audio_decode_us, video_decode_us;
    uint64_t convert_us, scale_us, display_us, sleep_requested_us, sleep_actual_us;
    uint64_t latency_us, refill_block_us, refill_delayed_ready_us;
    unsigned long video_decode_max_us, display_max_us, sleep_max_error_us;
    unsigned decoded, presented, late, dropped, samples;
    uint64_t rtg_prepare_us, rtg_scale_us, rtg_convert_us, rtg_copy_us;
    uint64_t rtg_blit_us, rtg_clip_us, rtg_total_us;
    uint64_t frame_copy_us;
    unsigned long rtg_prepare_max_us, rtg_blit_max_us;
    uint64_t h264_input_us, h264_core_us, h264_output_us;
    unsigned long h264_input_max_us, h264_core_max_us, h264_output_max_us;
    uint64_t rescue_us;
    unsigned rescue_entries, rescue_packets, rescue_audio_packets;
    unsigned rescue_video_decoded, rescue_video_queued, rescue_video_skipped;
    unsigned rescue_critical, rescue_noncritical, rescue_video_replaced;
    uint64_t rescue_newest_retained_pts_us;
    int64_t rescue_post_lateness_us;
    unsigned long rescue_max_us;
    unsigned rescue_exit_target, rescue_exit_limit, rescue_exit_eof;
    long rescue_min_margin_ms;
    unsigned rescue_negative_margin, rescue_hw_starvations;
    unsigned dropped_after_scale;
    unsigned long audio_before, audio_after;
    uint64_t frame_pts_us, audio_clock_us;
    int64_t calculated_lateness_us;
    unsigned queue_head, dropped_in_pass, timing_rebases;
    mr_display_timing last_rtg;
} playback_stats;

/*
 * Present-during-fetch context.
 *
 * The player is single-threaded: while it is parked in a blocking segment fetch
 * (mr_demux_next_packet -> HLS segment open/read) the main loop cannot reach its
 * own presentation path, so without help the picture freezes for the whole
 * fetch. The demux/decoder/display service hooks already pump
 * service_audio_for_display() throughout that stall to keep Paula fed; this
 * struct lets that same callback also advance video, presenting frames from the
 * already-decoded queue as they fall due against the audio clock.
 *
 * This is NOT a second presenter racing the loop. The main loop hands the queue
 * to the callback only for the duration of the blocking read (vp->released) and
 * re-reads qhead/qcount afterwards, so exactly one side ever touches the queue
 * at a time - single task, no lock, nothing to kill on teardown. `presenting`
 * guards the reentrancy a blit's own service pump would otherwise cause. All
 * fields point at the scheduler's live locals so the two paths share one queue.
 */
typedef struct video_presenter {
    int                  released;      /* main loop blocked: callback may present */
    int                  presenting;    /* reentrancy guard (a blit re-enters us)  */
    amiga_display       *disp;
    mr_audio            *audio;
    media_clock         *mc;
    const mr_video_info *vi;
    queued_video        *vq;
    int                  video_cap;        /* ring modulus - see main()'s sizing     */
    int                 *qhead, *qcount;
    int                 *playback_started;
    uint64_t            *mono_base_us;
    int                 *frames;
    playback_stats      *stats;
    int                  want_time;
} video_presenter;

typedef struct scheduler_trace {
    mr_audio *audio;
    const char *phase, *previous_phase;
    uint64_t phase_started_us, previous_duration_us, last_service_us;
    uint64_t sleep_requested_us, sleep_actual_us;
    unsigned long delay_ticks;
    int enabled;
    video_presenter *presenter;        /* NULL until the scheduler wires it up   */
} scheduler_trace;

static uint64_t monotonic_us(void);

static void trace_phase(scheduler_trace *trace, const char *phase)
{
    uint64_t now;
    if (!trace) return;
    now = monotonic_us();
    if (trace->phase) {
        trace->previous_phase = trace->phase;
        trace->previous_duration_us = now - trace->phase_started_us;
    }
    trace->phase = phase;
    trace->phase_started_us = now;
}

/*
 * Present the queue's front frame if it is due, from inside a service pump.
 *
 * Runs only while the main loop has released the queue (vp->released) for a
 * blocking fetch, so it never contends with the loop's own presentation path.
 * It mirrors the scheduler's media-clock derivation and its "drop frames more
 * than a period overdue, keep the newest" policy, so presenting here looks
 * identical to presenting from the loop - it just happens while the one task is
 * otherwise stuck in recv(). At most one frame is blitted per call; the loop
 * re-reads qhead/qcount when the fetch returns. `presenting` swallows the nested
 * service call display_show_rgb's own pump triggers.
 */
static void present_service_frame(video_presenter *vp)
{
    media_clock *mc;
    uint64_t period_us, master_clock_us, now;
    int64_t late_us;
    queued_video *front;

    if (!vp || !vp->released || vp->presenting) return;
    if (!*vp->playback_started || *vp->qcount <= 0) return;
    vp->presenting = 1;

    mc = vp->mc;
    now = monotonic_us();
    period_us = vp->vi->rate
        ? (uint64_t)(vp->vi->scale ? vp->vi->scale : 1) * 1000000ULL / vp->vi->rate
        : 83333ULL;

    /* Same master clock the scheduler uses: the audio clock when present (with
     * its starvation holdover), the monotonic fallback otherwise. */
    if (vp->audio)
        master_clock_us = current_media_clock_us(mc, 1, audio_starved(vp->audio),
                                                 audio_elapsed_us(vp->audio),
                                                 now, vp->want_time);
    else {
        mc->source = MCLOCK_MONO;
        master_clock_us = now - *vp->mono_base_us;
    }

    front = &vp->vq[*vp->qhead];
    late_us = (int64_t)master_clock_us - (int64_t)front->pts_us;

    /* Front not due yet: leave it for a later service tick or the loop. */
    if (late_us < -(int64_t)PRESENTATION_GUARD_US) { vp->presenting = 0; return; }

    /* Resync to the clock rather than replay a backlog when the stall clears:
     * drop frames a whole period or more overdue, keeping the newest. */
    while (late_us > (int64_t)period_us && *vp->qcount > 1) {
        vp->stats->dropped++;
        *vp->qhead = (*vp->qhead + 1) % vp->video_cap;
        (*vp->qcount)--;
        front = &vp->vq[*vp->qhead];
        late_us = (int64_t)master_clock_us - (int64_t)front->pts_us;
    }
    if (late_us > 0) vp->stats->late++;

    if (h264_pipeline_diag_enabled && h264_pipeline_stage < 4) {
        h264_pipeline_checkpoint_player("pre-display-service",
                                        *vp->qcount, *vp->playback_started);
        h264_pipeline_stage = 4;
    }
    display_show_rgb(vp->disp, front->rgb, front->width, front->height,
                     front->stride, front->dirty_y0, front->dirty_y1);
    player_first_frame_presented();
    if (h264_pipeline_diag_enabled && h264_pipeline_stage < 5) {
        h264_pipeline_checkpoint_player("post-display-service",
                                        *vp->qcount, *vp->playback_started);
        h264_pipeline_stage = 5;
    }
    vp->stats->latency_us += monotonic_us() - front->decoded_at_us;
    vp->stats->presented++;
    if (vp->frames) (*vp->frames)++;
    *vp->qhead = (*vp->qhead + 1) % vp->video_cap;
    (*vp->qcount)--;
    vp->presenting = 0;
}

static void service_audio_for_display(void *opaque)
{
    scheduler_trace *trace = (scheduler_trace *)opaque;
    uint64_t now = monotonic_us();
    if (trace->enabled && trace->last_service_us &&
        now - trace->last_service_us > 40000ULL) {
        printf("audio-gap=%lu ms phase=%s phase-duration=%lu ms previous-phase=%s "
               "previous-duration=%lu ms sleep-request=%lu ms "
               "sleep-actual=%lu ms delay-ticks=%lu\n",
               (unsigned long)((now - trace->last_service_us) / 1000),
               trace->phase ? trace->phase : "unknown",
               (unsigned long)((now - trace->phase_started_us) / 1000),
               trace->previous_phase ? trace->previous_phase : "none",
               (unsigned long)(trace->previous_duration_us / 1000),
               (unsigned long)(trace->sleep_requested_us / 1000),
               (unsigned long)(trace->sleep_actual_us / 1000),
               trace->delay_ticks);
    }
    audio_service(trace->audio);
    trace->last_service_us = monotonic_us();
    /* Audio first (it is the master clock), then advance video against it if the
     * scheduler has released the queue for a blocking fetch. */
    if (trace->presenter) present_service_frame(trace->presenter);
}

/* EClock is per-machine monotonic and normally much finer than the 20 ms DOS
 * tick. Keeping all scheduling in integer microseconds avoids truncating a
 * 25 fps period into alternating/coarse Delay() ticks. */
static uint64_t monotonic_us(void)
{
    struct EClockVal value;
    ULONG frequency = TimerBase ? ReadEClock(&value) : 0;
    uint64_t ticks = ((uint64_t)value.ev_hi << 32) | value.ev_lo;
    return frequency ? ticks * 1000000ULL / frequency :
           (uint64_t)clock() * 1000000ULL / CLOCKS_PER_SEC;
}

static void paced_sleep(uint64_t usec, scheduler_trace *trace,
                        playback_stats *st)
{
    uint64_t begin, end;
    if (!usec) return;
    begin = monotonic_us();
    trace_phase(trace, "paced-sleep");
    trace->sleep_requested_us = usec;
    trace->sleep_actual_us = 0;
    trace->delay_ticks = 0;
    st->sleep_requested_us += usec;
    /* Delay is only the coarse backoff. Recheck the absolute deadline so an
     * oversleep is measured rather than carried into the next frame. */
    while ((end = monotonic_us()) - begin < usec) {
        uint64_t left = usec - (end - begin);
        if (trace->audio) service_audio_for_display(trace);
        /* A one-tick Delay is a forced 20 ms oversleep for short deadlines.
         * Spin on EClock and service Paula until at least two ticks remain. */
        if (left > 40000) {
            LONG ticks = (LONG)((left - 20000) / 20000);
            uint64_t delay_begin = monotonic_us();
            if (ticks < 1) ticks = 1;
            trace->delay_ticks = (unsigned long)ticks;
            Delay(ticks);
            trace->sleep_actual_us += monotonic_us() - delay_begin;
            if (trace->audio) service_audio_for_display(trace);
        }
    }
    end = monotonic_us();
    st->sleep_actual_us += end - begin;
    trace->sleep_actual_us = end - begin;
    if (end - begin > usec && end - begin - usec > st->sleep_max_error_us)
        st->sleep_max_error_us = (unsigned long)(end - begin - usec);
}

static int queue_copy(queued_video *q, const mr_frame *fr, uint64_t pts,
                      uint64_t decoded_at, unsigned long decode_us)
{
    size_t bytes = (size_t)fr->stride * fr->height;
    if (q->capacity < bytes) {
        unsigned char *p = (unsigned char *)realloc(q->rgb, bytes);
        if (!p) return 0;
        q->rgb = p; q->capacity = bytes;
    }
    memcpy(q->rgb, fr->data, bytes);
    q->width = fr->width; q->height = fr->height; q->stride = fr->stride;
    q->dirty_y0 = fr->dirty_y0; q->dirty_y1 = fr->dirty_y1;
    q->pts_us = pts; q->decoded_at_us = decoded_at; q->decode_us = decode_us;
    return 1;
}

static unsigned long average_hundredths(uint64_t usec, unsigned count)
{
    return count ? (unsigned long)(usec / ((uint64_t)count * 10ULL)) : 0;
}

static unsigned long rate_hundredths(unsigned count, uint64_t elapsed_us)
{
    return elapsed_us
         ? (unsigned long)((uint64_t)count * 100000000ULL / elapsed_us) : 0;
}

static void report_stats(playback_stats *st, mr_audio *audio, mr_demux *demux,
                         scheduler_trace *trace, int depth, uint64_t now)
{
    uint64_t elapsed_us = now - st->since_us;
    unsigned long vd = average_hundredths(st->video_decode_us, st->decoded);
    unsigned long dm = average_hundredths(st->demux_us, st->samples);
    unsigned long ad = average_hundredths(st->audio_decode_us, st->samples);
    unsigned long cv = average_hundredths(st->convert_us, st->presented);
    unsigned long sc = average_hundredths(st->scale_us, st->presented);
    unsigned long ds = average_hundredths(st->display_us, st->presented);
    unsigned long la = average_hundredths(st->latency_us, st->presented);
    unsigned long pf = rate_hundredths(st->presented, elapsed_us);
    unsigned long df = rate_hundredths(st->decoded, elapsed_us);
    mr_source_timing io;
    mr_audio_diagnostics audio_diag;
    mr_demux_timing demux_timing;
    mr_source_timing_get(&io);
    audio_diagnostics(audio, &audio_diag);
    mr_demux_timing_get(demux, &demux_timing, 1);
    printf("rtg timing: vdecode=%lu.%02lu/%lu ms network-blocked=%lu ms "
           "hls-segment=%lu ms demux=%lu.%02lu ms adecode=%lu.%02lu ms "
           "convert=%lu.%02lu ms scale=%lu.%02lu ms display=%lu.%02lu/%lu ms "
           "audio-buffered=%lu ms vqueue=%d late=%u dropped=%u "
           "presented=%lu.%02lu fps decoded=%lu.%02lu fps sleep=%lu/%lu ms "
           "sleep-max-error=%lu us latency=%lu.%02lu ms "
           "refill-blocked=%lu ms ready-delayed-by-refill=%lu ms\n",
           vd / 100, vd % 100, st->video_decode_max_us / 1000,
           (unsigned long)(st->network_us / 1000) + io.network_ms,
           io.hls_segment_ms, dm / 100, dm % 100, ad / 100, ad % 100,
           cv / 100, cv % 100, sc / 100, sc % 100,
           ds / 100, ds % 100, st->display_max_us / 1000,
           audio ? audio_buffered_ms(audio) : 0, depth, st->late, st->dropped,
           pf / 100, pf % 100, df / 100, df % 100,
           (unsigned long)(st->sleep_requested_us / 1000),
           (unsigned long)(st->sleep_actual_us / 1000), st->sleep_max_error_us,
           la / 100, la % 100,
           (unsigned long)(st->refill_block_us / 1000),
           (unsigned long)(st->refill_delayed_ready_us / 1000));
    if (audio) service_audio_for_display(trace);
    printf("audio diagnostics: hw-starvations=%lu minimum-buffered=%lu ms "
           "minimum-active=%lu ms "
           "longest-service-gap=%lu ms longest-no-active=%lu ms "
           "fifo=%lu fifo-dropped=%lu req0=%u/%lu req1=%u/%lu active=%u\n",
           audio_diag.hardware_starvations, audio_diag.minimum_buffered_ms,
           audio_diag.minimum_active_ms,
           audio_diag.longest_service_gap_ms, audio_diag.longest_no_active_ms,
           audio_diag.fifo_samples, audio_diag.fifo_dropped_samples,
           (unsigned)audio_diag.request_state[0],
           audio_diag.request_samples[0], (unsigned)audio_diag.request_state[1],
           audio_diag.request_samples[1], (unsigned)audio_diag.active_requests);
    if (audio) service_audio_for_display(trace);
    printf("audio timeline: clock=%lu us fifo=%lu ms playing-remain=%lu ms "
           "queued=%lu ms total=%lu ms max-step=%lu us oldest=%lu "
           "startup-max-step=%lu us req0=%u req1=%u\n",
           (unsigned long)audio_diag.audio_clock_us,
           audio_diag.fifo_buffered_ms,
           audio_diag.hardware_playing_remaining_ms,
           audio_diag.hardware_queued_ms, audio_diag.total_buffered_ms,
           (unsigned long)audio_diag.clock_largest_step_us,
           (unsigned long)audio_diag.oldest_request_sequence,
           (unsigned long)audio_diag.startup_clock_largest_step_us,
           (unsigned)audio_diag.request_timeline_state[0],
           (unsigned)audio_diag.request_timeline_state[1]);
    printf("audio hardware: timeline-covered=%u actual-active-requests=%u "
           "actual-no-active-duration=%lu ms\n",
           (unsigned)audio_diag.timeline_covered,
           (unsigned)audio_diag.active_requests,
           audio_diag.longest_no_active_ms);
    if (audio) service_audio_for_display(trace);
    printf("demux timing: calls=%lu max-call=%lu us max-scanned=%lu "
           "service=%lu\n", demux_timing.calls, demux_timing.call_max_us,
           demux_timing.scanned_max, demux_timing.service_calls);
    if (audio) service_audio_for_display(trace);
    if (st->decoded) {
        printf("h264 stages: input=%lu/%lu us libavc-core=%lu/%lu us "
               "rgb-output=%lu/%lu us\n",
               (unsigned long)(st->h264_input_us / st->decoded),
               st->h264_input_max_us,
               (unsigned long)(st->h264_core_us / st->decoded),
               st->h264_core_max_us,
               (unsigned long)(st->h264_output_us / st->decoded),
               st->h264_output_max_us);
        if (audio) service_audio_for_display(trace);
    }
    if (st->rescue_entries) {
        printf("audio rescue: entries=%u critical=%u noncritical=%u packets=%u "
               "audio=%u video=%u retained=%u replaced=%u not-copied=%u "
               "newest-pts=%lu post-late=%ld us duration-avg=%lu us max=%lu us "
               "min-margin=%ld ms negative=%u rescue-starvations=%u "
               "exit(target/limit/eof)=%u/%u/%u\n",
               st->rescue_entries, st->rescue_critical,
               st->rescue_noncritical, st->rescue_packets,
               st->rescue_audio_packets, st->rescue_video_decoded,
               st->rescue_video_queued, st->rescue_video_replaced,
               st->rescue_video_skipped,
               (unsigned long)st->rescue_newest_retained_pts_us,
               (long)st->rescue_post_lateness_us,
               (unsigned long)(st->rescue_us / st->rescue_entries),
               st->rescue_max_us, st->rescue_min_margin_ms,
               st->rescue_negative_margin, st->rescue_hw_starvations,
               st->rescue_exit_target, st->rescue_exit_limit, st->rescue_exit_eof);
        if (audio) service_audio_for_display(trace);
    }
    if (st->last_rtg.src_w) {
        unsigned n = st->presented ? st->presented : 1;
        printf("rtg src=%ux%u dst=%ux%u srcfmt=%s dstfmt=%s "
               "prepare=%lu us scale=%lu us convert=%lu us copy=%lu us "
               "prepare-max=%lu us cgx-blit=%lu us cgx-blit-max=%lu us "
               "clip=%lu us display-total=%lu us "
               "audio-before=%lu ms audio-after=%lu ms "
               "pixels=%lu bytes=%lu copies=%u displayed=%u "
               "dropped-before-scale=%u dropped-after-scale=%u "
               "frame_pts=%lu audio_clock=%lu lateness=%ld us "
               "queue-head=%u dropped-pass=%u timing-rebases=%u\n",
               st->last_rtg.src_w, st->last_rtg.src_h,
               st->last_rtg.dst_w, st->last_rtg.dst_h,
               st->last_rtg.src_format, st->last_rtg.dst_format,
               (unsigned long)(st->rtg_prepare_us / n),
               (unsigned long)(st->rtg_scale_us / n),
               (unsigned long)(st->rtg_convert_us / n),
               (unsigned long)(st->rtg_copy_us / n),
               st->rtg_prepare_max_us,
               (unsigned long)(st->rtg_blit_us / n),
               st->rtg_blit_max_us,
               (unsigned long)(st->rtg_clip_us / n),
               (unsigned long)(st->rtg_total_us / n),
               st->audio_before, st->audio_after,
               st->last_rtg.pixels, st->last_rtg.bytes,
               st->last_rtg.copies, st->presented, st->dropped,
               st->dropped_after_scale,
               (unsigned long)st->frame_pts_us,
               (unsigned long)st->audio_clock_us,
               (long)st->calculated_lateness_us, st->queue_head,
               st->dropped_in_pass, st->timing_rebases);
        if (audio) service_audio_for_display(trace);
    }
    memset(st, 0, sizeof *st); st->since_us = now;
    mr_source_timing_reset();
}

static unsigned char *slurp(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    if (out_len) *out_len = n;
    return b;
}

/* Ticks are 1/50 s (dos Delay). frame period = 50*scale/rate, min 1. */
static long frame_ticks(unsigned long rate, unsigned long scale)
{
    long t;
    if (!rate) return 4;
    t = (long)((50UL * scale + rate / 2) / rate);
    return t < 1 ? 1 : t;
}

static void decoded_audio_sink(void *user, const int16_t *pcm,
                               unsigned frames, unsigned channels)
{
    audio_write_s16((mr_audio *)user, (const short *)pcm, frames,
                    (int)channels);
}

static mr_h264_speed_mode effective_h264_speed(int requested, int width,
                                                int height)
{
    if (requested == MR_H264_SPEED_QUALITY ||
        requested == MR_H264_SPEED_BALANCED ||
        requested == MR_H264_SPEED_FAST)
        return (mr_h264_speed_mode)requested;
    return (uint64_t)width * (uint64_t)height > 854ULL * 480ULL
         ? MR_H264_SPEED_BALANCED : MR_H264_SPEED_QUALITY;
}

static int apply_h264_speed(mr_decoder *dec, int requested, int verbose)
{
    mr_h264_speed_mode mode;
    const char *name;
    if (!dec || dec->codec != &mr_codec_h264) return 1;
    mode = effective_h264_speed(requested, dec->width, dec->height);
    name = mode == MR_H264_SPEED_FAST ? "Fast" :
           mode == MR_H264_SPEED_BALANCED ? "Balanced" : "Quality";
    if (!mr_h264_set_speed_mode(dec, mode)) return 0;
    if (verbose || requested < 0)
        printf("H.264 performance: %s%s\n", name,
               requested < 0 ? " (automatic)" : "");
    return 1;
}

/* The ReAction controller uses Ctrl-F for a normal stop so AmigaDOS does not
 * abort the CLI process before display/audio cleanup runs.  Shell Ctrl-C is
 * still accepted when mrplay sees it itself; Ctrl-D toggles pause and Ctrl-E
 * toggles fast-forward. */
static int control_signal_event(amiga_display *disp)
{
    ULONG sig = SetSignal(0, SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D |
                             SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F);
    ULONG commands = 0;
    if (sig & (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_F)) return MR_EV_QUIT;
    if (sig & SIGBREAKF_CTRL_D) return MR_EV_PAUSE;
    if (sig & SIGBREAKF_CTRL_E) {
        if (control_block) {
            Forbid();
            commands = control_block->commands;
            control_block->commands = 0;
            Permit();
        }
        if (commands & MR_PLAYER_COMMAND_FULLSCREEN)
            display_toggle_fullscreen(disp);
        if (commands & MR_PLAYER_COMMAND_VOLUME_DOWN) {
            control_volume -= 8;
            if (control_volume < 0) control_volume = 0;
            if (control_audio) audio_set_volume(control_audio, control_volume);
            printf("volume: %d%%\n", control_volume * 100 / 64);
        }
        if (commands & MR_PLAYER_COMMAND_VOLUME_UP) {
            control_volume += 8;
            if (control_volume > 64) control_volume = 64;
            if (control_audio) audio_set_volume(control_audio, control_volume);
            printf("volume: %d%%\n", control_volume * 100 / 64);
        }
        if (commands & MR_PLAYER_COMMAND_PAUSE) return MR_EV_PAUSE;
        if (commands & MR_PLAYER_COMMAND_FAST) return MR_EV_SEEK_FWD;
        /* Compatibility with older controllers that used bare Ctrl-E. */
        if (!commands) return MR_EV_SEEK_FWD;
    }
    return MR_EV_NONE;
}

#define PREFETCH_INTERRUPT_MASK (SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | \
                                 SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F)
#define PREFETCH_WAIT_MASK(disp_) \
    (PREFETCH_INTERRUPT_MASK | display_wait_mask((disp_)))
#define STOP_PREFETCH(disp_) \
    hls_prefetch_stop(PREFETCH_WAIT_MASK((disp_)), prefetch_quit_probe, (disp_))

static int prefetch_quit_probe(void *opaque)
{
    amiga_display *disp = (amiga_display *)opaque;
    int ev = control_signal_event(disp);
    if (ev == MR_EV_NONE && disp) ev = display_poll_event(disp);
    /* display_poll_event() may have toggled fullscreen, replacing the CGX
     * window and therefore its UserPort signal bit.  Publish the new mask
     * before pf_wait_busy() enters its next Wait(). */
    if (disp)
        hls_prefetch_set_interrupt(PREFETCH_WAIT_MASK(disp),
                                   prefetch_quit_probe, disp);
    /* Pause/seek events cannot be acted on inside the worker wait, but they
     * must not disappear merely because a segment fetch was in flight. */
    if (ev != MR_EV_NONE && ev != MR_EV_QUIT)
        deferred_player_event = ev;
    return ev == MR_EV_QUIT;
}

static int player_event(amiga_display *disp)
{
    int ev;
    if (deferred_player_event != MR_EV_NONE) {
        ev = deferred_player_event;
        deferred_player_event = MR_EV_NONE;
    } else {
        ev = control_signal_event(disp);
    }
    if (ev == MR_EV_NONE) ev = display_poll_event(disp);
    /* Keep an active HLS worker's interrupt context synchronized with a CGX
     * window replaced by either the F key or the controller button. */
    if (disp)
        hls_prefetch_set_interrupt(PREFETCH_WAIT_MASK(disp),
                                   prefetch_quit_probe, disp);
    return ev;
}

/* HTTP's synchronous reader runs on the player task.  Pump fresh Intuition
 * input while it waits, but leave pause/seek/quit queued for the scheduler to
 * perform at its normal safe point.  Fullscreen is handled immediately by the
 * display backend, so its replacement window starts being drained at once. */
static int service_player_during_io(void *opaque)
{
    scheduler_trace *trace = (scheduler_trace *)opaque;
    amiga_display *disp = trace && trace->presenter ? trace->presenter->disp
                                                    : NULL;
    int ev = MR_EV_NONE;
    if (disp && (!trace->presenter || !trace->presenter->presenting)) {
        ev = control_signal_event(disp);
        if (ev == MR_EV_NONE) ev = display_poll_event(disp);
        if (ev != MR_EV_NONE &&
            (deferred_player_event == MR_EV_NONE || ev == MR_EV_QUIT))
            deferred_player_event = ev;
        hls_prefetch_set_interrupt(PREFETCH_WAIT_MASK(disp),
                                   prefetch_quit_probe, disp);
    }
    if (trace) {
        if (trace->audio) service_audio_for_display(trace);
        else if (trace->presenter) present_service_frame(trace->presenter);
    }
    return ev == MR_EV_QUIT || deferred_player_event == MR_EV_QUIT;
}

/*
 * Wait hook for the HLS live-playlist re-fetch loop (mr_hls_set_wait).
 *
 * When playback reaches the live edge the reader must poll the playlist for new
 * segments. That poll used to spin without pause and without servicing anything,
 * so a stalled edge froze the single task for tens of seconds - audio and video
 * stopped and even ESC/Close could not be seen until the spin ended. This paces
 * each poll and, throughout the wait, pumps the same service hook the scheduler
 * uses: audio keeps playing, already-decoded video keeps presenting (the queue
 * is released for the blocking fetch), and a quit request is honoured promptly.
 * opaque is the scheduler_trace, which carries the audio handle and presenter.
 */
static int hls_wait_service(void *opaque, unsigned wait_ms)
{
    scheduler_trace *trace = (scheduler_trace *)opaque;
    amiga_display *disp = trace && trace->presenter ? trace->presenter->disp
                                                    : NULL;
    uint64_t begin = monotonic_us();
    uint64_t target = (uint64_t)wait_ms * 1000ULL;
    for (;;) {
        if (disp && player_event(disp) == MR_EV_QUIT) return 1;
        if (trace->audio) service_audio_for_display(trace);
        else if (trace->presenter) present_service_frame(trace->presenter);
        if (monotonic_us() - begin >= target) return 0;
        Delay(1);                            /* 20 ms; stay responsive to ESC  */
    }
}

/* MPEG-1 program streams (.mpg/.mpeg) play through pl_mpeg (video + MP2 audio),
 * reusing the display and Paula audio backends. Separate from the AVI/MOV +
 * codec path because .mpg is a self-contained stream. */
static int play_mpeg1(const unsigned char *buf, long len, int loop, int want_time)
{
    mr_mpeg1      *mp;
    amiga_display *disp;
    mr_audio      *audio = NULL;
    unsigned       sr;
    int            w, h, frames = 0, paused = 0, quit = 0, fast_forward = 0;
    unsigned long  period, clock_base = 0;
    long           ntick;
    unsigned char *abuf;                         /* heap, not stack (4.6 KB)  */
    clock_t        t_dec = 0, t_show = 0;
    mr_frame       fr;
    int64_t        pts_us;
    unsigned       fps_millihz;

    mp = mr_mpeg1_open((const uint8_t *)buf, (size_t)len);
    if (!mp) { printf("cannot open MPEG-1 stream\n");
               player_status(MR_PLAYER_STATE_ERROR, "MPEG-1",
                             "cannot open MPEG-1 stream");
               status_hold(); return 10; }
    abuf = (unsigned char *)malloc(1152 * 4);    /* max: 1152 frames stereo16 */
    if (!abuf) { mr_mpeg1_close(mp); return 10; }
    w = mr_mpeg1_width(mp); h = mr_mpeg1_height(mp);
    printf("mpeg1: %dx%d, opening display...\n", w, h);
    {
        char line[MR_PLAYER_STATUS_TEXT_MAX];
        snprintf(line, sizeof line, "%dx%d, MPEG-1", w, h);
        player_status(MR_PLAYER_STATE_PLAYING, "MPEG-1", line);
    }
    disp = display_open(w, h, "MintRIVA");
    if (!disp) { printf("cannot open a display\n");
                 player_status(MR_PLAYER_STATE_ERROR, "MPEG-1",
                               "cannot open a display");
                 status_hold(); mr_mpeg1_close(mp); return 10; }
    printf("display backend: %s\n", display_backend_name(disp));

    sr = mr_mpeg1_samplerate(mp);
    if (sr) {
        audio = audio_open(sr, 2, 16);
        control_audio = audio;
        if (audio) audio_set_volume(audio, control_volume);
        printf(audio ? "audio: Paula out, %u Hz (MP2 stereo)\n"
                     : "audio: Paula open failed, silent\n", sr);
    }
    fps_millihz = mr_mpeg1_framerate_millihz(mp);
    period = fps_millihz ? (1000000UL + fps_millihz / 2) / fps_millihz : 40;
    if (period < 1) period = 1;
    ntick = (long)((period + 19) / 20);
    if (ntick < 1) ntick = 1;

    printf("playing: space=pause, ESC=quit%s...\n", loop ? ", loop on" : "");

    while (!quit) {
        int got;
        while (paused && !quit) {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1; else if (ev == MR_EV_PAUSE) paused = 0;
            Delay(2);
        }
        if (quit) break;

        { clock_t a = clock(); got = mr_mpeg1_next(mp, &fr, &pts_us); t_dec += clock() - a; }
        if (!got) {
            if (loop) { mr_mpeg1_rewind(mp); frames = 0;
                        clock_base = audio ? audio_elapsed_ms(audio) : 0; continue; }
            break;
        }
        if (audio) {                             /* top up audio (bounded)    */
            int n, k = 0;
            /* ~2 MP2 frames per video frame keeps Paula just ahead; draining
             * everything here would stall video before the first frame shows. */
            while (k < 2 && (n = mr_mpeg1_audio(mp, abuf)) > 0) {
                audio_write(audio, abuf, (unsigned)(n * 4));
                audio_service(audio);
                k++;
            }
        }

        if (audio) {                             /* pace to the audio clock   */
            unsigned long target = clock_base + (unsigned long)frames * period;
            for (;;) {
                int ev = player_event(disp);
                if (ev == MR_EV_QUIT)  { quit = 1; break; }
                if (ev == MR_EV_PAUSE) { paused = 1; break; }
                if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
                audio_service(audio);
                if (fast_forward) break;
                if (audio_elapsed_ms(audio) >= target) break;
                if (audio_starved(audio)) break;
                Delay(1);
            }
        } else {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            else if (ev == MR_EV_PAUSE) paused = 1;
            else if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
            if (!fast_forward) Delay(ntick);
        }
        if (quit) break;

        { clock_t a = clock();
          display_show_rgb(disp, fr.data, fr.width, fr.height, fr.stride,
                           fr.dirty_y0, fr.dirty_y1);
          t_show += clock() - a; }
        frames++;
        if (audio) audio_service(audio);
    }

    if (want_time && frames > 0) {
        unsigned long e = 0, bl = 0;
        display_aga_timing(&e, &bl);
        printf("timing/%d frames: decode=%lu ms, display=%lu ms (encode=%lu, blit=%lu)\n",
               frames, (unsigned long)(t_dec * 1000 / CLOCKS_PER_SEC),
               (unsigned long)(t_show * 1000 / CLOCKS_PER_SEC), e, bl);
        if (display_aga_kalms_timing(&bl))
            printf("Kalms conversion: %lu ms\n", bl);
    }
    if (audio) {
        int g = 0;
        while (!audio_starved(audio) && g++ < 4000) {
            if (player_event(disp) == MR_EV_QUIT) {
                quit = 1;
                break;
            }
            audio_service(audio);
            Delay(1);
        }
    }
    if (!quit) {
        printf("played %d frames - press ESC or close the window to exit\n",
               frames);
        while (player_event(disp) != MR_EV_QUIT) {
            if (audio) audio_service(audio);
            Delay(2);
        }
    }
    player_status(MR_PLAYER_STATE_ENDED, "MPEG-1", "stream ended");
    control_audio = NULL;
    if (audio) audio_close(audio);
    display_close(disp);
    mr_mpeg1_close(mp);
    free(abuf);
    return 0;
}

int main(int argc, char **argv)
{
    long len = 0;
    unsigned char *buf = NULL;
    mr_demux *dx;
    const mr_video_info *vi;
    const mr_codec *codec;
    mr_decoder dec;
    amiga_display *disp;
    mr_audio *audio = NULL;
    mr_audio_decoder *audio_dec = NULL;
    mr_packet pkt;
    long ticks;
    int frames = 0;
    int want_time = 0, loop = 0, paused = 0, quit = 0, fast_forward = 0;
    int fullscreen = 0;
    int raw_diag_printed = 0;
    int hls_low = 0;
    unsigned hls_max_width = 0, hls_max_height = 0, hls_max_fps = 0;
    int hls_prefetch = 0;  /* --hls-prefetch: background segment reader (opt-in) */
    int prefetch_on = 0;   /* worker actually running                            */
    int net_queue = 0;  /* 0 = built-in default (network depth 1)             */
    int live_resync = 0;  /* --live-resync: catch up after a big stall, and
                           * reconnect a live stream that drops out            */
    int auto_close_eof = 0; /* finite GUI media should release its window      */
    int h264_speed = -1; /* automatic: Quality <=480p, Balanced above 480p    */
    const char *media_path = NULL;
    const char *user_agent = NULL;
    const char *referer = NULL;
    char youtube_media[MR_HTTP_URL_MAX];
    mr_youtube_media_kind youtube_kind = MR_YOUTUBE_MEDIA_NONE;
    mr_http_options http_options;
    int have_http_options = 0;
    media_clock mc;
    memset(&mc, 0, sizeof mc);
    int64_t container_pts_adjust_us = 0;
    uint64_t last_container_pts_us = 0;
    int have_container_pts = 0;
    clock_t t_dec = 0, t_show = 0;
    queued_video vq[VIDEO_QUEUE_CAP];
    int qhead = 0, qcount = 0, input_eof = 0;
    int oom_warned = 0; /* set after first queue_copy OOM is logged */
    int rescue_active = 0;
    int rescue_priority = 0;
    unsigned rescue_cooldown = 0;
    unsigned rescue_episode_packets = 0, rescue_episode_audio = 0;
    unsigned rescue_episode_video = 0, rescue_episode_queued = 0;
    unsigned rescue_episode_skipped = 0;
    unsigned rescue_episode_replaced = 0;
    int rescue_episode_critical = 0;
    uint64_t rescue_newest_retained_pts_us = 0;
    unsigned long rescue_buffer_before = 0;
    unsigned long rescue_entry_threshold = 0;
    unsigned long rescue_min_buffer = 0;
    unsigned long rescue_hw_before = 0;
    uint64_t rescue_started_us = 0;
    uint64_t decoded_index = 0, mono_base_us = 0;
    playback_stats stats;
    scheduler_trace trace;

    /* Unbuffered so every diagnostic reaches the shell immediately, even if a
     * later step hangs or crashes (libnix stdout can otherwise block-buffer). */
    setvbuf(stdout, NULL, _IONBF, 0);
    control_port_open();
    DeleteFile((CONST_STRPTR)MR_PLAYER_STATUS_FILE);
    DeleteFile((CONST_STRPTR)MR_PLAYER_STATUS_TMP);
    player_status(MR_PLAYER_STATE_STARTING, "", "Starting player...");
    if (!playback_timer_open())
        printf("warning: timer.device unavailable; pacing timer is coarse\n");

    if (argc < 2) {
        printf("usage: mrplay [--user-agent <value>] [--referer <value>] "
               "<file.avi|file.mov|file.ts|file.m2ts|"
               "file.mjpeg|file.m4v> "
               "[--aga] [--ham] [--ham6] "
               "[--2x] [--lace] [--loop] [--wpa|--c2p|--riva-c2p|--kalms-c2p] "
               "[--cd32] [--fullscreen] [--hls-low] [--net-queue=N] [--live-resync] "
               "[--h264-speed=auto|quality|balanced|fast] "
               "[--time]\n");
        return mrplay_exit(5);
    }
    {   /* display options anywhere on the command line */
        int i;
        for (i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--user-agent")) {
                if (++i >= argc) {
                    printf("--user-agent requires a value\n");
                    return mrplay_exit(5);
                }
                user_agent = argv[i];
            } else if (!strcmp(argv[i], "--referer")) {
                if (++i >= argc) {
                    printf("--referer requires a value\n");
                    return mrplay_exit(5);
                }
                referer = argv[i];
            }
            else if (!strcmp(argv[i], "--aga"))  display_set_force_aga(1);
            else if (!strcmp(argv[i], "--ham"))  display_set_ham(8);
            else if (!strcmp(argv[i], "--ham6")) display_set_ham(6);
            else if (!strcmp(argv[i], "--2x"))   display_set_scale(2);
            else if (!strcmp(argv[i], "--wpa"))  display_set_c2p(0);
            else if (!strcmp(argv[i], "--c2p"))  display_set_c2p(1);
            else if (!strcmp(argv[i], "--riva-c2p")) display_set_riva_c2p(1);
            else if (!strcmp(argv[i], "--kalms-c2p")) display_set_kalms_c2p(1);
            else if (!strcmp(argv[i], "--loop")) loop = 1;
            else if (!strcmp(argv[i], "--lace")) display_set_lace(1);
            else if (!strcmp(argv[i], "--cd32")) display_set_akiko(1);
            else if (!strcmp(argv[i], "--time")) { want_time = 1; display_set_timing_mode(1); }
            else if (!strcmp(argv[i], "--fullscreen")) fullscreen = 1;
            else if (!strcmp(argv[i], "--hls-low")) hls_low = 1;
            else if (!strncmp(argv[i], "--hls-max-width=", 16))
                hls_max_width = (unsigned)strtoul(argv[i] + 16, NULL, 10);
            else if (!strncmp(argv[i], "--hls-max-height=", 17))
                hls_max_height = (unsigned)strtoul(argv[i] + 17, NULL, 10);
            else if (!strncmp(argv[i], "--hls-max-fps=", 14))
                hls_max_fps = (unsigned)strtoul(argv[i] + 14, NULL, 10);
            else if (!strncmp(argv[i], "--net-queue=", 12))
                net_queue = (int)strtoul(argv[i] + 12, NULL, 10);
            else if (!strcmp(argv[i], "--hls-prefetch")) hls_prefetch = 1;
            else if (!strcmp(argv[i], "--live-resync")) live_resync = 1;
            else if (!strncmp(argv[i], "--h264-speed=", 13)) {
                const char *mode = argv[i] + 13;
                if (!strcmp(mode, "auto")) h264_speed = -1;
                else if (!strcmp(mode, "quality")) h264_speed = MR_H264_SPEED_QUALITY;
                else if (!strcmp(mode, "balanced")) h264_speed = MR_H264_SPEED_BALANCED;
                else if (!strcmp(mode, "fast")) h264_speed = MR_H264_SPEED_FAST;
                else {
                    printf("invalid H.264 speed mode: %s\n", mode);
                    return mrplay_exit(5);
                }
            }
            else if (argv[i][0] != '-' && !media_path) media_path = argv[i];
        }
    }
    if (!media_path) {
        printf("no media URL or filename supplied\n");
        return mrplay_exit(5);
    }
    display_set_fullscreen(fullscreen);
    if (!mr_http_options_init(&http_options, user_agent, referer)) {
        printf("invalid HTTP options: %s\n", mr_source_last_error());
        return mrplay_exit(5);
    }
    http_options.hls_low = hls_low;
    http_options.hls_max_width = hls_max_width;
    http_options.hls_max_height = hls_max_height;
    http_options.hls_max_fps = hls_max_fps;
    have_http_options = user_agent || referer || hls_low || hls_max_width ||
                        hls_max_height || hls_max_fps;
    if (mr_youtube_is_url(media_path)) {
        mr_http_options youtube_options;
        if (!mr_youtube_http_options_init(&youtube_options, &http_options)) {
            printf("invalid YouTube HTTP options: %s\n",
                   mr_source_last_error());
            return mrplay_exit(5);
        }
        http_options = youtube_options;
        have_http_options = 1;
    }
    if (want_time) {
        printf("HTTP User-Agent: %s\n", http_options.user_agent);
        if (http_options.referer[0])
            printf("HTTP Referer: %s\n", http_options.referer);
        if (hls_low)
            printf("HLS preference: low bandwidth (max %ux%u @ %u fps)\n",
                   hls_max_width, hls_max_height, hls_max_fps);
    }
    if (mr_youtube_is_url(media_path)) {
        printf("YouTube: resolving...\n");
        player_status(MR_PLAYER_STATE_OPENING, "",
                      "Resolving YouTube media...");
        if (!mr_youtube_resolve_media(media_path,
                                      have_http_options ? &http_options : NULL,
                                      youtube_media, sizeof youtube_media,
                                      &youtube_kind) ||
            !mr_youtube_media_http_options_init(
                &http_options, have_http_options ? &http_options : NULL)) {
            char reason[MR_PLAYER_STATUS_TEXT_MAX];
            const char *why = mr_source_last_error();
            printf("cannot resolve YouTube media: %s\n", why);
            snprintf(reason, sizeof reason, "YouTube: %s", why);
            player_status(MR_PLAYER_STATE_UNSUPPORTED, "", reason);
            status_hold();
            return mrplay_exit(10);
        }
        have_http_options = 1;
        media_path = youtube_media;
        if (youtube_kind == MR_YOUTUBE_MEDIA_HLS)
            printf("YouTube: live HLS manifest found\n");
        else if (youtube_kind == MR_YOUTUBE_MEDIA_PROGRESSIVE_720P)
            printf("YouTube: progressive 720p MP4 found\n");
        else
            printf("YouTube: progressive 360p MP4 found\n");
        /* A progressive YouTube URL is a finite MP4.  The GUI enables
         * --live-resync for IPTV by default, but carrying that policy into a
         * VOD turns its clean EOF into a bogus "Reconnecting..." cycle. */
        if (youtube_kind != MR_YOUTUBE_MEDIA_HLS) {
            if (live_resync && want_time)
                printf("YouTube: finite video; live reconnect disabled\n");
            live_resync = 0;
            auto_close_eof = 1;
        }
        if (want_time)
            printf("YouTube: source client %s\n",
                   mr_youtube_last_client());
        /* The optional HLS worker must own bsdsocket/AmiSSL from its own task.
         * Resolution ran on the main task, so release that task's persistent
         * network state before starting the worker. */
        if (hls_prefetch && youtube_kind == MR_YOUTUBE_MEDIA_HLS)
            mr_http_net_shutdown();
    }
    printf("mrplay: opening %s\n", media_path);
    player_status(MR_PLAYER_STATE_OPENING, "", "Connecting to stream...");
    mr_hls_set_verbose(want_time);

    /* Opt-in background segment reader (HLS only): downloads segments into RAM
     * ahead of the reader so a fetch never freezes presentation. Installed
     * before the demuxer opens so the very first playlist/segment go through it.
     * If it fails to come up we simply fall back to synchronous fetching. */
    if (hls_prefetch && mr_source_is_hls(media_path)) {
        prefetch_on = hls_prefetch_start(want_time, PREFETCH_INTERRUPT_MASK,
                                         prefetch_quit_probe, NULL);
        printf("hls prefetch: %s\n", prefetch_on ? "on" : "unavailable");
    }

    dx = mr_demux_open_file_ex(media_path,
                               have_http_options ? &http_options : NULL);
    if (dx) {
        printf("streaming %s from %s\n", mr_demux_container_name(dx),
               !strncmp(media_path, "http://", 7) ||
               !strncmp(media_path, "https://", 8) ? "network" : "disk");
    } else {
        if (mr_demux_is_file_backed_container(media_path)) {
            char reason[MR_PLAYER_STATUS_TEXT_MAX];
            const char *why = mr_demux_last_open_error();
            /* A "not supported"/"no decoder" reason is a codec/container we
             * can't play (e.g. HEVC); anything else is a connection/read fault.
             * Report them as different states so the browser can distinguish
             * "wrong codec" from "bad link". */
            int unsupported = why && (strstr(why, "not supported") ||
                                      strstr(why, "unsupported") ||
                                      strstr(why, "no decoder"));
            printf("cannot open stream: %s\n", why);
            if (unsupported) {
                snprintf(reason, sizeof reason, "%s", why);
                player_status(MR_PLAYER_STATE_UNSUPPORTED, "", reason);
            } else {
                snprintf(reason, sizeof reason, "cannot open stream: %s",
                         why ? why : "connection failed");
                player_status(MR_PLAYER_STATE_ERROR, "", reason);
            }
            status_hold();
            if (prefetch_on) STOP_PREFETCH(NULL);
            return mrplay_exit(10);
        }
        /* MPEG-1 and raw elementary streams still require a contiguous input
         * buffer because their current decoders parse directly from it. */
        buf = slurp(media_path, &len);
        if (!buf) { printf("cannot read %s\n", media_path);
                    player_status(MR_PLAYER_STATE_ERROR, "",
                                  "cannot read stream data");
                    status_hold(); return mrplay_exit(10); }
        printf("loaded %ld bytes\n", len);

        if (mr_mpeg1_probe(buf, (size_t)len)) {  /* .mpg via pl_mpeg         */
            int rc = play_mpeg1(buf, len, loop, want_time);
            free(buf);
            return mrplay_exit(rc);
        }
        dx = mr_demux_open(buf, (size_t)len);
        if (!dx) {
            printf("unsupported container (need AVI, MOV/MP4, MPEG-TS/PS, "
                   "raw MJPEG/M4V or MPEG-1)\n");
            player_status(MR_PLAYER_STATE_UNSUPPORTED, "",
                          "unsupported container (not AVI/MOV/MP4/TS/PS/"
                          "MJPEG/M4V/MPEG-1)");
            status_hold();
            free(buf);
            return mrplay_exit(10);
        }
    }

    vi = mr_demux_video(dx);
    codec = mr_codec_find(vi->fourcc);
    if (!codec) { char reason[MR_PLAYER_STATUS_TEXT_MAX];
                  printf("no decoder for this video codec\n");
                  describe_unsupported_video(vi->fourcc, reason, sizeof reason);
                  player_status(MR_PLAYER_STATE_UNSUPPORTED, "", reason);
                  status_hold();
                  mr_demux_close(dx);
                  if (prefetch_on) STOP_PREFETCH(NULL);
                  free(buf); return mrplay_exit(10); }

    if (want_time)
        printf("video fourcc='%c%c%c%c'\n", (int)(vi->fourcc & 255),
               (int)((vi->fourcc >> 8) & 255),
               (int)((vi->fourcc >> 16) & 255),
               (int)((vi->fourcc >> 24) & 255));

    if (mr_decoder_open_config(&dec, codec, vi->width, vi->height,
                               vi->config, vi->config_len) != MR_OK) {
        char reason[MR_PLAYER_STATUS_TEXT_MAX];
        printf("decoder init failed\n");
        snprintf(reason, sizeof reason, "%s decoder failed to initialise",
                 codec->name);
        player_status(MR_PLAYER_STATE_ERROR, codec->name, reason);
        status_hold();
        mr_demux_close(dx);
        if (prefetch_on) STOP_PREFETCH(NULL);
        free(buf); return mrplay_exit(10);
    }
    if (!apply_h264_speed(&dec, h264_speed, want_time)) {
        player_status(MR_PLAYER_STATE_ERROR, codec->name,
                      "H.264 performance mode was rejected by decoder");
        status_hold();
        mr_decoder_close(&dec); mr_demux_close(dx);
        if (prefetch_on) STOP_PREFETCH(NULL);
        free(buf); return mrplay_exit(10);
    }

    h264_pipeline_diag_enabled = want_time && codec == &mr_codec_h264 &&
        (uint64_t)vi->width * (uint64_t)vi->height >= 1280ULL * 720ULL;
    h264_pipeline_width = vi->width;
    h264_pipeline_height = vi->height;
    h264_pipeline_stage = 0;
    if (h264_pipeline_diag_enabled)
        h264_pipeline_checkpoint_player("decoder-open", 0, 0);

    printf("media: file=%s, container=%s, video=%s (%c%c%c%c), "
           "%dx%d, %lu.%03lu fps\n", media_path, mr_demux_container_name(dx),
           codec->name, (int)(vi->fourcc & 255), (int)((vi->fourcc >> 8) & 255),
           (int)((vi->fourcc >> 16) & 255), (int)((vi->fourcc >> 24) & 255),
           vi->width, vi->height,
           (unsigned long)(vi->rate / (vi->scale ? vi->scale : 1)),
           (unsigned long)(((vi->rate % (vi->scale ? vi->scale : 1)) * 1000) /
                           (vi->scale ? vi->scale : 1)));
    printf("%dx%d, opening display...\n", vi->width, vi->height);
    /* Prepare the eventual first-frame status, but do not claim Playing until
     * a frame has actually reached the display. */
    {
        char line[MR_PLAYER_STATUS_TEXT_MAX];
        snprintf(line, sizeof line, "%dx%d, %s", vi->width, vi->height,
                 mr_demux_container_name(dx));
        player_prepare_playing_status(codec->name, line);
        snprintf(line, sizeof line, "Opening display for %dx%d %s...",
                 vi->width, vi->height, codec->name);
        player_status(MR_PLAYER_STATE_OPENING, codec->name, line);
    }
    disp = display_open(vi->width, vi->height, "MintRIVA");
    if (!disp) { printf("cannot open a display (RTG or AGA)\n");
                 player_status(MR_PLAYER_STATE_ERROR, codec->name,
                               "cannot open a display (RTG or AGA)");
                 status_hold();
                 mr_decoder_close(&dec); mr_demux_close(dx);
                 if (prefetch_on) STOP_PREFETCH(NULL);
                 free(buf); return mrplay_exit(10); }
    printf("display backend: %s\n", display_backend_name(disp));
    player_status(MR_PLAYER_STATE_OPENING, codec->name,
                  "Display open; buffering first frame...");
    if (prefetch_on)
        hls_prefetch_set_interrupt(PREFETCH_WAIT_MASK(disp),
                                   prefetch_quit_probe, disp);

    /* Every decoder feeds signed S16 to the common Paula sink.  In particular,
     * PCM byte signedness is resolved before downmixing and S16-to-S8 output. */
    {
        const mr_audio_info *ai = mr_demux_audio(dx);
        if (ai->valid && ai->format_tag == MR_AUDIO_FORMAT_PCM) {
            audio_dec = mr_audio_decoder_open(ai);
            if (want_time && audio_dec) {
                if (ai->codec_tag > 0xffff)
                    printf("audio: %s %s %lu Hz (%c%c%c%c)\n",
                           mr_audio_decoder_name(audio_dec),
                           ai->channels == 1 ? "mono" : "stereo",
                           (unsigned long)ai->sample_rate,
                           (int)(ai->codec_tag & 255),
                           (int)((ai->codec_tag >> 8) & 255),
                           (int)((ai->codec_tag >> 16) & 255),
                           (int)((ai->codec_tag >> 24) & 255));
                else
                    printf("audio: %s %s %lu Hz\n",
                           mr_audio_decoder_name(audio_dec),
                           ai->channels == 1 ? "mono" : "stereo",
                           (unsigned long)ai->sample_rate);
            }
            if (audio_dec)
                audio = audio_open(mr_audio_decoder_rate(audio_dec),
                                   (int)mr_audio_decoder_channels(audio_dec), 16);
            if (audio && audio_dec)
                printf("audio: Paula out, %u Hz (%s, %u ch)\n",
                       mr_audio_decoder_rate(audio_dec),
                       mr_audio_decoder_name(audio_dec),
                       mr_audio_decoder_channels(audio_dec));
            else {
                printf("audio: unsupported PCM layout or Paula open failed, "
                       "playing silent\n");
                if (audio_dec) {
                    mr_audio_decoder_close(audio_dec);
                    audio_dec = NULL;
                }
            }
        } else if (ai->valid &&
                   (ai->format_tag == MR_AUDIO_FORMAT_MP2 ||
                    ai->format_tag == MR_AUDIO_FORMAT_MP3 ||
                    ai->format_tag == MR_AUDIO_FORMAT_AAC)) {
            audio_dec = mr_audio_decoder_open(ai);
            if (audio_dec)
                audio = audio_open(mr_audio_decoder_rate(audio_dec),
                                   (int)mr_audio_decoder_channels(audio_dec), 16);
            if (audio && audio_dec)
                printf("audio: Paula out, %u Hz (%s, %u ch)\n",
                       mr_audio_decoder_rate(audio_dec),
                       mr_audio_decoder_name(audio_dec),
                       mr_audio_decoder_channels(audio_dec));
            else {
                printf("audio: unsupported %s setup or Paula open failed, "
                       "playing silent\n",
                       ai->format_tag == MR_AUDIO_FORMAT_MP2 ? "MP2" :
                       ai->format_tag == MR_AUDIO_FORMAT_MP3 ? "MP3" : "AAC");
                if (audio_dec) {
                    mr_audio_decoder_close(audio_dec);
                    audio_dec = NULL;
                }
            }
        }
    }
    control_audio = audio;
    if (audio) audio_set_volume(audio, control_volume);

    ticks = frame_ticks(vi->rate, vi->scale);
    memset(&trace, 0, sizeof trace);
    trace.audio = audio; trace.enabled = want_time;
    trace.phase = "startup"; trace.phase_started_us = monotonic_us();
    display_set_service(disp, audio ? service_audio_for_display : NULL, &trace);
    mr_demux_set_service(dx, audio ? service_audio_for_display : NULL, &trace);
    mr_h264_set_service(&dec, audio ? service_audio_for_display : NULL, &trace);
    /* Keep audio/video/UI alive (and quit responsive) while the HLS live path
     * polls for new segments at the edge; see hls_wait_service. */
    mr_hls_set_wait(hls_wait_service, &trace);
    /* Present queued video / feed audio between the socket reads of a blocking
     * segment fetch, so an ordinary ~350 ms download no longer freezes video for
     * its whole duration. Fires while released is set around the demux read. */
    mr_http_set_service(service_player_during_io, &trace);
    {
        unsigned long period = vi->rate ? (1000UL * (vi->scale ? vi->scale : 1)
                                           / vi->rate) : 83;
        if (period < 1) period = 1;

    printf("playing: space=pause, F=fullscreen, right=fast, ESC=quit%s...\n",
           loop ? ", loop on" : "");

    memset(vq, 0, sizeof vq);
    memset(&stats, 0, sizeof stats);
    stats.since_us = monotonic_us();
    mr_source_timing_reset();
    {
        struct EClockVal ev;
        ULONG hz = TimerBase ? ReadEClock(&ev) : 0;
        if (want_time)
            {
                unsigned long gran_ns = hz ? 1000000000UL / hz : 0;
                printf("timer: EClock=%lu Hz (%lu.%03lu us nominal), "
                       "DOS tick=20.000 ms\n", (unsigned long)hz,
                       gran_ns / 1000, gran_ns % 1000);
            }
    }

    {
        int playback_started = 0;
        int network_source = mr_source_is_url(media_path);
        int frames_at_last_reconnect = 0, reconnects_without_progress = 0;
        int startup_depth = network_source ? 1 : 2;
        /* Network sources default to a single decoded frame (see the comment on
         * the decode gate below). --net-queue=N opts into a read-ahead cushion.
         * A few frames smooth per-frame decode jitter (e.g. a GOP-boundary
         * I-frame); a deep buffer (tens of frames) additionally lets video sit
         * ahead of the audio clock and present in PTS order as frames fall due,
         * keeps the loop demuxing so the audio FIFO stays fed, and carries the
         * picture across a segment-boundary refetch. Clamped to VIDEO_QUEUE_CAP;
         * startup latency is unchanged because startup_depth still gates when
         * playback begins. */
        int net_target = net_queue > 0
            ? (net_queue > VIDEO_QUEUE_CAP ? VIDEO_QUEUE_CAP : net_queue) : 1;
        int target_depth = network_source ? net_target : 3;
        /* Decoded-frame queue depth. As deep as RAM allows: while video runs
         * ahead of the audio clock, topping up the audio cushion decodes past
         * the queue cap and discards those frames, so the presented rate is
         * roughly depth / cushion_seconds - a deeper queue shows more frames
         * before each gap. A stall longer than the queue drains it and flips
         * playback into the smooth present-as-decoded regime (video trailing the
         * clock, no discards); a deeper queue also rides short stalls outright.
         * The clamps below keep the footprint within a safe slice of free RAM,
         * and video_cap is the ring modulus so the footprint is exactly
         * video_cap * frame_bytes. (Fully gap-free video would need the queue to
         * span the whole cushion - ~63 frames / ~42 MB here - which no RAM-safe
         * queue can hold; that is the compressed-prefetch worker's job.) */
        unsigned long cushion_ms;
        size_t frame_bytes = (size_t)vi->width * (size_t)vi->height * 3;
        ULONG free_any = AvailMem(MEMF_ANY);
        /* Only a third of the (post-floor) free pool is a safety ceiling; the
         * shallow default is far below it, but it protects a tight machine and
         * an explicit --net-queue. video_cap is the ring modulus below, so the
         * RGB footprint is exactly video_cap * frame_bytes. */
        size_t budget = free_any > VIDEO_QUEUE_MEM_FLOOR
                      ? (size_t)(free_any - VIDEO_QUEUE_MEM_FLOOR) / 3 : 0;
        int budget_frames = frame_bytes ? (int)(budget / frame_bytes) : 0;
        int video_cap = network_source ? VIDEO_QUEUE_NET_DEPTH
                                        : VIDEO_QUEUE_DISK_DEPTH;
        if (net_queue > 0 && video_cap < net_target) video_cap = net_target;
        if (video_cap < target_depth) video_cap = target_depth;
        if (budget_frames > 0 && video_cap > budget_frames)
            video_cap = budget_frames;
        if (video_cap < 2) video_cap = 2;          /* ring needs >= 2 slots    */
        if (video_cap > VIDEO_QUEUE_CAP) video_cap = VIDEO_QUEUE_CAP;
        if (prefetch_on) {
            cushion_ms = AUDIO_CUSHION_PREFETCH_MS;
        } else if (network_source) {
            uint64_t frame_period_us = vi->rate
                ? (uint64_t)(vi->scale ? vi->scale : 1) * 1000000ULL / vi->rate
                : 83333ULL;
            uint64_t queue_cushion_us =
                (uint64_t)(video_cap - 2) * frame_period_us;
            cushion_ms = (unsigned long)(queue_cushion_us / 1000ULL);
            if (cushion_ms < AUDIO_CUSHION_MIN_MS)
                cushion_ms = AUDIO_CUSHION_MIN_MS;
            if (cushion_ms > AUDIO_CUSHION_TARGET_MS)
                cushion_ms = AUDIO_CUSHION_TARGET_MS;
        } else {
            cushion_ms = AUDIO_CUSHION_TARGET_MS;
        }
        int prev_master_source = -1;
        if (want_time)
            printf("video-queue: cap=%d frames (%s, cushion=%lu ms, "
                   "frame=%lu KB, free=%lu KB)\n",
                   video_cap, network_source ? "network" : "disk", cushion_ms,
                   (unsigned long)(frame_bytes / 1024),
                   (unsigned long)(free_any / 1024));

        /* Wire the present-during-fetch context onto the shared service callback.
         * It touches the queue only while `released` is set around the blocking
         * demux read below, so it can advance video through a segment stall the
         * single loop is stuck in - the smooth-playback goal the prefetch worker
         * chased, without a second task, its socket lifecycle, or its teardown. */
        video_presenter presenter;
        presenter.released = 0;
        presenter.presenting = 0;
        presenter.disp = disp;
        presenter.audio = audio;
        presenter.mc = &mc;
        presenter.vi = vi;
        presenter.vq = vq;
        presenter.video_cap = video_cap;   /* same ring modulus the loop uses  */
        presenter.qhead = &qhead;
        presenter.qcount = &qcount;
        presenter.playback_started = &playback_started;
        presenter.mono_base_us = &mono_base_us;
        presenter.frames = &frames;
        presenter.stats = &stats;
        presenter.want_time = want_time;
        trace.presenter = &presenter;

    /* The live-resync term keeps the loop alive on EOF so the reconnect block in
     * the body can run; without it the loop would exit here (empty queue, no
     * --loop) before reconnect ever gets a chance. That block owns its own exits
     * (give-up, mismatch, no-progress backstop). */
    while (!quit && (!input_eof || qcount || loop ||
                     (network_source && live_resync))) {
        queued_video *front = qcount ? &vq[qhead] : NULL;
        uint64_t now = monotonic_us();
        uint64_t period_us = vi->rate
            ? (uint64_t)(vi->scale ? vi->scale : 1) * 1000000ULL / vi->rate
            : 83333ULL;
        int64_t late_us = 0;
        uint64_t master_clock_us = 0;
        unsigned long audio_ms = 0;
        int startup_refill = 0;
        int have_deadline = 0;
        int starved = 1;
        uint64_t audio_elapsed_raw_us = 0;
        uint64_t audio_media_clock_us = 0;
        uint64_t mono_media_clock_us = now - mono_base_us;

        /* Build the startup cushion in the software FIFO before starting
         * Paula; otherwise each small packet starts playing immediately and
         * the prebuffer can never grow to the warning threshold. */
        trace_phase(&trace, "scheduler");
        if (audio && playback_started) service_audio_for_display(&trace);
        if (audio) audio_ms = audio_buffered_ms(audio);
        startup_refill = audio && !playback_started &&
                         audio_ms < AUDIO_STARTUP_TARGET_MS;
        if (rescue_cooldown) rescue_cooldown--;
        {
            mr_audio_diagnostics rd;
            int rescue_critical = 0;
            int rescue_needed = 0;
            memset(&rd, 0, sizeof rd);
            if (audio) {
                unsigned long hardware_ms;
                audio_diagnostics(audio, &rd);
                hardware_ms = rd.hardware_playing_remaining_ms +
                              rd.hardware_queued_ms;
                rescue_critical =
                    rd.fifo_buffered_ms < AUDIO_RESCUE_FIFO_NEAR_EMPTY_MS &&
                    hardware_ms < AUDIO_RESCUE_ONE_REQUEST_MS;
                /* With two live requests, a useful FIFO cushion is healthy;
                 * the output task no longer needs scheduler intervention.
                 * Rescue only a genuinely short PCM horizon or a request
                 * queue which has already fallen below two active writes. */
                rescue_needed = rescue_critical ||
                    (audio_ms < AUDIO_RESCUE_ENTRY_MS &&
                     (rd.active_requests < 2 ||
                      rd.fifo_buffered_ms < AUDIO_RESCUE_FIFO_NEAR_EMPTY_MS));
            }
        if (audio && playback_started && !rescue_active && !input_eof &&
            !rescue_cooldown && (rescue_priority || rescue_needed)) {
            rescue_active = 1;
            rescue_priority = 1;
            rescue_episode_critical = rescue_critical;
            rescue_started_us = now;
            rescue_buffer_before = audio_ms;
            rescue_entry_threshold = AUDIO_RESCUE_ENTRY_MS;
            rescue_min_buffer = audio_ms;
            rescue_hw_before = rd.hardware_starvations;
            rescue_episode_packets = rescue_episode_audio = 0;
            rescue_episode_video = rescue_episode_queued = 0;
            rescue_episode_skipped = rescue_episode_replaced = 0;
            rescue_newest_retained_pts_us = 0;
            stats.rescue_entries++;
            if (rescue_critical) stats.rescue_critical++;
            else stats.rescue_noncritical++;
        }
        }
        if (rescue_active) {
            const char *reason = NULL;
            uint64_t rescue_elapsed = now - rescue_started_us;
            mr_audio_diagnostics current_audio;
            int ev = player_event(disp);
            memset(&current_audio, 0, sizeof current_audio);
            audio_diagnostics(audio, &current_audio);
            if (audio_ms < rescue_min_buffer) rescue_min_buffer = audio_ms;
            if (ev == MR_EV_QUIT) { quit = 1; break; }
            if (ev == MR_EV_PAUSE) { paused = 1; reason = "limit"; }
            if (rescue_episode_audio && current_audio.active_requests == 2 &&
                audio_ms >= AUDIO_RESCUE_ENTRY_MS) {
                reason = "audio"; stats.rescue_exit_target++;
            } else if (audio_ms >= AUDIO_RESCUE_TARGET_MS) {
                reason = "target"; stats.rescue_exit_target++;
            } else if (rescue_episode_packets >= AUDIO_RESCUE_MAX_PACKETS ||
                       rescue_elapsed >= AUDIO_RESCUE_MAX_US || paused) {
                reason = "limit"; stats.rescue_exit_limit++;
            } else if (input_eof) {
                reason = "eof"; stats.rescue_exit_eof++;
            }
            if (reason) {
                long margin_ms = (long)rescue_buffer_before -
                                 (long)(rescue_elapsed / 1000);
                mr_audio_diagnostics rd;
                audio_diagnostics(audio, &rd);
                rescue_active = 0;
                rescue_priority = 0;
                if (reason[0] == 'l') rescue_cooldown = 2;
                stats.rescue_us += rescue_elapsed;
                if (rescue_elapsed > stats.rescue_max_us)
                    stats.rescue_max_us = (unsigned long)rescue_elapsed;
                if (stats.rescue_entries == 1 || margin_ms < stats.rescue_min_margin_ms)
                    stats.rescue_min_margin_ms = margin_ms;
                if (margin_ms < 0) stats.rescue_negative_margin++;
                stats.rescue_hw_starvations +=
                    rd.hardware_starvations - rescue_hw_before;
                stats.rescue_newest_retained_pts_us =
                    rescue_newest_retained_pts_us;
                if (qcount && playback_started) {
                    uint64_t rescue_clock =
                        media_clock_rescue_estimate(&mc,
                                                    audio_elapsed_us(audio),
                                                    now);
                    queued_video *new_front = &vq[qhead];
                    int64_t post_late = (int64_t)rescue_clock -
                                        (int64_t)new_front->pts_us;
                    while (post_late > (int64_t)period_us && qcount > 1) {
                        stats.dropped++;
                        qhead = (qhead + 1) % video_cap;
                        qcount--;
                        new_front = &vq[qhead];
                        post_late = (int64_t)rescue_clock -
                                    (int64_t)new_front->pts_us;
                    }
                    stats.rescue_post_lateness_us = post_late;
                }
                front = qcount ? &vq[qhead] : NULL;
                if (want_time)
                    printf("audio-rescue reason=%s urgency=%s packets=%u audio=%u "
                           "video=%u retained=%u replaced=%u reference-only=%u "
                           "newest-pts=%lu post-late=%ld us duration=%lu us "
                           "buffer=%lu->%lu ms min=%lu ms consumed=%lu ms "
                           "entry-threshold=%lu ms margin=%ld ms "
                           "hw-starvations=%lu\n", reason,
                           rescue_episode_critical ? "critical" : "warning",
                           rescue_episode_packets, rescue_episode_audio,
                           rescue_episode_video, rescue_episode_queued,
                           rescue_episode_replaced, rescue_episode_skipped,
                           (unsigned long)rescue_newest_retained_pts_us,
                           (long)stats.rescue_post_lateness_us,
                           (unsigned long)rescue_elapsed,
                           rescue_buffer_before, audio_ms, rescue_min_buffer,
                           (unsigned long)(rescue_elapsed / 1000),
                           rescue_entry_threshold, margin_ms,
                           rd.hardware_starvations - rescue_hw_before);
                if (audio) service_audio_for_display(&trace);
            }
        }
        if (playback_started && front) {
            starved = audio ? audio_starved(audio) : 1;
            audio_elapsed_raw_us = audio ? audio_elapsed_us(audio) : 0;
            mono_media_clock_us = now - mono_base_us;
            if (audio) {
                audio_media_clock_us = current_media_clock_us(
                    &mc, 1, starved, audio_elapsed_raw_us, now, want_time);
                master_clock_us = audio_media_clock_us;
            } else {
                audio_media_clock_us = 0;
                mc.source = MCLOCK_MONO;
                master_clock_us = mono_media_clock_us;
            }
            late_us = (int64_t)master_clock_us - (int64_t)front->pts_us;
            have_deadline = 1;
        }

        /* Catastrophic fall-behind recovery. A multi-second network stall can
         * leave a live stream many seconds behind the wall clock, the audio
         * clock unable to climb back. Fast-consume the buffered backlog (decode
         * video reference-only, discard audio) up to the download frontier, then
         * flush audio and re-prime exactly like first start - landing near the
         * live edge. Opt-in, network-only; the trigger sits far above the ~1 s
         * lag ordinary boundary stalls produce, so it never fires in normal
         * playback. have_deadline guarantees both clocks are valid here. */
        if (live_resync && network_source && audio && have_deadline &&
            mono_media_clock_us >
                audio_media_clock_us + (uint64_t)LIVE_RESYNC_BEHIND_US) {
            uint64_t behind = mono_media_clock_us - audio_media_clock_us;
            uint64_t want = behind > LIVE_RESYNC_TARGET_US
                          ? behind - LIVE_RESYNC_TARGET_US : behind;
            uint64_t cu_start = monotonic_us();
            uint64_t base_pts = 0;
            int have_base = 0;
            if (want_time)
                printf("live-resync: %lu ms behind live, catching up\n",
                       (unsigned long)(behind / 1000));
            audio_set_running(audio, 0);
            display_set_status(disp, "Buffering...");
            qcount = 0; qhead = 0;               /* stale pictures, far behind */
            mr_h264_set_skip_output(&dec, 1);    /* reference-only: fast/no RGB */
            for (;;) {
                uint64_t r0, rdt;
                mr_status ns;
                if (player_event(disp) == MR_EV_QUIT) { quit = 1; break; }
                r0 = monotonic_us();
                ns = mr_demux_next_packet(dx, &pkt);
                rdt = monotonic_us() - r0;
                if (ns != MR_OK) { input_eof = 1; break; }
                if (rdt > LIVE_RESYNC_EDGE_US) break;   /* reached the frontier */
                if (pkt.is_video && pkt.len) {
                    mr_h264_set_input_pts(&dec, pkt.has_pts, pkt.pts_us);
                    mr_decoder_decode(&dec, pkt.data, pkt.len);
                }
                /* audio packets are discarded: the FIFO is flushed below */
                if (pkt.has_pts) {
                    if (!have_base) { base_pts = pkt.pts_us; have_base = 1; }
                    else if (pkt.pts_us > base_pts &&
                             pkt.pts_us - base_pts >= want) break;
                }
                if (monotonic_us() - cu_start > LIVE_RESYNC_MAX_US) break;
                service_audio_for_display(&trace);
            }
            mr_h264_set_skip_output(&dec, 0);
            audio_flush(audio);
            /* Re-prime from the new position; the startup path below refills the
             * queue and audio cushion and restarts playback near the edge. */
            playback_started = 0;
            decoded_index = 0; mono_base_us = 0;
            have_container_pts = 0; last_container_pts_us = 0;
            container_pts_adjust_us = 0;
            media_clock_rebase(&mc, audio_elapsed_us(audio), 0);
            stats.timing_rebases++;
            continue;
        }

        if (!rescue_active && have_deadline &&
            late_us >= -(int64_t)PRESENTATION_GUARD_US) {
            int ev;
            trace_phase(&trace, "event-processing");
            ev = player_event(disp);
            if (ev == MR_EV_QUIT) { quit = 1; break; }
            if (ev == MR_EV_PAUSE) {
                paused = 1;
                if (audio) audio_set_running(audio, 0);
            }
            if (ev == MR_EV_SEEK_FWD) fast_forward = !fast_forward;
            while (paused && !quit) {
                ev = player_event(disp);
                if (ev == MR_EV_QUIT) quit = 1;
                else if (ev == MR_EV_PAUSE) {
                    paused = 0; mono_base_us = monotonic_us() - front->pts_us;
                    if (audio) {
                        media_clock_rebase(&mc, audio_elapsed_us(audio),
                                           front->pts_us);
                        stats.timing_rebases++;
                        audio_set_running(audio, 1);
                    }
                }
                if (audio) service_audio_for_display(&trace);
                {
                    uint64_t delay_begin = monotonic_us();
                    Delay(1);
                    trace.delay_ticks = 1;
                    trace.sleep_actual_us = monotonic_us() - delay_begin;
                }
            }
            if (quit) break;
            now = monotonic_us();
            trace_phase(&trace, "deadline-drop");
            starved = audio ? audio_starved(audio) : 1;
            audio_elapsed_raw_us = audio ? audio_elapsed_us(audio) : 0;
            mono_media_clock_us = now - mono_base_us;
            if (audio) {
                audio_media_clock_us = current_media_clock_us(
                    &mc, 1, starved, audio_elapsed_raw_us, now, want_time);
                master_clock_us = audio_media_clock_us;
            } else {
                audio_media_clock_us = 0;
                mc.source = MCLOCK_MONO;
                master_clock_us = mono_media_clock_us;
            }
            late_us = (int64_t)master_clock_us - (int64_t)front->pts_us;
            if (late_us > 0) stats.late++;
            stats.dropped_in_pass = 0;
            /* Select once per scheduler pass. Re-evaluate each newer queued
             * PTS against the same audio-clock sample, stopping as soon as it
             * is useful or only the newest decoded frame remains. */
            while (!fast_forward && late_us > (int64_t)period_us && qcount > 1) {
                stats.dropped++; stats.dropped_in_pass++;
                qhead = (qhead + 1) % video_cap; qcount--;
                front = &vq[qhead];
                late_us = (int64_t)master_clock_us - (int64_t)front->pts_us;
            }
            {
                int source_changed = prev_master_source >= 0 &&
                                     prev_master_source != mc.source;
                int64_t delta_clocks_us = (int64_t)audio_media_clock_us -
                                          (int64_t)mono_media_clock_us;
                uint64_t abs_delta_us = delta_clocks_us < 0
                    ? (uint64_t)(-delta_clocks_us) : (uint64_t)delta_clocks_us;
                int big_delta = abs_delta_us > period_us;
                int multi_drop = stats.dropped_in_pass > 1;
                if (want_time && (source_changed || big_delta || multi_drop)) {
                    printf("clock-trace src=%c->%c starved=%d "
                           "audio-elapsed=%lu offset=%ld "
                           "audio-clock=%lu mono-clock=%lu delta=%ld "
                           "front-pts=%lu late=%ld qcount=%d drops=%u\n",
                           prev_master_source == MCLOCK_AUDIO ? 'A' :
                               prev_master_source == MCLOCK_HOLDOVER ? 'H' :
                               prev_master_source == MCLOCK_MONO ? 'M' : '?',
                           mc.source == MCLOCK_AUDIO ? 'A' :
                               mc.source == MCLOCK_HOLDOVER ? 'H' : 'M',
                           starved,
                           (unsigned long)audio_elapsed_raw_us,
                           (long)mc.audio_to_media_offset_us,
                           (unsigned long)audio_media_clock_us,
                           (unsigned long)mono_media_clock_us,
                           (long)delta_clocks_us,
                           (unsigned long)(front ? front->pts_us : 0),
                           (long)late_us,
                           qcount,
                           stats.dropped_in_pass);
                }
                prev_master_source = mc.source;
            }
            stats.frame_pts_us = front->pts_us;
            stats.audio_clock_us = master_clock_us;
            stats.calculated_lateness_us = late_us;
            stats.queue_head = (unsigned)qhead;
            if (late_us < -(int64_t)PRESENTATION_GUARD_US) continue;
            now = monotonic_us();
            {
                unsigned long audio_before = audio ? audio_buffered_ms(audio) : 0;
            trace_phase(&trace, "cgx-prepare/transfer");
            if (audio) service_audio_for_display(&trace);
            if (h264_pipeline_diag_enabled && h264_pipeline_stage < 4) {
                h264_pipeline_checkpoint_player("pre-display-main",
                                                qcount, playback_started);
                h264_pipeline_stage = 4;
            }
            display_show_rgb(disp, front->rgb, front->width, front->height,
                             front->stride, front->dirty_y0, front->dirty_y1);
            player_first_frame_presented();
            if (h264_pipeline_diag_enabled && h264_pipeline_stage < 5) {
                h264_pipeline_checkpoint_player("post-display-main",
                                                qcount, playback_started);
                h264_pipeline_stage = 5;
            }
                if (audio) service_audio_for_display(&trace);
                if (want_time) {
                    mr_display_timing rt;
                    if (display_rtg_frame_timing(disp, &rt)) {
                        stats.rtg_prepare_us += rt.prepare_us;
                        stats.rtg_scale_us += rt.scale_us;
                        stats.rtg_convert_us += rt.convert_us;
                        stats.rtg_copy_us += rt.copy_us;
                        stats.rtg_blit_us += rt.blit_us;
                        stats.rtg_clip_us += rt.clip_us;
                        stats.rtg_total_us += rt.total_us;
                        if (rt.prepare_us > stats.rtg_prepare_max_us)
                            stats.rtg_prepare_max_us = rt.prepare_us;
                        if (rt.blit_us > stats.rtg_blit_max_us)
                            stats.rtg_blit_max_us = rt.blit_us;
                        stats.last_rtg = rt;
                        stats.audio_before = audio_before;
                        stats.audio_after = audio ? audio_buffered_ms(audio) : 0;
                    }
                }
            }
            {
                unsigned long show_us = (unsigned long)(monotonic_us() - now);
                unsigned long enc_ms = 0, blit_ms = 0;
                display_aga_frame_timing(&enc_ms, &blit_ms);
                stats.convert_us += (uint64_t)enc_ms * 1000;
                stats.display_us += show_us;
                if (show_us > stats.display_max_us) stats.display_max_us = show_us;
            }
            stats.latency_us += monotonic_us() - front->decoded_at_us;
            stats.presented++; frames++;
            qhead = (qhead + 1) % video_cap; qcount--;
            now = monotonic_us();
            if (want_time && now - stats.since_us >= STATS_INTERVAL_US) {
                trace_phase(&trace, "scheduler-diagnostics");
                if (audio) service_audio_for_display(&trace);
                report_stats(&stats, audio, dx, &trace, qcount, now);
                if (audio) service_audio_for_display(&trace);
            }
            continue;
        }

        if (have_deadline) {
            int source_changed = prev_master_source >= 0 &&
                                 prev_master_source != mc.source;
            int64_t delta_clocks_us = (int64_t)audio_media_clock_us -
                                      (int64_t)mono_media_clock_us;
            uint64_t abs_delta_us = delta_clocks_us < 0
                ? (uint64_t)(-delta_clocks_us) : (uint64_t)delta_clocks_us;
            int big_delta = abs_delta_us > period_us;
            if (want_time && (source_changed || big_delta)) {
                printf("clock-trace src=%c->%c starved=%d "
                       "audio-elapsed=%lu offset=%ld "
                       "audio-clock=%lu mono-clock=%lu delta=%ld "
                       "front-pts=%lu late=%ld qcount=%d drops=%u\n",
                       prev_master_source == MCLOCK_AUDIO ? 'A' :
                           prev_master_source == MCLOCK_HOLDOVER ? 'H' :
                           prev_master_source == MCLOCK_MONO ? 'M' : '?',
                       mc.source == MCLOCK_AUDIO ? 'A' :
                           mc.source == MCLOCK_HOLDOVER ? 'H' : 'M',
                       starved,
                       (unsigned long)audio_elapsed_raw_us,
                       (long)mc.audio_to_media_offset_us,
                       (unsigned long)audio_media_clock_us,
                       (unsigned long)mono_media_clock_us,
                       (long)delta_clocks_us,
                       (unsigned long)(front ? front->pts_us : 0),
                       (long)late_us,
                       qcount,
                       0U);
            }
            prev_master_source = mc.source;
        }

        if (input_eof && !qcount && loop) {
            if (audio) audio_set_running(audio, 0);
            mr_demux_rewind(dx);
            if (mr_decoder_reset(&dec) != MR_OK ||
                !apply_h264_speed(&dec, h264_speed, 0)) break;
            if (audio_dec) mr_audio_decoder_reset(audio_dec);
            input_eof = 0; decoded_index = 0; mono_base_us = 0;
            have_container_pts = 0; last_container_pts_us = 0;
            container_pts_adjust_us = 0;
            playback_started = 0;
            if (audio)
                media_clock_rebase(&mc, audio_elapsed_us(audio), 0);
            else
                memset(&mc, 0, sizeof mc);
            continue;
        }

        /* A live network stream lost its source: a dropout exhausted the HLS
         * retry budget or the connection dropped, and mr_demux_next_packet
         * reported end of stream. Rather than ending playback, reopen the URL -
         * which resolves to the current live edge - and resume. Bounded, and a
         * mismatched restream stops cleanly. Gated with --live-resync; the
         * default still ends on EOF. */
        if (input_eof && !qcount && !loop && network_source && live_resync) {
            int tries, backoff = 12;                 /* ~0.5 s, grows to ~4 s   */
            const mr_video_info *nvi;
            /* If an unhealthy TLS drop has disabled HTTPS for this process, no
             * reopen can ever succeed - end cleanly instead of spinning through
             * every retry. (This is the AmiSSL "relaunch to resume" limitation.) */
            if (mr_http_tls_disabled()) {
                display_set_status(disp, "Connection lost - relaunch");
                if (want_time) printf("live-reconnect: HTTPS disabled, ending\n");
                break;
            }
            /* Give up if we keep reopening but never actually play: a stream that
             * reconnects and immediately ends again would otherwise spin on the
             * network. Any presented frame since the last reopen counts as
             * progress and resets the tally. */
            if (frames != frames_at_last_reconnect) {
                reconnects_without_progress = 0;
                frames_at_last_reconnect = frames;
            } else if (++reconnects_without_progress > LIVE_RECONNECT_STALL_LIMIT) {
                break;
            }
            if (audio) { audio_set_running(audio, 0); audio_flush(audio); }
            display_set_status(disp, "Reconnecting...");
            mr_demux_close(dx);
            dx = NULL;
            for (tries = 0; tries < LIVE_RECONNECT_TRIES && !quit; tries++) {
                int d;
                if (player_event(disp) == MR_EV_QUIT) { quit = 1; break; }
                if (want_time)
                    printf("live-reconnect: attempt %d/%d\n",
                           tries + 1, LIVE_RECONNECT_TRIES);
                dx = mr_demux_open_file_ex(media_path,
                        have_http_options ? &http_options : NULL);
                if (dx) break;
                /* Back off between attempts so a sustained outage is not hammered
                 * every half second; stay responsive to ESC throughout. */
                for (d = 0; d < backoff && !quit; d++) {
                    if (player_event(disp) == MR_EV_QUIT) { quit = 1; break; }
                    if (audio) audio_service(audio);
                    Delay(2);
                }
                backoff = backoff < 100 ? backoff * 2 : 100;   /* cap ~4 s */
            }
            if (quit) break;
            if (!dx) break;                     /* gave up: end playback */
            nvi = mr_demux_video(dx);
            if (!nvi || !nvi->valid || nvi->width != vi->width ||
                nvi->height != vi->height)
                break;                          /* different shape: stop cleanly */
            vi = nvi;
            if (mr_decoder_reset(&dec) != MR_OK ||
                !apply_h264_speed(&dec, h264_speed, 0)) break;
            if (audio_dec) mr_audio_decoder_reset(audio_dec);
            input_eof = 0; decoded_index = 0; mono_base_us = 0;
            have_container_pts = 0; last_container_pts_us = 0;
            container_pts_adjust_us = 0;
            playback_started = 0;
            qcount = 0; qhead = 0;
            if (audio) media_clock_rebase(&mc, audio_elapsed_us(audio), 0);
            else memset(&mc, 0, sizeof mc);
            stats.timing_rebases++;
            continue;
        }

        /* At most one packet per scheduler iteration. URL sources default to
         * depth one: without an asynchronous reader, a blocking HLS fetch
         * cannot be allowed to delay a frame already queued for presentation.
         * When --net-queue raises the network depth, read-ahead is instead
         * governed by the same lateness margin the disk path uses, so a frame
         * is only decoded ahead while the queue front is comfortably far from
         * its deadline. */
        {
            int refill_audio = audio && audio_ms < AUDIO_REFILL_WARNING_MS;
            /* With the video queue already full the loop would otherwise sleep,
             * which stops the audio FIFO being refilled - after a long stall the
             * cushion then never rebuilds and playback settles into a degraded
             * state. Keep demuxing to top the cushion back up; the audio lands
             * in the cheap PCM FIFO, and with the video queue shallow the video
             * decoded alongside it is presented as it comes (present-as-decoded)
             * rather than piling into a deep buffer.
             *
             * The cushion rides segment-fetch stalls for audio; it is deliberate
             * that it does NOT drive the decoded-video queue deep (that was the
             * judder). A smaller cushion is used when the prefetch worker is on,
             * since it absorbs stalls at the compressed level. */
            int feed_audio_full = audio && qcount >= target_depth &&
                                  audio_ms < cushion_ms;
            int can_decode = !input_eof && !(!rescue_active && rescue_priority) &&
                             (rescue_active || startup_refill ||
                              qcount < target_depth || feed_audio_full);
            if (can_decode && playback_started && qcount) {
                uint64_t margin = stats.video_decode_max_us +
                                  PRESENTATION_GUARD_US + 2000ULL;
                if (!refill_audio && !feed_audio_full &&
                    ((network_source && target_depth <= 1) ||
                     late_us > -(int64_t)margin))
                    can_decode = 0;
            }
            if (can_decode) {
                int ready_before = qcount > 0;
                int64_t due_before = late_us;
                uint64_t refill_started = monotonic_us();
                uint64_t a;
                trace_phase(&trace, "demux-read");
                if (audio) service_audio_for_display(&trace);
                a = monotonic_us();
                /* Hand the queue to the service callback for the duration of the
                 * fetch: this is the one place the single loop blocks long enough
                 * (a segment boundary can stall ~1.7 s) to freeze video. While
                 * released, the demux service hook presents due frames from the
                 * decode queue; we reclaim it the instant the read returns and
                 * read qhead/qcount fresh below. Network sources only - a local
                 * read never stalls, so nothing is released and behaviour there
                 * is unchanged. */
                if (network_source) presenter.released = 1;
                mr_status next = mr_demux_next_packet(dx, &pkt);
                presenter.released = 0;
                if (audio) service_audio_for_display(&trace);
                uint64_t b = monotonic_us();
                uint64_t blocked = b - a;
                stats.demux_us += blocked; stats.refill_block_us += blocked;
                stats.samples++;
                if (rescue_active) {
                    rescue_episode_packets++; stats.rescue_packets++;
                }
                if (network_source) stats.network_us += blocked;
                if (next != MR_OK) input_eof = 1;
                else if (!pkt.is_video) {
                    if (audio && audio_dec) {
                        uint64_t audio_end;
                        trace_phase(&trace, "audio-decode");
                        service_audio_for_display(&trace);
                        a = monotonic_us();
                        mr_audio_decoder_feed(audio_dec, pkt.data, pkt.len,
                                              decoded_audio_sink, audio);
                        audio_end = monotonic_us();
                        service_audio_for_display(&trace);
                        stats.audio_decode_us += audio_end - a;
                        if (rescue_active) {
                            rescue_episode_audio++;
                            stats.rescue_audio_packets++;
                        }
                    }
                } else if (pkt.len) {
                    mr_status decode_status;
                    uint64_t decode_end;
                    trace_phase(&trace, "h264-decode");
                    /* A frame decoded into a full queue is dropped (newest-out),
                     * so decode it reference-only: keep the reference chain
                     * intact without spending time producing RGB we discard.
                     * This is what makes the audio-cushion top-up above cheap. */
                    mr_h264_set_skip_output(&dec, qcount >= video_cap);
                    mr_h264_set_input_pts(&dec, pkt.has_pts, pkt.pts_us);
                    if (audio) service_audio_for_display(&trace);
                    a = monotonic_us();
                    decode_status = mr_decoder_decode(&dec, pkt.data, pkt.len);
                    decode_end = monotonic_us();
                    if (audio) service_audio_for_display(&trace);
                    {
                        mr_h264_timing ht;
                        mr_h264_frame_timing(&dec, &ht);
                        stats.h264_input_us += ht.input_us;
                        stats.h264_core_us += ht.core_us;
                        stats.h264_output_us += ht.output_us;
                        if (ht.input_us > stats.h264_input_max_us)
                            stats.h264_input_max_us = ht.input_us;
                        if (ht.core_us > stats.h264_core_max_us)
                            stats.h264_core_max_us = ht.core_us;
                        if (ht.output_us > stats.h264_output_max_us)
                            stats.h264_output_max_us = ht.output_us;
                    }
                    if (decode_status == MR_ENOMEM) {
                        printf("h264-decode-oom: packet %lu len=%lu - "
                               "stopping playback\n",
                               (unsigned long)decoded_index,
                               (unsigned long)pkt.len);
                        display_set_status(disp, "H.264 decoder out of memory");
                        /* libavc marks IVD_MEM_ALLOC_FAILED fatal. Do not keep
                         * demuxing into a dead decoder: take mrplay's existing
                         * quit/cleanup path so RTG, Paula and decoder buffers are
                         * released immediately. */
                        quit = 1;
                        continue;
                    }
                    if (decode_status == MR_EFORMAT) {
                        printf("h264-decode-error: packet %lu len=%lu\n",
                               (unsigned long)decoded_index,
                               (unsigned long)pkt.len);
                    }
                    if (decode_status == MR_OK) {
                        if (h264_pipeline_diag_enabled && h264_pipeline_stage < 1) {
                            h264_pipeline_checkpoint_player("decoder-return",
                                                            qcount, playback_started);
                            h264_pipeline_stage = 1;
                        }
                        unsigned long decode_us =
                            (unsigned long)(decode_end - a);
                        uint64_t synthetic_pts = vi->rate
                            ? decoded_index *
                              (uint64_t)(vi->scale ? vi->scale : 1) *
                              1000000ULL / vi->rate
                            : decoded_index * 83333ULL;
                        uint64_t frame_pts_us = pkt.pts_us;
                        int frame_has_pts = pkt.has_pts;
                        uint64_t pts = synthetic_pts;
                        /* Libavc emits pictures in display order, which may
                         * differ from the current packet's decode order when
                         * B-frames are present.  Use the PTS mapped from the
                         * decoder's returned AU timestamp.  If that mapping is
                         * unavailable, synthetic output-order timing is safer
                         * than attaching the current packet's wrong PTS. */
                        if (codec == &mr_codec_h264)
                            frame_has_pts = mr_h264_output_pts(
                                &dec, &frame_pts_us);
                        if (frame_has_pts) {
                            int discontinuity = have_container_pts &&
                                (frame_pts_us + period_us * 10 < last_container_pts_us ||
                                 frame_pts_us > last_container_pts_us + period_us * 10);
                            if (!have_container_pts || discontinuity) {
                                container_pts_adjust_us =
                                    (int64_t)synthetic_pts - (int64_t)frame_pts_us;
                                if (have_container_pts) stats.timing_rebases++;
                            }
                            pts = (uint64_t)((int64_t)frame_pts_us +
                                             container_pts_adjust_us);
                            last_container_pts_us = frame_pts_us;
                            have_container_pts = 1;
                        }
                        stats.video_decode_us += decode_us; stats.decoded++;
                        if (decode_us > stats.video_decode_max_us)
                            stats.video_decode_max_us = decode_us;
                        {
                            int retain = !startup_refill ||
                                         qcount < video_cap ||
                                         rescue_active;
                            if (rescue_active) {
                                rescue_episode_video++;
                                stats.rescue_video_decoded++;
                            }
                            if (!retain) {
                                if (rescue_active) {
                                    rescue_episode_skipped++;
                                    stats.rescue_video_skipped++;
                                }
                                stats.dropped++;
                                decoded_index++;
                                continue;
                            }
                            if (qcount >= video_cap) {
                                /* Queue full. Every queued frame is closer to
                                 * its deadline than this freshly decoded one,
                                 * which sits furthest in the future, so keep
                                 * them and drop the newcomer. Evicting the
                                 * oldest instead (the previous behaviour) left
                                 * the queue front permanently ahead of the
                                 * audio clock, so presentation stalled and
                                 * never recovered after a long stall. The
                                 * packet was still consumed, so audio rescue
                                 * makes progress regardless. */
                                if (rescue_active) {
                                    rescue_episode_skipped++;
                                    stats.rescue_video_skipped++;
                                }
                                stats.dropped++;
                                decoded_index++;
                                continue;
                            }
                            queued_video *tail =
                                &vq[(qhead + qcount) % video_cap];
                            trace_phase(&trace, "frame-copy");
                            if (audio) service_audio_for_display(&trace);
                            a = monotonic_us();
                            if (!queue_copy(tail, &dec.frame, pts, monotonic_us(),
                                            decode_us)) {
                                /* Out of RAM for another RGB slot (a backstop -
                                 * the queue is sized to fit; see main). Never
                                 * quit: drop this frame exactly as a full queue
                                 * would and keep recycling the slots we own. The
                                 * cap is the ring modulus so it must not change;
                                 * the slot simply stays empty and is retried.
                                 * Log once so hardware tests can confirm whether
                                 * a grey window is caused by this OOM path. */
                                if (!oom_warned) {
                                    printf("warning: queue_copy OOM - "
                                           "frame %ux%u dropped; "
                                           "further OOM drops will be silent\n",
                                           dec.frame.width, dec.frame.height);
                                    oom_warned = 1;
                                }
                                stats.dropped++;
                                decoded_index++;
                                continue;
                            }
                            decode_end = monotonic_us();
                            if (audio) service_audio_for_display(&trace);
                            stats.frame_copy_us += decode_end - a;
                            qcount++;
                            if (h264_pipeline_diag_enabled && h264_pipeline_stage < 2) {
                                h264_pipeline_checkpoint_player("queue-copy",
                                                                qcount, playback_started);
                                h264_pipeline_stage = 2;
                            }
                            if (rescue_active) {
                                rescue_episode_queued++;
                                stats.rescue_video_queued++;
                                rescue_newest_retained_pts_us = pts;
                            }
                        }
                        decoded_index++;
                    }
                }
                if (ready_before && due_before < 0) {
                    uint64_t refill_elapsed = monotonic_us() - refill_started;
                    if (refill_elapsed > (uint64_t)(-due_before))
                        stats.refill_delayed_ready_us +=
                            refill_elapsed - (uint64_t)(-due_before);
                }
                if (!playback_started &&
                    (qcount >= startup_depth || input_eof) &&
                    (!audio || audio_buffered_ms(audio) >=
                               AUDIO_STARTUP_TARGET_MS || input_eof)) {
                    playback_started = qcount > 0;
                    if (playback_started) {
                        now = monotonic_us();
                        mono_base_us = now - vq[qhead].pts_us;
                        if (audio)
                            media_clock_rebase(&mc, audio_elapsed_us(audio), 0);
                        if (audio) {
                            service_audio_for_display(&trace);
                            audio_set_running(audio, 1);
                        }
                        display_set_status(disp, NULL);  /* clear Buffering... */
                        if (h264_pipeline_diag_enabled && h264_pipeline_stage < 3) {
                            h264_pipeline_checkpoint_player("playback-start",
                                                            qcount, playback_started);
                            h264_pipeline_stage = 3;
                        }
                    }
                }
                continue;
            }
        }

        if (audio && audio_buffered_ms(audio) < AUDIO_REFILL_WARNING_MS &&
            !input_eof && qcount < target_depth) {
            /* Do not burn a 20 ms DOS tick while audio is in refill mode. */
            service_audio_for_display(&trace);
            continue;
        } else if (qcount && playback_started) {
            uint64_t wait_us = late_us < -(int64_t)PRESENTATION_GUARD_US
                ? (uint64_t)(-late_us) - PRESENTATION_GUARD_US : 0;
            if (wait_us > 20000) wait_us = 20000;
            if (audio) service_audio_for_display(&trace);
            paced_sleep(wait_us, &trace, &stats);
        } else {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            {
                uint64_t delay_begin = monotonic_us();
                Delay(1);
                trace.delay_ticks = 1;
                trace.sleep_actual_us = monotonic_us() - delay_begin;
            }
        }
    }
    }
    /* `presenter` lived in the scheduler block just closed; the display service
     * hook is still installed and fires during the teardown flush's blits, so
     * drop the now-dangling pointer before any of that can dereference it. */
    trace.presenter = NULL;
    /* The HLS wait and HTTP service hooks hold &trace too; the teardown flush no
     * longer reads the demux, so retire them here before the scheduler locals
     * die. */
    mr_hls_set_wait(NULL, NULL);
    mr_http_set_service(NULL, NULL);

    /* MPEG-4 B-frame/display reordering holds the final anchor until EOF.
     * Drain it through the same pacing and display path so the player does not
     * silently finish one frame short (e.g. 129/130 on legacy OpenDivX). */
    while (!quit) {
        clock_t a = clock();
        mr_status ds = mr_decoder_flush(&dec);
        t_dec += clock() - a;
        if (ds != MR_OK) break;

        if (audio) {
            /* Target audio raw time for frame N: invert the signed offset.
             * audio_raw = media_target - audio_to_media_offset_us */
            int64_t flush_media_us = (int64_t)frames * (int64_t)(period * 1000UL);
            int64_t flush_audio_us = flush_media_us - mc.audio_to_media_offset_us;
            unsigned long target = (unsigned long)(s64_to_us(flush_audio_us) / 1000ULL);
            while (audio_elapsed_ms(audio) < target &&
                   !audio_starved(audio)) {
                int ev = player_event(disp);
                if (ev == MR_EV_QUIT) { quit = 1; break; }
                audio_service(audio);
                Delay(1);
            }
        } else {
            int ev = player_event(disp);
            if (ev == MR_EV_QUIT) quit = 1;
            Delay(ticks);
        }
        if (quit) break;

        a = clock();
        display_show_rgb(disp, dec.frame.data, dec.frame.width,
                         dec.frame.height, dec.frame.stride,
                         dec.frame.dirty_y0, dec.frame.dirty_y1);
        player_first_frame_presented();
        t_show += clock() - a;
        frames++;
    }
    }
    if (want_time && frames > 0) {
        unsigned long enc_ms = 0, blit_ms = 0;
        display_aga_timing(&enc_ms, &blit_ms);
        printf("timing/%d frames: decode=%lu ms, display=%lu ms"
               " (encode=%lu ms, blit=%lu ms)\n", frames,
               (unsigned long)(t_dec  * 1000 / CLOCKS_PER_SEC),
               (unsigned long)(t_show * 1000 / CLOCKS_PER_SEC),
               enc_ms, blit_ms);
        if (display_aga_kalms_timing(&blit_ms))
            printf("Kalms conversion: %lu ms\n", blit_ms);
    }
    /* Let any queued audio drain (bounded, so a wedged clock can't loop). */
    if (audio && !quit) {
        int guard = 0;
        while (!audio_starved(audio) && guard++ < 4000) {
            if (player_event(disp) == MR_EV_QUIT) {
                quit = 1;
                break;
            }
            audio_service(audio);
            Delay(1);
        }
    }
    if (audio) audio_set_running(audio, 0);

    if (!quit && !auto_close_eof) {
        printf("played %d frames - press ESC or close the window to exit\n",
               frames);
        while (player_event(disp) != MR_EV_QUIT) {
            if (audio) audio_service(audio);
            Delay(2);
        }
    }

    player_status(MR_PLAYER_STATE_ENDED, codec->name, "stream ended");
    { int qi; for (qi = 0; qi < VIDEO_QUEUE_CAP; qi++) free(vq[qi].rgb); }
    if (audio_dec) mr_audio_decoder_close(audio_dec);
    control_audio = NULL;
    if (audio) audio_close(audio);
    display_close(disp);
    mr_decoder_close(&dec);
    mr_demux_close(dx);
    /* Stop the prefetch worker from the main task before we exit, so it releases
     * its own socket/TLS state ahead of the atexit teardown. */
    if (prefetch_on) STOP_PREFETCH(NULL);
    free(buf);
    return mrplay_exit(0);
}
