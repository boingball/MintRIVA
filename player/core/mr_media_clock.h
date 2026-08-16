#ifndef MR_MEDIA_CLOCK_H
#define MR_MEDIA_CLOCK_H

#include <stdint.h>

/* Media-clock source tags used by the Amiga scheduler diagnostics. */
#define MCLOCK_MONO     0
#define MCLOCK_AUDIO    1
#define MCLOCK_HOLDOVER 2

/* Consecutive healthy samples required before a recovered audio clock is
 * labelled healthy again. While this gate is running, media time still follows
 * played audio samples; it never follows wall time. */
#define MCLOCK_RESUME_STABLE_ITERS 3U

typedef struct {
    uint64_t last_output_us;           /* last returned value (monotonic gate) */
    uint64_t holdover_media_us;        /* last position reached by real audio  */
    int64_t  audio_to_media_offset_us; /* media = audio_raw + this (signed)    */
    unsigned healthy_audio_samples;    /* non-starved iters since underrun     */
    int      holdover_active;
    int      source;                   /* MCLOCK_MONO / AUDIO / HOLDOVER       */
} media_clock;

/* Establish a new correspondence between Paula's cumulative played-sample
 * counter and the media PTS timeline. */
void media_clock_rebase(media_clock *mc, uint64_t audio_raw,
                        uint64_t target_media_us);

/* Estimate media time without mutating the controller. Used by audio rescue
 * before the scheduler's main clock tick. */
uint64_t media_clock_rescue_estimate(const media_clock *mc,
                                     uint64_t audio_raw);

/* Advance the clock by one scheduler iteration. A genuine audio underrun
 * freezes media time; once audio resumes, the clock advances only by samples
 * Paula has actually played. */
uint64_t current_media_clock_us(media_clock *mc, int have_audio,
                                int starved, uint64_t audio_raw,
                                int want_time);

#endif
