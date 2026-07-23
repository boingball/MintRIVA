/*
 * Minimal portable configuration for MintRIVA's embedded libmpeg2 build.
 *
 * Architecture-specific acceleration is deliberately disabled: the generic C
 * IDCT and motion compensation paths build on both the host and m68k-amigaos.
 */
#ifndef MINTRIVA_LIBMPEG2_CONFIG_H
#define MINTRIVA_LIBMPEG2_CONFIG_H

#if defined(__GNUC__)
#define HAVE_BUILTIN_EXPECT 1
#define ATTRIBUTE_ALIGNED_MAX 8
#endif

#if defined(__mc68000__) || defined(__mc68020__) || defined(__m68k__) || \
    defined(__M68K__) || defined(__M68000__) || defined(mc68000) || \
    (defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
     __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WORDS_BIGENDIAN 1
#endif

#endif /* MINTRIVA_LIBMPEG2_CONFIG_H */
