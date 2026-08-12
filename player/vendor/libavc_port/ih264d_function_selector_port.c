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
#if defined(AMIGA_M68K) && !defined(MR_HOST_BUILD)
    /* Replace only bit-exact leaf primitives.  Keeping selection here, rather
     * than modifying the imported libavc tree, makes the port auditable and
     * leaves every non-m68k build on Ittiam's reference C implementation. */
    codec->apf_inter_pred_luma[0] = mr_ih264_inter_pred_luma_copy_m68k;
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
