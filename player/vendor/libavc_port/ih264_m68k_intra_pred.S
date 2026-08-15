/*
 * Hand-written m68k assembly for H.264's luma 16x16 "Plane" intra
 * prediction mode (spec 8.3.3.4, Ittiam's
 * ih264_intra_pred_luma_16x16_mode_plane in ih264_luma_intra_pred_filters.c).
 *
 * Unlike the vert/horz/dc luma 16x16 predictors in ih264_m68k_optim.c
 * (ordinary C - see that file's header comment for why), plane's per-pixel
 * body is a serial `val = CLIP_U8((tmp + tmpx) >> 5)` with no 4-bytes-at-
 * once opportunity (clipping breaks byte-parallel tricks without real
 * SIMD), so real hand asm is the same call this project already made for
 * the interpolation filters and iquant/itrans/recon - see those files.
 *
 * Ittiam's own C is written "with no multiplications" (a hand-unrolled
 * shift-add chain, since some historical target apparently lacked a cheap
 * multiplier). m68k has had a hardware multiplier since the 68000, and
 * this project already uses MULS.L elsewhere (ih264_m68k_iquant_itrans_
 * recon.S), so this port does NOT transcribe that shift-add chain -
 * instead it uses the mathematically equivalent (integer multiply by a
 * constant 1..8 has exactly one correct value regardless of how it's
 * computed) closed form, re-derived directly from the vendored pointer
 * arithmetic:
 *
 *   H = sum_{x=1}^{7} x * (top[7+x] - top[7-x]) + 8 * (top[15] - topleft)
 *   V = sum_{y=1}^{7} y * (left[-(7+y)] - left[-(7-y)]) + 8 * (left[-15] - topleft)
 *   a = 16 * (top[15] + left[-15])
 *   b = (5*H + 32) >> 6
 *   c = (5*V + 32) >> 6
 *   pred(row, col) = CLIP_U8((a + b*(col-7) + c*(row-7) + 16) >> 5)
 *
 * (`top`/`left`/`topleft` and the p(x,y) indexing match
 * ih264_intra_pred_luma_16x16_mode_vert/horz's pointer bases: pu1_top =
 * pu1_src+17, pu1_left = pu1_src+15, pu1_topleft = pu1_src+16.) Since
 * x/y only ever range 1..8, every term below is addressed as a fixed
 * constant displacement from pu1_src rather than via a loop - top[7+x] =
 * *(pu1_src+24+x), top[7-x] = *(pu1_src+24-x), left[-(7+y)] =
 * *(pu1_src+8-y), left[-(7-y)] = *(pu1_src+8+y), top[15] = *(pu1_src+32),
 * left[-15] = *pu1_src, topleft = *(pu1_src+16) - all verified against the
 * vendored code's own pointer walk term-by-term (see the PR description
 * for the derivation) and cross-checked by the fuzz test's independent
 * reference, which instead transcribes Ittiam's literal shift-add chain
 * (so a mistake in this derivation and a mistake in that transcription
 * would have to agree to hide a bug).
 *
 * The row/col fill loop mirrors Ittiam's own incremental form (`tmp += c`
 * once per row, `tmpx += b` once per col) rather than multiplying b/c by
 * (col-7)/(row-7) per pixel - avoids a multiply on the actual 256-pixel
 * hot loop, only the one-time H/V/a/b/c setup above uses MULS.L.
 *
 * void mr_ih264_intra_pred_luma_16x16_plane_m68k(UWORD8 *pu1_src,
 *     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd,
 *     WORD32 ngbr_avail);
 *
 * src_strd and ngbr_avail are UNUSED (UNUSED() in the vendored C too -
 * plane's inputs are always the fixed 33-byte neighbour buffer laid out
 * around pu1_src, plane is never invoked without both neighbours
 * available). Saves %d2-%d7/%a2-%a3 (32 bytes): pu1_src at 36(%sp) (4+32),
 * pu1_dst at 40(%sp) (8+32), dst_strd at 48(%sp) (16+32).
 *
 * Register plan: %a2 = pu1_src (base for every H/V/a offset below), %a3 =
 * pu1_dst row cursor. %d6 = b, %d7 = c (persistent for the whole
 * function once computed). %d0 = tmp (the row accumulator, persistent
 * across the row loop, += c each row). %d1 = tmpx (the column
 * accumulator, reset from -8b each row, += b each col). %d2 = column
 * counter, %d3 = dst_strd (persistent), %d4 = row counter, %d5 = per-
 * pixel clip scratch (and holds `a` transiently during setup, before the
 * row loop starts) - all 8 data registers are live simultaneously during
 * the fill loop, no spares needed or available.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_intra_pred_luma_16x16_plane_m68k

mr_ih264_intra_pred_luma_16x16_plane_m68k:
	movem.l	%d2-%d7/%a2-%a3,-(%sp)
	move.l	36(%sp),%a2		/* pu1_src   (4+32)  */
	move.l	40(%sp),%a3		/* pu1_dst   (8+32)  */

	/* ---- H = sum x=1..7 of x*(top[7+x]-top[7-x]) + 8*(top[15]-topleft) */
	clr.l	%d1
	move.b	25(%a2),%d1
	clr.l	%d2
	move.b	23(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* H, x=1 term (coeff 1) */

	clr.l	%d1
	move.b	26(%a2),%d1
	clr.l	%d2
	move.b	22(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#2,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	27(%a2),%d1
	clr.l	%d2
	move.b	21(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	28(%a2),%d1
	clr.l	%d2
	move.b	20(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	29(%a2),%d1
	clr.l	%d2
	move.b	19(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#5,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	30(%a2),%d1
	clr.l	%d2
	move.b	18(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#6,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	31(%a2),%d1
	clr.l	%d2
	move.b	17(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#7,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	32(%a2),%d1		/* top[15] */
	clr.l	%d2
	move.b	16(%a2),%d2		/* topleft */
	sub.l	%d2,%d1
	muls.l	#8,%d1
	add.l	%d1,%d0			/* d0 = H */

	move.l	%d0,%d1
	muls.l	#5,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d6			/* d6 = b */

	/* ---- V = sum y=1..7 of y*(left[-(7+y)]-left[-(7-y)])
	 *          + 8*(left[-15]-topleft) ---- */
	clr.l	%d1
	move.b	7(%a2),%d1
	clr.l	%d2
	move.b	9(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* V, y=1 term (coeff 1) */

	clr.l	%d1
	move.b	6(%a2),%d1
	clr.l	%d2
	move.b	10(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#2,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	5(%a2),%d1
	clr.l	%d2
	move.b	11(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	4(%a2),%d1
	clr.l	%d2
	move.b	12(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	3(%a2),%d1
	clr.l	%d2
	move.b	13(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#5,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	2(%a2),%d1
	clr.l	%d2
	move.b	14(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#6,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	1(%a2),%d1
	clr.l	%d2
	move.b	15(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#7,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	(%a2),%d1		/* left[-15] = *pu1_src */
	clr.l	%d2
	move.b	16(%a2),%d2		/* topleft */
	sub.l	%d2,%d1
	muls.l	#8,%d1
	add.l	%d1,%d0			/* d0 = V */

	move.l	%d0,%d1
	muls.l	#5,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d7			/* d7 = c */

	/* ---- a = 16*(top[15] + left[-15]); tmp = a - 8c + 16 ---- */
	clr.l	%d5
	move.b	32(%a2),%d5		/* top[15] */
	clr.l	%d1
	move.b	(%a2),%d1		/* left[-15] */
	add.l	%d1,%d5
	lsl.l	#4,%d5			/* d5 = a */

	move.l	%d7,%d1
	asl.l	#3,%d1			/* d1 = 8c */
	sub.l	%d1,%d5
	add.l	#16,%d5
	move.l	%d5,%d0			/* d0 = tmp = a - 8c + 16 */

	move.l	48(%sp),%d3		/* dst_strd  (16+32) */
	moveq	#16,%d4			/* row counter */

.Lplane_row:
	add.l	%d7,%d0			/* tmp += c */
	move.l	%d6,%d1
	asl.l	#3,%d1			/* d1 = 8b */
	neg.l	%d1			/* d1 = tmpx = -8b */

	moveq	#16,%d2			/* column counter */
.Lplane_col:
	add.l	%d6,%d1			/* tmpx += b */
	move.l	%d0,%d5
	add.l	%d1,%d5
	asr.l	#5,%d5
	bmi.s	.Lplane_clip_lo
	cmp.l	#255,%d5
	ble.s	.Lplane_store
	move.l	#255,%d5
	bra.s	.Lplane_store
.Lplane_clip_lo:
	moveq	#0,%d5
.Lplane_store:
	move.b	%d5,(%a3)+
	subq.l	#1,%d2
	bne.s	.Lplane_col

	suba.l	#16,%a3			/* back to this row's start ... */
	adda.l	%d3,%a3			/* ... then += dst_strd */
	subq.l	#1,%d4
	bne	.Lplane_row

	movem.l	(%sp)+,%d2-%d7/%a2-%a3
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
