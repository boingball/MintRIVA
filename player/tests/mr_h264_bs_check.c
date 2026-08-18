/* Differential check for Ittiam's ih264d_fill_bs1_16x16mb_pslice() and the
 * m68k implementation installed in dec_struct_t::pf_fill_bs1[0][0]. */
#include "ih264_typedefs.h"
#include "ih264d_structs.h"
#include "ih264d_deblocking.h"
#if defined(MR_M68K_ASM)
#include "ih264_m68k_bs.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t xrand(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

#if defined(MR_M68K_ASM)
static int check_bs1_16x16_pslice(void)
{
    void *map_store[64];
    void **map = map_store + 8; /* covers the valid -1 reference index */
    uint32_t seed = 0x68060360u;
    int iter;

    for(iter = 0; iter < 100000; iter++)
    {
        mv_pred_t cur[16], top[4], left[16];
        neighbouradd_t left_addr;
        void *pic_addr[4];
        UWORD32 bs_c[8], bs_asm[8];
        WORD32 ver_mvlimit;
        int i;

        for(i = 0; i < 64; i++)
            map_store[i] = (void *)(uintptr_t)((xrand(&seed) & 31u) << 4);
        for(i = 0; i < 16; i++)
        {
            cur[i].i2_mv[0] = (WORD16)xrand(&seed);
            cur[i].i2_mv[1] = (WORD16)xrand(&seed);
            cur[i].i2_mv[2] = (WORD16)xrand(&seed);
            cur[i].i2_mv[3] = (WORD16)xrand(&seed);
            cur[i].i1_ref_frame[0] = (WORD8)((int)(xrand(&seed) % 18) - 1);
            cur[i].i1_ref_frame[1] = -1;
            left[i] = cur[i];
        }
        for(i = 0; i < 4; i++)
        {
            top[i] = cur[xrand(&seed) & 15u];
            if(xrand(&seed) & 1u)
            {
                top[i].i2_mv[0] = (WORD16)(cur[0].i2_mv[0] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                top[i].i2_mv[1] = (WORD16)(cur[0].i2_mv[1] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                left[i * 4].i2_mv[0] = (WORD16)(cur[0].i2_mv[0] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                left[i * 4].i2_mv[1] = (WORD16)(cur[0].i2_mv[1] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
            }
        }
        for(i = 0; i < 4; i += 2)
        {
            if(xrand(&seed) & 1u)
            {
                pic_addr[i] = map[cur[0].i1_ref_frame[0]];
                pic_addr[i + 1] = NULL;
            }
            else
            {
                pic_addr[i] = map[(int)(xrand(&seed) % 18) - 1];
                pic_addr[i + 1] = (xrand(&seed) & 1u) ?
                    map[(int)(xrand(&seed) % 18) - 1] : NULL;
            }
            if(xrand(&seed) & 1u)
            {
                left_addr.u4_add[i] = map[cur[0].i1_ref_frame[0]];
                left_addr.u4_add[i + 1] = NULL;
            }
            else
            {
                left_addr.u4_add[i] =
                    map[(int)(xrand(&seed) % 18) - 1];
                left_addr.u4_add[i + 1] = (xrand(&seed) & 1u) ?
                    map[(int)(xrand(&seed) % 18) - 1] : NULL;
            }
        }

        /* Exercise both the common output of fill_bs2 (zero or 2 in each
         * nibble) and arbitrary pre-set nonzero nibbles. */
        for(i = 0; i < 8; i++)
        {
            uint32_t r = xrand(&seed);
            bs_c[i] = (r & 1u) ? (r & 0x03030303u) : (r & 0x0f0f0f0fu);
        }
        memcpy(bs_asm, bs_c, sizeof(bs_c));
        ver_mvlimit = (xrand(&seed) & 1u) ? 4 : 2;

        ih264d_fill_bs1_16x16mb_pslice(
            cur, top, map, bs_c, left, &left_addr, pic_addr,
            ver_mvlimit);
        mr_ih264d_fill_bs1_16x16mb_pslice_m68k(
            cur, top, map, bs_asm, left, &left_addr, pic_addr,
            ver_mvlimit);

        if(memcmp(bs_c, bs_asm, sizeof(bs_c)) != 0)
        {
            fprintf(stderr,
                    "mr_h264_bs_check: mismatch at iter %d: "
                    "C=%08lx/%08lx asm=%08lx/%08lx\n",
                    iter, (unsigned long)bs_c[0], (unsigned long)bs_c[4],
                    (unsigned long)bs_asm[0], (unsigned long)bs_asm[4]);
            return 1;
        }
    }
    return 0;
}

static int check_bs1_non16_pslice(void)
{
    void *map_store[64];
    void **map = map_store + 8;
    uint32_t seed = 0x4e313678u;
    int iter;

    for(iter = 0; iter < 100000; iter++)
    {
        mv_pred_t cur[16], top[4], left[16];
        neighbouradd_t left_addr;
        void *pic_addr[4];
        UWORD32 bs_c[8], bs_asm[8];
        WORD32 ver_mvlimit;
        int i;

        for(i = 0; i < 64; i++)
            map_store[i] = (void *)(uintptr_t)((xrand(&seed) & 31u) << 4);
        for(i = 0; i < 16; i++)
        {
            cur[i].i2_mv[0] = (WORD16)xrand(&seed);
            cur[i].i2_mv[1] = (WORD16)xrand(&seed);
            cur[i].i2_mv[2] = (WORD16)xrand(&seed);
            cur[i].i2_mv[3] = (WORD16)xrand(&seed);
            cur[i].i1_ref_frame[0] = (WORD8)((int)(xrand(&seed) % 18) - 1);
            cur[i].i1_ref_frame[1] = -1;
            left[i] = cur[i];
        }

        /* Make a substantial fraction of internal edges land around the
         * exact MV thresholds instead of letting random picture/MV data make
         * virtually every edge bS=1 immediately. */
        for(i = 0; i < 16; i++)
        {
            int neighbour = -1;
            if((i & 3) && (xrand(&seed) & 1u)) neighbour = i - 1;
            else if(i >= 4 && (xrand(&seed) & 1u)) neighbour = i - 4;
            if(neighbour >= 0)
            {
                cur[i].i1_ref_frame[0] = cur[neighbour].i1_ref_frame[0];
                cur[i].i2_mv[0] = (WORD16)(cur[neighbour].i2_mv[0] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[1] = (WORD16)(cur[neighbour].i2_mv[1] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
            }
        }
        for(i = 0; i < 4; i++)
        {
            top[i] = cur[i];
            top[i].i2_mv[0] = (WORD16)(cur[i].i2_mv[0] +
                (WORD16)((int)(xrand(&seed) % 11) - 5));
            top[i].i2_mv[1] = (WORD16)(cur[i].i2_mv[1] +
                (WORD16)((int)(xrand(&seed) % 11) - 5));
            left[i * 4].i2_mv[0] = (WORD16)(cur[i * 4].i2_mv[0] +
                (WORD16)((int)(xrand(&seed) % 11) - 5));
            left[i * 4].i2_mv[1] = (WORD16)(cur[i * 4].i2_mv[1] +
                (WORD16)((int)(xrand(&seed) % 11) - 5));
        }
        for(i = 0; i < 4; i += 2)
        {
            pic_addr[i] = (xrand(&seed) & 1u) ?
                map[cur[i].i1_ref_frame[0]] :
                map[(int)(xrand(&seed) % 18) - 1];
            pic_addr[i + 1] = (xrand(&seed) & 3u) ? NULL :
                map[(int)(xrand(&seed) % 18) - 1];
            left_addr.u4_add[i] = (xrand(&seed) & 1u) ?
                map[cur[i * 4].i1_ref_frame[0]] :
                map[(int)(xrand(&seed) % 18) - 1];
            left_addr.u4_add[i + 1] = (xrand(&seed) & 3u) ? NULL :
                map[(int)(xrand(&seed) % 18) - 1];
        }
        for(i = 0; i < 8; i++)
        {
            uint32_t r = xrand(&seed);
            bs_c[i] = (r & 1u) ? (r & 0x03030303u) : (r & 0x0f0f0f0fu);
        }
        memcpy(bs_asm, bs_c, sizeof(bs_c));
        ver_mvlimit = (xrand(&seed) & 1u) ? 4 : 2;

        ih264d_fill_bs1_non16x16mb_pslice(
            cur, top, map, bs_c, left, &left_addr, pic_addr, ver_mvlimit);
        mr_ih264d_fill_bs1_non16x16mb_pslice_m68k(
            cur, top, map, bs_asm, left, &left_addr, pic_addr, ver_mvlimit);

        if(memcmp(bs_c, bs_asm, sizeof(bs_c)) != 0)
        {
            fprintf(stderr,
                    "mr_h264_bs_check non16: mismatch at iter %d "
                    "C=%08lx/%08lx/%08lx/%08lx asm=%08lx/%08lx/%08lx/%08lx\n",
                    iter, (unsigned long)bs_c[0], (unsigned long)bs_c[1],
                    (unsigned long)bs_c[4], (unsigned long)bs_c[5],
                    (unsigned long)bs_asm[0], (unsigned long)bs_asm[1],
                    (unsigned long)bs_asm[4], (unsigned long)bs_asm[5]);
            return 1;
        }
    }
    return 0;
}

static void make_b_neighbour(mv_pred_t *dst, const mv_pred_t *cur,
                             uint32_t *seed)
{
    unsigned mode = xrand(seed) % 3u;
    *dst = *cur;
    if(mode == 0)
    {
        dst->i2_mv[0] = (WORD16)(cur->i2_mv[0] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[1] = (WORD16)(cur->i2_mv[1] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[2] = (WORD16)(cur->i2_mv[2] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[3] = (WORD16)(cur->i2_mv[3] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
    }
    else if(mode == 1)
    {
        dst->i2_mv[0] = (WORD16)(cur->i2_mv[2] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[1] = (WORD16)(cur->i2_mv[3] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[2] = (WORD16)(cur->i2_mv[0] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
        dst->i2_mv[3] = (WORD16)(cur->i2_mv[1] +
            (WORD16)((int)(xrand(seed) % 11) - 5));
    }
    else
    {
        int j;
        for(j = 0; j < 4; j++) dst->i2_mv[j] = (WORD16)xrand(seed);
    }
}

static int check_bs1_16x16_bslice(void)
{
    void *map_store[64];
    void **map = map_store + 8;
    uint32_t seed = 0xb1600360u;
    int iter;

    for(iter = 0; iter < 100000; iter++)
    {
        mv_pred_t cur[16], top[4], left[16];
        neighbouradd_t left_addr;
        void *pic_addr[4];
        void *cur_pic0, *cur_pic1;
        UWORD32 bs_c[8], bs_asm[8];
        WORD32 ver_mvlimit;
        int i;

        for(i = 0; i < 64; i++)
            map_store[i] = (void *)(uintptr_t)((xrand(&seed) & 31u) << 4);
        memset(cur, 0, sizeof(cur));
        cur[0].i2_mv[0] = (WORD16)xrand(&seed);
        cur[0].i2_mv[1] = (WORD16)xrand(&seed);
        cur[0].i2_mv[2] = (WORD16)xrand(&seed);
        cur[0].i2_mv[3] = (WORD16)xrand(&seed);
        cur[0].i1_ref_frame[0] = (WORD8)((int)(xrand(&seed) % 18) - 1);
        cur[0].i1_ref_frame[1] = (WORD8)((int)(xrand(&seed) % 18) - 1);
        cur_pic0 = map[cur[0].i1_ref_frame[0]];
        cur_pic1 = map[33 + cur[0].i1_ref_frame[1]];

        for(i = 0; i < 16; i++) left[i] = cur[0];
        for(i = 0; i < 4; i++)
        {
            make_b_neighbour(&top[i], &cur[0], &seed);
            make_b_neighbour(&left[i * 4], &cur[0], &seed);
        }
        for(i = 0; i < 4; i += 2)
        {
            unsigned mode = xrand(&seed) % 3u;
            if(mode == 0)
            {
                pic_addr[i] = cur_pic0;
                pic_addr[i + 1] = cur_pic1;
            }
            else if(mode == 1)
            {
                pic_addr[i] = cur_pic1;
                pic_addr[i + 1] = cur_pic0;
            }
            else
            {
                pic_addr[i] = map[(int)(xrand(&seed) % 18) - 1];
                pic_addr[i + 1] = map[33 +
                    (int)(xrand(&seed) % 18) - 1];
            }
            mode = xrand(&seed) % 3u;
            if(mode == 0)
            {
                left_addr.u4_add[i] = cur_pic0;
                left_addr.u4_add[i + 1] = cur_pic1;
            }
            else if(mode == 1)
            {
                left_addr.u4_add[i] = cur_pic1;
                left_addr.u4_add[i + 1] = cur_pic0;
            }
            else
            {
                left_addr.u4_add[i] = map[(int)(xrand(&seed) % 18) - 1];
                left_addr.u4_add[i + 1] = map[33 +
                    (int)(xrand(&seed) % 18) - 1];
            }
        }
        for(i = 0; i < 8; i++)
        {
            uint32_t r = xrand(&seed);
            bs_c[i] = (r & 1u) ? (r & 0x03030303u) : (r & 0x0f0f0f0fu);
        }
        memcpy(bs_asm, bs_c, sizeof(bs_c));
        ver_mvlimit = (xrand(&seed) & 1u) ? 4 : 2;

        ih264d_fill_bs1_16x16mb_bslice(
            cur, top, map, bs_c, left, &left_addr, pic_addr, ver_mvlimit);
        mr_ih264d_fill_bs1_16x16mb_bslice_m68k(
            cur, top, map, bs_asm, left, &left_addr, pic_addr, ver_mvlimit);

        if(memcmp(bs_c, bs_asm, sizeof(bs_c)) != 0)
        {
            fprintf(stderr,
                    "mr_h264_bs_check B16: mismatch at iter %d: "
                    "C=%08lx/%08lx asm=%08lx/%08lx\n",
                    iter, (unsigned long)bs_c[0], (unsigned long)bs_c[4],
                    (unsigned long)bs_asm[0], (unsigned long)bs_asm[4]);
            return 1;
        }
    }
    return 0;
}

static int check_bs1_non16_bslice(void)
{
    void *map_store[64];
    void **map = map_store + 8;
    uint32_t seed = 0xb16b0360u;
    int iter;

    for(iter = 0; iter < 100000; iter++)
    {
        mv_pred_t cur[16], top[4], left[16];
        neighbouradd_t left_addr;
        void *pic_addr[4];
        UWORD32 bs_c[8], bs_asm[8];
        WORD32 ver_mvlimit;
        int i;

        /* Four picture identities make direct and swapped reference matches
         * frequent enough to exercise the MV paths, while distinct indices
         * still exercise every signed table lookup. */
        for(i = 0; i < 64; i++)
            map_store[i] = (void *)(uintptr_t)((xrand(&seed) & 3u) << 4);
        for(i = 0; i < 16; i++)
        {
            int j;
            for(j = 0; j < 4; j++) cur[i].i2_mv[j] = (WORD16)xrand(&seed);
            cur[i].i1_ref_frame[0] =
                (WORD8)((int)(xrand(&seed) % 18) - 1);
            cur[i].i1_ref_frame[1] =
                (WORD8)((int)(xrand(&seed) % 18) - 1);
            left[i] = cur[i];
        }
        for(i = 1; i < 16; i++)
        {
            int neighbour;
            unsigned mode;
            if((i & 3) && (xrand(&seed) & 1u)) neighbour = i - 1;
            else if(i >= 4) neighbour = i - 4;
            else continue;
            mode = xrand(&seed) & 1u;
            if(mode == 0)
            {
                cur[i].i1_ref_frame[0] = cur[neighbour].i1_ref_frame[0];
                cur[i].i1_ref_frame[1] = cur[neighbour].i1_ref_frame[1];
                cur[i].i2_mv[0] = (WORD16)(cur[neighbour].i2_mv[0] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[1] = (WORD16)(cur[neighbour].i2_mv[1] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[2] = (WORD16)(cur[neighbour].i2_mv[2] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[3] = (WORD16)(cur[neighbour].i2_mv[3] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
            }
            else
            {
                cur[i].i2_mv[0] = (WORD16)(cur[neighbour].i2_mv[2] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[1] = (WORD16)(cur[neighbour].i2_mv[3] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[2] = (WORD16)(cur[neighbour].i2_mv[0] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
                cur[i].i2_mv[3] = (WORD16)(cur[neighbour].i2_mv[1] +
                    (WORD16)((int)(xrand(&seed) % 11) - 5));
            }
        }
        for(i = 0; i < 4; i++)
        {
            make_b_neighbour(&top[i], &cur[i], &seed);
            make_b_neighbour(&left[i * 4], &cur[i * 4], &seed);
        }
        for(i = 0; i < 4; i += 2)
        {
            mv_pred_t *tc = &cur[i];
            mv_pred_t *lc = &cur[i * 4];
            if(xrand(&seed) & 1u)
            {
                pic_addr[i] = map[tc->i1_ref_frame[0]];
                pic_addr[i + 1] = map[33 + tc->i1_ref_frame[1]];
            }
            else
            {
                pic_addr[i] = map[33 + tc->i1_ref_frame[1]];
                pic_addr[i + 1] = map[tc->i1_ref_frame[0]];
            }
            if(xrand(&seed) & 1u)
            {
                left_addr.u4_add[i] = map[lc->i1_ref_frame[0]];
                left_addr.u4_add[i + 1] = map[33 + lc->i1_ref_frame[1]];
            }
            else
            {
                left_addr.u4_add[i] = map[33 + lc->i1_ref_frame[1]];
                left_addr.u4_add[i + 1] = map[lc->i1_ref_frame[0]];
            }
        }
        for(i = 0; i < 8; i++)
        {
            uint32_t r = xrand(&seed);
            bs_c[i] = (r & 1u) ? (r & 0x03030303u) : (r & 0x0f0f0f0fu);
        }
        memcpy(bs_asm, bs_c, sizeof(bs_c));
        ver_mvlimit = (xrand(&seed) & 1u) ? 4 : 2;

        ih264d_fill_bs1_non16x16mb_bslice(
            cur, top, map, bs_c, left, &left_addr, pic_addr, ver_mvlimit);
        mr_ih264d_fill_bs1_non16x16mb_bslice_m68k(
            cur, top, map, bs_asm, left, &left_addr, pic_addr, ver_mvlimit);

        if(memcmp(bs_c, bs_asm, sizeof(bs_c)) != 0)
        {
            fprintf(stderr,
                    "mr_h264_bs_check B-non16: mismatch at iter %d "
                    "C=%08lx/%08lx/%08lx/%08lx asm=%08lx/%08lx/%08lx/%08lx\n",
                    iter, (unsigned long)bs_c[0], (unsigned long)bs_c[1],
                    (unsigned long)bs_c[4], (unsigned long)bs_c[5],
                    (unsigned long)bs_asm[0], (unsigned long)bs_asm[1],
                    (unsigned long)bs_asm[4], (unsigned long)bs_asm[5]);
            return 1;
        }
    }
    return 0;
}
#endif

int main(void)
{
#if defined(MR_M68K_ASM)
    if(check_bs1_16x16_pslice()) return 1;
    if(check_bs1_non16_pslice()) return 1;
    if(check_bs1_16x16_bslice()) return 1;
    if(check_bs1_non16_bslice()) return 1;
    puts("mr_h264_bs_check: OK");
#else
    puts("mr_h264_bs_check: SKIPPED (not MR_M68K_ASM)");
#endif
    return 0;
}
