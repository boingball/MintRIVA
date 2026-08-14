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
 * Assembled only when MR_M68K_ASM is enabled, using the same GNU AS syntax
 * and stack-argument-passing convention as MintAMP's
 * amiga_m68k_polyphase.S.  Host builds preprocess this file to an empty
 * translation unit; m68k-amigaos-gcc and the m68k-linux-gnu test toolchain
 * both assemble the real implementation.
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
	.globl	mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k
	.globl	mr_ih264_inter_pred_luma_horz_qpel_m68k
	.globl	mr_ih264_inter_pred_luma_vert_qpel_m68k
	.globl	mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k

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

/* ---------------------------------------------------------------------- */
/* Quarter-pel diagonal: dydx 5/7/13/15 in apf_inter_pred_luma[] all route */
/* here (ih264d_function_selector_generic.c). Ittiam's reference           */
/* (ih264_inter_pred_luma_horz_qpel_vert_qpel in                           */
/* ih264_inter_pred_filters.c) runs the *same* 6-tap filter as horz/vert   */
/* above twice - once vertically from a predictor based at                */
/* pu1_src + (x_offset>>1) columns, once horizontally from a predictor     */
/* based at pu1_src + (y_offset>>1) rows - then averages the two clipped   */
/* 6-tap results with (+1)>>1 rounding. x_offset = dydx&3, y_offset =      */
/* (dydx>>2)&3, so x_offset>>1 and y_offset>>1 are each always exactly 0   */
/* or 1 (never anything else, since offset is a 2-bit field): reducing to  */
/* bit tests on dydx directly (confirmed by exhaustive check over dydx     */
/* 0..15) rather than reproducing the shift/mask C does:                  */
/*   x_offset>>1 == bit 1 of dydx  (dydx=5:0 7:1 13:0 15:1)                */
/*   y_offset>>1 == bit 3 of dydx  (dydx=5:0 7:0 13:1 15:1)                */
/* This is the only one of the three predictors here that reads dydx -    */
/* horz/vert above ignore it because their tap window never depends on    */
/* the sub-pel position within the family they cover.                     */
/*                                                                          */
/* void mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k(UWORD8 *pu1_src, */
/*     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd, WORD32 ht,       */
/*     WORD32 wd, UWORD8 *pu1_tmp, WORD32 dydx);                          */
/*                                                                          */
/* Saves %d2-%d7/%a2-%a6 (11 registers, 44 bytes), so stack offsets below  */
/* are +44 (e.g. pu1_src at 48(%sp) = 4+44). a0/a1/d0/d1 are used as       */
/* scratch without saving (caller-saved by the same convention the        */
/* horz/vert functions above rely on for their tap scratch).               */
/*                                                                          */
/* Register plan: a2/a3 are the pred_vert/pred_horz row bases (each        */
/* pu1_src plus its conditional offset, advanced by src_strd per row);     */
/* a5/a6 are the per-row column cursors for those two predictors (each     */
/* advanced +1 per column); a4/a1 are the dst row base / column cursor;    */
/* a0 is the tap-address scratch used by the vertical 6-tap sub-block,     */
/* exactly as in mr_ih264_inter_pred_luma_vert_m68k above. d3 = src_strd   */
/* (read-only after setup). d7 holds the clipped vertical result while     */
/* d0-d2 are reused for the horizontal 6-tap sub-block's scratch, since    */
/* both sub-filters are needed at once to average.                        */
/* ---------------------------------------------------------------------- */
mr_ih264_inter_pred_luma_horz_qpel_vert_qpel_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	64(%sp),%d5		/* ht        (20+44) */
	tst.l	%d5
	ble	.Lhq_done
	move.l	68(%sp),%d6		/* wd        (24+44) */
	tst.l	%d6
	ble	.Lhq_done

	move.l	56(%sp),%d3		/* src_strd  (12+44) */
	move.l	48(%sp),%a2		/* pred_vert row base = pu1_src (4+44) */
	move.l	48(%sp),%a3		/* pred_horz row base = pu1_src */
	move.l	76(%sp),%d0		/* dydx      (32+44) */
	btst	#1,%d0			/* bit 1 = x_offset>>1 */
	beq.s	.Lhq_no_xoff
	addq.l	#1,%a2
.Lhq_no_xoff:
	btst	#3,%d0			/* bit 3 = y_offset>>1 */
	beq.s	.Lhq_no_yoff
	adda.l	%d3,%a3
.Lhq_no_yoff:
	move.l	52(%sp),%a4		/* pu1_dst row base (8+44) */

.Lhq_row:
	movea.l	%a2,%a5			/* pred_vert column cursor */
	movea.l	%a3,%a6			/* pred_horz column cursor */
	movea.l	%a4,%a1			/* dst column cursor */
	move.l	%d6,%d4			/* column counter = wd */

.Lhq_col:
	/* ---- vertical 6-tap on pred_vert (a5), tap addresses via a0 ---- */
	movea.l	%a5,%a0
	suba.l	%d3,%a0
	suba.l	%d3,%a0			/* a0 = p0 addr (a5 - 2*strd) */
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */

	movea.l	%a5,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0			/* a0 = p5 addr (a5 + 3*strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p5 */
	add.w	%d1,%d0			/* d0 = p0+p5 */

	movea.l	%a5,%a0
	suba.l	%d3,%a0			/* a0 = p1 addr (a5 - strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p1 */

	movea.l	%a5,%a0
	adda.l	%d3,%a0
	adda.l	%d3,%a0			/* a0 = p4 addr (a5 + 2*strd) */
	clr.l	%d2
	move.b	(%a0),%d2		/* p4 */
	add.w	%d2,%d1			/* d1 = p1+p4 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2			/* d2 = 5*(p1+p4) */
	sub.w	%d2,%d0			/* d0 = (p0+p5) - 5*(p1+p4) */

	clr.l	%d1
	move.b	(%a5),%d1		/* p2 */

	movea.l	%a5,%a0
	adda.l	%d3,%a0			/* a0 = p3 addr (a5 + strd) */
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
	bmi.s	.Lhq_vclip_lo
	cmp.w	#255,%d0
	ble.s	.Lhq_vstore
	move.w	#255,%d0
	bra.s	.Lhq_vstore
.Lhq_vclip_lo:
	moveq	#0,%d0
.Lhq_vstore:
	move.w	%d0,%d7			/* stash clipped vertical result */

	/* ---- horizontal 6-tap on pred_horz (a6), constant displacements --- */
	clr.l	%d0
	move.b	-2(%a6),%d0		/* p0 */
	clr.l	%d1
	move.b	3(%a6),%d1		/* p5 */
	add.w	%d1,%d0			/* d0 = p0+p5 */

	clr.l	%d1
	move.b	-1(%a6),%d1		/* p1 */
	clr.l	%d2
	move.b	2(%a6),%d2		/* p4 */
	add.w	%d2,%d1			/* d1 = p1+p4 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2			/* d2 = 5*(p1+p4) */
	sub.w	%d2,%d0			/* d0 = (p0+p5) - 5*(p1+p4) */

	clr.l	%d1
	move.b	(%a6),%d1		/* p2 */
	clr.l	%d2
	move.b	1(%a6),%d2		/* p3 */
	add.w	%d2,%d1			/* d1 = p2+p3 */
	move.w	%d1,%d2
	lsl.w	#2,%d2
	add.w	%d1,%d2
	lsl.w	#2,%d2			/* d2 = 20*(p2+p3) */
	add.w	%d2,%d0			/* d0 = total */

	add.w	#16,%d0
	asr.w	#5,%d0
	bmi.s	.Lhq_hclip_lo
	cmp.w	#255,%d0
	ble.s	.Lhq_hstore
	move.w	#255,%d0
	bra.s	.Lhq_hstore
.Lhq_hclip_lo:
	moveq	#0,%d0
.Lhq_hstore:
	/* d0 = clipped horizontal result, d7 = clipped vertical result */
	add.w	%d7,%d0
	addq.w	#1,%d0
	asr.w	#1,%d0
	move.b	%d0,(%a1)+

	addq.l	#1,%a5
	addq.l	#1,%a6
	subq.l	#1,%d4
	bne	.Lhq_col

	adda.l	%d3,%a2
	adda.l	%d3,%a3
	move.l	60(%sp),%d0		/* dst_strd (16+44) */
	adda.l	%d0,%a4
	subq.l	#1,%d5
	bne	.Lhq_row

.Lhq_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* horz_qpel (dydx 1,3) / vert_qpel (dydx 4,12): a single 6-tap pass       */
/* averaged against a predictor offset by 0 or 1 columns/rows from        */
/* pu1_src (Ittiam's ih264_inter_pred_luma_horz_qpel /                    */
/* ..._vert_qpel in ih264_inter_pred_filters.c). Simpler than the         */
/* diagonal case above: only one predictor, and because it advances in    */
/* lockstep with pu1_src (same +1/+src_strd per column/row), its row base */
/* is computed once at setup - a6 = pu1_src +/- the fixed offset - and    */
/* then walked by src_strd every row exactly like pu1_src itself, rather  */
/* than re-derived from dydx on every row the way the diagonal filter's   */
/* pred_vert/pred_horz needed to be (there both offsets could differ from */
/* each other; here there is only one, so the bit-test happens once).     */
/*                                                                          */
/* void mr_ih264_inter_pred_luma_horz_qpel_m68k(UWORD8 *pu1_src,           */
/*     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd, WORD32 ht,       */
/*     WORD32 wd, UWORD8 *pu1_tmp, WORD32 dydx);                          */
/* void mr_ih264_inter_pred_luma_vert_qpel_m68k(... same signature ...);   */
/*                                                                          */
/* Both save %d2-%d6/%a2-%a6 (10 registers, 40 bytes): stack offsets below */
/* are +40 (pu1_src at 44(%sp) = 4+40). a0/a1 are scratch, no save needed. */
/* ---------------------------------------------------------------------- */
mr_ih264_inter_pred_luma_horz_qpel_m68k:
	movem.l	%d2-%d6/%a2-%a6,-(%sp)
	move.l	60(%sp),%d5		/* ht        (20+40) */
	tst.l	%d5
	ble	.Lhqp_done
	move.l	64(%sp),%d6		/* wd        (24+40) */
	tst.l	%d6
	ble	.Lhqp_done

	move.l	44(%sp),%a2		/* pu1_src   (4+40)  */
	move.l	48(%sp),%a3		/* pu1_dst   (8+40)  */
	movea.l	%a2,%a6			/* pred1 row base = pu1_src + xoff_half */
	move.l	72(%sp),%d0		/* dydx      (32+40) */
	btst	#1,%d0			/* bit 1 = x_offset>>1 */
	beq.s	.Lhqp_no_xoff
	addq.l	#1,%a6
.Lhqp_no_xoff:

.Lhqp_row:
	movea.l	%a2,%a4			/* src column cursor  */
	movea.l	%a3,%a5			/* dst column cursor  */
	movea.l	%a6,%a1			/* pred1 column cursor */
	move.l	%d6,%d4			/* column counter = wd */

.Lhqp_col:
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
	asr.w	#5,%d0
	bmi.s	.Lhqp_clip_lo
	cmp.w	#255,%d0
	ble.s	.Lhqp_store
	move.w	#255,%d0
	bra.s	.Lhqp_store
.Lhqp_clip_lo:
	moveq	#0,%d0
.Lhqp_store:
	clr.l	%d1
	move.b	(%a1),%d1		/* *pred1 */
	add.w	%d1,%d0
	addq.w	#1,%d0
	asr.w	#1,%d0			/* (clipped + *pred1 + 1) >> 1 */
	move.b	%d0,(%a5)+
	addq.l	#1,%a4
	addq.l	#1,%a1
	subq.l	#1,%d4
	bne	.Lhqp_col

	move.l	52(%sp),%d0		/* src_strd (12+40) */
	adda.l	%d0,%a2
	adda.l	%d0,%a6
	move.l	56(%sp),%d0		/* dst_strd (16+40) */
	adda.l	%d0,%a3
	subq.l	#1,%d5
	bne	.Lhqp_row

.Lhqp_done:
	movem.l	(%sp)+,%d2-%d6/%a2-%a6
	rts

mr_ih264_inter_pred_luma_vert_qpel_m68k:
	movem.l	%d2-%d6/%a2-%a6,-(%sp)
	move.l	60(%sp),%d5		/* ht        (20+40) */
	tst.l	%d5
	ble	.Lvqp_done
	move.l	64(%sp),%d6		/* wd        (24+40) */
	tst.l	%d6
	ble	.Lvqp_done

	move.l	44(%sp),%a2		/* pu1_src   (4+40)  */
	move.l	48(%sp),%a3		/* pu1_dst   (8+40)  */
	move.l	52(%sp),%d3		/* src_strd  (12+40) */
	movea.l	%a2,%a6			/* pred1 row base = pu1_src + yoff_half*strd */
	move.l	72(%sp),%d0		/* dydx      (32+40) */
	btst	#3,%d0			/* bit 3 = y_offset>>1 */
	beq.s	.Lvqp_no_yoff
	adda.l	%d3,%a6
.Lvqp_no_yoff:

.Lvqp_row:
	movea.l	%a2,%a4			/* src column cursor  */
	movea.l	%a3,%a5			/* dst column cursor  */
	movea.l	%a6,%a1			/* pred1 column cursor */
	move.l	%d6,%d4			/* column counter = wd */

.Lvqp_col:
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
	bmi.s	.Lvqp_clip_lo
	cmp.w	#255,%d0
	ble.s	.Lvqp_store
	move.w	#255,%d0
	bra.s	.Lvqp_store
.Lvqp_clip_lo:
	moveq	#0,%d0
.Lvqp_store:
	clr.l	%d1
	move.b	(%a1),%d1		/* *pred1 */
	add.w	%d1,%d0
	addq.w	#1,%d0
	asr.w	#1,%d0			/* (clipped + *pred1 + 1) >> 1 */
	move.b	%d0,(%a5)+
	addq.l	#1,%a4
	addq.l	#1,%a1
	subq.l	#1,%d4
	bne	.Lvqp_col

	adda.l	%d3,%a2
	adda.l	%d3,%a6
	move.l	56(%sp),%d0		/* dst_strd (16+40) */
	adda.l	%d0,%a3
	subq.l	#1,%d5
	bne	.Lvqp_row

.Lvqp_done:
	movem.l	(%sp)+,%d2-%d6/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* horz_hpel_vert_hpel (dydx == 10): the true diagonal half-pel position - */
/* the one 2D filter in this table that is a genuine two-stage separable   */
/* 6-tap (Ittiam's ih264_inter_pred_luma_horz_hpel_vert_hpel in            */
/* ih264_inter_pred_filters.c), not a single 6-tap averaged against an     */
/* offset predictor like every function above. Stage 1 runs the vertical   */
/* 6-tap (same {1,-5,20} coefficients, byte taps) at every column from -2  */
/* to wd+2 inclusive (wd+5 columns) into a WORD16 scratch buffer (pu1_tmp, */
/* row pitch (wd+5) elements) as a *raw* sum with no shift/round/clip -    */
/* fits a signed 16-bit word for the same reason the leaf horz_m68k/       */
/* vert_m68k filters above do (range [-2550,10726], byte inputs). Stage 2  */
/* runs the horizontal 6-tap over *those* 16-bit intermediates: six taps   */
/* each up to +-10726 with coefficients up to 20 can reach roughly         */
/* +-560000, well past a 16-bit word, so - unlike every filter above -     */
/* stage 2's accumulation is done in .l (32-bit), sign-extending each      */
/* 16-bit tap via ext.l before combining, only narrowing back down after   */
/* the final (+512)>>10 combined-shift and CLIP_U8.                        */
/*                                                                          */
/* void mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k(UWORD8 *pu1_src, */
/*     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd, WORD32 ht,       */
/*     WORD32 wd, UWORD8 *pu1_tmp, WORD32 dydx);                          */
/*                                                                          */
/* dydx is unused (like horz_m68k/vert_m68k - this is the one fixed        */
/* diagonal position, no sub-pel routing needed). Saves %d2-%d7/%a2-%a6    */
/* (11 registers, 44 bytes): stack offsets below are +44 (pu1_src at       */
/* 48(%sp) = 4+44). a0/a1 are scratch, no save needed.                     */
/*                                                                          */
/* Register plan, stage 1: a2 = pu1_src row base, a3 = temp row base,      */
/* a4 = src column cursor (starts at column -2), a5 = temp column cursor,  */
/* a0 = tap-address scratch (as in vert_m68k above), d2 = src_strd,        */
/* d3 = row pitch in bytes ((wd+5)*2, needed by both stages so computed    */
/* once up front), d4 = column counter, d5 = ht, d6 = wd, a6 = the         */
/* original pu1_tmp pointer, kept untouched so stage 2 can rederive its    */
/* row base from it. Stage 2 reuses a2 (now pu1_dst row base), a3 (now     */
/* pred1 row base = pu1_tmp + 2 WORD16 elements, matching Ittiam's         */
/* pi2_pred1_temp += 2), a4 (pred1 column cursor), a5 (dst column cursor); */
/* d0/d1/d7 are the .l tap-computation scratch (d2 and d6 are free again   */
/* by then, but d7 avoids clobbering wd mid-row for no reason).            */
/* ---------------------------------------------------------------------- */
mr_ih264_inter_pred_luma_horz_hpel_vert_hpel_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	64(%sp),%d5		/* ht        (20+44) */
	tst.l	%d5
	ble	.Lhh_done
	move.l	68(%sp),%d6		/* wd        (24+44) */
	tst.l	%d6
	ble	.Lhh_done

	move.l	72(%sp),%a6		/* pu1_tmp   (28+44) */
	move.l	%d6,%d3
	addq.l	#5,%d3
	lsl.l	#1,%d3			/* d3 = row pitch bytes = (wd+5)*2 */

	/* ---- stage 1: vertical raw 6-tap sums -> WORD16 temp buffer ---- */
	move.l	48(%sp),%a2		/* pu1_src   (4+44)  */
	movea.l	%a6,%a3			/* temp row base */
	move.l	56(%sp),%d2		/* src_strd  (12+44) */

.Lhh_s1_row:
	movea.l	%a2,%a4
	suba.l	#2,%a4			/* a4 = row base - 2 (column -2) */
	movea.l	%a3,%a5			/* temp column cursor */
	move.l	%d6,%d4
	addq.l	#5,%d4			/* column counter = wd+5 */

.Lhh_s1_col:
	movea.l	%a4,%a0
	suba.l	%d2,%a0
	suba.l	%d2,%a0			/* a0 = p0 addr (a4 - 2*strd) */
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */

	movea.l	%a4,%a0
	adda.l	%d2,%a0
	adda.l	%d2,%a0
	adda.l	%d2,%a0			/* a0 = p5 addr (a4 + 3*strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p5 */
	add.w	%d1,%d0			/* d0 = p0+p5 */

	movea.l	%a4,%a0
	suba.l	%d2,%a0			/* a0 = p1 addr (a4 - strd) */
	clr.l	%d1
	move.b	(%a0),%d1		/* p1 */

	movea.l	%a4,%a0
	adda.l	%d2,%a0
	adda.l	%d2,%a0			/* a0 = p4 addr (a4 + 2*strd) */
	clr.l	%d7
	move.b	(%a0),%d7		/* p4 */
	add.w	%d7,%d1			/* d1 = p1+p4 */
	move.w	%d1,%d7
	lsl.w	#2,%d7
	add.w	%d1,%d7			/* d7 = 5*(p1+p4) */
	sub.w	%d7,%d0			/* d0 = (p0+p5) - 5*(p1+p4) */

	clr.l	%d1
	move.b	(%a4),%d1		/* p2 */

	movea.l	%a4,%a0
	adda.l	%d2,%a0			/* a0 = p3 addr (a4 + strd) */
	clr.l	%d7
	move.b	(%a0),%d7		/* p3 */
	add.w	%d7,%d1			/* d1 = p2+p3 */
	move.w	%d1,%d7
	lsl.w	#2,%d7
	add.w	%d1,%d7
	lsl.w	#2,%d7			/* d7 = 20*(p2+p3) */
	add.w	%d7,%d0			/* d0 = raw vertical 6-tap sum */

	move.w	%d0,(%a5)+
	addq.l	#1,%a4
	subq.l	#1,%d4
	bne	.Lhh_s1_col

	adda.l	%d2,%a2
	adda.l	%d3,%a3
	subq.l	#1,%d5
	bne	.Lhh_s1_row

	/* ---- stage 2: horizontal 6-tap over the WORD16 intermediates ---- */
	move.l	64(%sp),%d5		/* ht (reload - stage 1 counted it down) */
	move.l	52(%sp),%a2		/* pu1_dst   (8+44)  */
	movea.l	%a6,%a3
	adda.l	#4,%a3			/* pred1 row base = pu1_tmp + 2 WORD16s */

.Lhh_s2_row:
	movea.l	%a2,%a5			/* dst column cursor   */
	movea.l	%a3,%a4			/* pred1 column cursor */
	move.l	%d6,%d4			/* column counter = wd */

.Lhh_s2_col:
	move.w	-4(%a4),%d0
	ext.l	%d0
	move.w	6(%a4),%d1
	ext.l	%d1
	add.l	%d1,%d0			/* d0 = pred1[col-2]+pred1[col+3] */

	move.w	-2(%a4),%d1
	ext.l	%d1
	move.w	4(%a4),%d7
	ext.l	%d7
	add.l	%d7,%d1			/* d1 = pred1[col-1]+pred1[col+2] */
	move.l	%d1,%d7
	asl.l	#2,%d7
	add.l	%d1,%d7			/* d7 = 5*(...) */
	sub.l	%d7,%d0			/* d0 -= 5*(...) */

	move.w	(%a4),%d1
	ext.l	%d1
	move.w	2(%a4),%d7
	ext.l	%d7
	add.l	%d7,%d1			/* d1 = pred1[col]+pred1[col+1] */
	move.l	%d1,%d7
	asl.l	#2,%d7
	add.l	%d1,%d7
	asl.l	#2,%d7			/* d7 = 20*(...) */
	add.l	%d7,%d0			/* d0 = total (.l) */

	add.l	#512,%d0
	moveq	#10,%d1			/* m68k ASR immediate counts only go 1-8; */
	asr.l	%d1,%d0			/* combined 1/32 * 1/32 shift, with rounding */
	bmi.s	.Lhh_clip_lo
	cmp.l	#255,%d0
	ble.s	.Lhh_store
	move.l	#255,%d0
	bra.s	.Lhh_store
.Lhh_clip_lo:
	moveq	#0,%d0
.Lhh_store:
	move.b	%d0,(%a5)+
	adda.l	#2,%a4
	subq.l	#1,%d4
	bne	.Lhh_s2_col

	move.l	60(%sp),%d0		/* dst_strd (16+44) */
	adda.l	%d0,%a2
	adda.l	%d3,%a3
	subq.l	#1,%d5
	bne	.Lhh_s2_row

.Lhh_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
