/*
 * MintVID - portable integer YUV420 to RGB24 conversion.
 *
 * Each U/V sample belongs to two neighbouring luma pixels. Calculating its
 * red/green/blue contribution once per pair removes four multiplications per
 * pixel pair while remaining byte-identical to the original scalar formula.
 *
 * The four remaining multiplications (298*luma, 409*e, -100*d, -208*e) are
 * themselves replaced with 256-entry lookup tables, built once on first use.
 * A 68030 without a fast integer multiplier spends far more cycles on MULU.L
 * than on a table read, and every coefficient here is applied to an 8-bit
 * input, so the table approach is exact - not an approximation - for every
 * legal sample value.
 */
#include "mr_yuv.h"

#if defined(MR_M68K_ASM)
/* core/mr_yuv_m68k.S - hand-written m68k loop over the same formula and
 * tables below. __asm__ binds to the bare .globl name (m68k-amigaos-gcc
 * decorates C symbols but not hand-written asm labels), matching every
 * other hand-asm entry point in this project. */
void mr_yuv420_to_rgb24_m68k(uint8_t *dst, int dst_stride,
                             const uint8_t *y_plane, int y_stride,
                             const uint8_t *u_plane, int u_stride,
                             const uint8_t *v_plane, int v_stride,
                             int width, int height,
                             mr_yuv_service_fn service, void *service_opaque,
                             const int *luma_x298, const int *e_x409,
                             const int *d_xm100, const int *e_xm208,
                             const int *d_x516)
    __asm__("mr_yuv420_to_rgb24_m68k");
#endif

static int g_luma_x298[256];
static int g_e_x409[256];
static int g_d_xm100[256];
static int g_e_xm208[256];
static int g_d_x516[256];
static int g_tables_ready = 0;

static void build_tables(void)
{
    int i;
    for (i = 0; i < 256; i++) {
        int d = i - 128, e = i - 128;
        g_luma_x298[i] = 298 * i;
        g_e_x409[i] = 409 * e;
        g_d_xm100[i] = -100 * d;
        g_e_xm208[i] = -208 * e;
        g_d_x516[i] = 516 * d;
    }
    g_tables_ready = 1;
}

static uint8_t clip8(int value)
{
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (uint8_t)value;
}

static void emit_pixel(uint8_t *dst, int luma,
                       int red_add, int green_add, int blue_add)
{
    int scaled_y;
    if (luma < 0) luma = 0;
    scaled_y = g_luma_x298[luma];
    dst[0] = clip8((scaled_y + red_add) >> 8);
    dst[1] = clip8((scaled_y + green_add) >> 8);
    dst[2] = clip8((scaled_y + blue_add) >> 8);
}

static void emit_pixel_bgr(uint8_t *dst, int luma,
                           int red_add, int green_add, int blue_add)
{
    int scaled_y;
    if (luma < 0) luma = 0;
    scaled_y = g_luma_x298[luma];
    dst[0] = clip8((scaled_y + blue_add) >> 8);
    dst[1] = clip8((scaled_y + green_add) >> 8);
    dst[2] = clip8((scaled_y + red_add) >> 8);
}

void mr_yuv420_to_rgb24(uint8_t *dst, int dst_stride,
                        const uint8_t *y_plane, int y_stride,
                        const uint8_t *u_plane, int u_stride,
                        const uint8_t *v_plane, int v_stride,
                        int width, int height,
                        mr_yuv_service_fn service, void *service_opaque)
{
    int row;
    if (!dst || !y_plane || !u_plane || !v_plane ||
        width <= 0 || height <= 0)
        return;
    if (!g_tables_ready) build_tables();

#if defined(MR_M68K_ASM)
    /* A real-Pistorm run of an earlier version of this dispatch crashed
     * (illegal instruction, error 80000004, plus visible corruption of the
     * mouse pointer and clock gadget - a wild write) the first time it ran
     * with real audio servicing active: mr_yuv420_to_rgb24_m68k was
     * clobbering d0/a0/a1 across the periodic service() callback, since
     * those registers are only safe from *our own caller's* perspective,
     * not across a call this function makes itself - see
     * core/mr_yuv_m68k.S for the fix and tests/mr_yuv_check.c's
     * check_yuv_service_clobber() for the regression test (which
     * reproduces the crash against the unfixed asm before confirming the
     * fix). Fixed and confirmed clean on real Pistorm hardware with audio
     * servicing active (`make -f Makefile.amiga mrplay YUV_ASM=1`, since
     * folded back into the default build here). */
    mr_yuv420_to_rgb24_m68k(dst, dst_stride, y_plane, y_stride, u_plane,
                            u_stride, v_plane, v_stride, width, height,
                            service, service_opaque, g_luma_x298, g_e_x409,
                            g_d_xm100, g_e_xm208, g_d_x516);
    return;
#endif

    for (row = 0; row < height; row++) {
        const uint8_t *src_y = y_plane + row * y_stride;
        const uint8_t *src_u = u_plane + (row >> 1) * u_stride;
        const uint8_t *src_v = v_plane + (row >> 1) * v_stride;
        uint8_t *out = dst + row * dst_stride;
        int x;

        for (x = 0; x + 1 < width; x += 2) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            int red_add = g_e_x409[vv] + 128;
            int green_add = g_d_xm100[uu] + g_e_xm208[vv] + 128;
            int blue_add = g_d_x516[uu] + 128;
            emit_pixel(out + x * 3, (int)src_y[x] - 16,
                       red_add, green_add, blue_add);
            emit_pixel(out + (x + 1) * 3, (int)src_y[x + 1] - 16,
                       red_add, green_add, blue_add);
        }
        if (x < width) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            emit_pixel(out + x * 3, (int)src_y[x] - 16,
                       g_e_x409[vv] + 128,
                       g_d_xm100[uu] + g_e_xm208[vv] + 128,
                       g_d_x516[uu] + 128);
        }
        if (service && (row & 15) == 15) service(service_opaque);
    }
}

void mr_yuv420_to_bgr24(uint8_t *dst, int dst_stride,
                        const uint8_t *y_plane, int y_stride,
                        const uint8_t *u_plane, int u_stride,
                        const uint8_t *v_plane, int v_stride,
                        int width, int height,
                        mr_yuv_service_fn service, void *service_opaque)
{
    int row;
    if (!dst || !y_plane || !u_plane || !v_plane ||
        width <= 0 || height <= 0)
        return;
    if (!g_tables_ready) build_tables();

#if defined(MR_M68K_ASM)
    /*
     * Reuse the bit-exact hand-written RGB kernel without adding a branch to
     * its hot pixel loop.  Swap U/V and permute the coefficient tables:
     *
     *   RGB kernel R slot = 409 * V' = 516 * U  -> B
     *   RGB kernel G slot = -100*U' -208*V'     -> unchanged G
     *   RGB kernel B slot = 516 * U' = 409 * V  -> R
     *
     * with U'=V and V'=U.  The kernel therefore writes B,G,R directly, with
     * exactly the same clipping/service behaviour and no post-conversion
     * channel shuffle.
     */
    mr_yuv420_to_rgb24_m68k(dst, dst_stride, y_plane, y_stride,
                            v_plane, v_stride, u_plane, u_stride,
                            width, height, service, service_opaque,
                            g_luma_x298,
                            g_d_x516,   /* kernel e_x409:  B from original U */
                            g_e_xm208,  /* kernel d_xm100: V green term      */
                            g_d_xm100,  /* kernel e_xm208: U green term      */
                            g_e_x409);  /* kernel d_x516:  R from original V */
    return;
#endif

    for (row = 0; row < height; row++) {
        const uint8_t *src_y = y_plane + row * y_stride;
        const uint8_t *src_u = u_plane + (row >> 1) * u_stride;
        const uint8_t *src_v = v_plane + (row >> 1) * v_stride;
        uint8_t *out = dst + row * dst_stride;
        int x;

        for (x = 0; x + 1 < width; x += 2) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            int red_add = g_e_x409[vv] + 128;
            int green_add = g_d_xm100[uu] + g_e_xm208[vv] + 128;
            int blue_add = g_d_x516[uu] + 128;
            emit_pixel_bgr(out + x * 3, (int)src_y[x] - 16,
                           red_add, green_add, blue_add);
            emit_pixel_bgr(out + (x + 1) * 3, (int)src_y[x + 1] - 16,
                           red_add, green_add, blue_add);
        }
        if (x < width) {
            unsigned uu = src_u[x >> 1], vv = src_v[x >> 1];
            emit_pixel_bgr(out + x * 3, (int)src_y[x] - 16,
                           g_e_x409[vv] + 128,
                           g_d_xm100[uu] + g_e_xm208[vv] + 128,
                           g_d_x516[uu] + 128);
        }
        if (service && (row & 15) == 15) service(service_opaque);
    }
}
