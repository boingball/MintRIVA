/*
 * Portable libavc function selector for architectures without a specialised
 * backend.  In particular this keeps the m68k build on libavc's integer C
 * implementations instead of pulling in x86/ARM assembly.
 */
#include "ih264_typedefs.h"
#include "iv.h"
#include "ivd.h"
#include "ih264d_structs.h"
#include "ih264d_function_selector.h"
#include "ih264_m68k_optim.h"

void ih264d_init_function_ptr(dec_struct_t *codec)
{
    ih264d_init_function_ptr_generic(codec);
    /* MR_M68K_ASM is an explicit build flag (set by Makefile.amiga and
     * tests/run_m68k_check.sh), not GCC's own __mc68000__ predefine: a real
     * m68k-amigaos-gcc build hit an undefined-reference link error against
     * ih264_m68k_interp.S's functions even though this file's __mc68000__
     * guard clearly *did* activate (the call sites below were compiled in) -
     * meaning that predefine did not reach the .S file identically on that
     * toolchain, for reasons not reproducible on the m68k-linux-gnu test
     * toolchain here. An explicit, build-system-controlled flag removes the
     * dependency on that predefine matching across every GCC fork/version
     * this project might be built with, on both a real AmigaOS target and
     * the m68k-linux-gnu test build `make check-m68k` uses to exercise this
     * through the real decode pipeline under qemu-m68k instead of only in
     * unit-test isolation. Deliberately narrower than AMIGA_M68K: it says
     * nothing about dos.h/exec.h being available, unlike mr_h264.c's
     * diagnostic hooks. */
#if defined(MR_M68K_ASM)
    /* Replace only bit-exact leaf primitives.  Keeping selection here, rather
     * than modifying the imported libavc tree, makes the port auditable and
     * leaves every non-m68k build on Ittiam's reference C implementation. */
    codec->apf_inter_pred_luma[0] = mr_ih264_inter_pred_luma_copy_m68k;
    codec->apf_inter_pred_luma[2] = mr_ih264_inter_pred_luma_horz_m68k;
    codec->apf_inter_pred_luma[8] = mr_ih264_inter_pred_luma_vert_m68k;
    codec->apf_inter_pred_luma[5] =
        mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k;
    codec->apf_inter_pred_luma[7] =
        mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k;
    codec->apf_inter_pred_luma[13] =
        mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k;
    codec->apf_inter_pred_luma[15] =
        mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k;
    codec->pf_default_weighted_pred_luma =
        mr_ih264_default_weighted_pred_luma_m68k;
    codec->pf_default_weighted_pred_chroma =
        mr_ih264_default_weighted_pred_chroma_m68k;
    codec->apf_intra_pred_luma_16x16[0] =
        mr_ih264_intra_pred_luma_16x16_vert_m68k;
    codec->apf_intra_pred_luma_16x16[1] =
        mr_ih264_intra_pred_luma_16x16_horz_m68k;
#endif
}

void ih264d_init_arch(dec_struct_t *codec)
{
    codec->e_processor_arch = ARCH_X86_GENERIC;
}
