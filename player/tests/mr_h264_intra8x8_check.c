/*
 * Real-vs-asm differential check for the H.264 luma 8x8 intra prediction
 * modes (vendor/libavc/common/ih264_luma_intra_pred_filters.c) versus this
 * project's m68k primitives (vendor/libavc_port/ih264_m68k_intra_pred.S).
 * Random neighbour-buffer contents are fed identically to the real
 * vendored function and the asm primitive, and every reconstructed output
 * byte is compared.
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
/* S[0..24]: S[0..7]=left reversed, S[8]=topleft, S[9..24]=top+topright -
 * see ih264_m68k_intra_pred.S's own header comment on this layout. STRD
 * wider than 8 so an out-of-bounds column write lands in padding instead
 * of silently matching by accident. */
#define SBUF 25
#define STRD 12
#define OUTBUF (STRD * 8)

typedef void mode_fn(UWORD8 *, UWORD8 *, WORD32, WORD32, WORD32);

static void run_mode(const char *name, ih264_intra_pred_luma_ft *real_fn,
                     mode_fn *asm_fn, uint32_t seed)
{
    UWORD8 s[SBUF];
    UWORD8 out_real[OUTBUF], out_asm[OUTBUF];
    uint32_t state = seed;
    int i;

    for (i = 0; i < SBUF; i++)
        s[i] = (UWORD8)(xrand(&state) & 0xff);
    for (i = 0; i < OUTBUF; i++)
        out_real[i] = out_asm[i] = (UWORD8)(0xAA + i);

    real_fn(s, out_real, STRD, STRD, 0x0f);
    asm_fn(s, out_asm, STRD, STRD, 0x0f);

    for (i = 0; i < OUTBUF; i++) {
        if (out_real[i] != out_asm[i])
            report(name, seed, i, out_real[i], out_asm[i]);
    }
}

/* Reference-sample filtering: pu1_left/pu1_topleft/pu1_top are separate
 * raw (unfiltered) buffers, not one combined S[] buffer - this is the
 * step that *produces* that layout, not one that consumes it. left[]
 * is addressed at runtime-stride offsets (left[idx*strd]) so needs a
 * real strided buffer, not a flat 8-byte array. pu1_topleft may
 * legitimately be NULL, but only when top_left_avail (ngbr_avail bit 1)
 * is false - the vendored C unconditionally dereferences *pu1_topleft
 * whenever that bit is set, in code this test doesn't want to fuzz past
 * (a real decoder would never pass that combination either). */
#define RF_STRD 12
static void run_ref_filtering(uint32_t seed)
{
    UWORD8 left_buf[8 * RF_STRD];
    UWORD8 top_buf[16];
    UWORD8 topleft_val;
    UWORD8 out_real[SBUF], out_asm[SBUF];
    uint32_t state = seed;
    WORD32 ngbr_avail;
    UWORD8 *topleft_real, *topleft_asm;
    int i;

    for (i = 0; i < (int)sizeof(left_buf); i++)
        left_buf[i] = (UWORD8)(xrand(&state) & 0xff);
    for (i = 0; i < 16; i++)
        top_buf[i] = (UWORD8)(xrand(&state) & 0xff);
    topleft_val = (UWORD8)(xrand(&state) & 0xff);
    for (i = 0; i < SBUF; i++)
        out_real[i] = out_asm[i] = (UWORD8)(0xAA + i);

    ngbr_avail = (WORD32)(xrand(&state) & 0x0f);
    if (ngbr_avail & 0x02) {
        topleft_real = topleft_asm = &topleft_val;
    } else if (xrand(&state) & 1) {
        topleft_real = topleft_asm = &topleft_val;
    } else {
        topleft_real = topleft_asm = NULL;
    }

    ih264_intra_pred_luma_8x8_mode_ref_filtering(
        left_buf, topleft_real, top_buf, out_real, RF_STRD, ngbr_avail);
    mr_ih264_intra_pred_luma_8x8_ref_filtering_m68k(
        left_buf, topleft_asm, top_buf, out_asm, RF_STRD, ngbr_avail);

    for (i = 0; i < SBUF; i++) {
        if (out_real[i] != out_asm[i])
            report("ref_filtering", seed, i, out_real[i], out_asm[i]);
    }
}

static void check_intra8x8(void)
{
    uint32_t seed = 999;
    int iter;

    for (iter = 0; iter < 20000; iter++) {
        run_mode("diag_dl", ih264_intra_pred_luma_8x8_mode_diag_dl,
                 mr_ih264_intra_pred_luma_8x8_diag_dl_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_mode("diag_dr", ih264_intra_pred_luma_8x8_mode_diag_dr,
                 mr_ih264_intra_pred_luma_8x8_diag_dr_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_mode("vert_r", ih264_intra_pred_luma_8x8_mode_vert_r,
                 mr_ih264_intra_pred_luma_8x8_vert_r_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_mode("horz_d", ih264_intra_pred_luma_8x8_mode_horz_d,
                 mr_ih264_intra_pred_luma_8x8_horz_d_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_mode("vert_l", ih264_intra_pred_luma_8x8_mode_vert_l,
                 mr_ih264_intra_pred_luma_8x8_vert_l_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_mode("horz_u", ih264_intra_pred_luma_8x8_mode_horz_u,
                 mr_ih264_intra_pred_luma_8x8_horz_u_m68k, seed);
        seed = seed * 2654435761u + 1;
        run_ref_filtering(seed);
        seed = seed * 2654435761u + 1;
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
#if defined(MR_M68K_ASM)
    check_intra8x8();
    if (g_failures) {
        fprintf(stderr, "mr_h264_intra8x8_check: FAILED (%d mismatches)\n",
                g_failures);
        return 1;
    }
    printf("mr_h264_intra8x8_check: OK\n");
#else
    printf("mr_h264_intra8x8_check: SKIPPED (not MR_M68K_ASM)\n");
#endif
    return 0;
}
