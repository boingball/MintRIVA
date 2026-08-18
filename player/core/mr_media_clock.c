/*
 * MintVID - audio-master media clock.
 *
 * Paula's cumulative audio clock counts samples that were actually played; it
 * deliberately does not advance while the hardware and software FIFO are both
 * empty. Media time must obey the same rule. Advancing from wall time through
 * a real underrun permanently puts video ahead of audio: when audio restarts,
 * both clocks run at 1x and the lost interval can never be recovered.
 */
#include "mr_media_clock.h"

#include <stdio.h>

static uint64_t mapped_audio_us(const media_clock *mc, uint64_t audio_raw)
{
    int64_t mapped = (int64_t)audio_raw + mc->audio_to_media_offset_us;
    return mapped > 0 ? (uint64_t)mapped : 0;
}

void media_clock_rebase(media_clock *mc, uint64_t audio_raw,
                        uint64_t target_media_us)
{
    mc->audio_to_media_offset_us = (int64_t)target_media_us -
                                    (int64_t)audio_raw;
    mc->holdover_media_us = target_media_us;
    mc->holdover_active = 0;
    mc->healthy_audio_samples = 0;
    mc->last_output_us = target_media_us;
    mc->source = MCLOCK_AUDIO;
}

uint64_t media_clock_rescue_estimate(const media_clock *mc,
                                     uint64_t audio_raw)
{
    uint64_t candidate = mapped_audio_us(mc, audio_raw);
    if (mc->holdover_active && candidate < mc->holdover_media_us)
        candidate = mc->holdover_media_us;
    return candidate < mc->last_output_us ? mc->last_output_us : candidate;
}

uint64_t current_media_clock_us(media_clock *mc, int have_audio,
                                int starved, uint64_t audio_raw,
                                int want_time)
{
    uint64_t candidate;

    if (!have_audio) {
        mc->source = MCLOCK_MONO;
        return mc->last_output_us;
    }

    candidate = mapped_audio_us(mc, audio_raw);

    if (starved) {
        /* Capture any samples completed since the previous scheduler tick, then
         * freeze. Repeated calls during a long network wait return exactly this
         * value instead of advancing video from wall time. */
        if (!mc->holdover_active) {
            if (candidate < mc->last_output_us) candidate = mc->last_output_us;
            mc->holdover_media_us = candidate;
            mc->holdover_active = 1;
            if (want_time)
                printf("clock-holdover entered media=%lu us\n",
                       (unsigned long)mc->holdover_media_us);
        } else if (candidate > mc->holdover_media_us) {
            /* A partial recovery may have played samples before starving again.
             * Preserve that genuine progress while discarding elapsed silence. */
            mc->holdover_media_us = candidate;
        }
        mc->healthy_audio_samples = 0;
        mc->source = MCLOCK_HOLDOVER;
        mc->last_output_us = mc->holdover_media_us;
        return mc->last_output_us;
    }

    if (mc->holdover_active) {
        /* During the stability gate follow the recovered sample clock, not wall
         * time. If it starves again, the branch above freezes at this position. */
        mc->healthy_audio_samples++;
        if (candidate < mc->holdover_media_us)
            candidate = mc->holdover_media_us;
        if (candidate < mc->last_output_us)
            candidate = mc->last_output_us;
        mc->holdover_media_us = candidate;
        mc->last_output_us = candidate;

        if (mc->healthy_audio_samples < MCLOCK_RESUME_STABLE_ITERS) {
            mc->source = MCLOCK_HOLDOVER;
            return candidate;
        }

        mc->holdover_active = 0;
        if (want_time)
            printf("clock-holdover exit audio-raw=%lu us resume-at=%lu us "
                   "offset=%ld us\n",
                   (unsigned long)audio_raw,
                   (unsigned long)candidate,
                   (long)mc->audio_to_media_offset_us);
    }

    mc->source = MCLOCK_AUDIO;
    if (candidate < mc->last_output_us) candidate = mc->last_output_us;
    mc->last_output_us = candidate;
    return candidate;
}
