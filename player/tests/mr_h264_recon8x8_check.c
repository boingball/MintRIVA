/*
 * Real-vs-asm differential check for ih264_iquant_itrans_recon_8x8() /
 * ih264_iquant_itrans_recon_8x8_dc() (vendor/libavc/common/
 * ih264_iquant_itrans_recon.c) versus mr_ih264_iquant_itrans_recon_8x8_m68k()
 * / mr_ih264_iquant_itrans_recon_8x8_dc_m68k() (ih264_m68k_iquant_itrans_
 * recon.S). Random quantised coefficients, scaling matrices, qp_div and
 * prediction buffer are fed identically to both the real vendored function
 * and the asm primitive, and every reconstructed output byte is compared.
 *
 * Only assembled/exercised under MR_M68K_ASM - see tests/run_m68k_check.sh.
 */
#include "ih264_typedefs.h"
#include "ih264_trans_quant_itrans_iquant.h"
#if defined(MR_M68K_ASM)
#include "ih264_m68k_optim.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;

static uint32_t xrand(uint32_t *state)
{
    *state = *state * 1103515245u + 12345u;
    return (*state >> 16) & 0x7fffu;
}

static void report(const char *what, int iter, int idx, long expected, long got)
{
    if (g_failures < 20)
        fprintf(stderr, "FAIL %s: iter=%d idx=%d expected=%ld got=%ld\n",
                what, iter, idx, expected, got);
    g_failures++;
}

#if defined(MR_M68K_ASM)
#define BLK 8
#define NPIX (BLK * BLK)
/* Output buffer is laid out with a stride wider than BLK so an
 * out-of-bounds column write (a real bug this test would otherwise miss)
 * shows up as a mismatch against the pristine copy instead of silently
 * landing in unused padding. */
#define STRD 12
#define OUTBUF (STRD * NPIX)

static void run_recon8x8(int use_dc, uint32_t seed)
{
    WORD16 src[NPIX];
    UWORD16 iscal[NPIX], weigh[NPIX];
    UWORD8 pred[OUTBUF];
    UWORD8 out_real[OUTBUF], out_asm[OUTBUF];
    WORD16 tmp_real[NPIX], tmp_asm[NPIX];
    UWORD32 qp_div;
    uint32_t state = seed;
    int i;

    for (i = 0; i < NPIX; i++) {
        src[i] = (WORD16)((xrand(&state) & 0x3ff) - 512);
        iscal[i] = (UWORD16)(16 + (xrand(&state) & 0x3f));
        weigh[i] = (UWORD16)(16 + (xrand(&state) & 0x3f));
    }
    if (use_dc) {
        /* Only src[0] is read by the _dc variant; leave the rest as noise
         * to prove the primitive genuinely ignores it. */
    }
    for (i = 0; i < OUTBUF; i++)
        pred[i] = (UWORD8)(xrand(&state) & 0xff);
    /* QP_Y is spec-bounded to 0..51, so qp_div = QP_Y/6 never exceeds 8 in
     * a real bitstream; bounded to that range mainly to stay clear of
     * genuine 32-bit-overflow territory in INV_QUANT's left shift, which
     * is undefined in the C standard proper (this project's own -fwrapv
     * build flag defines it, but that's this project's choice to rely on,
     * not something worth stress-testing here). Testing this DID catch a
     * real bug at an out-of-spec qp_div - see .Ldc_clip_store's header
     * comment in ih264_m68k_iquant_itrans_recon.S - now fixed. */
    qp_div = xrand(&state) % 9;

    memcpy(out_real, pred, OUTBUF);
    memcpy(out_asm, pred, OUTBUF);
    memset(tmp_real, 0xAA, sizeof(tmp_real));
    memset(tmp_asm, 0xAA, sizeof(tmp_asm));

    if (use_dc) {
        ih264_iquant_itrans_recon_8x8_dc(src, out_real, out_real, STRD, STRD,
                                         iscal, weigh, qp_div, tmp_real, 0,
                                         NULL);
        mr_ih264_iquant_itrans_recon_8x8_dc_m68k(src, out_asm, out_asm, STRD,
                                                  STRD, iscal, weigh, qp_div,
                                                  tmp_asm, 0, NULL);
    } else {
        ih264_iquant_itrans_recon_8x8(src, out_real, out_real, STRD, STRD,
                                      iscal, weigh, qp_div, tmp_real, 0, NULL);
        mr_ih264_iquant_itrans_recon_8x8_m68k(src, out_asm, out_asm, STRD,
                                              STRD, iscal, weigh, qp_div,
                                              tmp_asm, 0, NULL);
    }

    for (i = 0; i < OUTBUF; i++) {
        if (out_real[i] != out_asm[i])
            report(use_dc ? "recon8x8_dc" : "recon8x8", (int)seed, i,
                   out_real[i], out_asm[i]);
    }
}

static void check_recon8x8(void)
{
    uint32_t seed = 12345;
    int iter;

    for (iter = 0; iter < 20000; iter++) {
        run_recon8x8(0, seed);
        seed = seed * 2654435761u + 1;
        run_recon8x8(1, seed);
        seed = seed * 2654435761u + 1;
    }
}
#endif /* MR_M68K_ASM */

int main(void)
{
#if defined(MR_M68K_ASM)
    check_recon8x8();
    if (g_failures) {
        fprintf(stderr, "mr_h264_recon8x8_check: FAILED (%d mismatches)\n",
                g_failures);
        return 1;
    }
    printf("mr_h264_recon8x8_check: OK\n");
#else
    printf("mr_h264_recon8x8_check: SKIPPED (not MR_M68K_ASM)\n");
#endif
    return 0;
}
