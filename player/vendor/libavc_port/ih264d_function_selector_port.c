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

void ih264d_init_function_ptr(dec_struct_t *codec)
{
    ih264d_init_function_ptr_generic(codec);
}

void ih264d_init_arch(dec_struct_t *codec)
{
    codec->e_processor_arch = ARCH_X86_GENERIC;
}
