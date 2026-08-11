/* MintRIVA portability configuration for Rockbox's fixed-point liba52. */
#ifndef MINTRIVA_CONFIG_A52_H
#define MINTRIVA_CONFIG_A52_H

#include <stdint.h>
#include <stdlib.h>

#define LIBA52_FIXED 1

/* Rockbox places hot tables in target-specific sections.  MintRIVA keeps the
 * same source portable and lets the Amiga linker choose ordinary sections. */
#define IBSS_ATTR
#define IDATA_ATTR
#define ICONST_ATTR

#if defined(__m68k__) || defined(__M68K__) || \
    (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define betoh32(x) ((uint32_t)(x))
#elif defined(__GNUC__)
#define betoh32(x) __builtin_bswap32((uint32_t)(x))
#else
#define betoh32(x) ((((uint32_t)(x) & 0x000000ffU) << 24) | \
                    (((uint32_t)(x) & 0x0000ff00U) << 8)  | \
                    (((uint32_t)(x) & 0x00ff0000U) >> 8)  | \
                    (((uint32_t)(x) & 0xff000000U) >> 24))
#endif

#endif
