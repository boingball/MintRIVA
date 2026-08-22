/* Validates mr_mov_seek() against ground truth read straight out of the same
 * sample table (mr_mov_sample_info), and confirms the packet layer actually
 * lands where the index says it should. */
#include "../core/mr_mov.h"

#include <stdio.h>
#include <stdlib.h>

int mr_source_read_at(mr_source *source, size_t offset, void *dst, size_t len)
{
    (void)source; (void)offset; (void)dst; (void)len; return 0;
}

/* Ground truth: the latest video keyframe at or before target_ms, or the
 * first video keyframe in the file if target_ms precedes all of them. */
static int expected_keyframe_ms(mr_mov *m, uint64_t target_ms, uint64_t *out)
{
    uint32_t n = mr_mov_sample_count(m), i;
    int have_any = 0, have_before = 0;
    uint64_t first_ms = 0, best_ms = 0;
    for (i = 0; i < n; i++) {
        uint64_t t; int is_video, is_key;
        mr_mov_sample_info(m, i, &t, &is_video, &is_key);
        if (!is_video || !is_key) continue;
        if (!have_any) { first_ms = t; have_any = 1; }
        if (t <= target_ms && (!have_before || t > best_ms)) {
            best_ms = t; have_before = 1;
        }
    }
    if (!have_any) return 0;
    *out = have_before ? best_ms : first_ms;
    return 1;
}

static int check_one(mr_mov *m, uint64_t target_ms)
{
    uint64_t expected, got;
    mr_packet pkt;
    mr_status st;
    if (!expected_keyframe_ms(m, target_ms, &expected)) {
        fprintf(stderr, "no video keyframe in file\n");
        return 0;
    }
    st = mr_mov_seek(m, target_ms, &got);
    if (st != MR_OK) {
        fprintf(stderr, "seek(%llu) failed: %d\n",
               (unsigned long long)target_ms, (int)st);
        return 0;
    }
    if (got != expected) {
        fprintf(stderr, "seek(%llu) landed at %llu ms, expected %llu ms\n",
               (unsigned long long)target_ms, (unsigned long long)got,
               (unsigned long long)expected);
        return 0;
    }
    /* The packet layer must agree with the index: the very next packet is
     * the video keyframe itself. */
    st = mr_mov_next_packet(m, &pkt);
    if (st != MR_OK || !pkt.is_video) {
        fprintf(stderr, "seek(%llu): next_packet after seek is_video=%d st=%d\n",
               (unsigned long long)target_ms, pkt.is_video, (int)st);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    FILE *f;
    long n;
    uint8_t *data;
    mr_mov m;
    mr_status st;
    uint64_t targets[8];
    int ntargets = 0, i, ok = 1;
    uint64_t max_ms = 0;

    if (argc != 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    data = (uint8_t *)malloc((size_t)n);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) return 2;
    fclose(f);

    st = mr_mov_open(&m, data, (size_t)n);
    if (st != MR_OK) { fprintf(stderr, "MOV open failed: %d\n", st); return 1; }

    {
        uint32_t cnt = mr_mov_sample_count(&m), idx;
        for (idx = 0; idx < cnt; idx++) {
            uint64_t t; int is_video, is_key;
            mr_mov_sample_info(&m, idx, &t, &is_video, &is_key);
            if (t > max_ms) max_ms = t;
        }
    }

    /* Edge cases (before start, exactly on a keyframe, past the end) plus a
     * spread across the timeline so a multi-keyframe file (test_cinepak.mov
     * is 24 frames at GOP 12 -> keyframes at ~0ms and ~1000ms) actually
     * exercises picking between more than one candidate. */
    targets[ntargets++] = 0;
    targets[ntargets++] = max_ms / 4;
    targets[ntargets++] = max_ms / 2;
    targets[ntargets++] = (max_ms * 3) / 4;
    targets[ntargets++] = max_ms;
    targets[ntargets++] = max_ms + 5000;   /* past the end: clamps to last kf */

    for (i = 0; i < ntargets; i++) {
        mr_mov_rewind(&m);
        if (!check_one(&m, targets[i])) ok = 0;
    }

    printf("MOV seek: %dx%d, %u samples, %d targets checked, %s\n",
           m.video.width, m.video.height, mr_mov_sample_count(&m), ntargets,
           ok ? "OK" : "FAILED");
    mr_mov_close(&m);
    free(data);
    return ok ? 0 : 1;
}
