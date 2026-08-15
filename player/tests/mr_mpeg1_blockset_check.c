/*
 * MintVID - conformance check for the m68k MPEG-1 motion-compensation
 * block-set routines (core/mr_mpeg1_blockset_m68k.S).
 *
 * m68k-only (like mr_h264_m68k_check.c/mr_c2p_check.c's asm-exercising
 * cases): these 8 functions only exist in the .S file, which is only
 * compiled into the m68k build (see tests/run_m68k_check.sh) - there is no
 * portable-C twin to call on the host, so this binary is built and run only
 * under qemu-m68k, not by the host `make check`.
 *
 * Each of the 8 asm functions is checked against an independent reference
 * reimplementation of pl_mpeg's PLM_MB_CASE body (core/pl_mpeg.h) - written
 * fresh here rather than shared with either the asm or pl_mpeg.h's own C
 * fallback, so a bug shared between the asm and its "reference" cannot hide.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void plm_bs_copy_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_copy_m68k");
void plm_bs_avg_v_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_avg_v_m68k");
void plm_bs_avg_h_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_avg_h_m68k");
void plm_bs_avg_hv_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_avg_hv_m68k");
void plm_bs_interp_copy_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_interp_copy_m68k");
void plm_bs_interp_avg_v_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_interp_avg_v_m68k");
void plm_bs_interp_avg_h_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_interp_avg_h_m68k");
void plm_bs_interp_avg_hv_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size)
    __asm__("plm_bs_interp_avg_hv_m68k");

typedef void (*blockset_fn)(uint8_t *, const uint8_t *, int, int);

static unsigned state = 0x6d70316dU;
static unsigned char random_byte(void)
{
    state = state * 1664525U + 1013904223U;
    return (unsigned char)(state >> 24);
}

/* Independent reference: pl_mpeg's PLM_MB_CASE bodies, spelled out directly
 * (not via PLM_BLOCK_SET) for a src/dest pair already offset to [0]. */
static void ref_block(uint8_t *dest, const uint8_t *src, int dw,
                      int block_size, int mode)
{
    int x, y;
    int dest_scan = dw - block_size;
    for (y = 0; y < block_size; y++) {
        for (x = 0; x < block_size; x++) {
            int val;
            switch (mode) {
            case 0: val = src[0]; break;
            case 1: val = (src[0] + src[dw] + 1) >> 1; break;
            case 2: val = (src[0] + src[1] + 1) >> 1; break;
            case 3: val = (src[0] + src[1] + src[dw] + src[dw + 1] + 2) >> 2; break;
            case 4: val = (dest[0] + src[0] + 1) >> 1; break;
            case 5: val = (dest[0] + ((src[0] + src[dw] + 1) >> 1) + 1) >> 1; break;
            case 6: val = (dest[0] + ((src[0] + src[1] + 1) >> 1) + 1) >> 1; break;
            default: val = (dest[0] + ((src[0] + src[1] + src[dw] + src[dw + 1] + 2) >> 2) + 1) >> 1; break;
            }
            dest[0] = (uint8_t)val;
            src++; dest++;
        }
        src += dest_scan;
        dest += dest_scan;
    }
}

static const blockset_fn fns[8] = {
    plm_bs_copy_m68k, plm_bs_avg_v_m68k, plm_bs_avg_h_m68k, plm_bs_avg_hv_m68k,
    plm_bs_interp_copy_m68k, plm_bs_interp_avg_v_m68k, plm_bs_interp_avg_h_m68k,
    plm_bs_interp_avg_hv_m68k
};
static const char *names[8] = {
    "copy", "avg_v", "avg_h", "avg_hv",
    "interp_copy", "interp_avg_v", "interp_avg_h", "interp_avg_hv"
};

static int run_case(int block_size, int dw, int seed)
{
    /* +1 row and +1 column of headroom: the hv/interp_hv modes read
     * src[dw+1] on the block's last row/column. */
    size_t n = (size_t)(block_size + 1) * dw + 1;
    uint8_t *src = (uint8_t *)malloc(n);
    uint8_t *dest_asm = (uint8_t *)malloc(n);
    uint8_t *dest_ref = (uint8_t *)malloc(n);
    int mode, fails = 0;
    size_t i;

    state = 0x6d70316dU ^ (unsigned)seed;
    for (i = 0; i < n; i++) src[i] = random_byte();
    for (i = 0; i < n; i++) dest_asm[i] = dest_ref[i] = random_byte();

    for (mode = 0; mode < 8; mode++) {
        uint8_t *da = (uint8_t *)malloc(n);
        uint8_t *dr = (uint8_t *)malloc(n);
        memcpy(da, dest_asm, n);
        memcpy(dr, dest_ref, n);

        fns[mode](da, src, dw, block_size);
        ref_block(dr, src, dw, block_size, mode);

        if (memcmp(da, dr, n) != 0) {
            int x, y;
            for (y = 0; y < block_size; y++)
                for (x = 0; x < block_size; x++) {
                    size_t idx = (size_t)y * dw + x;
                    if (da[idx] != dr[idx]) {
                        printf("FAIL mode=%s block_size=%d dw=%d seed=%d "
                               "(%d,%d): asm=%u ref=%u\n",
                               names[mode], block_size, dw, seed, x, y,
                               da[idx], dr[idx]);
                        fails++;
                        goto next_mode;
                    }
                }
        }
    next_mode:
        free(da);
        free(dr);
    }

    free(src);
    free(dest_asm);
    free(dest_ref);
    return fails;
}

int main(void)
{
    static const int block_sizes[] = { 8, 16 };
    static const int extra_dw[] = { 0, 1, 7, 32 };
    int bs, e, seed, total_fails = 0;

    for (bs = 0; bs < 2; bs++) {
        int block_size = block_sizes[bs];
        for (e = 0; e < 4; e++) {
            int dw = block_size + extra_dw[e];
            for (seed = 0; seed < 6; seed++)
                total_fails += run_case(block_size, dw, seed * 97 + bs * 13 + e);
        }
    }

    /* Extreme pixel values (0/255 only) stress the rounding constants. */
    {
        int block_size = 16, dw = 176; /* CIF-ish luma row stride */
        size_t n = (size_t)(block_size + 1) * dw + 1;
        uint8_t *src = (uint8_t *)malloc(n);
        uint8_t *dest_asm = (uint8_t *)malloc(n);
        uint8_t *dest_ref = (uint8_t *)malloc(n);
        size_t i;
        int mode;
        for (i = 0; i < n; i++) src[i] = (i & 1) ? 255 : 0;
        for (i = 0; i < n; i++) dest_asm[i] = dest_ref[i] = (i & 2) ? 255 : 0;
        for (mode = 0; mode < 8; mode++) {
            uint8_t *da = (uint8_t *)malloc(n);
            uint8_t *dr = (uint8_t *)malloc(n);
            memcpy(da, dest_asm, n);
            memcpy(dr, dest_ref, n);
            fns[mode](da, src, dw, block_size);
            ref_block(dr, src, dw, block_size, mode);
            if (memcmp(da, dr, n) != 0) {
                printf("FAIL extreme-values mode=%s\n", names[mode]);
                total_fails++;
            }
            free(da);
            free(dr);
        }
        free(src);
        free(dest_asm);
        free(dest_ref);
    }

    if (total_fails) {
        printf("MPEG-1 block-set checks FAILED: %d mismatch(es)\n", total_fails);
        return 1;
    }
    printf("MPEG-1 block-set checks passed\n");
    return 0;
}
