/*
 * Real-vs-asm differential check for ih264d_read_coeff4x4_cabac() - CABAC
 * residual coefficient parsing, the single hottest function in the H.264
 * decoder per stage-profiling (ih264d_stage_profile.c / a host callgrind
 * pass, see ih264_m68k_cabac.S's header for the earlier CABAC work this
 * follows up on).
 *
 * Unlike mr_h264_m68k_check.c's other checks (which compare hand-asm
 * against an independently-derived reference, deliberately isolated from
 * libavc headers/types so that file stays includable from a portable-C-
 * only build), this one links directly against the real vendored
 * vendor/libavc/decoder/ih264d_parse_cabac.c and calls the genuine
 * ih264d_read_coeff4x4_cabac() for comparison. That function's ~500
 * lines of stateful, multi-threshold renormalisation logic (four
 * separately-inlined bin-decode variants, each renormalising at a
 * different point - see mr_ih264_read_coeff4x4_cabac_m68k's own header
 * comment in ih264_m68k_cabac_coeff.S for the full derivation) are
 * complex enough that comparing against a hand-transcribed reference
 * would only prove "my transcription matches my asm", not "my asm
 * matches the real decoder" - this proves the latter directly, with no
 * transcription step in between to hide a shared mistake in.
 *
 * The cabac_table[] contents are random, not real CABAC probability-
 * model values - matching check_decode_bin_cabac()'s own approach in
 * mr_h264_m68k_check.c. This is a pure differential test of the
 * arithmetic/control-flow the two implementations perform on identical
 * inputs, not a validation of H.264 correctness (that is make
 * check-m68k's job, against the real table via the real decode
 * pipeline and an ffmpeg reference).
 *
 * Only assembled/exercised under MR_M68K_ASM (mr_ih264d_read_coeff4x4_
 * cabac_m68k does not exist otherwise) - see tests/run_m68k_check.sh.
 */
#include "ih264_typedefs.h"
#include "ih264d_structs.h"
#include "ih264d_cabac.h"
#include "ih264d_bitstrm.h"
#include "ih264d_parse_cabac.h"
#include "ih264_m68k_optim.h"

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

enum { N_TABLE = 512, N_BUF_WORDS = 4096, N_SIG_CTX = 128, N_LEVEL_CTX = 16 };

typedef struct {
    dec_struct_t *ps_dec;
    dec_bit_stream_t s_bitstrm;
    UWORD32 au4_buf[N_BUF_WORDS];
    UWORD8 au1_sig_ctx[N_SIG_CTX];
    UWORD8 au1_level_ctx[N_LEVEL_CTX];
    UWORD8 u1_coded_ctx;
    tu_sblk4x4_coeff_data_t s_tu4x4;
} run_state_t;

static void setup_run(run_state_t *rs, uint32_t *seed, const UWORD32 *table,
                      UWORD32 u4_ctxcat, UWORD32 start_range,
                      UWORD32 start_val_ofst, UWORD32 start_offset)
{
    int i;

    memset(rs, 0, sizeof *rs);
    /* Poison the coefficient output buffer so the test can catch a write
     * the real function performs unconditionally (u2_sig_coeff_map = 0,
     * even when coded_flag ends up false) that an asm port might skip. */
    memset(&rs->s_tu4x4, 0xAA, sizeof rs->s_tu4x4);
    rs->ps_dec = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));

    for (i = 0; i < N_BUF_WORDS; i++)
        rs->au4_buf[i] = xrand(seed) | (xrand(seed) << 15) | (xrand(seed) << 30);
    for (i = 0; i < N_SIG_CTX; i++)
        rs->au1_sig_ctx[i] = (UWORD8)(xrand(seed) & 0x7f);
    for (i = 0; i < N_LEVEL_CTX; i++)
        rs->au1_level_ctx[i] = (UWORD8)(xrand(seed) & 0x7f);
    rs->u1_coded_ctx = (UWORD8)(xrand(seed) & 0x7f);

    rs->s_bitstrm.u4_ofst = start_offset;
    rs->s_bitstrm.pu4_buffer = rs->au4_buf;

    rs->ps_dec->s_cab_dec_env.u4_code_int_range = start_range;
    rs->ps_dec->s_cab_dec_env.u4_code_int_val_ofst = start_val_ofst;
    rs->ps_dec->s_cab_dec_env.cabac_table = (const void *)table;
    rs->ps_dec->pv_parse_tu_coeff_data = (void *)&rs->s_tu4x4;
    rs->ps_dec->p_coeff_abs_level_minus1_t[u4_ctxcat] =
        (bin_ctxt_model_t *)rs->au1_level_ctx;
}

static void free_run(run_state_t *rs)
{
    free(rs->ps_dec);
}

static void check_read_coeff4x4_cabac(void)
{
    /* uc_last_coeff_idx per ctxcat, transcribed from
     * ih264d_read_coeff4x4_cabac()'s own u4_start computation - exercised
     * here as an input choice, not reimplemented. */
    static const UWORD32 ctxcats[] = {
        LUMA_DC_CTXCAT, LUMA_AC_CTXCAT, LUMA_4X4_CTXCAT, CHROMA_DC_CTXCAT,
        CHROMA_AC_CTXCAT
    };
    enum { N_ITER = 20000 };
    uint32_t seed = 4242;
    int iter;

    for (iter = 0; iter < N_ITER; iter++) {
        UWORD32 table[N_TABLE];
        UWORD32 u4_ctxcat = ctxcats[xrand(&seed) % 5];
        UWORD32 start_range = 256u + (xrand(&seed) % 255u);
        UWORD32 start_val_ofst = xrand(&seed) % (start_range * 2u + 1u);
        UWORD32 start_offset = xrand(&seed) % 64u;
        uint32_t seed_snapshot;
        run_state_t rs_real, rs_asm;
        UWORD8 coded_ctx_real, coded_ctx_asm;
        UWORD8 result_real, result_asm;
        int i;

        for (i = 0; i < N_TABLE; i++)
            table[i] = xrand(&seed) | (xrand(&seed) << 15) | (xrand(&seed) << 30);

        /* Same random auxiliary state (buffer/contexts) for both runs -
         * snapshot the seed and re-seed identically before each setup. */
        seed_snapshot = seed;
        setup_run(&rs_real, &seed, table, u4_ctxcat, start_range,
                 start_val_ofst, start_offset);
        seed = seed_snapshot;
        setup_run(&rs_asm, &seed, table, u4_ctxcat, start_range,
                 start_val_ofst, start_offset);

        coded_ctx_real = rs_real.u1_coded_ctx;
        coded_ctx_asm = rs_asm.u1_coded_ctx;

        result_real = ih264d_read_coeff4x4_cabac(
            &rs_real.s_bitstrm, u4_ctxcat, (bin_ctxt_model_t *)rs_real.au1_sig_ctx,
            rs_real.ps_dec, (bin_ctxt_model_t *)&coded_ctx_real);
        result_asm = mr_ih264d_read_coeff4x4_cabac_m68k(
            &rs_asm.s_bitstrm, u4_ctxcat, rs_asm.au1_sig_ctx,
            rs_asm.ps_dec, &coded_ctx_asm);

        if (result_real != result_asm)
            report("coeff4x4(coded_flag)", iter, (int)u4_ctxcat,
                  result_real, result_asm);
        if (rs_real.ps_dec->s_cab_dec_env.u4_code_int_range !=
            rs_asm.ps_dec->s_cab_dec_env.u4_code_int_range)
            report("coeff4x4(range)", iter, (int)u4_ctxcat,
                  (long)rs_real.ps_dec->s_cab_dec_env.u4_code_int_range,
                  (long)rs_asm.ps_dec->s_cab_dec_env.u4_code_int_range);
        if (rs_real.ps_dec->s_cab_dec_env.u4_code_int_val_ofst !=
            rs_asm.ps_dec->s_cab_dec_env.u4_code_int_val_ofst)
            report("coeff4x4(val_ofst)", iter, (int)u4_ctxcat,
                  (long)rs_real.ps_dec->s_cab_dec_env.u4_code_int_val_ofst,
                  (long)rs_asm.ps_dec->s_cab_dec_env.u4_code_int_val_ofst);
        if (rs_real.s_bitstrm.u4_ofst != rs_asm.s_bitstrm.u4_ofst)
            report("coeff4x4(bitstrm_ofst)", iter, (int)u4_ctxcat,
                  (long)rs_real.s_bitstrm.u4_ofst, (long)rs_asm.s_bitstrm.u4_ofst);
        if (coded_ctx_real != coded_ctx_asm)
            report("coeff4x4(coded_ctx)", iter, (int)u4_ctxcat,
                  coded_ctx_real, coded_ctx_asm);
        for (i = 0; i < N_SIG_CTX; i++)
            if (rs_real.au1_sig_ctx[i] != rs_asm.au1_sig_ctx[i])
                report("coeff4x4(sig_ctx)", iter, i,
                      rs_real.au1_sig_ctx[i], rs_asm.au1_sig_ctx[i]);
        for (i = 0; i < N_LEVEL_CTX; i++)
            if (rs_real.au1_level_ctx[i] != rs_asm.au1_level_ctx[i])
                report("coeff4x4(level_ctx)", iter, i,
                      rs_real.au1_level_ctx[i], rs_asm.au1_level_ctx[i]);
        /* u2_sig_coeff_map is written unconditionally by the real function
         * (even when coded_flag ends up false), so check it regardless. */
        if (rs_real.s_tu4x4.u2_sig_coeff_map != rs_asm.s_tu4x4.u2_sig_coeff_map)
            report("coeff4x4(sig_coeff_map)", iter, (int)u4_ctxcat,
                  rs_real.s_tu4x4.u2_sig_coeff_map,
                  rs_asm.s_tu4x4.u2_sig_coeff_map);
        if (result_real) {
            for (i = 0; i < 16; i++)
                if (rs_real.s_tu4x4.ai2_level[i] != rs_asm.s_tu4x4.ai2_level[i])
                    report("coeff4x4(level)", iter, i,
                          rs_real.s_tu4x4.ai2_level[i],
                          rs_asm.s_tu4x4.ai2_level[i]);
            {
                long off_real = (UWORD8 *)rs_real.ps_dec->pv_parse_tu_coeff_data -
                                (UWORD8 *)&rs_real.s_tu4x4;
                long off_asm = (UWORD8 *)rs_asm.ps_dec->pv_parse_tu_coeff_data -
                               (UWORD8 *)&rs_asm.s_tu4x4;
                if (off_real != off_asm)
                    report("coeff4x4(tu_data_advance)", iter, (int)u4_ctxcat,
                          off_real, off_asm);
            }
        }

        free_run(&rs_real);
        free_run(&rs_asm);
    }
}

/*
 * Same approach, for mr_ih264d_read_coeff8x8_cabac_m68k() (ih264_m68k_
 * cabac_coeff8x8.S) vs the real vendored ih264d_read_coeff8x8_cabac() -
 * see that file's header for why this function needed its own port
 * rather than reusing the 4x4 primitive with a different context array
 * (the significance/last-coefficient context indexing is table-driven,
 * not a flat stride, and that is exactly the kind of thing this
 * differential test is positioned to catch: a plausible-looking but
 * wrong context selection still produces *a* result, just against the
 * wrong CABAC state).
 */
enum { N_SIG8X8_CTX = 64, N_LEVEL8X8_CTX = 16 };

typedef struct {
    dec_struct_t *ps_dec;
    dec_mb_info_t s_cur_mb_info;
    mb_neigbour_params_t s_curmb;
    dec_bit_stream_t s_bitstrm;
    UWORD32 au4_buf[N_BUF_WORDS];
    UWORD8 au1_sig_ctx[N_SIG8X8_CTX];
    UWORD8 au1_level_ctx[N_LEVEL8X8_CTX];
    tu_blk8x8_coeff_data_t s_tu8x8;
} run_state8x8_t;

static void setup_run8x8(run_state8x8_t *rs, uint32_t *seed,
                         const UWORD32 *table, UWORD32 start_range,
                         UWORD32 start_val_ofst, UWORD32 start_offset,
                         UWORD8 u1_mb_fld)
{
    int i;

    memset(rs, 0, sizeof *rs);
    /* Poison the coefficient output buffer - same reasoning as setup_run():
     * catch an unconditional write (the sig-coeff bitmap) an asm port
     * might accidentally skip. */
    memset(&rs->s_tu8x8, 0xAA, sizeof rs->s_tu8x8);
    rs->ps_dec = (dec_struct_t *)calloc(1, sizeof(dec_struct_t));

    for (i = 0; i < N_BUF_WORDS; i++)
        rs->au4_buf[i] = xrand(seed) | (xrand(seed) << 15) | (xrand(seed) << 30);
    for (i = 0; i < N_SIG8X8_CTX; i++)
        rs->au1_sig_ctx[i] = (UWORD8)(xrand(seed) & 0x7f);
    for (i = 0; i < N_LEVEL8X8_CTX; i++)
        rs->au1_level_ctx[i] = (UWORD8)(xrand(seed) & 0x7f);

    rs->s_bitstrm.u4_ofst = start_offset;
    rs->s_bitstrm.pu4_buffer = rs->au4_buf;

    rs->ps_dec->s_cab_dec_env.u4_code_int_range = start_range;
    rs->ps_dec->s_cab_dec_env.u4_code_int_val_ofst = start_val_ofst;
    rs->ps_dec->s_cab_dec_env.cabac_table = (const void *)table;
    rs->ps_dec->pv_parse_tu_coeff_data = (void *)&rs->s_tu8x8;
    rs->ps_dec->p_coeff_abs_level_minus1_t[LUMA_8X8_CTXCAT] =
        (bin_ctxt_model_t *)rs->au1_level_ctx;
    rs->ps_dec->s_high_profile.ps_sigcoeff_8x8_frame =
        (bin_ctxt_model_t *)rs->au1_sig_ctx;
    rs->ps_dec->s_high_profile.ps_sigcoeff_8x8_field =
        (bin_ctxt_model_t *)rs->au1_sig_ctx;

    rs->s_curmb.u1_mb_fld = u1_mb_fld;
    rs->s_cur_mb_info.ps_curmb = &rs->s_curmb;
}

static void free_run8x8(run_state8x8_t *rs)
{
    free(rs->ps_dec);
}

static void check_read_coeff8x8_cabac(void)
{
    enum { N_ITER = 20000 };
    uint32_t seed = 8181;
    int iter;

    for (iter = 0; iter < N_ITER; iter++) {
        UWORD32 table[N_TABLE];
        UWORD32 start_range = 256u + (xrand(&seed) % 255u);
        UWORD32 start_val_ofst = xrand(&seed) % (start_range * 2u + 1u);
        UWORD32 start_offset = xrand(&seed) % 64u;
        UWORD8 u1_mb_fld = (UWORD8)(xrand(&seed) & 1u);
        uint32_t seed_snapshot;
        run_state8x8_t rs_real, rs_asm;
        int i;

        for (i = 0; i < N_TABLE; i++)
            table[i] = xrand(&seed) | (xrand(&seed) << 15) | (xrand(&seed) << 30);

        seed_snapshot = seed;
        setup_run8x8(&rs_real, &seed, table, start_range, start_val_ofst,
                    start_offset, u1_mb_fld);
        seed = seed_snapshot;
        setup_run8x8(&rs_asm, &seed, table, start_range, start_val_ofst,
                    start_offset, u1_mb_fld);

        ih264d_read_coeff8x8_cabac(&rs_real.s_bitstrm, rs_real.ps_dec,
                                   &rs_real.s_cur_mb_info);
        mr_ih264d_read_coeff8x8_cabac_m68k(&rs_asm.s_bitstrm, rs_asm.ps_dec,
                                           &rs_asm.s_cur_mb_info);

        if (rs_real.ps_dec->s_cab_dec_env.u4_code_int_range !=
            rs_asm.ps_dec->s_cab_dec_env.u4_code_int_range)
            report("coeff8x8(range)", iter, (int)u1_mb_fld,
                  (long)rs_real.ps_dec->s_cab_dec_env.u4_code_int_range,
                  (long)rs_asm.ps_dec->s_cab_dec_env.u4_code_int_range);
        if (rs_real.ps_dec->s_cab_dec_env.u4_code_int_val_ofst !=
            rs_asm.ps_dec->s_cab_dec_env.u4_code_int_val_ofst)
            report("coeff8x8(val_ofst)", iter, (int)u1_mb_fld,
                  (long)rs_real.ps_dec->s_cab_dec_env.u4_code_int_val_ofst,
                  (long)rs_asm.ps_dec->s_cab_dec_env.u4_code_int_val_ofst);
        if (rs_real.s_bitstrm.u4_ofst != rs_asm.s_bitstrm.u4_ofst)
            report("coeff8x8(bitstrm_ofst)", iter, (int)u1_mb_fld,
                  (long)rs_real.s_bitstrm.u4_ofst, (long)rs_asm.s_bitstrm.u4_ofst);
        for (i = 0; i < N_SIG8X8_CTX; i++)
            if (rs_real.au1_sig_ctx[i] != rs_asm.au1_sig_ctx[i])
                report("coeff8x8(sig_ctx)", iter, i,
                      rs_real.au1_sig_ctx[i], rs_asm.au1_sig_ctx[i]);
        for (i = 0; i < N_LEVEL8X8_CTX; i++)
            if (rs_real.au1_level_ctx[i] != rs_asm.au1_level_ctx[i])
                report("coeff8x8(level_ctx)", iter, i,
                      rs_real.au1_level_ctx[i], rs_asm.au1_level_ctx[i]);
        /* au4_sig_coeff_map is written unconditionally, same reasoning as
         * the 4x4 test's u2_sig_coeff_map check. */
        if (rs_real.s_tu8x8.au4_sig_coeff_map[0] !=
            rs_asm.s_tu8x8.au4_sig_coeff_map[0])
            report("coeff8x8(sig_coeff_map0)", iter, (int)u1_mb_fld,
                  rs_real.s_tu8x8.au4_sig_coeff_map[0],
                  rs_asm.s_tu8x8.au4_sig_coeff_map[0]);
        if (rs_real.s_tu8x8.au4_sig_coeff_map[1] !=
            rs_asm.s_tu8x8.au4_sig_coeff_map[1])
            report("coeff8x8(sig_coeff_map1)", iter, (int)u1_mb_fld,
                  rs_real.s_tu8x8.au4_sig_coeff_map[1],
                  rs_asm.s_tu8x8.au4_sig_coeff_map[1]);
        for (i = 0; i < 64; i++)
            if (rs_real.s_tu8x8.ai2_level[i] != rs_asm.s_tu8x8.ai2_level[i])
                report("coeff8x8(level)", iter, i,
                      rs_real.s_tu8x8.ai2_level[i], rs_asm.s_tu8x8.ai2_level[i]);
        {
            long off_real = (UWORD8 *)rs_real.ps_dec->pv_parse_tu_coeff_data -
                            (UWORD8 *)&rs_real.s_tu8x8;
            long off_asm = (UWORD8 *)rs_asm.ps_dec->pv_parse_tu_coeff_data -
                           (UWORD8 *)&rs_asm.s_tu8x8;
            if (off_real != off_asm)
                report("coeff8x8(tu_data_advance)", iter, (int)u1_mb_fld,
                      off_real, off_asm);
        }

        free_run8x8(&rs_real);
        free_run8x8(&rs_asm);
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
#if defined(MR_M68K_ASM)
    check_read_coeff4x4_cabac();
    check_read_coeff8x8_cabac();
#endif
    if (g_failures) {
        fprintf(stderr, "mr_h264_cabac_coeff_check: %d mismatches\n", g_failures);
        return 1;
    }
    printf("mr_h264_cabac_coeff_check: OK\n");
    return 0;
}
