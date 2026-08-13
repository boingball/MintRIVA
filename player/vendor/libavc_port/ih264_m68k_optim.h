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
 * on an m68k target, hence only declared here under the same guard so a
 * host build never tries to reference a symbol that does not exist there. */
#if defined(__mc68000__)
void mr_ih264_inter_pred_luma_horz_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                        WORD32, WORD32, UWORD8 *, WORD32);
void mr_ih264_inter_pred_luma_vert_m68k(UWORD8 *, UWORD8 *, WORD32, WORD32,
                                        WORD32, WORD32, UWORD8 *, WORD32);
#endif

#endif
