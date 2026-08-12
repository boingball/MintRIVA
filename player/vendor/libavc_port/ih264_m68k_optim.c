/*
 * Small, bit-exact H.264 leaf primitives shaped for the 68020+ memory unit.
 *
 * H.264 prediction works on 4/8/16-byte blocks.  Ittiam's generic routines
 * copy or average one byte per loop iteration.  Classic 68k has no SIMD, but
 * it can move and combine four independent samples in one 32-bit operation.
 * The Amiga builds already require a 68030, where unaligned longword accesses
 * are legal; prediction source positions are not guaranteed to be aligned.
 *
 * These remain ordinary C deliberately: GCC emits MOVE.L/AND.L/EOR.L/SUB.L
 * for the packed operations and can allocate registers for the surrounding
 * ABI better than a hand-written wrapper.  It is the useful mild-assembly
 * layer without baking GCC stack offsets into the decoder.
 */
#include "ih264_m68k_optim.h"

#include <stdint.h>

#define AVG_MASK UINT32_C(0xfefefefe)
#if defined(__GNUC__)
#define MR_FORCE_INLINE static inline __attribute__((always_inline))
#else
#define MR_FORCE_INLINE static inline
#endif

MR_FORCE_INLINE uint32_t load_u32(const UWORD8 *p)
{
    return *(const uint32_t *)(const void *)p;
}

MR_FORCE_INLINE void store_u32(UWORD8 *p, uint32_t value)
{
    *(uint32_t *)(void *)p = value;
}

/* Rounded average of four independent unsigned bytes.  The mask prevents
 * carries crossing byte lanes. */
MR_FORCE_INLINE uint32_t avg_u8x4(uint32_t a, uint32_t b)
{
    return (a | b) - (((a ^ b) & AVG_MASK) >> 1);
}

MR_FORCE_INLINE void copy_block_row(UWORD8 *dst, const UWORD8 *src,
                                    WORD32 width)
{
    switch(width)
    {
        case 16:
            store_u32(dst + 12, load_u32(src + 12));
            /* fall through */
        case 12:
            store_u32(dst + 8, load_u32(src + 8));
            /* fall through */
        case 8:
            store_u32(dst + 4, load_u32(src + 4));
            /* fall through */
        case 4:
            store_u32(dst, load_u32(src));
            break;
        default:
        {
            WORD32 x;
            for(x = 0; x < width; x++) dst[x] = src[x];
            break;
        }
    }
}

static void average_block(UWORD8 *src1, UWORD8 *src2, UWORD8 *dst,
                          WORD32 stride1, WORD32 stride2, WORD32 dst_stride,
                          WORD32 height, WORD32 width)
{
    WORD32 y;
    for(y = 0; y < height; y++)
    {
        WORD32 x = 0;
        for(; x + 4 <= width; x += 4)
            store_u32(dst + x,
                      avg_u8x4(load_u32(src1 + x), load_u32(src2 + x)));
        for(; x < width; x++)
            dst[x] = (UWORD8)(((unsigned)src1[x] + src2[x] + 1U) >> 1);
        src1 += stride1;
        src2 += stride2;
        dst += dst_stride;
    }
}

void mr_ih264_inter_pred_luma_copy_m68k(UWORD8 *src, UWORD8 *dst,
                                        WORD32 src_stride, WORD32 dst_stride,
                                        WORD32 height, WORD32 width,
                                        UWORD8 *temporary, WORD32 dydx)
{
    WORD32 y;
    (void)temporary;
    (void)dydx;
    for(y = 0; y < height; y++)
    {
        copy_block_row(dst, src, width);
        src += src_stride;
        dst += dst_stride;
    }
}

void mr_ih264_default_weighted_pred_luma_m68k(
    UWORD8 *src1, UWORD8 *src2, UWORD8 *dst, WORD32 stride1, WORD32 stride2,
    WORD32 dst_stride, WORD32 height, WORD32 width)
{
    average_block(src1, src2, dst, stride1, stride2, dst_stride,
                  height, width);
}

void mr_ih264_default_weighted_pred_chroma_m68k(
    UWORD8 *src1, UWORD8 *src2, UWORD8 *dst, WORD32 stride1, WORD32 stride2,
    WORD32 dst_stride, WORD32 height, WORD32 width)
{
    average_block(src1, src2, dst, stride1, stride2, dst_stride,
                  height, width << 1);
}

void mr_ih264_intra_pred_luma_16x16_vert_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *top = src + 17;
    uint32_t a = load_u32(top), b = load_u32(top + 4);
    uint32_t c = load_u32(top + 8), d = load_u32(top + 12);
    WORD32 y;
    (void)src_stride;
    (void)neighbour_available;
    for(y = 0; y < 16; y++)
    {
        store_u32(dst, a); store_u32(dst + 4, b);
        store_u32(dst + 8, c); store_u32(dst + 12, d);
        dst += dst_stride;
    }
}

void mr_ih264_intra_pred_luma_16x16_horz_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *left = src + 15;
    WORD32 y;
    (void)src_stride;
    (void)neighbour_available;
    for(y = 0; y < 16; y++)
    {
        uint32_t value = (uint32_t)*left-- * UINT32_C(0x01010101);
        store_u32(dst, value); store_u32(dst + 4, value);
        store_u32(dst + 8, value); store_u32(dst + 12, value);
        dst += dst_stride;
    }
}
