/* C99/Amiga libc compatibility used while compiling the pinned libavc. */
#ifndef MR_LIBAVC_COMPAT_H
#define MR_LIBAVC_COMPAT_H

#include <stddef.h>

size_t mr_libavc_strnlen(const char *text, size_t maximum);
#define strnlen mr_libavc_strnlen

#endif /* MR_LIBAVC_COMPAT_H */
