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

/* H.264 8.3.3.3 - the value predicted is a single average of the available
 * top/left neighbours (or 128 if neither is available), broadcast over the
 * whole 16x16 block. Ittiam's generic version calls memset() once per row;
 * this instead replicates the byte to a 32-bit pattern once and stores it
 * with store_u32 (same trick as the horz predictor above), which avoids a
 * memset() call per row for a value gcc cannot fold into a compile-time
 * constant memset. neighbour_available bit 0 = left available, bit 2 = top
 * available (LEFT_MB_AVAILABLE_MASK / TOP_MB_AVAILABLE_MASK in
 * ih264_defs.h - not included here, see this file's header comment on why
 * these small integer masks are inlined instead). */
void mr_ih264_intra_pred_luma_16x16_dc_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *top = src + 17;
    const UWORD8 *left = src + 15;
    WORD32 use_left = neighbour_available & 0x01;
    WORD32 use_top = (neighbour_available >> 2) & 0x01;
    WORD32 val = 0;
    WORD32 y;
    uint32_t bval;
    (void)src_stride;

    if(use_left)
    {
        for(y = 0; y < 16; y++) val += left[-y];
        val += 8;
    }
    if(use_top)
    {
        for(y = 0; y < 16; y++) val += top[y];
        val += 8;
    }
    /* Since 8 is added if either left/top pred is there, val still being
     * zero implies both preds are unavailable. */
    val = val ? (val >> (3 + use_left + use_top)) : 128;

    bval = (uint32_t)val * UINT32_C(0x01010101);
    for(y = 0; y < 16; y++)
    {
        store_u32(dst, bval); store_u32(dst + 4, bval);
        store_u32(dst + 8, bval); store_u32(dst + 12, bval);
        dst += dst_stride;
    }
}

/* H.264 8.3.1.2.1/2/3 - luma 4x4 vert/horz/dc, the exact same three shapes
 * as the 16x16 versions above (single-row-of-neighbours broadcast, or a
 * left-column byte broadcast per row), just one store_u32 per row instead
 * of four since the block is only 4 bytes wide. apf_intra_pred_luma_4x4[]
 * call counts on this project's H.264 test clip (qemu exec trace) put
 * these three at 227-773 hits each - higher than any already-ported
 * 16x16 intra mode and comparable to the 4x4 iquant/itrans/recon
 * functions - while the other six 4x4 modes (diagonal/vert_r/horz_d/
 * vert_l/horz_u) sit at 20-56 hits each and would need genuine multi-tap
 * hand asm to port, not this style; not worth it at that call volume. */
void mr_ih264_intra_pred_luma_4x4_vert_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    uint32_t top = load_u32(src + 5);
    (void)src_stride;
    (void)neighbour_available;
    store_u32(dst, top);
    store_u32(dst + dst_stride, top);
    store_u32(dst + 2 * dst_stride, top);
    store_u32(dst + 3 * dst_stride, top);
}

void mr_ih264_intra_pred_luma_4x4_horz_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *left = src + 3;
    (void)src_stride;
    (void)neighbour_available;
    store_u32(dst, (uint32_t)left[0] * UINT32_C(0x01010101));
    store_u32(dst + dst_stride, (uint32_t)left[-1] * UINT32_C(0x01010101));
    store_u32(dst + 2 * dst_stride,
             (uint32_t)left[-2] * UINT32_C(0x01010101));
    store_u32(dst + 3 * dst_stride,
             (uint32_t)left[-3] * UINT32_C(0x01010101));
}

void mr_ih264_intra_pred_luma_4x4_dc_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *top = src + 5;
    const UWORD8 *left = src + 3;
    WORD32 use_left = neighbour_available & 0x01;
    WORD32 use_top = (neighbour_available >> 2) & 0x01;
    WORD32 val = 0;
    uint32_t bval;
    (void)src_stride;

    if(use_left)
        val += left[0] + left[-1] + left[-2] + left[-3] + 2;
    if(use_top)
        val += top[0] + top[1] + top[2] + top[3] + 2;
    /* Since 2 is added if either left/top pred is there, val still being
     * zero implies both preds are unavailable. */
    val = val ? (val >> (1 + use_left + use_top)) : 128;

    bval = (uint32_t)val * UINT32_C(0x01010101);
    store_u32(dst, bval);
    store_u32(dst + dst_stride, bval);
    store_u32(dst + 2 * dst_stride, bval);
    store_u32(dst + 3 * dst_stride, bval);
}

/* H.264 8.3.4 chroma_8x8 vert/horz/dc - Ittiam's chroma functions take one
 * pu1_src laid out with interleaved U/V samples (ih264_chroma_intra_pred_
 * filters.c): pu1_top = pu1_src+18 (2*BLK8x8SIZE+2), pu1_left = pu1_src+14
 * (2*BLK8x8SIZE-2), pu1_topleft implicitly at pu1_src+16 - i.e. exactly
 * this project's usual S[] neighbour-buffer convention (see
 * ih264_m68k_intra_pred.S's own header comment on the luma version of
 * this layout) but with every logical sample doubled to its (U,V) byte
 * pair. Same broadcast shapes as the luma vert/horz/dc predictors above -
 * plane is the only one with a genuine per-pixel multiply+clip and no
 * broadcast opportunity, so that one is real hand asm (see
 * ih264_m68k_intra_pred.S) exactly like the luma 16x16 case. Big-endian
 * m68k stores the high byte of a store_u32() value at the lowest address,
 * so a (U,V,U,V) 4-byte broadcast pattern is built as
 * (U<<24)|(V<<16)|(U<<8)|V. */
void mr_ih264_intra_pred_chroma_8x8_vert_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *top = src + 18;
    uint32_t a = load_u32(top), b = load_u32(top + 4);
    uint32_t c = load_u32(top + 8), d = load_u32(top + 12);
    WORD32 y;
    (void)src_stride;
    (void)neighbour_available;
    for(y = 0; y < 8; y++)
    {
        store_u32(dst, a); store_u32(dst + 4, b);
        store_u32(dst + 8, c); store_u32(dst + 12, d);
        dst += dst_stride;
    }
}

void mr_ih264_intra_pred_chroma_8x8_horz_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *left = src + 14;
    WORD32 y;
    (void)src_stride;
    (void)neighbour_available;
    for(y = 0; y < 8; y++)
    {
        uint32_t u = left[0], v = left[1];
        uint32_t pattern = (u << 24) | (v << 16) | (u << 8) | v;
        store_u32(dst, pattern); store_u32(dst + 4, pattern);
        store_u32(dst + 8, pattern); store_u32(dst + 12, pattern);
        left -= 2;
        dst += dst_stride;
    }
}

/* H.264 8.3.4.1: DC prediction is computed independently for each of the
 * 4x4 U/V quadrants (top-left/top-right/bottom-left/bottom-right of the
 * 8x8 chroma block), each averaging whichever of its 4 top and/or 4 left
 * neighbour samples are actually available (falling back to 128 if
 * neither side is available for that quadrant) - a direct transcription
 * of the vendored C's own quadrant structure, just with pu1_left's
 * pointer-decrement walk turned into fixed negative offsets from `left`
 * (src+14, the same base ih264_intra_pred_chroma_8x8_mode_horz above
 * uses) since every offset here is a compile-time constant. */
void mr_ih264_intra_pred_chroma_8x8_dc_m68k(
    UWORD8 *src, UWORD8 *dst, WORD32 src_stride, WORD32 dst_stride,
    WORD32 neighbour_available)
{
    const UWORD8 *top = src + 18;
    const UWORD8 *left = src + 14;
    WORD32 left_avail1 = neighbour_available & 1;
    WORD32 left_avail2 = (neighbour_available >> 4) & 1;
    WORD32 top_avail = (neighbour_available >> 2) & 1;
    WORD32 val_u_l1 = 0, val_u_l2 = 0, val_u_t1 = 0, val_u_t2 = 0;
    WORD32 val_v_l1 = 0, val_v_l2 = 0, val_v_t1 = 0, val_v_t2 = 0;
    WORD32 val_u1, val_u2, val_v1, val_v2;
    WORD32 y;
    (void)src_stride;

    if(left_avail1)
    {
        val_u_l1 = left[0] + left[-2] + left[-4] + left[-6] + 2;
        val_v_l1 = left[1] + left[-1] + left[-3] + left[-5] + 2;
    }
    if(left_avail2)
    {
        val_u_l2 = left[-8] + left[-10] + left[-12] + left[-14] + 2;
        val_v_l2 = left[-7] + left[-9] + left[-11] + left[-13] + 2;
    }
    if(top_avail)
    {
        val_u_t1 = top[0] + top[2] + top[4] + top[6] + 2;
        val_u_t2 = top[8] + top[10] + top[12] + top[14] + 2;
        val_v_t1 = top[1] + top[3] + top[5] + top[7] + 2;
        val_v_t2 = top[9] + top[11] + top[13] + top[15] + 2;
    }

    if(left_avail1 || left_avail2 || top_avail)
    {
        uint32_t pattern1, pattern2;

        val_u1 = (left_avail1 || top_avail) ?
            (val_u_l1 + val_u_t1) >> (1 + left_avail1 + top_avail) : 128;
        val_v1 = (left_avail1 || top_avail) ?
            (val_v_l1 + val_v_t1) >> (1 + left_avail1 + top_avail) : 128;
        if(top_avail)
        {
            val_u2 = val_u_t2 >> 2;
            val_v2 = val_v_t2 >> 2;
        }
        else if(left_avail1)
        {
            val_u2 = val_u_l1 >> 2;
            val_v2 = val_v_l1 >> 2;
        }
        else
        {
            val_u2 = val_v2 = 128;
        }

        pattern1 = ((uint32_t)val_u1 << 24) | ((uint32_t)val_v1 << 16) |
                   ((uint32_t)val_u1 << 8) | (uint32_t)val_v1;
        pattern2 = ((uint32_t)val_u2 << 24) | ((uint32_t)val_v2 << 16) |
                   ((uint32_t)val_u2 << 8) | (uint32_t)val_v2;
        for(y = 0; y < 4; y++)
        {
            store_u32(dst, pattern1); store_u32(dst + 4, pattern1);
            store_u32(dst + 8, pattern2); store_u32(dst + 12, pattern2);
            dst += dst_stride;
        }

        if(left_avail2)
        {
            val_u1 = val_u_l2 >> 2;
            val_v1 = val_v_l2 >> 2;
        }
        else if(top_avail)
        {
            val_u1 = val_u_t1 >> 2;
            val_v1 = val_v_t1 >> 2;
        }
        else
        {
            val_u1 = val_v1 = 128;
        }
        val_u2 = (left_avail2 || top_avail) ?
            (val_u_l2 + val_u_t2) >> (1 + left_avail2 + top_avail) : 128;
        val_v2 = (left_avail2 || top_avail) ?
            (val_v_l2 + val_v_t2) >> (1 + left_avail2 + top_avail) : 128;

        pattern1 = ((uint32_t)val_u1 << 24) | ((uint32_t)val_v1 << 16) |
                   ((uint32_t)val_u1 << 8) | (uint32_t)val_v1;
        pattern2 = ((uint32_t)val_u2 << 24) | ((uint32_t)val_v2 << 16) |
                   ((uint32_t)val_u2 << 8) | (uint32_t)val_v2;
        for(y = 4; y < 8; y++)
        {
            store_u32(dst, pattern1); store_u32(dst + 4, pattern1);
            store_u32(dst + 8, pattern2); store_u32(dst + 12, pattern2);
            dst += dst_stride;
        }
    }
    else
    {
        uint32_t pattern128 = UINT32_C(0x80808080);
        for(y = 0; y < 8; y++)
        {
            store_u32(dst, pattern128); store_u32(dst + 4, pattern128);
            store_u32(dst + 8, pattern128); store_u32(dst + 12, pattern128);
            dst += dst_stride;
        }
    }
}
