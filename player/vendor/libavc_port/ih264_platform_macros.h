/*
 * Architecture-neutral libavc platform primitives for MintRIVA.
 *
 * These are deliberately plain integer C/GCC builtins.  The byte-swap is an
 * identity on the big-endian 68k target and a swap on little-endian hosts.
 */
#ifndef _IH264_PLATFORM_MACROS_H_
#define _IH264_PLATFORM_MACROS_H_

#include <stdint.h>

#define CLIP_U8(x)  CLIP3(0, UINT8_MAX, (x))
#define CLIP_S8(x)  CLIP3(INT8_MIN, INT8_MAX, (x))
#define CLIP_U10(x) CLIP3(0, 1023, (x))
#define CLIP_S10(x) CLIP3(-512, 511, (x))
#define CLIP_U11(x) CLIP3(0, 2047, (x))
#define CLIP_S11(x) CLIP3(-1024, 1023, (x))
#define CLIP_U12(x) CLIP3(0, 4095, (x))
#define CLIP_S12(x) CLIP3(-2048, 2047, (x))
#define CLIP_U16(x) CLIP3(0, UINT16_MAX, (x))
#define CLIP_S16(x) CLIP3(INT16_MIN, INT16_MAX, (x))
#define CLIP_U32(x) CLIP3(0, UINT32_MAX, (x))
#define CLIP_S32(x) CLIP3(INT32_MIN, INT32_MAX, (x))

#define SHL(x,y) (((y) < 32) ? ((x) << (y)) : 0)
#define SHR(x,y) (((y) < 32) ? ((x) >> (y)) : 0)
#define SHR_NEG(value,shift) \
    (((shift) > 0) ? ((value) >> (shift)) : ((value) << (-(shift))))
#define SHL_NEG(value,shift) \
    (((shift) < 0) ? ((value) >> (-(shift))) : ((value) << (shift)))

#if defined(__mc68000__) || \
    (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__))
#define ITT_BIG_ENDIAN(x) (x)
#else
#define ITT_BIG_ENDIAN(x) __builtin_bswap32(x)
#endif

#define NOP(count)      ((void)(count))
#define PLD(address)    ((void)(address))
#define PREFETCH(p,t)   ((void)(p))
#define DATA_SYNC()     ((void)0)
#define INLINE inline

static INLINE UWORD32 CLZ(UWORD32 word)
{
    return word ? (UWORD32)__builtin_clz(word) : 31u;
}

static INLINE UWORD32 CTZ(UWORD32 word)
{
    return word ? (UWORD32)__builtin_ctz(word) : 31u;
}

#define MEM_ALIGN8  __attribute__((aligned(8)))
#define MEM_ALIGN16 __attribute__((aligned(16)))
#define MEM_ALIGN32 __attribute__((aligned(32)))

#endif /* _IH264_PLATFORM_MACROS_H_ */
