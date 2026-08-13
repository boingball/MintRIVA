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
    /* __mc68000__ is GCC's own predefine for "compiling for the m68k ISA" -
     * true both for AMIGA_M68K (bebbo's m68k-amigaos-gcc, the real target)
     * and for a m68k-linux-gnu test build (no AmigaOS dependency in this
     * file or in ih264_m68k_optim.c/the asm below - just the ISA), which
     * lets `make check-m68k` exercise these through the real decode
     * pipeline under qemu-m68k instead of only in unit-test isolation.
     * Deliberately narrower than AMIGA_M68K: it says nothing about dos.h/
     * exec.h being available, unlike mr_h264.c's diagnostic hooks. */
#if defined(__mc68000__)
    /* Replace only bit-exact leaf primitives.  Keeping selection here, rather
     * than modifying the imported libavc tree, makes the port auditable and
     * leaves every non-m68k build on Ittiam's reference C implementation. */
    codec->apf_inter_pred_luma[0] = mr_ih264_inter_pred_luma_copy_m68k;
    codec->apf_inter_pred_luma[2] = mr_ih264_inter_pred_luma_horz_m68k;
    codec->apf_inter_pred_luma[8] = mr_ih264_inter_pred_luma_vert_m68k;
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
