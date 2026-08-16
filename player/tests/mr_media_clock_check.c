#include "../core/mr_media_clock.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int expect(const char *what, uint64_t got, uint64_t want)
{
    if (got == want) return 1;
    fprintf(stderr, "%s: got %llu, want %llu\n", what,
            (unsigned long long)got, (unsigned long long)want);
    return 0;
}

int main(void)
{
    media_clock mc;
    int ok = 1;

    memset(&mc, 0, sizeof mc);
    media_clock_rebase(&mc, 0, 0);
    ok &= expect("normal audio", current_media_clock_us(&mc, 1, 0,
                                                        100000, 0), 100000);

    /* The final 20 ms completed since the previous tick is captured, then a
     * one-second wall-time stall must not move the media clock at all. */
    ok &= expect("underrun entry", current_media_clock_us(&mc, 1, 1,
                                                          120000, 0), 120000);
    ok &= expect("underrun remains frozen",
                 current_media_clock_us(&mc, 1, 1, 120000, 0), 120000);
    ok &= expect("rescue estimate remains frozen",
                 media_clock_rescue_estimate(&mc, 120000), 120000);

    /* Recovery follows samples immediately but remains tagged holdover until
     * three consecutive healthy observations have passed. */
    ok &= expect("recovery sample 1", current_media_clock_us(&mc, 1, 0,
                                                             130000, 0), 130000);
    ok &= mc.source == MCLOCK_HOLDOVER;
    ok &= expect("recovery sample 2", current_media_clock_us(&mc, 1, 0,
                                                             140000, 0), 140000);
    ok &= mc.source == MCLOCK_HOLDOVER;
    ok &= expect("recovery sample 3", current_media_clock_us(&mc, 1, 0,
                                                             150000, 0), 150000);
    ok &= mc.source == MCLOCK_AUDIO && !mc.holdover_active;

    /* A partial recovery followed by another underrun may advance only by the
     * samples that really played, never by time spent empty. */
    ok &= expect("second underrun", current_media_clock_us(&mc, 1, 1,
                                                           160000, 0), 160000);
    ok &= expect("partial recovery", current_media_clock_us(&mc, 1, 0,
                                                            165000, 0), 165000);
    ok &= expect("partial recovery re-starves",
                 current_media_clock_us(&mc, 1, 1, 170000, 0), 170000);
    ok &= expect("second gap remains frozen",
                 current_media_clock_us(&mc, 1, 1, 170000, 0), 170000);

    /* Exercise a non-zero signed mapping as used after pause/rebase. */
    media_clock_rebase(&mc, 5000000, 1000000);
    ok &= expect("signed mapping", current_media_clock_us(&mc, 1, 0,
                                                          5100000, 0), 1100000);
    ok &= expect("signed mapping underrun",
                 current_media_clock_us(&mc, 1, 1, 5120000, 0), 1120000);
    ok &= expect("signed mapping frozen",
                 current_media_clock_us(&mc, 1, 1, 5120000, 0), 1120000);

    if (!ok) return 1;
    puts("media clock underrun/resume checks passed");
    return 0;
}
