/*
 * Hand-written m68k assembly for H.264 CABAC 8x8-transform residual
 * coefficient parsing (spec 7.3.5.3.2 / 9.3.2.3, High-Profile transform_
 * size_8x8_flag path): ih264d_read_coeff8x8_cabac(), vendor/libavc/
 * decoder/ih264d_parse_cabac.c lines 582-1025.
 *
 * This is ih264_m68k_cabac_coeff.S's sibling (the 4x4 residual parser,
 * already ported) for the 8x8 case, deliberately left untouched by that
 * earlier work: "no High-Profile/transform8x8 test content exists in this
 * project" (see ih264d_parse_cabac_coeff_port.c's header comment). That
 * turned out to be an unsafe assumption to build on rather than verify -
 * libx264's High Profile preset enables 8x8dct by default, including for
 * this project's own test_h264_high.mp4 fixture (`ffmpeg -v verbose`
 * reports "8x8 transform intra:8.8% inter:24.9%" on it) - so every High
 * Profile stream this player has ever decoded, real or test, has been
 * running a meaningful fraction of its residual coefficients through this
 * exact unaccelerated generic-C path the whole time.  A stage-profiled
 * qemu run against a 320x240 High-Profile clip (50 frames) counted 744
 * calls to this function, alongside 22930 calls to the (already hand-asm)
 * motion-compensation filters and 41432 to deblocking - a small fraction
 * by call count, but each call here parses a full 64-coefficient block
 * (4x the coefficients of a 4x4 call) with CABAC's own more elaborate
 * 8x8 significance-map context structure, so its share of actual decode
 * time is disproportionately larger than that count alone suggests.
 *
 * UWORD8 mr_ih264d_read_coeff8x8_cabac_m68k(void *ps_bitstrm,
 *     void *ps_dec, void *ps_cur_mb_info);
 *
 * Not wired in via -Wl,--wrap on the vendored symbol name, for the same
 * reason as the 4x4 port: all four of this function's call sites are
 * inside ih264d_parse_residual4x4_cabac() - itself already reimplemented
 * (not wrapped) in ih264d_parse_cabac_coeff_port.c because *its* own call
 * sites are same-file-only - and --wrap cannot intercept a same-object
 * direct call (confirmed the hard way during the mvpred port: a first
 * attempt wrapping the callee directly linked cleanly and simply never
 * executed). That reimplementation is updated by this same change to call
 * this primitive at its four ih264d_read_coeff8x8_cabac() call sites
 * instead of the real vendored function - see that file.
 *
 * ---- Design: reuses the 4x4 port's three shared subroutines verbatim ----
 *
 * ih264_m68k_cabac_coeff.S's own header explains the three-subroutine
 * factoring in full; the short version: the vendored C hand-inlines the
 * same "DECODE_ONE_BIN_MACRO" arithmetic and the same bypass-bit/lookahead-
 * refill shapes repeatedly, and this function's own vendored C (read side
 * by side with the 4x4 version while porting) turns out to hand-inline
 * the *exact same three shapes*, with the exact same three renormalisation
 * thresholds (256 is not used here - this function has no coded_block_flag
 * of its own to decode - but 16384 for significant_coeff_flag, 0/never for
 * last_significant_coeff_flag, and 512 for the abs_level_minus1 unary
 * prefix bins are all identical) and the exact same bypass/ensure-bits
 * shapes for the unary suffix length, FLC suffix value and sign flag.
 * Rather than reference the 4x4 file's local (.L-prefixed, so
 * assembler-local and not linkable across object files) subroutine labels
 * across a file boundary, this file carries its own copies - three
 * duplicated but independently-labelled routines, verified against their
 * own already-verified twins by direct line-for-line comparison, not by
 * re-deriving the CABAC arithmetic from the spec a second time. This
 * mirrors this project's existing preference for self-contained .S files
 * over introducing cross-file bsr/call-boundary complexity that has not
 * been needed anywhere else in this port yet.
 *
 * ---- What is genuinely different from the 4x4 case, not just renamed ---
 *
 * 1. Context category is a single fixed constant (LUMA_8X8_CTXCAT, 5) -
 *    unlike 4x4, which is called for five different block categories
 *    (luma AC/DC, chroma AC/DC, luma 4x4) and has to branch on u4_ctxcat
 *    to work out first_coeff_offset/uc_last_coeff_idx. This function
 *    always decodes a full 64-coefficient block (uc_last_coeff_idx is
 *    simply 63, no branching needed), so that whole piece of the 4x4
 *    port's logic has no counterpart here at all.
 * 2. The significance-map and last-significant-coefficient context
 *    pointers are NOT simply "base + running index" the way 4x4's are
 *    (ps_ctxt_sig_coeff + i via a plain addq.l #1 each iteration,
 *    p_last_org + i via a plain adda.l of the loop counter) - this
 *    function's vendored C explicitly indexes through two lookup tables,
 *    pu1_sigcoeff_context_inc[i+1] (selecting the *next* iteration's
 *    significance context, frame- or field-coded variant) and
 *    pu1_lastcoeff_context_inc[i] (selecting the last-coefficient
 *    context for the position that was just found significant) - both
 *    external UWORD8[64] tables from vendor/libavc/decoder/ih264d_tables.c,
 *    referenced here exactly as the C does, not reproduced or
 *    reinterpreted. Missing this and reusing 4x4's flat "+i" stepping
 *    would still run, still terminate, and still write plausible-looking
 *    coefficient values - it would just be reading the wrong CABAC
 *    contexts, exactly the "silently drifts every following frame until
 *    the next IDR" failure mode this codebase's own working notes warn
 *    about, so this is the one piece of this port that most needed
 *    getting right rather than pattern-matched from the sibling file.
 * 3. u1_field_coding_flag (ps_cur_mb_info->ps_curmb->u1_mb_fld) selects
 *    between a frame- and a field-coded variant of both the significance
 *    context base pointer (ps_dec->s_high_profile.ps_sigcoeff_8x8_frame
 *    vs _field) and the sigcoeff_context_inc table (gau1_ih264d_
 *    sigcoeff_context_inc_frame vs _field) - read and branched on once at
 *    entry, not per-iteration. In practice frame_mbs_only_flag content
 *    (essentially all real web/YouTube H.264) never sets this, so the
 *    field path is exercised by neither the conformance suite nor any
 *    known real content, but it costs one branch at entry to implement
 *    correctly rather than assume away.
 * 4. The significant-coefficient bitmap is 64 bits (au4_sig_coeff_map[2],
 *    two UWORD32 words) instead of 4x4's 16-bit map packed into the first
 *    half-word of tu_sblk4x4_coeff_data_t - both words are written out
 *    once, after the significance loop, before the level-decode loop
 *    that follows starts consuming au5's coefficient-write cursor.
 * 5. The level-decode loop itself (unary prefix, long-level bypass
 *    suffix, sign flag, coeff_abs_level_minus1 context-increment update)
 *    is otherwise byte-for-byte identical to the 4x4 version once ctxcat
 *    is fixed at LUMA_8X8_CTXCAT - the H.264 spec defines this decode
 *    identically regardless of transform block size, only the context
 *    array (p_coeff_abs_level_minus1_t[ctxcat]) it reads from differs.
 *
 * ---- Struct offsets ----
 * Computed with offsetof() under m68k-linux-gnu-gcc/qemu-m68k specifically
 * (see the 4x4 file's header for why the host x86-64 compiler cannot be
 * used for this - pointer alignment differs by target):
 *   dec_struct_t:                96=s_cab_dec_env, 252=pv_parse_tu_coeff_data,
 *                                452=p_coeff_abs_level_minus1_t[LUMA_8X8_CTXCAT],
 *                                42408=s_high_profile
 *   high_profile_tools_t (relative to s_high_profile, so 42408+N below):
 *                                518=ps_sigcoeff_8x8_frame,
 *                                530=ps_sigcoeff_8x8_field
 *   dec_mb_info_t:                54=ps_curmb
 *   mb_neigbour_params_t:         28=u1_mb_fld
 *   tu_blk8x8_coeff_data_t:       0=au4_sig_coeff_map[0], 4=au4_sig_coeff_map[1],
 *                                8=ai2_level[64]
 *   dec_bit_stream_t:             0=u4_ofst, 4=pu4_buffer      (same as 4x4)
 *   decoding_envirnoment_t:       0=u4_code_int_range, 4=u4_code_int_val_ofst,
 *                                8=cabac_table                  (same as 4x4)
 *   bin_ctxt_model_t:             1 byte (u1_mps_state)         (same as 4x4)
 *   LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT_8X8 = 15 (ih264d_cabac.h)
 *
 * Verification: a differential fuzz test (check_read_coeff8x8_cabac() in
 * tests/mr_h264_cabac_coeff_check.c) compares this primitive directly
 * against the real vendored ih264d_read_coeff8x8_cabac() - not a
 * transcription of it, for the same reason the 4x4 test does the same:
 * a hand-transcribed reference could only prove "the asm matches the
 * transcription", not "the asm matches the real decoder". Random CABAC
 * engine state, bitstream contents and u1_mb_fld are fed to both and
 * every output field (updated range/val_ofst/offset, the full
 * tu_blk8x8_coeff_data_t contents, both context-model arrays consumed)
 * is compared bit-exact. Plus the full make check-m68k H.264-vs-ffmpeg
 * conformance suite (worst per-frame MAE must stay unchanged) and a qemu
 * -d exec trace confirming this function's own address - not the
 * original vendored C - is what actually executes once wired in.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264d_read_coeff8x8_cabac_m68k

/* Locals frame offsets (relative to sp after the prologue's sub.l). */
L_BITSTRM     = 0
L_SIGCTXORG   = 4	/* base pointer: ps_dec->s_high_profile.ps_sigcoeff_8x8_(frame|field) */
L_SIGCTXCUR   = 8	/* next significance-decode context pointer                          */
L_PLASTORG    = 12	/* L_SIGCTXORG + LAST_COEFF_CTXT_MINUS_SIG_COEFF_CTXT_8X8 (15)        */
L_SIGINCTBL   = 16	/* gau1_ih264d_sigcoeff_context_inc_(frame|field), selected once      */
L_NUMSIG      = 20
L_SIGMAP0     = 24	/* au4_sig_coeff_map[0]: positions 0-31                               */
L_SIGMAP1     = 28	/* au4_sig_coeff_map[1]: positions 32-63                               */
L_I           = 32
L_CTXINC      = 36
L_EQ1         = 40
L_GT1         = 44
L_VALUE       = 48
L_PREFIX      = 52
L_ABSLVL      = 56
L_SUFS        = 60
FRAME_SIZE    = 64

mr_ih264d_read_coeff8x8_cabac_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)		/* 44 bytes */
	movea.l	48(%sp),%a0			/* a0 = ps_bitstrm    (4+44)  */
	move.l	%a0,%d6				/* stash for later save into frame */
	move.l	(%a0),%d2			/* d2 = offset */
	movea.l	4(%a0),%a2			/* a2 = pu4_buffer */

	movea.l	52(%sp),%a4			/* a4 = ps_dec        (8+44)  */
	move.l	96(%a4),%d0			/* d0 = range */
	move.l	100(%a4),%d1			/* d1 = val_ofst */
	movea.l	104(%a4),%a3			/* a3 = cabac_table */
	movea.l	252(%a4),%a5			/* a5 = ps_tu_8x8 base */

	movea.l	56(%sp),%a1			/* a1 = ps_cur_mb_info (12+44) */
	movea.l	54(%a1),%a1			/* a1 = ps_curmb */
	clr.l	%d3
	move.b	28(%a1),%d3			/* d3 = u1_mb_fld */

	sub.l	#FRAME_SIZE,%sp

	move.l	%d6,L_BITSTRM(%sp)

	tst.b	%d3
	bne.b	.Lc8_field
	lea	42408+518(%a4),%a0		/* &ps_dec->s_high_profile.ps_sigcoeff_8x8_frame */
	movea.l	(%a0),%a1
	lea	gau1_ih264d_sigcoeff_context_inc_frame,%a0
	bra.b	.Lc8_fielddone
.Lc8_field:
	lea	42408+530(%a4),%a0		/* &ps_dec->s_high_profile.ps_sigcoeff_8x8_field */
	movea.l	(%a0),%a1
	lea	gau1_ih264d_sigcoeff_context_inc_field,%a0
.Lc8_fielddone:
	move.l	%a1,L_SIGCTXORG(%sp)
	move.l	%a1,L_SIGCTXCUR(%sp)
	lea	15(%a1),%a1
	move.l	%a1,L_PLASTORG(%sp)
	move.l	%a0,L_SIGINCTBL(%sp)

	clr.l	L_NUMSIG(%sp)
	clr.l	L_SIGMAP0(%sp)
	clr.l	L_SIGMAP1(%sp)
	clr.l	L_I(%sp)

	/* --- significance-map / last-significant-coefficient loop ---
	 * do { ...; i++; } while (i < 63) - see the header comment: both
	 * context pointers step through their own UWORD8[64] lookup table,
	 * not a flat per-position increment. */
.Lc8_sigloop:
	movea.l	L_SIGCTXCUR(%sp),%a0
	move.l	#16384,%d5
	bsr	.Lc8_bin_decode

	/* next iteration's significance context: L_SIGCTXORG + tbl[i+1] */
	movea.l	L_SIGINCTBL(%sp),%a0
	move.l	L_I(%sp),%d3
	addq.l	#1,%d3
	clr.l	%d4
	move.b	(%a0,%d3.l),%d4
	movea.l	L_SIGCTXORG(%sp),%a1
	adda.l	%d4,%a1
	move.l	%a1,L_SIGCTXCUR(%sp)

	tst.l	%d6
	beq.b	.Lc8_sig_notset

	addq.l	#1,L_NUMSIG(%sp)

	move.l	L_I(%sp),%d3
	cmp.l	#31,%d3
	bgt.b	.Lc8_sig_hiword
	moveq	#1,%d4
	lsl.l	%d3,%d4
	or.l	%d4,L_SIGMAP0(%sp)
	bra.b	.Lc8_sig_bitdone
.Lc8_sig_hiword:
	sub.l	#32,%d3
	moveq	#1,%d4
	lsl.l	%d3,%d4
	or.l	%d4,L_SIGMAP1(%sp)
.Lc8_sig_bitdone:

	/* last_significant_coeff_flag context: L_PLASTORG + lastcoeff_tbl[i] */
	lea	gau1_ih264d_lastcoeff_context_inc,%a0
	move.l	L_I(%sp),%d3
	clr.l	%d4
	move.b	(%a0,%d3.l),%d4
	movea.l	L_PLASTORG(%sp),%a0
	adda.l	%d4,%a0
	moveq	#0,%d5
	bsr	.Lc8_bin_decode
	tst.l	%d6
	bne	.Lc8_readlevels

.Lc8_sig_notset:
	addq.l	#1,L_I(%sp)
	move.l	L_I(%sp),%d3
	cmp.l	#63,%d3
	blt	.Lc8_sigloop

	/* loop exhausted (not via early goto): position 63 is significant */
	addq.l	#1,L_NUMSIG(%sp)
	moveq	#1,%d4
	moveq	#31,%d7
	lsl.l	%d7,%d4
	or.l	%d4,L_SIGMAP1(%sp)

	/* --- level decode: byte-for-byte the same as the 4x4 port's, with
	 * ctxcat fixed at LUMA_8X8_CTXCAT (5) instead of computed - see the
	 * header comment, point 5. --- */
.Lc8_readlevels:
	move.l	L_SIGMAP0(%sp),(%a5)
	move.l	L_SIGMAP1(%sp),4(%a5)
	lea	8(%a5),%a6			/* a6 = coeff write cursor */

	movea.l	452(%a4),%a1			/* p_coeff_abs_level_minus1_t[5] */
	move.l	#0x51,L_CTXINC(%sp)
	moveq	#1,%d3
	move.l	%d3,L_EQ1(%sp)
	clr.l	L_GT1(%sp)

.Lc8_levelloop:
	clr.l	L_VALUE(%sp)
	move.l	L_CTXINC(%sp),%d3
	and.l	#0xf,%d3
	movea.l	452(%a4),%a0
	adda.l	%d3,%a0
.Lc8_unaryloop:
	move.l	#512,%d5
	bsr	.Lc8_bin_decode
	addq.l	#1,L_VALUE(%sp)
	move.l	L_CTXINC(%sp),%d3
	lsr.l	#4,%d3
	movea.l	452(%a4),%a0
	adda.l	%d3,%a0
	tst.l	%d6
	beq.b	.Lc8_unarydone
	move.l	L_VALUE(%sp),%d3
	cmp.l	#14,%d3
	blt	.Lc8_unaryloop
.Lc8_unarydone:
	move.l	L_VALUE(%sp),%d3
	subq.l	#1,%d3
	add.l	%d6,%d3
	move.l	%d3,L_PREFIX(%sp)

	cmp.l	#14,%d3
	bne	.Lc8_shortlevel

	move.l	#14,L_ABSLVL(%sp)
	bsr	.Lc8_ensure_bits

	moveq	#0,%d3
.Lc8_bpu_loop:
	addq.l	#1,%d3
	bsr	.Lc8_bypass_bit
	cmp.l	#23,%d3
	bge.b	.Lc8_bpu_done
	tst.l	%d6
	bne	.Lc8_bpu_loop
.Lc8_bpu_done:
	subq.l	#1,%d3
	move.l	%d3,L_VALUE(%sp)
	moveq	#1,%d4
	lsl.l	%d3,%d4
	move.l	%d4,L_SUFS(%sp)

	tst.l	%d3
	beq.b	.Lc8_flcdone

	cmp.l	#10,%d3
	ble.b	.Lc8_flcnoensure
	bsr	.Lc8_ensure_bits
.Lc8_flcnoensure:
	move.l	L_VALUE(%sp),%d4
	moveq	#0,%d7
	moveq	#0,%d3
.Lc8_flcloop:
	addq.l	#1,%d3
	bsr	.Lc8_bypass_bit
	lsl.l	#1,%d7
	or.l	%d6,%d7
	cmp.l	%d4,%d3
	blt	.Lc8_flcloop
	move.l	%d7,L_VALUE(%sp)
.Lc8_flcdone:
	move.l	L_SUFS(%sp),%d3
	add.l	L_VALUE(%sp),%d3
	move.l	L_ABSLVL(%sp),%d4
	add.l	%d3,%d4
	move.l	%d4,L_ABSLVL(%sp)
	bra	.Lc8_levelcommon

.Lc8_shortlevel:
	moveq	#1,%d4
	add.l	L_PREFIX(%sp),%d4
	move.l	%d4,L_ABSLVL(%sp)

.Lc8_levelcommon:
	move.l	L_ABSLVL(%sp),%d3
	cmp.l	#1,%d3
	ble.b	.Lc8_notgt1
	addq.l	#1,L_GT1(%sp)
.Lc8_notgt1:
	tst.l	L_GT1(%sp)
	bne.b	.Lc8_isgt1

	addq.l	#1,L_EQ1(%sp)
	move.l	L_EQ1(%sp),%d3
	cmp.l	#4,%d3
	ble.b	.Lc8_eq1ok
	moveq	#4,%d3
.Lc8_eq1ok:
	move.l	#0x50,%d4
	add.l	%d3,%d4
	move.l	%d4,L_CTXINC(%sp)
	bra.b	.Lc8_ctxincdone
.Lc8_isgt1:
	move.l	L_GT1(%sp),%d3
	cmp.l	#4,%d3
	ble.b	.Lc8_gt1ok
	moveq	#4,%d3
.Lc8_gt1ok:
	add.l	#5,%d3
	lsl.l	#4,%d3
	move.l	%d3,L_CTXINC(%sp)
.Lc8_ctxincdone:

	bsr	.Lc8_bypass_bit
	tst.l	%d6
	beq.b	.Lc8_nosign
	move.l	L_ABSLVL(%sp),%d3
	neg.l	%d3
	move.l	%d3,L_ABSLVL(%sp)
.Lc8_nosign:

	subq.l	#1,L_NUMSIG(%sp)
	move.l	L_ABSLVL(%sp),%d3
	move.w	%d3,(%a6)
	addq.l	#2,%a6

	move.l	L_NUMSIG(%sp),%d3
	bgt	.Lc8_levelloop

	/* pv_parse_tu_coeff_data += ALIGN4(coeff_cursor - tu8x8_base) */
	move.l	%a6,%d3
	sub.l	%a5,%d3
	addq.l	#3,%d3
	and.l	#-4,%d3
	movea.l	%a5,%a0
	adda.l	%d3,%a0
	move.l	%a0,252(%a4)

	move.l	%d0,96(%a4)
	move.l	%d1,100(%a4)
	movea.l	L_BITSTRM(%sp),%a0
	move.l	%d2,(%a0)

	add.l	#FRAME_SIZE,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---- shared: context-modeled bin decode (copy of ih264_m68k_cabac_
 * coeff.S's .Lbin_decode - see that file for the derivation) ----
 * IN:  d0=range d1=val_ofst d2=offset a2=buf a3=table a0=ctx_byte_ptr
 *      d5=renorm threshold (0 = never renormalise here)
 * OUT: d0,d1,d2 updated; d6=symbol; (a0) updated
 * CLOBBERS: d3,d4,d5,d7,a1
 */
.Lc8_bin_decode:
	clr.l	%d3
	move.b	(%a0),%d3

	move.l	%d3,%d6
	lsr.l	#6,%d6
	and.l	#1,%d6

	tst.l	%d0
	bne.b	.Lc8bd_nz
	moveq	#31,%d4
	bra.b	.Lc8bd_clzdone
.Lc8bd_nz:
	bfffo	%d0{#0:#0},%d4
.Lc8bd_clzdone:
	move.l	%d0,%d7
	lsl.l	%d4,%d7
	bfextu	%d7{#1:#2},%d7

	lsl.l	#2,%d3
	add.l	%d7,%d3
	move.l	(%a3,%d3.l*4),%d3

	move.l	%d3,%d7
	and.l	#255,%d7
	neg.l	%d4
	add.l	#23,%d4
	lsl.l	%d4,%d7

	sub.l	%d7,%d0

	move.l	%d3,%d4
	lsr.l	#8,%d4
	and.l	#0x7f,%d4

	cmp.l	%d0,%d1
	blo.b	.Lc8bd_mps
	eori.l	#1,%d6
	sub.l	%d0,%d1
	move.l	%d7,%d0
	move.l	%d3,%d4
	moveq	#15,%d7
	lsr.l	%d7,%d4
	and.l	#0x7f,%d4
.Lc8bd_mps:
	move.b	%d4,(%a0)

	cmp.l	%d5,%d0
	bhs.b	.Lc8bd_norenorm
	tst.l	%d0
	bne.b	.Lc8bd_rn_nz
	moveq	#31,%d7
	bra.b	.Lc8bd_rn_clzdone
.Lc8bd_rn_nz:
	bfffo	%d0{#0:#0},%d7
.Lc8bd_rn_clzdone:
	move.l	%d2,%d5
	add.l	#23,%d5
	move.l	%d5,%d3
	and.l	#31,%d3
	lsr.l	#5,%d5
	lea	(%a2,%d5.l*4),%a1
	move.l	(%a1),%d5
	lsl.l	%d3,%d5

	tst.l	%d3
	beq.b	.Lc8bd_rn_nomerge
	move.l	4(%a1),%d4
	neg.l	%d3
	add.l	#32,%d3
	lsr.l	%d3,%d4
	or.l	%d4,%d5
.Lc8bd_rn_nomerge:
	moveq	#32,%d3
	sub.l	%d7,%d3
	lsr.l	%d3,%d5

	add.l	%d7,%d2
	lsl.l	%d7,%d0
	lsl.l	%d7,%d1
	or.l	%d5,%d1
.Lc8bd_norenorm:
	rts

/* ---- shared: bypass bit decode (copy of .Lbypass_bit) ----
 * IN:  d0=range d1=val_ofst
 * OUT: d0,d1 updated; d6=bit
 * CLOBBERS: nothing else
 */
.Lc8_bypass_bit:
	lsr.l	#1,%d0
	cmp.l	%d0,%d1
	blo.b	.Lc8bb_zero
	sub.l	%d0,%d1
	moveq	#1,%d6
	rts
.Lc8bb_zero:
	moveq	#0,%d6
	rts

/* ---- shared: ensure 23 lookahead bits (copy of .Lensure_bits) ----
 * IN:  d0=range d1=val_ofst d2=offset a2=buf
 * OUT: d0,d1,d2 updated
 * CLOBBERS: d3,d4,d5,d6,a1
 */
.Lc8_ensure_bits:
	tst.l	%d0
	bne.b	.Lc8eb_nz
	moveq	#31,%d4
	bra.b	.Lc8eb_clzdone
.Lc8eb_nz:
	bfffo	%d0{#0:#0},%d4
.Lc8eb_clzdone:
	add.l	%d4,%d2

	move.l	%d2,%d5
	and.l	#31,%d5
	move.l	%d2,%d3
	lsr.l	#5,%d3
	lea	(%a2,%d3.l*4),%a1
	move.l	(%a1),%d3
	lsl.l	%d5,%d3

	tst.l	%d5
	beq.b	.Lc8eb_nomerge
	move.l	4(%a1),%d6
	neg.l	%d5
	add.l	#32,%d5
	lsr.l	%d5,%d6
	or.l	%d6,%d3
.Lc8eb_nomerge:
	moveq	#9,%d6
	lsr.l	%d6,%d3

	lsl.l	%d4,%d0
	lsl.l	%d4,%d1
	or.l	%d3,%d1
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
