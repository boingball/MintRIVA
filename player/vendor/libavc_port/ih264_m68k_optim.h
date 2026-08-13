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
#endif

#endif
