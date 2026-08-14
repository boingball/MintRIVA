/* C99/Amiga libc compatibility used while compiling the pinned libavc. */
#ifndef MR_LIBAVC_COMPAT_H
#define MR_LIBAVC_COMPAT_H

/* LIBAVC_FLAGS applies -include compat.h uniformly across every LIBAVC_SRC
 * file, including the hand-written .S files (ih264_m68k_interp.S): GCC runs
 * every .S file through the same C preprocessor, so this content is
 * injected whether or not the file is actually C. __ASSEMBLER__ is GCC's
 * own predefine for exactly this case - it lets an assembly file receive
 * the -include for free without the assembler choking on a C declaration
 * that was never meant for it. */
#ifndef __ASSEMBLER__
#include <stddef.h>

size_t mr_libavc_strnlen(const char *text, size_t maximum);
#define strnlen mr_libavc_strnlen
#endif /* __ASSEMBLER__ */

#endif /* MR_LIBAVC_COMPAT_H */
