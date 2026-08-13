/*
 * Diagnostic timing wrappers - see ih264d_stage_profile.h.  Every wrapper
 * below is a plain, semantics-preserving pass-through: call the captured
 * original function pointer with the exact same arguments, time it with the
 * same clock()/CLOCKS_PER_SEC pattern mr_h264.c already uses for
 * input_us/core_us/output_us, and add the elapsed microseconds to that
 * stage's accumulator. Nothing here changes what gets decoded, only how
 * long each already-existing call takes to measure.
 */
#include "ih264_typedefs.h"
#include "ih264_deblk_edge_filters.h"
#include "ih264_inter_pred_filters.h"
#include "ih264_trans_quant_itrans_iquant.h"
#include "iv.h"
#include "ivd.h"
#include "ih264d_structs.h"
#include "ih264d_stage_profile.h"

#include <time.h>

static unsigned long g_mc_us;
static unsigned long g_deblock_us;
static unsigned long g_recon_us;

static unsigned long stage_elapsed_us(clock_t begin)
{
    return (unsigned long)((clock() - begin) * 1000000UL / CLOCKS_PER_SEC);
}

/* ---- motion compensation: apf_inter_pred_luma[16] ---------------------- */
static ih264_inter_pred_luma_ft *g_mc_orig[16];

#define MR_MC_WRAP(N) \
static void mc_wrap_##N(UWORD8 *pu1_src, UWORD8 *pu1_dst, WORD32 src_strd, \
                        WORD32 dst_strd, WORD32 ht, WORD32 wd, \
                        UWORD8 *pu1_tmp, WORD32 dydx) \
{ \
    clock_t t0 = clock(); \
    g_mc_orig[N](pu1_src, pu1_dst, src_strd, dst_strd, ht, wd, pu1_tmp, dydx); \
    g_mc_us += stage_elapsed_us(t0); \
}
MR_MC_WRAP(0)  MR_MC_WRAP(1)  MR_MC_WRAP(2)  MR_MC_WRAP(3)
MR_MC_WRAP(4)  MR_MC_WRAP(5)  MR_MC_WRAP(6)  MR_MC_WRAP(7)
MR_MC_WRAP(8)  MR_MC_WRAP(9)  MR_MC_WRAP(10) MR_MC_WRAP(11)
MR_MC_WRAP(12) MR_MC_WRAP(13) MR_MC_WRAP(14) MR_MC_WRAP(15)
#undef MR_MC_WRAP

static ih264_inter_pred_luma_ft * const g_mc_wrap[16] = {
    mc_wrap_0,  mc_wrap_1,  mc_wrap_2,  mc_wrap_3,
    mc_wrap_4,  mc_wrap_5,  mc_wrap_6,  mc_wrap_7,
    mc_wrap_8,  mc_wrap_9,  mc_wrap_10, mc_wrap_11,
    mc_wrap_12, mc_wrap_13, mc_wrap_14, mc_wrap_15
};

/* ---- deblocking: 8 non-MBAFF luma/chroma vert/horz edge filters -------- */
#define MR_DEBLK_BS4_WRAP(NAME) \
static ih264_deblk_edge_bs4_ft *g_##NAME##_orig; \
static void NAME##_wrap(UWORD8 *pu1_src, WORD32 src_strd, WORD32 alpha, \
                        WORD32 beta) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pu1_src, src_strd, alpha, beta); \
    g_deblock_us += stage_elapsed_us(t0); \
}
MR_DEBLK_BS4_WRAP(deblk_luma_vert_bs4)
MR_DEBLK_BS4_WRAP(deblk_luma_horz_bs4)
#undef MR_DEBLK_BS4_WRAP

#define MR_DEBLK_BSLT4_WRAP(NAME) \
static ih264_deblk_edge_bslt4_ft *g_##NAME##_orig; \
static void NAME##_wrap(UWORD8 *pu1_src, WORD32 src_strd, WORD32 alpha, \
                        WORD32 beta, UWORD32 u4_bs, \
                        const UWORD8 *pu1_cliptab) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pu1_src, src_strd, alpha, beta, u4_bs, pu1_cliptab); \
    g_deblock_us += stage_elapsed_us(t0); \
}
MR_DEBLK_BSLT4_WRAP(deblk_luma_vert_bslt4)
MR_DEBLK_BSLT4_WRAP(deblk_luma_horz_bslt4)
#undef MR_DEBLK_BSLT4_WRAP

#define MR_DEBLK_CHROMA_BS4_WRAP(NAME) \
static ih264_deblk_chroma_edge_bs4_ft *g_##NAME##_orig; \
static void NAME##_wrap(UWORD8 *pu1_src, WORD32 src_strd, WORD32 alpha_cb, \
                        WORD32 beta_cb, WORD32 alpha_cr, WORD32 beta_cr) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pu1_src, src_strd, alpha_cb, beta_cb, alpha_cr, beta_cr); \
    g_deblock_us += stage_elapsed_us(t0); \
}
MR_DEBLK_CHROMA_BS4_WRAP(deblk_chroma_vert_bs4)
MR_DEBLK_CHROMA_BS4_WRAP(deblk_chroma_horz_bs4)
#undef MR_DEBLK_CHROMA_BS4_WRAP

#define MR_DEBLK_CHROMA_BSLT4_WRAP(NAME) \
static ih264_deblk_chroma_edge_bslt4_ft *g_##NAME##_orig; \
static void NAME##_wrap(UWORD8 *pu1_src, WORD32 src_strd, WORD32 alpha_cb, \
                        WORD32 beta_cb, WORD32 alpha_cr, WORD32 beta_cr, \
                        UWORD32 u4_bs, const UWORD8 *pu1_cliptab_cb, \
                        const UWORD8 *pu1_cliptab_cr) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pu1_src, src_strd, alpha_cb, beta_cb, alpha_cr, beta_cr, \
                    u4_bs, pu1_cliptab_cb, pu1_cliptab_cr); \
    g_deblock_us += stage_elapsed_us(t0); \
}
MR_DEBLK_CHROMA_BSLT4_WRAP(deblk_chroma_vert_bslt4)
MR_DEBLK_CHROMA_BSLT4_WRAP(deblk_chroma_horz_bslt4)
#undef MR_DEBLK_CHROMA_BSLT4_WRAP

/* ---- IDCT/reconstruction: 4 luma + 2 chroma variants -------------------- */
#define MR_RECON_LUMA_WRAP(NAME) \
static ih264_iquant_itrans_recon_ft *g_##NAME##_orig; \
static void NAME##_wrap(WORD16 *pi2_src, UWORD8 *pu1_pred, UWORD8 *pu1_out, \
                        WORD32 pred_strd, WORD32 out_strd, \
                        const UWORD16 *pu2_iscale_mat, \
                        const UWORD16 *pu2_weigh_mat, UWORD32 qp_div, \
                        WORD16 *pi2_tmp, WORD32 iq_start_idx, \
                        WORD16 *pi2_dc_ld_addr) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pi2_src, pu1_pred, pu1_out, pred_strd, out_strd, \
                    pu2_iscale_mat, pu2_weigh_mat, qp_div, pi2_tmp, \
                    iq_start_idx, pi2_dc_ld_addr); \
    g_recon_us += stage_elapsed_us(t0); \
}
MR_RECON_LUMA_WRAP(recon_luma_4x4)
MR_RECON_LUMA_WRAP(recon_luma_4x4_dc)
MR_RECON_LUMA_WRAP(recon_luma_8x8)
MR_RECON_LUMA_WRAP(recon_luma_8x8_dc)
#undef MR_RECON_LUMA_WRAP

#define MR_RECON_CHROMA_WRAP(NAME) \
static ih264_iquant_itrans_recon_chroma_ft *g_##NAME##_orig; \
static void NAME##_wrap(WORD16 *pi2_src, UWORD8 *pu1_pred, UWORD8 *pu1_out, \
                        WORD32 pred_strd, WORD32 out_strd, \
                        const UWORD16 *pu2_iscal_mat, \
                        const UWORD16 *pu2_weigh_mat, UWORD32 u4_qp_div_6, \
                        WORD16 *pi2_tmp, WORD16 *pi2_dc_src) \
{ \
    clock_t t0 = clock(); \
    g_##NAME##_orig(pi2_src, pu1_pred, pu1_out, pred_strd, out_strd, \
                    pu2_iscal_mat, pu2_weigh_mat, u4_qp_div_6, pi2_tmp, \
                    pi2_dc_src); \
    g_recon_us += stage_elapsed_us(t0); \
}
MR_RECON_CHROMA_WRAP(recon_chroma_4x4)
MR_RECON_CHROMA_WRAP(recon_chroma_4x4_dc)
#undef MR_RECON_CHROMA_WRAP

void mr_h264_stage_profile_install(void *codec_v)
{
    dec_struct_t *codec = (dec_struct_t *)codec_v;
    int i;
    for (i = 0; i < 16; i++) {
        g_mc_orig[i] = codec->apf_inter_pred_luma[i];
        codec->apf_inter_pred_luma[i] = g_mc_wrap[i];
    }

    g_deblk_luma_vert_bs4_orig = codec->pf_deblk_luma_vert_bs4;
    codec->pf_deblk_luma_vert_bs4 = deblk_luma_vert_bs4_wrap;
    g_deblk_luma_horz_bs4_orig = codec->pf_deblk_luma_horz_bs4;
    codec->pf_deblk_luma_horz_bs4 = deblk_luma_horz_bs4_wrap;
    g_deblk_luma_vert_bslt4_orig = codec->pf_deblk_luma_vert_bslt4;
    codec->pf_deblk_luma_vert_bslt4 = deblk_luma_vert_bslt4_wrap;
    g_deblk_luma_horz_bslt4_orig = codec->pf_deblk_luma_horz_bslt4;
    codec->pf_deblk_luma_horz_bslt4 = deblk_luma_horz_bslt4_wrap;
    g_deblk_chroma_vert_bs4_orig = codec->pf_deblk_chroma_vert_bs4;
    codec->pf_deblk_chroma_vert_bs4 = deblk_chroma_vert_bs4_wrap;
    g_deblk_chroma_horz_bs4_orig = codec->pf_deblk_chroma_horz_bs4;
    codec->pf_deblk_chroma_horz_bs4 = deblk_chroma_horz_bs4_wrap;
    g_deblk_chroma_vert_bslt4_orig = codec->pf_deblk_chroma_vert_bslt4;
    codec->pf_deblk_chroma_vert_bslt4 = deblk_chroma_vert_bslt4_wrap;
    g_deblk_chroma_horz_bslt4_orig = codec->pf_deblk_chroma_horz_bslt4;
    codec->pf_deblk_chroma_horz_bslt4 = deblk_chroma_horz_bslt4_wrap;

    g_recon_luma_4x4_orig = codec->pf_iquant_itrans_recon_luma_4x4;
    codec->pf_iquant_itrans_recon_luma_4x4 = recon_luma_4x4_wrap;
    g_recon_luma_4x4_dc_orig = codec->pf_iquant_itrans_recon_luma_4x4_dc;
    codec->pf_iquant_itrans_recon_luma_4x4_dc = recon_luma_4x4_dc_wrap;
    g_recon_luma_8x8_orig = codec->pf_iquant_itrans_recon_luma_8x8;
    codec->pf_iquant_itrans_recon_luma_8x8 = recon_luma_8x8_wrap;
    g_recon_luma_8x8_dc_orig = codec->pf_iquant_itrans_recon_luma_8x8_dc;
    codec->pf_iquant_itrans_recon_luma_8x8_dc = recon_luma_8x8_dc_wrap;
    g_recon_chroma_4x4_orig = codec->pf_iquant_itrans_recon_chroma_4x4;
    codec->pf_iquant_itrans_recon_chroma_4x4 = recon_chroma_4x4_wrap;
    g_recon_chroma_4x4_dc_orig = codec->pf_iquant_itrans_recon_chroma_4x4_dc;
    codec->pf_iquant_itrans_recon_chroma_4x4_dc = recon_chroma_4x4_dc_wrap;
}

void mr_h264_stage_profile_reset(void)
{
    g_mc_us = 0;
    g_deblock_us = 0;
    g_recon_us = 0;
}

void mr_h264_stage_profile_get(mr_h264_stage_us *out)
{
    out->mc_us = g_mc_us;
    out->deblock_us = g_deblock_us;
    out->recon_us = g_recon_us;
}
