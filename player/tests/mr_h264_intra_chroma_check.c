/*
 * Real-vs-asm differential check for the H.264 chroma_8x8 intra
 * prediction modes (vendor/libavc/common/ih264_chroma_intra_pred_filters.c)
 * versus this project's m68k primitives (vert/horz/dc: ordinary C in
 * ih264_m68k_optim.c; plane: real hand asm in ih264_m68k_intra_pred.S).
 * Random neighbour-buffer contents are fed identically to the real
 * vendored function and the asm/C primitive, and every reconstructed
 * output byte is compared.
 *
 * Only assembled/exercised under MR_M68K_ASM - see tests/run_m68k_check.sh.
 */
#include "ih264_typedefs.h"
#include "ih264_intra_pred_filters.h"
#if defined(MR_M68K_ASM)
#include "ih264_m68k_optim.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static uint32_t xrand(uint32_t *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7fffu;
}

static void report(const char *what, uint32_t seed, int idx, int expected,
                   int got)
{
    if (g_failures < 20)
        fprintf(stderr, "FAIL %s: seed=%u idx=%d expected=%d got=%d\n",
                what, seed, idx, expected, got);
    g_failures++;
}

#if defined(MR_M68K_ASM)
/* Chroma S[]: S[0..15]=left reversed (interleaved U,V pairs), S[16..17]=
 * topleft (U,V), S[18..33]=top+topright (interleaved) - see
 * ih264_m68k_optim.c's chroma vert/horz/dc header comment. STRD wider
 * than 16 so an out-of-bounds column write lands in padding instead of
 * silently matching by accident. */
#define SBUF 34
#define STRD 24
#define OUTBUF (STRD * 8)

typedef void mode_fn(UWORD8 *, UWORD8 *, WORD32, WORD32, WORD32);

static void run_mode(const char *name, ih264_intra_pred_chroma_ft *real_fn,
                     mode_fn *asm_fn, uint32_t seed, WORD32 ngbr_avail)
{
    UWORD8 s[SBUF];
    UWORD8 out_real[OUTBUF], out_asm[OUTBUF];
    uint32_t state = seed;
    int i;

    for (i = 0; i < SBUF; i++)
        s[i] = (UWORD8)(xrand(&state) & 0xff);
    for (i = 0; i < OUTBUF; i++)
        out_real[i] = out_asm[i] = (UWORD8)(0xAA + i);

    real_fn(s, out_real, STRD, STRD, ngbr_avail);
    asm_fn(s, out_asm, STRD, STRD, ngbr_avail);

    for (i = 0; i < OUTBUF; i++) {
        if (out_real[i] != out_asm[i])
            report(name, seed, i, out_real[i], out_asm[i]);
    }
}

static void check_intra_chroma(void)
{
    uint32_t seed = 424242;
    int iter;

    for (iter = 0; iter < 20000; iter++) {
        /* vert/horz/plane ignore ngbr_avail (UNUSED() in the vendored C),
         * but dc's quadrant logic depends heavily on it - randomise the
         * low 5 bits (left/top_left/top/top_right plus left_avail2 at
         * bit 4) to exercise every quadrant-availability combination.
         * Derived from a copy of seed, not seed itself - xrand() mutates
         * its argument in place and seed must stay the actual per-mode
         * PRNG seed reported on failure. */
        uint32_t ngbr_state = seed;
        WORD32 ngbr_avail = (WORD32)(xrand(&ngbr_state) & 0x1f);

        run_mode("vert", ih264_intra_pred_chroma_8x8_mode_vert,
                 mr_ih264_intra_pred_chroma_8x8_vert_m68k, seed, ngbr_avail);
        seed = seed * 2654435761u + 1;
        run_mode("horz", ih264_intra_pred_chroma_8x8_mode_horz,
                 mr_ih264_intra_pred_chroma_8x8_horz_m68k, seed, ngbr_avail);
        seed = seed * 2654435761u + 1;
        run_mode("dc", ih264_intra_pred_chroma_8x8_mode_dc,
                 mr_ih264_intra_pred_chroma_8x8_dc_m68k, seed, ngbr_avail);
        seed = seed * 2654435761u + 1;
        run_mode("plane", ih264_intra_pred_chroma_8x8_mode_plane,
                 mr_ih264_intra_pred_chroma_8x8_plane_m68k, seed, ngbr_avail);
        seed = seed * 2654435761u + 1;
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
#if defined(MR_M68K_ASM)
    check_intra_chroma();
    if (g_failures) {
        fprintf(stderr,
                "mr_h264_intra_chroma_check: FAILED (%d mismatches)\n",
                g_failures);
        return 1;
    }
    printf("mr_h264_intra_chroma_check: OK\n");
#else
    printf("mr_h264_intra_chroma_check: SKIPPED (not MR_M68K_ASM)\n");
#endif
    return 0;
}
