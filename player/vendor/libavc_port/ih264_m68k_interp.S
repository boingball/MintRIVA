/*
 * Hand-written m68k assembly for H.264's 6-tap half-pel luma interpolation
 * filter (H.264 spec 8.4.2.2.1), the pure horizontal-only and vertical-only
 * cases (dydx == 2 and dydx == 8 in libavc's apf_inter_pred_luma[] table -
 * see ih264d_function_selector_port.c). These are the two "leaf" 6-tap
 * filters libavc calls directly (the various quarter-pel table entries each
 * run their own two-pass 6-tap+average variant instead of calling these),
 * and the simplest, best-understood member of the interpolation family to
 * hand-optimise first.
 *
 * Filter: out = CLIP_U8((1*(p0+p5) - 5*(p1+p4) + 20*(p2+p3) + 16) >> 5)
 * for the 6 taps p0..p5 = 6 consecutive samples (horizontally or
 * vertically) centred so p2,p3 straddle the half-pel position. Intermediate
 * range is [-2550, 10726] (worst case all-0/all-255 taps), comfortably
 * inside a signed 16-bit word - confirmed against ih264_inter_pred_filters.c
 * (the WORD16 i2_tmp there), so every intermediate here stays in .w and the
 * final arithmetic shift matches C's `>>` on that same type on this same
 * target (both are hardware ASR - no separate rounding-mode risk).
 *
 * What this buys over `gcc -O2 -m68030` on libavc's own reference C for
 * this filter: GCC already turns the times-1/-5/20 coefficients into
 * shift-and-add (no real multiply) on its own - that part it gets right.
 * What it does not do is
 * reuse the address register across taps: its generated loop rebuilds an
 * address register from scratch for essentially every tap access and every
 * output pixel (`move.l %a0,%a5` then offset from that), where a single
 * cursor register with constant -2/-1/0/1/2/3 displacements (horizontal) or
 * a handful of stride adds (vertical) does the same addressing with far
 * fewer address computations, plus a tighter three-branch clip than the
 * bitfield-extract sequence GCC emits.
 *
 * Assembled only for m68k builds, with the same GNU AS syntax and
 * stack-argument-passing convention MintAMP's own amiga_m68k_polyphase.S
 * already uses on this project - m68k-amigaos-gcc and the m68k-linux-gnu
 * test toolchain (see player/tests/run_m68k_check.sh) are the same
 * GCC/binutils m68k backend, so both assemble and call this identically;
 * only the runtime/OS differs, and this file touches neither.
 *
 * void mr_ih264_inter_pred_luma_horz_m68k(UWORD8 *pu1_src, UWORD8 *pu1_dst,
 *     WORD32 src_strd, WORD32 dst_strd, WORD32 ht, WORD32 wd,
 *     UWORD8 *pu1_tmp, WORD32 dydx);
 * void mr_ih264_inter_pred_luma_vert_m68k(UWORD8 *pu1_src, UWORD8 *pu1_dst,
 *     WORD32 src_strd, WORD32 dst_strd, WORD32 ht, WORD32 wd,
 *     UWORD8 *pu1_tmp, WORD32 dydx);
 *
 * Neither pu1_tmp nor dydx is used - both are part of the shared
 * ih264_inter_pred_luma_ft function-pointer type (ih264_inter_pred_filters.h)
 * because some of the *other* 14 entries in that table need them; the fixed
 * horizontal/vertical half-pel case does not.
 *
 * m68k C ABI here (confirmed against `m68k-linux-gnu-gcc -S` output for this
 * exact 8-argument signature): every argument is 4 bytes wide and passed on
 * the stack in declaration order, starting at 4(%sp):
 *   4(%sp)  = pu1_src      20(%sp) = ht
 *   8(%sp)  = pu1_dst      24(%sp) = wd
 *  12(%sp)  = src_strd     28(%sp) = pu1_tmp  (unused)
 *  16(%sp)  = dst_strd     32(%sp) = dydx     (unused)
 * Both functions here save %d2-%d6/%a2-%a5 (9 registers, 36 bytes), so
 * those stack offsets become +36 for the duration of the function body.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_inter_pred_luma_horz_m68k
	.globl	mr_ih264_inter_pred_luma_vert_m68k

/* ---------------------------------------------------------------------- */
/* Horizontal: a4/a5 are the column cursors into the current row; taps sit */
/* at constant -2/-1/0/1/2/3 displacements from a4, so no address needs   */
/* recomputing except the +1 step to the next column.                     */
/* ---------------------------------------------------------------------- */
mr_ih264_inter_pred_luma_horz_m68k:
	movem.l	%d2-%d6/%a2-%a5,-(%sp)
	move.l	40(%sp),%a2		/* pu1_src  (4+36)  */
	move.l	44(%sp),%a3		/* pu1_dst  (8+36)  */
	move.l	56(%sp),%d5		/* ht       (20+36) */
	tst.l	%d5
	ble	.Lh_done
	move.l	60(%sp),%d6		/* wd       (24+36) */
	tst.l	%d6
	ble	.Lh_done

.Lh_row:
	movea.l	%a2,%a4			/* column src cursor for this row */
	movea.l	%a3,%a5			/* column dst cursor for this row */
	move.l	%d6,%d4			/* column counter = wd            */

.Lh_col:
	clr.l	%d0
	move.b	-2(%a4),%d0		/* p0 */
	clr.l	%d1
	move.b	3(%a4),%d1		/* p5 */
	add.w	%d1,%d0			/* d0 = p0+p5 */

	clr.l	%d1
	move.b	-1(%a4),%d1		/* p1 */
	clr.l	%d2
	move.b	2(%a4),%d2		/* p4 */
	add.w	%d2,%d1			/* d1 = p1+p4 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2			/* d2 = 5*(p1+p4) */
	sub.w	%d2,%d0			/* d0 = (p0+p5) - 5*(p1+p4) */

	clr.l	%d1
	move.b	(%a4),%d1		/* p2 */
	clr.l	%d2
	move.b	1(%a4),%d2		/* p3 */
	add.w	%d2,%d1			/* d1 = p2+p3 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2
	lsl.w	#2,%d2			/* d2 = 20*(p2+p3) */
	add.w	%d2,%d0			/* d0 = total */

	add.w	#16,%d0
	asr.w	#5,%d0			/* arithmetic shift: matches C's >> on WORD16 */
	bmi.s	.Lh_clip_lo
	cmp.w	#255,%d0
	ble.s	.Lh_store
	move.w	#255,%d0
	bra.s	.Lh_store
.Lh_clip_lo:
	moveq	#0,%d0
.Lh_store:
	move.b	%d0,(%a5)+
	addq.l	#1,%a4
	subq.l	#1,%d4
	bne.s	.Lh_col

	move.l	48(%sp),%d0		/* src_strd (12+36) */
	adda.l	%d0,%a2
	move.l	52(%sp),%d0		/* dst_strd (16+36) */
	adda.l	%d0,%a3
	subq.l	#1,%d5
	bne.s	.Lh_row

.Lh_done:
	movem.l	(%sp)+,%d2-%d6/%a2-%a5
	rts

/* ---------------------------------------------------------------------- */
/* Vertical: same shape, but taps are at -2*strd/-1*strd/0/+strd/+2*strd/  */
/* +3*strd from a4, so each tap's address is (re)computed with a0 as a    */
/* scratch cursor. Computed fresh from a4 each time (not chained from the */
/* previous tap's address) so a slip in one tap's offset arithmetic can't */
/* propagate into the next.                                               */
/* ---------------------------------------------------------------------- */
mr_ih264_inter_pred_luma_vert_m68k:
	movem.l	%d2-%d6/%a2-%a5,-(%sp)
	move.l	40(%sp),%a2		/* pu1_src  (4+36)  */
	move.l	44(%sp),%a3		/* pu1_dst  (8+36)  */
	move.l	48(%sp),%d3		/* src_strd (12+36) */
	move.l	56(%sp),%d5		/* ht       (20+36) */
	tst.l	%d5
	ble	.Lv_done
	move.l	60(%sp),%d6		/* wd       (24+36) */
	tst.l	%d6
	ble	.Lv_done

.Lv_row:
	movea.l	%a2,%a4			/* column src cursor for this row */
	movea.l	%a3,%a5			/* column dst cursor for this row */
	move.l	%d6,%d4			/* column counter = wd            */

.Lv_col:
	movea.l	%a4,%a0
	suba.l	%d3,%a0
	suba.l	%d3,%a0			/* a0 = p0 addr (a4 - 2*strd) */
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */

	movea.l	%a4,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0			/* a0 = p5 addr (a4 + 3*strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p5 */
	add.w	%d1,%d0			/* d0 = p0+p5 */

	movea.l	%a4,%a0
	suba.l	%d3,%a0			/* a0 = p1 addr (a4 - strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p1 */

	movea.l	%a4,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0			/* a0 = p4 addr (a4 + 2*strd) */
	clr.l	%d2
	move.b	(%a0),%d2		/* p4 */
	add.w	%d2,%d1			/* d1 = p1+p4 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2			/* d2 = 5*(p1+p4) */
	sub.w	%d2,%d0			/* d0 = (p0+p5) - 5*(p1+p4) */

	clr.l	%d1
	move.b	(%a4),%d1		/* p2 */

	movea.l	%a4,%a0
	adda.l	%d3,%a0			/* a0 = p3 addr (a4 + strd) */
	clr.l	%d2
	move.b	(%a0),%d2		/* p3 */
	add.w	%d2,%d1			/* d1 = p2+p3 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2
	lsl.w	#2,%d2			/* d2 = 20*(p2+p3) */
	add.w	%d2,%d0			/* d0 = total */

	add.w	#16,%d0
	asr.w	#5,%d0
	bmi.s	.Lv_clip_lo
	cmp.w	#255,%d0
	ble.s	.Lv_store
	move.w	#255,%d0
	bra.s	.Lv_store
.Lv_clip_lo:
	moveq	#0,%d0
.Lv_store:
	move.b	%d0,(%a5)+
	addq.l	#1,%a4
	subq.l	#1,%d4
	bne.s	.Lv_col

	adda.l	%d3,%a2
	move.l	52(%sp),%d0		/* dst_strd (16+36) */
	adda.l	%d0,%a3
	subq.l	#1,%d5
	bne.s	.Lv_row

.Lv_done:
	movem.l	(%sp)+,%d2-%d6/%a2-%a5
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
