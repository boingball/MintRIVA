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

/* ih264_m68k_deblk.S - bS==4 luma deblocking edge filters. */
void mr_ih264_deblk_luma_vert_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32)
    __asm__("mr_ih264_deblk_luma_vert_bs4_m68k");
void mr_ih264_deblk_luma_horz_bs4_m68k(UWORD8 *, WORD32, WORD32, WORD32)
    __asm__("mr_ih264_deblk_luma_horz_bs4_m68k");
#endif

#endif
