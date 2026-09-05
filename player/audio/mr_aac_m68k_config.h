/*
 * MintVID's Amiga build compiles C and preprocessed assembly sources in one
 * GCC driver invocation. A raw command-line -include of MintAMP's C helper
 * would consequently feed its inline C functions to the assembler as well.
 * Keep the forced include assembly-safe and expose the helper only to C
 * translation units.
 */
#ifndef MR_AAC_M68K_CONFIG_H
#define MR_AAC_M68K_CONFIG_H

#ifndef __ASSEMBLER__
#include "aac_m68k_aac_optimized.h"

#define AMIGA_M68K_ASM_AAC_HUFFMAN 1
#define AMIGA_M68K_ASM_AAC_DEQUANT 1
#define AMIGA_M68K_ASM_AAC_STEREO 1
#define AMIGA_M68K_ASM_AAC_IMDCT 1
#endif

#endif
