/*
 * m68k linker-level override for ih264d_decode_bin() - see
 * ih264_m68k_cabac.S for the asm implementation and its full derivation/
 * verification notes.
 *
 * ih264d_decode_bin() is called directly by name from ~40+ sites across
 * every CABAC-coded syntax element in the decoder (mb_type, cbp, ref_idx,
 * mvd, intra pred modes, mb_qp_delta, coded_block_flag, residual
 * coefficients, ...) - it has no dec_struct_t function pointer to swap the
 * way apf_inter_pred_luma[]/deblock/recon/intra do, so it cannot be
 * replaced via ih264d_function_selector_port.c the way everything else in
 * this port is. Worse, the vendored ih264d_cabac.c that defines it lives
 * inside a git submodule (vendor/libavc/, pinned to an upstream Ittiam
 * commit) - editing a submodule file directly would not survive a fresh
 * `git submodule update --init` (which .claude/hooks/session-start.sh runs
 * on every session start, and which a clean checkout runs too), so the
 * vendored source has to stay completely untouched.
 *
 * The fix is GNU ld's --wrap: linking with
 * -Wl,--wrap=ih264d_decode_bin redirects *every* reference to that symbol,
 * from any translation unit (including inside the vendored tree itself),
 * to __wrap_ih264d_decode_bin below - no vendored file changes, every call
 * site keeps calling what it thinks is plain "ih264d_decode_bin". This is
 * standard GNU binutils, available on both the m68k-linux-gnu test
 * toolchain this was verified against and (expected, not verified here -
 * no AmigaOS toolchain on this dev host, see CLAUDE.md) Bebbo's
 * m68k-amigaos-gcc, which is also a full GNU binutils/ld port. See
 * tests/run_m68k_check.sh and Makefile.amiga for where the flag is added.
 *
 * This file is deliberately the *only* place that needs to know about the
 * wrap trick - everything else (the asm, the differential fuzz test) reads
 * exactly as if ih264d_decode_bin were an ordinary function-pointer swap.
 */
#include "ih264_typedefs.h"
#include "ih264d_cabac.h"
#include "ih264_m68k_optim.h"

#if defined(MR_M68K_ASM)
UWORD32 __wrap_ih264d_decode_bin(UWORD32 u4_ctx_inc,
                                 bin_ctxt_model_t *ps_src_bin_ctxt,
                                 dec_bit_stream_t *ps_bitstrm,
                                 decoding_envirnoment_t *ps_cab_env)
{
    return mr_ih264d_decode_bin_m68k(u4_ctx_inc, (UWORD8 *)ps_src_bin_ctxt,
                                     ps_bitstrm, ps_cab_env);
}
#endif
