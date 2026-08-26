/*
 * Audio-only MPEG-TS acceptance shim for dual HLS playback.
 *
 * The established TS demuxer already discovers and decodes supported audio
 * PES streams, but its probe historically required a video PID because every
 * caller used it as an A/V container. VisionOS recorded HLS exposes audio as
 * its own TS media playlist. Keep the original probe untouched and relax only
 * its final result when there is genuinely no video stream, no unsupported
 * video codec, and a supported audio track was successfully probed.
 */
#define mr_ts_open mr_ts_open_video_required
#define mr_ts_open_source mr_ts_open_source_video_required
#include "mr_ts_base.c"
#undef mr_ts_open
#undef mr_ts_open_source

static int mr_ts_audio_only_ready(const mr_ts *t)
{
    return t &&
           t->video_pid == TS_PID_NONE &&
           !t->unsupported_video_type &&
           t->audio_pid != TS_PID_NONE &&
           t->audio.valid;
}

mr_status mr_ts_open(mr_ts *t, const uint8_t *buf, size_t len)
{
    mr_status st = mr_ts_open_video_required(t, buf, len);
    if (st == MR_EUNSUPPORTED && mr_ts_audio_only_ready(t)) {
        mr_ts_rewind(t);
        return MR_OK;
    }
    return st;
}

mr_status mr_ts_open_source(mr_ts *t, mr_source *source, size_t len)
{
    mr_status st = mr_ts_open_source_video_required(t, source, len);
    if (st == MR_EUNSUPPORTED && mr_ts_audio_only_ready(t)) {
        mr_ts_rewind(t);
        return MR_OK;
    }
    return st;
}
