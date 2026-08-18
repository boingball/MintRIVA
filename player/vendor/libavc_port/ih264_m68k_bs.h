#ifndef MR_IH264_M68K_BS_H
#define MR_IH264_M68K_BS_H

#include "ih264_typedefs.h"
#include "ih264d_structs.h"

#if defined(MR_M68K_ASM)
/* P-slice 16x16 inter-macroblock boundary-strength calculation.  The exact
 * decoder types live in this small, private header rather than the general
 * primitive header, which intentionally remains independent of libavc's
 * decoder structures for its standalone checks. */
void mr_ih264d_fill_bs1_16x16mb_pslice_m68k(
    mv_pred_t *ps_cur_mv_pred,
    mv_pred_t *ps_top_mv_pred,
    void **ppv_map_ref_idx_to_poc,
    UWORD32 *pu4_bs_table,
    mv_pred_t *ps_leftmost_mv_pred,
    neighbouradd_t *ps_left_addr,
    void **u4_pic_addrress,
    WORD32 i4_ver_mvlimit)
    __asm__("mr_ih264d_fill_bs1_16x16mb_pslice_m68k");
void mr_ih264d_fill_bs1_non16x16mb_pslice_m68k(
    mv_pred_t *ps_cur_mv_pred,
    mv_pred_t *ps_top_mv_pred,
    void **ppv_map_ref_idx_to_poc,
    UWORD32 *pu4_bs_table,
    mv_pred_t *ps_leftmost_mv_pred,
    neighbouradd_t *ps_left_addr,
    void **u4_pic_addrress,
    WORD32 i4_ver_mvlimit)
    __asm__("mr_ih264d_fill_bs1_non16x16mb_pslice_m68k");
void mr_ih264d_fill_bs1_16x16mb_bslice_m68k(
    mv_pred_t *ps_cur_mv_pred,
    mv_pred_t *ps_top_mv_pred,
    void **ppv_map_ref_idx_to_poc,
    UWORD32 *pu4_bs_table,
    mv_pred_t *ps_leftmost_mv_pred,
    neighbouradd_t *ps_left_addr,
    void **u4_pic_addrress,
    WORD32 i4_ver_mvlimit)
    __asm__("mr_ih264d_fill_bs1_16x16mb_bslice_m68k");
void mr_ih264d_fill_bs1_non16x16mb_bslice_m68k(
    mv_pred_t *ps_cur_mv_pred,
    mv_pred_t *ps_top_mv_pred,
    void **ppv_map_ref_idx_to_poc,
    UWORD32 *pu4_bs_table,
    mv_pred_t *ps_leftmost_mv_pred,
    neighbouradd_t *ps_left_addr,
    void **u4_pic_addrress,
    WORD32 i4_ver_mvlimit)
    __asm__("mr_ih264d_fill_bs1_non16x16mb_bslice_m68k");
#endif

#endif
