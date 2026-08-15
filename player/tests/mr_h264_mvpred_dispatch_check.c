/*
 * Real-vs-asm differential check for ih264d_mvpred_nonmbaff() /
 * ih264d_mvpred_nonmbaffB() - see ih264d_mvpred_dispatch_port.c for the
 * reimplementations this compares against (which call the hand-asm MV
 * predictor primitive, ih264_m68k_mvpred.S), and ih264d_mvpred.c for the
 * real vendored dispatchers.
 *
 * A mechanical source diff already proved the reimplementations are
 * byte-for-byte equivalent to the vendored dispatcher logic (see the
 * PR/commit history), but that only proves the *transcription* is
 * faithful - it does not exercise the code at runtime, and it says
 * nothing about the asm primitive underneath actually being invoked
 * correctly from within that control flow. This test closes that gap
 * directly: it calls the real vendored ih264d_mvpred_nonmbaff()/
 * _nonmbaffB() and this project's __wrap_ih264d_mvpred_nonmbaff()/
 * _nonmbaffB() on identical randomised neighbour-bank state and asserts
 * bit-exact agreement on every output.
 *
 * Only assembled/exercised under MR_M68K_ASM (the __wrap_ functions and
 * the asm primitive they call do not exist otherwise) - see
 * tests/run_m68k_check.sh.
 */
#include "ih264_typedefs.h"
#include "ih264d_structs.h"
#include "ih264d_defs.h"
#include "ih264d_mb_utils.h"
#include "ih264d_mvpred.h"

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

static void report(const char *what, int iter, int idx, long expected, long got)
{
    if (g_failures < 20)
        fprintf(stderr, "FAIL %s: iter=%d idx=%d expected=%ld got=%ld\n",
                what, iter, idx, expected, got);
    g_failures++;
}

#if defined(MR_M68K_ASM)
/* Not declared in any vendored header - internal --wrap targets, see
 * ih264d_mvpred_dispatch_port.c. */
UWORD8 __wrap_ih264d_mvpred_nonmbaff(dec_struct_t *ps_dec,
                                     dec_mb_info_t *ps_cur_mb_info,
                                     mv_pred_t *ps_mv_nmb,
                                     mv_pred_t *ps_mv_ntop,
                                     mv_pred_t *ps_mv_final_pred,
                                     UWORD32 u4_sub_mb_num,
                                     UWORD8 uc_mb_part_width,
                                     UWORD8 u1_lx_start,
                                     UWORD8 u1_lxend,
                                     UWORD8 u1_mb_mc_mode);
UWORD8 __wrap_ih264d_mvpred_nonmbaffB(dec_struct_t *ps_dec,
                                      dec_mb_info_t *ps_cur_mb_info,
                                      mv_pred_t *ps_mv_nmb,
                                      mv_pred_t *ps_mv_ntop,
                                      mv_pred_t *ps_mv_final_pred,
                                      UWORD32 u4_sub_mb_num,
                                      UWORD8 uc_mb_part_width,
                                      UWORD8 u1_lx_start,
                                      UWORD8 u1_lxend,
                                      UWORD8 u1_mb_mc_mode);

enum { BANK_SIZE = 128, NMB_IDX = 64, NTOP_IDX = 48 };

static void init_random_bank(mv_pred_t *bank, uint32_t *seed, int force_zero)
{
    int i, k;
    for (i = 0; i < BANK_SIZE; i++) {
        if (force_zero) {
            for (k = 0; k < 4; k++) bank[i].i2_mv[k] = 0;
            bank[i].i1_ref_frame[0] = 0;
            bank[i].i1_ref_frame[1] = 0;
        } else {
            for (k = 0; k < 4; k++)
                bank[i].i2_mv[k] = (WORD16)((xrand(seed) % 8192) - 4096);
            /* Small in-domain ref_frame range (-2..3) so both the "already
             * available and equal" shortcut and the genuine median path in
             * the underlying primitive get exercised, not just near-certain
             * mismatches a full WORD8 range would produce. */
            bank[i].i1_ref_frame[0] = (WORD8)((int)(xrand(seed) % 6) - 2);
            bank[i].i1_ref_frame[1] = (WORD8)((int)(xrand(seed) % 6) - 2);
        }
        bank[i].u1_col_ref_pic_idx = 0;
        bank[i].u1_pic_type = 0;
    }
}

static void run_one(UWORD8 mc_mode, int is_b_slice, int force_zero_nbrs,
                    uint32_t *seed, int iter)
{
    mv_pred_t bank_ref[BANK_SIZE], bank_asm[BANK_SIZE];
    dec_mb_info_t mb_ref, mb_asm;
    mv_pred_t final_ref, final_asm;
    dec_struct_t *dec_ref, *dec_asm;
    UWORD32 sub_mb_num = xrand(seed) % 16;
    UWORD8 part_width = (UWORD8)(1 + (xrand(seed) % 4));
    UWORD8 lx_start = 0;
    UWORD8 lx_end = is_b_slice ? (UWORD8)(1 + (xrand(seed) % 2)) : 1;
    UWORD8 ret_ref, ret_asm;
    uint32_t seed_snapshot;
    int i;

    seed_snapshot = *seed;
    init_random_bank(bank_ref, seed, force_zero_nbrs);
    *seed = seed_snapshot;
    init_random_bank(bank_asm, seed, force_zero_nbrs);

    memset(&mb_ref, 0, sizeof mb_ref);
    mb_ref.u1_mb_ngbr_availablity = (UWORD8)(xrand(seed) & 0x0f);
    mb_ref.u2_top_right_avail_mask = (UWORD16)(xrand(seed) & 0xffffu);
    mb_ref.u2_top_left_avail_mask = (UWORD16)(xrand(seed) & 0xffffu);
    memcpy(&mb_asm, &mb_ref, sizeof mb_ref);

    memset(&final_ref, 0, sizeof final_ref);
    final_ref.i1_ref_frame[0] = (WORD8)((int)(xrand(seed) % 4) - 1);
    final_ref.i1_ref_frame[1] = (WORD8)((int)(xrand(seed) % 4) - 1);
    memcpy(&final_asm, &final_ref, sizeof final_ref);

    dec_ref = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));
    dec_asm = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));

    ret_ref = ih264d_mvpred_nonmbaff(dec_ref, &mb_ref, &bank_ref[NMB_IDX],
                                     &bank_ref[NTOP_IDX], &final_ref,
                                     sub_mb_num, part_width, lx_start, lx_end,
                                     mc_mode);
    ret_asm = __wrap_ih264d_mvpred_nonmbaff(dec_asm, &mb_asm,
                                            &bank_asm[NMB_IDX],
                                            &bank_asm[NTOP_IDX], &final_asm,
                                            sub_mb_num, part_width, lx_start,
                                            lx_end, mc_mode);

    if (ret_ref != ret_asm)
        report("mvpred_nonmbaff(ret)", iter, mc_mode, ret_ref, ret_asm);
    for (i = 0; i < 4; i++)
        if (final_ref.i2_mv[i] != final_asm.i2_mv[i])
            report("mvpred_nonmbaff(mv)", iter, i, final_ref.i2_mv[i],
                  final_asm.i2_mv[i]);
    for (i = 0; i < 2; i++)
        if (final_ref.i1_ref_frame[i] != final_asm.i1_ref_frame[i])
            report("mvpred_nonmbaff(ref_frame)", iter, i,
                  final_ref.i1_ref_frame[i], final_asm.i1_ref_frame[i]);

    free(dec_ref);
    free(dec_asm);
}

static void run_one_b(UWORD8 mc_mode, int force_zero_nbrs, uint32_t *seed,
                      int iter)
{
    mv_pred_t bank_ref[BANK_SIZE], bank_asm[BANK_SIZE];
    dec_mb_info_t mb_ref, mb_asm;
    mv_pred_t final_ref, final_asm;
    dec_struct_t *dec_ref, *dec_asm;
    UWORD32 sub_mb_num = xrand(seed) % 16;
    UWORD8 part_width = (UWORD8)(1 + (xrand(seed) % 4));
    UWORD8 lx_start = 0;
    UWORD8 lx_end = (UWORD8)(1 + (xrand(seed) % 2));
    UWORD8 ret_ref, ret_asm;
    uint32_t seed_snapshot;
    int i;

    seed_snapshot = *seed;
    init_random_bank(bank_ref, seed, force_zero_nbrs);
    *seed = seed_snapshot;
    init_random_bank(bank_asm, seed, force_zero_nbrs);

    memset(&mb_ref, 0, sizeof mb_ref);
    mb_ref.u1_mb_ngbr_availablity = (UWORD8)(xrand(seed) & 0x0f);
    mb_ref.u2_top_right_avail_mask = (UWORD16)(xrand(seed) & 0xffffu);
    mb_ref.u2_top_left_avail_mask = (UWORD16)(xrand(seed) & 0xffffu);
    memcpy(&mb_asm, &mb_ref, sizeof mb_ref);

    memset(&final_ref, 0, sizeof final_ref);
    final_ref.i1_ref_frame[0] = (WORD8)((int)(xrand(seed) % 4) - 1);
    final_ref.i1_ref_frame[1] = (WORD8)((int)(xrand(seed) % 4) - 1);
    memcpy(&final_asm, &final_ref, sizeof final_ref);

    dec_ref = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));
    dec_asm = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));

    ret_ref = ih264d_mvpred_nonmbaffB(dec_ref, &mb_ref, &bank_ref[NMB_IDX],
                                      &bank_ref[NTOP_IDX], &final_ref,
                                      sub_mb_num, part_width, lx_start, lx_end,
                                      mc_mode);
    ret_asm = __wrap_ih264d_mvpred_nonmbaffB(dec_asm, &mb_asm,
                                             &bank_asm[NMB_IDX],
                                             &bank_asm[NTOP_IDX], &final_asm,
                                             sub_mb_num, part_width, lx_start,
                                             lx_end, mc_mode);

    if (ret_ref != ret_asm)
        report("mvpred_nonmbaffB(ret)", iter, mc_mode, ret_ref, ret_asm);
    for (i = 0; i < 4; i++)
        if (final_ref.i2_mv[i] != final_asm.i2_mv[i])
            report("mvpred_nonmbaffB(mv)", iter, i, final_ref.i2_mv[i],
                  final_asm.i2_mv[i]);
    for (i = 0; i < 2; i++)
        if (final_ref.i1_ref_frame[i] != final_asm.i1_ref_frame[i])
            report("mvpred_nonmbaffB(ref_frame)", iter, i,
                  final_ref.i1_ref_frame[i], final_asm.i1_ref_frame[i]);

    free(dec_ref);
    free(dec_asm);
}

static void check_mvpred_dispatch(void)
{
    static const UWORD8 modes[] = {
        PRED_16x8, PRED_8x16, B_DIRECT_SPATIAL, MB_SKIP, 0 /* default */
    };
    enum { N_ITER = 5000 };
    uint32_t seed = 8181;
    int iter, m;

    for (iter = 0; iter < N_ITER; iter++) {
        for (m = 0; m < 5; m++) {
            /* Every 8th iteration forces all-zero neighbour state so
             * MB_SKIP's "predictor is zero" early-return branch (and the
             * equivalent shortcut elsewhere) gets exercised, not just the
             * near-certain "neighbours differ" case random fill produces. */
            int force_zero = (iter % 8) == 0;
            run_one(modes[m], 0, force_zero, &seed, iter);
            run_one_b(modes[m], force_zero, &seed, iter);
        }
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
#if defined(MR_M68K_ASM)
    check_mvpred_dispatch();
#endif
    if (g_failures) {
        fprintf(stderr, "mr_h264_mvpred_dispatch_check: %d mismatches\n",
                g_failures);
        return 1;
    }
    printf("mr_h264_mvpred_dispatch_check: OK\n");
    return 0;
}
