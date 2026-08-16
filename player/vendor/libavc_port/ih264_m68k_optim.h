#ifndef MR_IH264_M68K_OPTIM_H
#define MR_IH264_M68K_OPTIM_H

#include "ih264_typedefs.h"

void mr_ih264_inter_pred_luma_copy_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                        WORD32, WORD32, UWORD8 *, WORD32);
void mr_ih264_default_weighted_pred_luma_m68k(UWORD8 *, UWORD8 *, UWORD8 *,
                                               WORD32, WORD32, WORD32,
                                               WORD32, WORD32);
void mr_ih264_default_weighted_pred_chroma_m68k(UWORD8 *, UWORD8 *, UWORD8 *,
                                                 WORD32, WORD32, WORD32,
                                                 WORD32, WORD32);
void mr_ih264_intra_pred_luma_16x16_vert_m68k(UWORD8 *, UWORD8 *, WORD32,
                                               WORD32, WORD32);
void mr_ih264_intra_pred_luma_16x16_horz_m68k(UWORD8 *, UWORD8 *, WORD32,
                                               WORD32, WORD32);
void mr_ih264_intra_pred_luma_16x16_dc_m68k(UWORD8 *, UWORD8 *, WORD32,
                                             WORD32, WORD32);
void mr_ih264_intra_pred_luma_4x4_vert_m68k(UWORD8 *, UWORD8 *, WORD32,
                                             WORD32, WORD32);
void mr_ih264_intra_pred_luma_4x4_horz_m68k(UWORD8 *, UWORD8 *, WORD32,
                                             WORD32, WORD32);
void mr_ih264_intra_pred_luma_4x4_dc_m68k(UWORD8 *, UWORD8 *, WORD32,
                                           WORD32, WORD32);

/* Hand-written m68k assembly (ih264_m68k_interp.S) - only assembled/linked
 * when MR_M68K_ASM is set (Makefile.amiga, tests/run_m68k_check.sh), hence
 * only declared here under the same guard so a host build never tries to
 * reference a symbol that does not exist there. */
#if defined(MR_M68K_ASM)
/* Bind the C declarations to the exact undecorated names exported by the
 * GNU assembly.  m68k-amigaos-gcc otherwise applies its target C symbol
 * decoration, leaving the bare .globl names unresolved at link time. */
void mr_ih264_inter_pred_luma_horz_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                        WORD32, WORD32, UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_m68k");
void mr_ih264_inter_pred_luma_vert_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                        WORD32, WORD32, UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_vert_m68k");
void mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k(UWORD8 *, UWORD8 *,
                                        WORD32, WORD32, WORD32, WORD32,
                                        UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k");
void mr_ih264_inter_pred_luma_horz_qpel_m68k(UWORD8 *, UWORD8 *, WORD32,
                                        WORD32, WORD32, WORD32, UWORD8 *,
                                        WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_qpel_m68k");
void mr_ih264_inter_pred_luma_vert_qpel_m68k(UWORD8 *, UWORD8 *, WORD32,
                                        WORD32, WORD32, WORD32, UWORD8 *,
                                        WORD32)
    __asm__("mr_ih264_inter_pred_luma_vert_qpel_m68k");
void mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k(UWORD8 *, UWORD8 *,
                                        WORD32, WORD32, WORD32, WORD32,
                                        UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k");
void mr_ih264_inter_pred_luma_horz_qpel_vert_hpel_m68k(UWORD8 *, UWORD8 *,
                                        WORD32, WORD32, WORD32, WORD32,
                                        UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_qpel_vert_hpel_m68k");
void mr_ih264_inter_pred_luma_horz_hpel_vert_qpel_m68k(UWORD8 *, UWORD8 *,
                                        WORD32, WORD32, WORD32, WORD32,
                                        UWORD8 *, WORD32)
    __asm__("mr_ih264_inter_pred_luma_horz_hpel_vert_qpel_m68k");

/* ih264_m68k_deblk.S - bS==4 and bS<4 luma deblocking edge filters. */
void mr_ih264_deblk_luma_vert_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32)
    __asm__("mr_ih264_deblk_luma_vert_bs4_m68k");
void mr_ih264_deblk_luma_horz_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32)
    __asm__("mr_ih264_deblk_luma_horz_bs4_m68k");
void mr_ih264_deblk_luma_vert_bslt4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                         UWORD32, const UWORD8 *)
    __asm__("mr_ih264_deblk_luma_vert_bslt4_m68k");
void mr_ih264_deblk_luma_horz_bslt4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                         UWORD32, const UWORD8 *)
    __asm__("mr_ih264_deblk_luma_horz_bslt4_m68k");
/* Chroma bs4/bslt4 take separate alpha/beta (and, for bslt4, separate clip
 * tables) per plane - alpha_cb/beta_cb for U, alpha_cr/beta_cr for V -
 * matching ih264_deblk_chroma_edge_bs4_ft/bslt4_ft in
 * ih264_deblk_edge_filters.h exactly (not the single-alpha/beta "_bp"
 * variants also present in that vendored file, which are a different,
 * unused entry point). */
void mr_ih264_deblk_chroma_vert_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                         WORD32, WORD32)
    __asm__("mr_ih264_deblk_chroma_vert_bs4_m68k");
void mr_ih264_deblk_chroma_horz_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                         WORD32, WORD32)
    __asm__("mr_ih264_deblk_chroma_horz_bs4_m68k");
void mr_ih264_deblk_chroma_vert_bslt4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                           WORD32, WORD32, UWORD32,
                                           const UWORD8 *, const UWORD8 *)
    __asm__("mr_ih264_deblk_chroma_vert_bslt4_m68k");
void mr_ih264_deblk_chroma_horz_bslt4_m68k(UWORD8 *, WORD32, WORD32, WORD32,
                                           WORD32, WORD32, UWORD32,
                                           const UWORD8 *, const UWORD8 *)
    __asm__("mr_ih264_deblk_chroma_horz_bslt4_m68k");

/* ih264_m68k_cabac.S - regular-mode CABAC bin decode (spec 9.3.3.2.2),
 * ported from ih264d_decode_bin() in ih264d_cabac.c. Struct layouts are
 * opaque here deliberately (this header must stay includable from a
 * portable-C test harness without pulling in libavc's decoder headers),
 * so the four pointer-ish parameters are passed as void pointer or UWORD8
 * pointer and cast on the caller side, matching the real bin_ctxt_model_t
 * (1 byte),
 * dec_bit_stream_t, and decoding_envirnoment_t layouts exactly. */
UWORD32 mr_ih264d_decode_bin_m68k(UWORD32 u4_ctx_inc, UWORD8 *ps_src_bin_ctxt,
                                  void *ps_bitstrm, void *ps_cab_env)
    __asm__("mr_ih264d_decode_bin_m68k");

/* ih264_m68k_chroma_mc.S - chroma sample interpolation (spec 8.4.2.2.2). */
void mr_ih264_inter_pred_chroma_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                     WORD32, WORD32, WORD32, WORD32)
    __asm__("mr_ih264_inter_pred_chroma_m68k");

/* ih264_m68k_mvpred.S - motion vector prediction (spec 8.4.1.2.1), ported
 * from ih264d_get_motion_vector_predictor(). Struct layouts are opaque
 * here deliberately (see ih264_m68k_cabac.S's declaration above for why),
 * so ps_result/ps_mv_pred are passed as UWORD8 pointers and cast on the
 * caller side, matching the real mv_pred_t layout exactly. Called from
 * ih264d_mvpred_dispatch_port.c, not swapped in under the vendored
 * primitive's own name - see that file's header comment for why. */
void mr_ih264d_get_motion_vector_predictor_m68k(UWORD8 *ps_result,
                                                 UWORD8 **ps_mv_pred,
                                                 UWORD32 u1_ref_idx,
                                                 UWORD32 u1_B,
                                                 const UWORD8 *pu1_mv_pred_condition)
    __asm__("mr_ih264d_get_motion_vector_predictor_m68k");

/* ih264_m68k_cabac_coeff.S - CABAC residual coefficient parsing, ported
 * from ih264d_read_coeff4x4_cabac(). ps_bitstrm/ps_dec are opaque void*
 * here (real struct layouts, not opaque single bytes like bin_ctxt_
 * model_t - the asm indexes into them by fixed, verified offsets, see
 * that file's header comment) so this declaration stays includable
 * without pulling in libavc's decoder headers. */
UWORD8 mr_ih264d_read_coeff4x4_cabac_m68k(void *ps_bitstrm, UWORD32 u4_ctxcat,
                                          UWORD8 *ps_ctxt_sig_coeff,
                                          void *ps_dec,
                                          UWORD8 *ps_ctxt_coded)
    __asm__("mr_ih264d_read_coeff4x4_cabac_m68k");

/* ih264_m68k_iquant_itrans_recon.S - 4x4 inverse quant/transform/recon
 * (ih264_iquant_itrans_recon_4x4/_dc and _chroma_4x4/_dc). Plain function-
 * pointer fields, overridden directly in ih264d_function_selector_port.c -
 * no --wrap needed, see that file's header comment. */
void mr_ih264_iquant_itrans_recon_4x4_m68k(WORD16 *pi2_src, UWORD8 *pu1_pred,
                                           UWORD8 *pu1_out, WORD32 pred_strd,
                                           WORD32 out_strd,
                                           const UWORD16 *pu2_iscal_mat,
                                           const UWORD16 *pu2_weigh_mat,
                                           UWORD32 u4_qp_div_6,
                                           WORD16 *pi2_tmp,
                                           WORD32 iq_start_idx,
                                           WORD16 *pi2_dc_ld_addr)
    __asm__("mr_ih264_iquant_itrans_recon_4x4_m68k");
void mr_ih264_iquant_itrans_recon_4x4_dc_m68k(WORD16 *pi2_src,
                                              UWORD8 *pu1_pred,
                                              UWORD8 *pu1_out,
                                              WORD32 pred_strd,
                                              WORD32 out_strd,
                                              const UWORD16 *pu2_iscal_mat,
                                              const UWORD16 *pu2_weigh_mat,
                                              UWORD32 u4_qp_div_6,
                                              WORD16 *pi2_tmp,
                                              WORD32 iq_start_idx,
                                              WORD16 *pi2_dc_ld_addr)
    __asm__("mr_ih264_iquant_itrans_recon_4x4_dc_m68k");
void mr_ih264_iquant_itrans_recon_chroma_4x4_m68k(WORD16 *pi2_src,
                                                  UWORD8 *pu1_pred,
                                                  UWORD8 *pu1_out,
                                                  WORD32 pred_strd,
                                                  WORD32 out_strd,
                                                  const UWORD16 *pu2_iscal_mat,
                                                  const UWORD16 *pu2_weigh_mat,
                                                  UWORD32 u4_qp_div_6,
                                                  WORD16 *pi2_tmp,
                                                  WORD16 *pi2_dc_src)
    __asm__("mr_ih264_iquant_itrans_recon_chroma_4x4_m68k");
void mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k(WORD16 *pi2_src,
                                                     UWORD8 *pu1_pred,
                                                     UWORD8 *pu1_out,
                                                     WORD32 pred_strd,
                                                     WORD32 out_strd,
                                                     const UWORD16 *pu2_iscal_mat,
                                                     const UWORD16 *pu2_weigh_mat,
                                                     UWORD32 u4_qp_div_6,
                                                     WORD16 *pi2_tmp,
                                                     WORD16 *pi2_dc_src)
    __asm__("mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k");
void mr_ih264_iquant_itrans_recon_8x8_m68k(WORD16 *pi2_src, UWORD8 *pu1_pred,
                                           UWORD8 *pu1_out, WORD32 pred_strd,
                                           WORD32 out_strd,
                                           const UWORD16 *pu2_iscal_mat,
                                           const UWORD16 *pu2_weigh_mat,
                                           UWORD32 u4_qp_div_6,
                                           WORD16 *pi2_tmp,
                                           WORD32 iq_start_idx,
                                           WORD16 *pi2_dc_ld_addr)
    __asm__("mr_ih264_iquant_itrans_recon_8x8_m68k");
void mr_ih264_iquant_itrans_recon_8x8_dc_m68k(WORD16 *pi2_src,
                                              UWORD8 *pu1_pred,
                                              UWORD8 *pu1_out,
                                              WORD32 pred_strd,
                                              WORD32 out_strd,
                                              const UWORD16 *pu2_iscal_mat,
                                              const UWORD16 *pu2_weigh_mat,
                                              UWORD32 u4_qp_div_6,
                                              WORD16 *pi2_tmp,
                                              WORD32 iq_start_idx,
                                              WORD16 *pi2_dc_ld_addr)
    __asm__("mr_ih264_iquant_itrans_recon_8x8_dc_m68k");

/* ih264_m68k_intra_pred.S - H.264 8.3.3.4 luma 16x16 plane intra
 * prediction (ih264_intra_pred_luma_16x16_mode_plane). Genuine hand asm
 * (unlike the vert/horz/dc predictors above, which are ordinary C in this
 * file) because plane's per-pixel body is a serial add+shift+clip formula
 * with no 4-bytes-at-once opportunity - the same reasoning that put the
 * interpolation filters and iquant/itrans/recon in real .S files instead
 * of here. */
void mr_ih264_intra_pred_luma_16x16_plane_m68k(UWORD8 *pu1_src,
                                               UWORD8 *pu1_dst,
                                               WORD32 src_strd,
                                               WORD32 dst_strd,
                                               WORD32 ngbr_avail)
    __asm__("mr_ih264_intra_pred_luma_16x16_plane_m68k");
#endif

#endif
