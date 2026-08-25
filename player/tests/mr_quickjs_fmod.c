/*
 * IEEE-754 double remainder for the static m68k/Linux QuickJS check.
 *
 * Debian's static m68k cross libm does not export fmod(), although the Amiga
 * toolchain and normal host libm do. The conformance executable only needs a
 * self-contained equivalent so it can reach the big-endian bytecode test.
 */
#include <stdint.h>

double mr_quickjs_fmod(double x, double y)
{
    union { double f; uint64_t i; } ux = { x }, uy = { y };
    uint64_t ax = ux.i & UINT64_C(0x7fffffffffffffff);
    uint64_t ay = uy.i & UINT64_C(0x7fffffffffffffff);
    uint64_t remainder;
    int exponent_x = (int)(ax >> 52);
    int exponent_y = (int)(ay >> 52);
    uint64_t sign = ux.i & UINT64_C(0x8000000000000000);

    if (!ay || ay > UINT64_C(0x7ff0000000000000) ||
        ax >= UINT64_C(0x7ff0000000000000))
        return (x * y) / (x * y);
    if (ax <= ay)
        return ax == ay ? 0.0 * x : x;

    if (!exponent_x) {
        uint64_t bit = ax << 12;
        while (!(bit >> 63)) {
            exponent_x--;
            bit <<= 1;
        }
        ax <<= (unsigned)(1 - exponent_x);
    } else {
        ax = (ax & UINT64_C(0x000fffffffffffff)) |
             UINT64_C(0x0010000000000000);
    }
    if (!exponent_y) {
        uint64_t bit = ay << 12;
        while (!(bit >> 63)) {
            exponent_y--;
            bit <<= 1;
        }
        ay <<= (unsigned)(1 - exponent_y);
    } else {
        ay = (ay & UINT64_C(0x000fffffffffffff)) |
             UINT64_C(0x0010000000000000);
    }

    while (exponent_x > exponent_y) {
        remainder = ax - ay;
        if (!(remainder >> 63)) {
            if (!remainder) return 0.0 * x;
            ax = remainder;
        }
        ax <<= 1;
        exponent_x--;
    }
    remainder = ax - ay;
    if (!(remainder >> 63)) {
        if (!remainder) return 0.0 * x;
        ax = remainder;
    }

    while (!(ax >> 52)) {
        ax <<= 1;
        exponent_x--;
    }
    if (exponent_x > 0) {
        ax -= UINT64_C(0x0010000000000000);
        ax |= (uint64_t)exponent_x << 52;
    } else {
        ax >>= (unsigned)(1 - exponent_x);
    }
    ux.i = sign | ax;
    return ux.f;
}
