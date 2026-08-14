/*
 * Hand-written m68k assembly for H.264 CABAC residual coefficient parsing
 * (spec 7.3.5.3.2 / 9.3.2.3): ih264d_read_coeff4x4_cabac(), vendor/libavc/
 * decoder/ih264d_parse_cabac.c lines 64-568. Per stage-profiling
 * (ih264d_stage_profile.c) and a host callgrind pass, this is the single
 * hottest function in the whole H.264 decoder - hotter than any of the
 * motion-compensation, deblocking, CABAC-decode_bin, or motion-vector-
 * prediction routines already ported (see those .S files for the earlier
 * rounds of this work).
 *
 * UWORD8 mr_ih264d_read_coeff4x4_cabac_m68k(void *ps_bitstrm,
 *     UWORD32 u4_ctxcat, UWORD8 *ps_ctxt_sig_coeff, void *ps_dec,
 *     UWORD8 *ps_ctxt_coded);
 *
 * Not wired in via a direct wrap on this symbol - every one of its ~20
 * real call sites is spread across only two files, and 5 of the 6 unique
 * call sites are inside ih264d_parse_cabac.c itself (same file as the
 * definition), which -Wl,--wrap cannot intercept (confirmed with a
 * minimal repro during the earlier motion-vector-prediction port: a
 * same-object direct call between two non-static C functions resolves to
 * a branch with no relocation entry left for the linker to redirect).
 * ih264d_parse_cabac_coeff_port.c reimplements the two orchestrating
 * callers (ih264d_cabac_parse_8x8block, ih264d_parse_residual4x4_cabac)
 * and calls this primitive directly instead - see that file for the full
 * derivation, mirroring the pattern from ih264d_mvpred_dispatch_port.c.
 *
 * ---- Design: three shared, bsr-callable subroutines ----
 *
 * The vendored C hand-inlines DECODE_ONE_BIN_MACRO's logic four separate
 * times at different call sites (coded_block_flag, significant_coeff_
 * flag, last_significant_coeff_flag, and the abs_level_minus1 unary
 * prefix bins) plus two more inlined copies of a simpler "bypass" bit
 * read (unary suffix length, FLC suffix value, sign flag) and two more
 * inlined copies of an "ensure 23 lookahead bits" refill (before the
 * bypass unary read, and again before an FLC read wider than 10 bits) -
 * eight inlined blocks total, each pair sharing identical logic modulo
 * one parameter. In C that inlining avoids per-call function-call
 * overhead (register spills across an opaque call the compiler cannot
 * see into); in hand-written asm the register set is already fully
 * under control, so there is no such overhead to avoid - factoring the
 * three distinct *shapes* into three bsr-callable internal subroutines
 * costs a single `bsr`/`rts` pair per call (cheap, predictable) and
 * removes over 300 lines of would-be duplicated logic, each duplicate a
 * separate place a transcription slip could hide.
 *
 * .Lbin_decode - context-modeled decode (the actual DECODE_ONE_BIN_MACRO
 *   arithmetic: table lookup, MPS/LPS branch, conditional renormalise).
 *   Takes the renormalisation threshold in d5, so all three distinct
 *   thresholds used in this function (256 for coded_block_flag, 16384 for
 *   significant_coeff_flag, 512 for the abs_level_minus1 prefix bins)
 *   share this one routine. last_significant_coeff_flag's decode has *no*
 *   renormalisation at all in the vendored C (relying on the immediately
 *   preceding significant_coeff_flag decode having already renormalised
 *   range back above the CABAC engine's working minimum) - passed here as
 *   threshold=0, which naturally never satisfies "range < threshold" for
 *   an unsigned range, reproducing "skip renormalisation" with no special
 *   case in the subroutine itself.
 * .Lbypass_bit - the context-free bypass decode (range>>=1; compare;
 *   conditional subtract). Used for the bypass-coded unary suffix-length
 *   read, the FLC suffix-value bits, and the sign flag.
 * .Lensure_bits - the "flush by clz(range), then refill val_ofst with a
 *   fresh 23-bit lookahead window" renormalisation variant bypass
 *   decoding needs before it can safely start reading raw bits (bypass
 *   decoding does not maintain the same per-bit lookahead invariant
 *   context-modeled decoding does, so it must top up explicitly).
 *
 * All three were built and fuzz-verified in isolation (against
 * independent from-spec references, 100k iterations each, real m68k/
 * qemu execution) before being wired into this function - see the
 * session's working notes; the differential fuzz test that exercises
 * this whole function is check_read_coeff4x4_cabac() in
 * tests/mr_h264_cabac_coeff_check.c, which compares directly against the
 * real vendored ih264d_read_coeff4x4_cabac() (not a transcription of it -
 * this function is complex and stateful enough that comparing against a
 * hand-transcribed reference would only prove "the transcription matches
 * the asm", not "the asm matches the real decoder").
 *
 * ---- Register plan ----
 *
 * Persistent for the whole function (never spilled): d0=range,
 * d1=val_ofst, d2=offset (bitstream bit cursor - kept in a register the
 * entire call, like the vendored C's own local u4_offset, only written
 * back to ps_bitstrm->u4_ofst once at the very end - this is the
 * "deferred renormalisation" optimisation the vendored source itself
 * uses, replicated exactly, not reinterpreted), a2=pu4_buffer,
 * a3=cabac_table, a4=ps_dec, a5=tu_sblk4x4_coeff_data_t base pointer,
 * a6=coefficient write cursor (&ai2_level[next]).
 *
 * The three subroutines between them clobber d3,d4,d5,d6,d7,a0,a1 (only
 * .Lbypass_bit is cheaper, touching just d0,d1,d6), so everything else
 * this function needs across a subroutine call - loop counters, the
 * significance bitmap, the level-magnitude context state - lives in a
 * small stack-resident locals frame rather than registers, exactly like
 * a compiler's own spill decisions would look for a routine with this
 * much concurrently-live state. The frame is allocated once at entry
 * (after every incoming argument has already been read into a register)
 * and freed once before return.
 *
 * ---- Struct offsets relied on here ----
 * Computed with offsetof() and cross-checked with m68k-linux-gnu-gcc
 * under qemu-m68k specifically (NOT the host x86-64 compiler - dec_
 * struct_t's layout genuinely differs by target: e.g. dec_bit_stream_t's
 * pu4_buffer sits at byte 4 on m68k but byte 8 on x86-64, because a
 * pointer needs 8-byte alignment on x86-64 but not on m68k, so the field
 * before it pads differently. Getting this from the host compiler would
 * have silently produced wrong offsets):
 *   dec_bit_stream_t:            0=u4_ofst, 4=pu4_buffer
 *   decoding_envirnoment_t:      0=u4_code_int_range, 4=u4_code_int_val_ofst,
 *                                8=cabac_table
 *   dec_struct_t:                96=s_cab_dec_env, 252=pv_parse_tu_coeff_data,
 *                                432=p_coeff_abs_level_minus1_t[6]
 *   tu_sblk4x4_coeff_data_t:     0=u2_sig_coeff_map, 2=ai2_level[16]
 *   bin_ctxt_model_t:            1 byte (u1_mps_state), same as the
 *                                already-verified decode_bin/mvpred ports
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264d_read_coeff4x4_cabac_m68k

/* Locals frame offsets (relative to sp after the prologue's sub.l). */
L_CTXCAT      = 0
L_NUMSIG      = 4
L_SIGMAP      = 8
L_LASTIDX     = 12
L_FIRSTOFF    = 16
L_I           = 20
L_PLASTORG    = 24
L_CTXINC      = 28
L_EQ1         = 32
L_GT1         = 36
L_CODEDFLAG   = 40
L_SIGCOEFFPTR = 44
L_VALUE       = 48
L_PREFIX      = 52
L_ABSLVL      = 56
L_SUFS        = 60
L_PCTXTABSLVL = 64
L_BITSTRM     = 68
FRAME_SIZE    = 72

mr_ih264d_read_coeff4x4_cabac_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)		/* 44 bytes */
	movea.l	48(%sp),%a0			/* a0 = ps_bitstrm    (4+44)  */
	move.l	%a0,%d6				/* stash for later save into frame */
	move.l	(%a0),%d2			/* d2 = offset */
	movea.l	4(%a0),%a2			/* a2 = pu4_buffer */

	movea.l	60(%sp),%a4			/* a4 = ps_dec        (16+44) */
	move.l	96(%a4),%d0			/* d0 = range */
	move.l	100(%a4),%d1			/* d1 = val_ofst */
	movea.l	104(%a4),%a3			/* a3 = cabac_table */
	movea.l	252(%a4),%a5			/* a5 = ps_tu_4x4 base */
	clr.w	(%a5)				/* sig_coeff_map = 0 */
	lea	2(%a5),%a6			/* a6 = coeff cursor */

	move.l	52(%sp),%d5			/* d5 = ctxcat (temp) (8+44) */
	movea.l	56(%sp),%a1			/* a1 = ps_ctxt_sig_coeff (12+44) */
	movea.l	64(%sp),%a0			/* a0 = ps_ctxt_coded (24+44) */

	sub.l	#FRAME_SIZE,%sp

	move.l	%d6,L_BITSTRM(%sp)
	move.l	%d5,L_CTXCAT(%sp)
	move.l	%a1,L_SIGCOEFFPTR(%sp)		/* stash before it can be clobbered -
						 * .Lbin_decode's renorm path uses a1
						 * as scratch, and the very next call
						 * below (coded_block_flag) may take
						 * that path */

	/* --- decode coded_block_flag (threshold 256) --- */
	move.l	#256,%d5
	bsr	.Lbin_decode
	move.l	%d6,L_CODEDFLAG(%sp)
	tst.l	%d6
	beq	.Lrc_notcoded

	/* --- first_coeff_offset = (ctxcat==LUMA_AC(1)||ctxcat==CHROMA_AC(4)) --- */
	move.l	L_CTXCAT(%sp),%d3
	moveq	#0,%d4
	cmp.l	#1,%d3
	beq.b	.Lrc_fco1
	cmp.l	#4,%d3
	bne.b	.Lrc_fcodone
.Lrc_fco1:
	moveq	#1,%d4
.Lrc_fcodone:
	move.l	%d4,L_FIRSTOFF(%sp)

	/* --- uc_last_coeff_idx --- */
	cmp.l	#3,%d3
	bne.b	.Lrc_notchromadc
	moveq	#3,%d4
	bra.b	.Lrc_lastidxdone
.Lrc_notchromadc:
	move.l	%d3,%d4
	and.l	#1,%d4
	move.l	%d3,%d6
	lsr.l	#2,%d6
	add.l	%d6,%d4
	moveq	#15,%d6
	sub.l	%d4,%d6
	move.l	%d6,%d4
.Lrc_lastidxdone:
	move.l	%d4,L_LASTIDX(%sp)

	/* --- p_last_org = ps_ctxt_sig_coeff + 61 --- */
	movea.l	L_SIGCOEFFPTR(%sp),%a0		/* reload - a1 may have been
						 * clobbered by the coded_block_flag
						 * decode above */
	lea	61(%a0),%a0
	move.l	%a0,L_PLASTORG(%sp)

	clr.l	L_NUMSIG(%sp)
	clr.l	L_SIGMAP(%sp)
	clr.l	L_I(%sp)

	/* --- significance-map loop --- */
.Lrc_sigloop:
	movea.l	L_SIGCOEFFPTR(%sp),%a0
	move.l	#16384,%d5
	bsr	.Lbin_decode
	movea.l	L_SIGCOEFFPTR(%sp),%a0
	addq.l	#1,%a0
	move.l	%a0,L_SIGCOEFFPTR(%sp)

	tst.l	%d6
	beq.b	.Lrc_sig_notset

	addq.l	#1,L_NUMSIG(%sp)

	move.l	L_I(%sp),%d3
	add.l	L_FIRSTOFF(%sp),%d3
	moveq	#1,%d4
	lsl.l	%d3,%d4
	or.l	%d4,L_SIGMAP(%sp)

	movea.l	L_PLASTORG(%sp),%a0
	adda.l	L_I(%sp),%a0
	moveq	#0,%d5
	bsr	.Lbin_decode
	tst.l	%d6
	bne	.Lrc_readlevels

.Lrc_sig_notset:
	addq.l	#1,L_I(%sp)
	move.l	L_I(%sp),%d3
	cmp.l	L_LASTIDX(%sp),%d3
	blt	.Lrc_sigloop

	/* loop exhausted (not via early goto): last coeff is significant */
	addq.l	#1,L_NUMSIG(%sp)
	move.l	L_I(%sp),%d3
	add.l	L_FIRSTOFF(%sp),%d3
	moveq	#1,%d4
	lsl.l	%d3,%d4
	or.l	%d4,L_SIGMAP(%sp)

	/* --- level decode --- */
.Lrc_readlevels:
	move.w	L_SIGMAP+2(%sp),(%a5)	/* low word of the 32-bit slot -
					 * big-endian, and only bits 0-15 are
					 * ever set (i+first_coeff_offset<=15) */

	move.l	L_CTXCAT(%sp),%d3
	lsl.l	#2,%d3
	movea.l	432(%a4,%d3.l),%a1
	move.l	%a1,L_PCTXTABSLVL(%sp)
	move.l	#0x51,%d3
	move.l	%d3,L_CTXINC(%sp)
	moveq	#1,%d3
	move.l	%d3,L_EQ1(%sp)
	clr.l	L_GT1(%sp)

.Lrc_levelloop:
	/* unary prefix (threshold 512, up to UCOFF_LEVEL=14 bins) */
	clr.l	L_VALUE(%sp)
	move.l	L_CTXINC(%sp),%d3
	and.l	#0xf,%d3
	movea.l	L_PCTXTABSLVL(%sp),%a0
	adda.l	%d3,%a0
.Lrc_unaryloop:
	move.l	#512,%d5
	bsr	.Lbin_decode
	addq.l	#1,L_VALUE(%sp)
	move.l	L_CTXINC(%sp),%d3
	lsr.l	#4,%d3
	movea.l	L_PCTXTABSLVL(%sp),%a0
	adda.l	%d3,%a0
	tst.l	%d6
	beq.b	.Lrc_unarydone
	move.l	L_VALUE(%sp),%d3
	cmp.l	#14,%d3
	blt	.Lrc_unaryloop
.Lrc_unarydone:
	move.l	L_VALUE(%sp),%d3
	subq.l	#1,%d3
	add.l	%d6,%d3
	move.l	%d3,L_PREFIX(%sp)

	cmp.l	#14,%d3
	bne	.Lrc_shortlevel

	/* --- long level: bypass unary suffix-length + suffix value --- */
	move.l	#14,L_ABSLVL(%sp)

	bsr	.Lensure_bits

	moveq	#0,%d3
.Lrc_bpu_loop:
	addq.l	#1,%d3
	bsr	.Lbypass_bit
	cmp.l	#23,%d3
	bge.b	.Lrc_bpu_done
	tst.l	%d6
	bne	.Lrc_bpu_loop
.Lrc_bpu_done:
	subq.l	#1,%d3
	move.l	%d3,L_VALUE(%sp)
	moveq	#1,%d4
	lsl.l	%d3,%d4
	move.l	%d4,L_SUFS(%sp)

	tst.l	%d3
	beq.b	.Lrc_flcdone

	cmp.l	#10,%d3
	ble.b	.Lrc_flcnoensure
	bsr	.Lensure_bits
.Lrc_flcnoensure:
	move.l	L_VALUE(%sp),%d4
	moveq	#0,%d7
	moveq	#0,%d3
.Lrc_flcloop:
	addq.l	#1,%d3
	bsr	.Lbypass_bit
	lsl.l	#1,%d7
	or.l	%d6,%d7
	cmp.l	%d4,%d3
	blt	.Lrc_flcloop
	move.l	%d7,L_VALUE(%sp)
.Lrc_flcdone:
	move.l	L_SUFS(%sp),%d3
	add.l	L_VALUE(%sp),%d3
	move.l	L_ABSLVL(%sp),%d4
	add.l	%d3,%d4
	move.l	%d4,L_ABSLVL(%sp)
	bra	.Lrc_levelcommon

.Lrc_shortlevel:
	moveq	#1,%d4
	add.l	L_PREFIX(%sp),%d4
	move.l	%d4,L_ABSLVL(%sp)

.Lrc_levelcommon:
	move.l	L_ABSLVL(%sp),%d3
	cmp.l	#1,%d3
	ble.b	.Lrc_notgt1
	addq.l	#1,L_GT1(%sp)
.Lrc_notgt1:
	tst.l	L_GT1(%sp)
	bne.b	.Lrc_isgt1

	addq.l	#1,L_EQ1(%sp)
	move.l	L_EQ1(%sp),%d3
	cmp.l	#4,%d3
	ble.b	.Lrc_eq1ok
	moveq	#4,%d3
.Lrc_eq1ok:
	move.l	#0x50,%d4
	add.l	%d3,%d4
	move.l	%d4,L_CTXINC(%sp)
	bra.b	.Lrc_ctxincdone
.Lrc_isgt1:
	move.l	L_GT1(%sp),%d3
	cmp.l	#4,%d3
	ble.b	.Lrc_gt1ok
	moveq	#4,%d3
.Lrc_gt1ok:
	add.l	#5,%d3
	lsl.l	#4,%d3
	move.l	%d3,L_CTXINC(%sp)
.Lrc_ctxincdone:

	/* sign flag (bypass) */
	bsr	.Lbypass_bit
	tst.l	%d6
	beq.b	.Lrc_nosign
	move.l	L_ABSLVL(%sp),%d3
	neg.l	%d3
	move.l	%d3,L_ABSLVL(%sp)
.Lrc_nosign:

	subq.l	#1,L_NUMSIG(%sp)
	move.l	L_ABSLVL(%sp),%d3
	move.w	%d3,(%a6)
	addq.l	#2,%a6

	move.l	L_NUMSIG(%sp),%d3
	bgt	.Lrc_levelloop

	/* pv_parse_tu_coeff_data += ALIGN4(coeff_cursor - tu4x4_base) */
	move.l	%a6,%d3
	sub.l	%a5,%d3
	addq.l	#3,%d3
	and.l	#-4,%d3
	movea.l	%a5,%a0
	adda.l	%d3,%a0
	move.l	%a0,252(%a4)
	bra	.Lrc_done

.Lrc_notcoded:
.Lrc_done:
	move.l	%d0,96(%a4)
	move.l	%d1,100(%a4)
	movea.l	L_BITSTRM(%sp),%a0
	move.l	%d2,(%a0)

	move.l	L_CODEDFLAG(%sp),%d0
	add.l	#FRAME_SIZE,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---- shared: context-modeled bin decode ----
 * IN:  d0=range d1=val_ofst d2=offset a2=buf a3=table a0=ctx_byte_ptr
 *      d5=renorm threshold (0 = never renormalise here)
 * OUT: d0,d1,d2 updated; d6=symbol; (a0) updated
 * CLOBBERS: d3,d4,d5,d7,a1
 */
.Lbin_decode:
	clr.l	%d3
	move.b	(%a0),%d3

	move.l	%d3,%d6
	lsr.l	#6,%d6
	and.l	#1,%d6

	tst.l	%d0
	bne.b	.Lbd_nz
	moveq	#31,%d4
	bra.b	.Lbd_clzdone
.Lbd_nz:
	bfffo	%d0{#0:#0},%d4
.Lbd_clzdone:
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
	blo.b	.Lbd_mps
	eori.l	#1,%d6
	sub.l	%d0,%d1
	move.l	%d7,%d0
	move.l	%d3,%d4
	moveq	#15,%d7
	lsr.l	%d7,%d4
	and.l	#0x7f,%d4
.Lbd_mps:
	move.b	%d4,(%a0)

	cmp.l	%d5,%d0
	bhs.b	.Lbd_norenorm
	tst.l	%d0
	bne.b	.Lbd_rn_nz
	moveq	#31,%d7
	bra.b	.Lbd_rn_clzdone
.Lbd_rn_nz:
	bfffo	%d0{#0:#0},%d7
.Lbd_rn_clzdone:
	move.l	%d2,%d5
	add.l	#23,%d5
	move.l	%d5,%d3
	and.l	#31,%d3
	lsr.l	#5,%d5
	lea	(%a2,%d5.l*4),%a1
	move.l	(%a1),%d5
	lsl.l	%d3,%d5

	tst.l	%d3
	beq.b	.Lbd_rn_nomerge
	move.l	4(%a1),%d4
	neg.l	%d3
	add.l	#32,%d3
	lsr.l	%d3,%d4
	or.l	%d4,%d5
.Lbd_rn_nomerge:
	moveq	#32,%d3
	sub.l	%d7,%d3
	lsr.l	%d3,%d5

	add.l	%d7,%d2
	lsl.l	%d7,%d0
	lsl.l	%d7,%d1
	or.l	%d5,%d1
.Lbd_norenorm:
	rts

/* ---- shared: bypass bit decode ----
 * IN:  d0=range d1=val_ofst
 * OUT: d0,d1 updated; d6=bit
 * CLOBBERS: nothing else
 */
.Lbypass_bit:
	lsr.l	#1,%d0
	cmp.l	%d0,%d1
	blo.b	.Lbb_zero
	sub.l	%d0,%d1
	moveq	#1,%d6
	rts
.Lbb_zero:
	moveq	#0,%d6
	rts

/* ---- shared: ensure 23 lookahead bits (bypass renorm) ----
 * IN:  d0=range d1=val_ofst d2=offset a2=buf
 * OUT: d0,d1,d2 updated
 * CLOBBERS: d3,d4,d5,d6,a1
 */
.Lensure_bits:
	tst.l	%d0
	bne.b	.Leb_nz
	moveq	#31,%d4
	bra.b	.Leb_clzdone
.Leb_nz:
	bfffo	%d0{#0:#0},%d4
.Leb_clzdone:
	add.l	%d4,%d2

	move.l	%d2,%d5
	and.l	#31,%d5
	move.l	%d2,%d3
	lsr.l	#5,%d3
	lea	(%a2,%d3.l*4),%a1
	move.l	(%a1),%d3
	lsl.l	%d5,%d3

	tst.l	%d5
	beq.b	.Leb_nomerge
	move.l	4(%a1),%d6
	neg.l	%d5
	add.l	#32,%d5
	lsr.l	%d5,%d6
	or.l	%d6,%d3
.Leb_nomerge:
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
