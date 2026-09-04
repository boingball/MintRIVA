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
	add.l	%d1,%d1
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
	lsl.l	#3,%d1
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
	add.l	%d1,%d1
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
	lsl.l	#3,%d1
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

/* ---------------------------------------------------------------------- */
/* H.264 luma 8x8 intra prediction (spec 8.3.2.2), Ittiam's ih264_intra_
 * pred_luma_8x8_mode_*() in ih264_luma_intra_pred_filters.c. Six of the
 * nine modes below (diag_dl/diag_dr/vert_r/horz_d/vert_l/horz_u) are
 * genuine multi-tap FILT121/FILT11 shuffles - no 4-bytes-at-once
 * opportunity, same reasoning as 16x16 Plane above - so these are real
 * hand asm. The other three (vert/horz/dc) are simple broadcasts, ported
 * as ordinary C in ih264_m68k_optim.c instead (see that file - same
 * treatment as the 16x16/4x4 vert/horz/dc predictors there).
 *
 * All nine modes (and the reference-sample filtering step below) share
 * one neighbour-buffer layout, `pu1_src` (called `S[]` in these comments):
 * S[0..7]   = left column, REVERSED (S[7]=left[0], nearest topleft;
 *             S[0]=left[7], farthest down)
 * S[8]      = topleft
 * S[9..24]  = top row + top-right (16 bytes: S[9]=top[0] .. S[24]=top[15])
 * (pu1_top = pu1_src+9, pu1_left = pu1_src+7, pu1_topleft = pu1_src+8 -
 * exactly the vendored C's own pointer bases, BLK8x8SIZE=8.)
 *
 * FILT121(a,b,c) = (a + 2b + c + 2) >> 2, FILT11(a,b) = (a + b + 1) >> 1
 * (ih264_intra_pred_filters.h) are factored into two tiny register-only
 * leaf subroutines (.Lip8_filt121/.Lip8_filt11, no %sp access at all) -
 * deliberately NOT following ih264_m68k_iquant_itrans_recon.S's
 * .Lc8r_idct1d pattern of a bsr-called subroutine reading/writing its
 * caller's %sp-relative locals frame, which needed a since-documented +4
 * correction for the return address bsr itself pushes; keeping these
 * two subroutines strictly register-in/register-out sidesteps that whole
 * class of bug rather than re-deriving the correction here too.
 *
 * Each of the six filtered modes below re-derives its predicted_pixels[]
 * entries as direct S[] index formulas (verified against the vendored
 * C's own pointer walk point-by-point, not pattern-matched by eyeballing
 * the shape) rather than copying the C's named ui4_a..ui4_q intermediate
 * variables - the win is that a run of entries sharing the same "3
 * consecutive S[] elements, window sliding by 1 or 2" shape becomes one
 * small indexed-addressing loop instead of 15-22 hand-picked-offset
 * instruction groups. Verified end to end by a differential fuzz test
 * against the real vendored functions (tests/mr_h264_intra8x8_check.c),
 * not just by this derivation being self-consistent.
 */

/* ---------------------------------------------------------------------- */
/* .Lip8_filt121 / .Lip8_filt11: FILT121/FILT11 above. IN: %d0=a, %d1=b,
 * (%d2=c for filt121 only). OUT: %d0=result. Clobbers %d0/%d3 only (filt121)
 * or %d0 only (filt11) - %d1/%d2 survive unread-back. bsr-callable, leaf,
 * no %sp access. Inputs are zero-extended UWORD8 values (0..255); FILT121's
 * max possible sum is 255+510+255+2=1022, FILT11's is 511 - both comfortably
 * within a 32-bit register, no truncation concerns unlike the WORD16
 * dequant path in ih264_m68k_iquant_itrans_recon.S.
 * ---------------------------------------------------------------------- */
.Lip8_filt121:
	move.l	%d1,%d3
	add.l	%d3,%d3
	add.l	%d0,%d3
	add.l	%d2,%d3
	add.l	#2,%d3
	asr.l	#2,%d3
	move.l	%d3,%d0
	rts

.Lip8_filt11:
	add.l	%d1,%d0
	add.l	#1,%d0
	asr.l	#1,%d0
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_diag_dl_m68k(UWORD8 *pu1_src,
 *     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd, WORD32 ngbr_avail);
 * src_strd/ngbr_avail UNUSED() in the vendored C too.
 *
 * predicted_pixels[i] = FILT121(top[i],top[i+1],top[i+2]) for i=0..13 -
 * i.e. FILT121(S[9+i],S[10+i],S[11+i]), a simple ascending 3-window - plus
 * one special case, predicted_pixels[14] = FILT121(top[14],top[15],top[15])
 * (the vendored C's own final entry duplicates the last tap: `FILT121
 * (ui4_o, ui4_p, ui4_p)`). Output: row r (0..7) = predicted_pixels[r..r+7].
 *
 * Saves %d2-%d4/%a2-%a4 (24 bytes) - args are read at these pre-sub
 * offsets first, then a 16-byte local scratch for predicted_pixels[0..14]
 * (15 bytes used, one pad byte) is allocated: 28(%sp)=pu1_src,
 * 32(%sp)=pu1_dst, 40(%sp)=dst_strd (36=src_strd and 44=ngbr_avail
 * unused, never read).
 * Register plan: %a2=pu1_src (S[] base), %a3=pu1_dst row cursor,
 * %a4=dst_strd (value). %d4 = loop index, doubling as both the S[]-window
 * offset and the predicted_pixels[] write offset (indexed addressing,
 * `9(%a2,%d4.l)` / `(%sp,%d4.l)`) since both advance in lockstep.
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_diag_dl_m68k
mr_ih264_intra_pred_luma_8x8_diag_dl_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src   (4+24, before the local sub) */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#16,%sp

	clr.l	%d4
.Ldl_loopA:
	clr.l	%d0
	move.b	9(%a2,%d4.l),%d0	/* top[i]   = S[9+i]  */
	clr.l	%d1
	move.b	10(%a2,%d4.l),%d1	/* top[i+1] = S[10+i] */
	clr.l	%d2
	move.b	11(%a2,%d4.l),%d2	/* top[i+2] = S[11+i] */
	bsr	.Lip8_filt121
	move.b	%d0,(%sp,%d4.l)
	addq.l	#1,%d4
	cmp.l	#14,%d4
	blt	.Ldl_loopA

	clr.l	%d0
	move.b	23(%a2),%d0		/* top[14] = S[23] */
	clr.l	%d1
	move.b	24(%a2),%d1		/* top[15] = S[24] */
	move.l	%d1,%d2			/* duplicate last tap */
	bsr	.Lip8_filt121
	move.b	%d0,14(%sp)

	clr.l	%d4
.Ldl_row:
	move.l	(%sp,%d4.l),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp,%d4.l),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3
	addq.l	#1,%d4
	cmp.l	#8,%d4
	blt	.Ldl_row

	add.l	#16,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_diag_dr_m68k(...) - same 5-arg
 * signature as diag_dl, same UNUSED() args.
 *
 * predicted_pixels[k] = FILT121(S[k+2],S[k+1],S[k]) for k=0..7 (descending
 * 3-window - note the *reversed* argument order vs. the ascending case
 * below: this one reads "high, mid, low"), then predicted_pixels[8+m] =
 * FILT121(S[8+m],S[9+m],S[10+m]) for m=0..6 (ascending 3-window). Both
 * re-derived and individually verified against the vendored C's 15
 * explicit ui4_*-based assignments (see this file's own PR description
 * for the point-by-point check) - not assumed from diag_dl's shape, since
 * dr's descending half uses the opposite tap order.
 *
 * Output: row r (0..7) = predicted_pixels[(7-r)..(14-r)] (`memcpy(pu1_dst
 * + r*dst_strd, predicted_pixels + 7 - r, 8)` in the vendored C).
 *
 * Same locals frame shape as diag_dl (24-byte save + 16-byte scratch,
 * same pre-sub arg-offset base) - predicted_pixels[0..14] at %sp+0..14.
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_diag_dr_m68k
mr_ih264_intra_pred_luma_8x8_diag_dr_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#16,%sp

	clr.l	%d4
.Ldr_loopA:
	clr.l	%d0
	move.b	2(%a2,%d4.l),%d0	/* S[k+2] */
	clr.l	%d1
	move.b	1(%a2,%d4.l),%d1	/* S[k+1] */
	clr.l	%d2
	move.b	0(%a2,%d4.l),%d2	/* S[k]   */
	bsr	.Lip8_filt121
	move.b	%d0,(%sp,%d4.l)
	addq.l	#1,%d4
	cmp.l	#8,%d4
	blt	.Ldr_loopA

	clr.l	%d4
.Ldr_loopB:
	clr.l	%d0
	move.b	8(%a2,%d4.l),%d0	/* S[8+m] */
	clr.l	%d1
	move.b	9(%a2,%d4.l),%d1	/* S[9+m] */
	clr.l	%d2
	move.b	10(%a2,%d4.l),%d2	/* S[10+m] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	#8,%d3
	move.b	%d0,(%sp,%d3.l)
	addq.l	#1,%d4
	cmp.l	#7,%d4
	blt	.Ldr_loopB

	moveq	#7,%d4
.Ldr_row:
	move.l	(%sp,%d4.l),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp,%d4.l),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3
	subq.l	#1,%d4
	bpl	.Ldr_row

	add.l	#16,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_vert_r_m68k(...) - same 5-arg
 * signature, same UNUSED() args as diag_dl/dr.
 *
 * predicted_pixels[] (22 entries) re-derived from the vendored C's 22
 * explicit ui4_*-based assignments as direct S[] formulas:
 *   pp[0]  = FILT121(S2,S3,S4)          pp[11] = FILT121(S1,S2,S3)
 *   pp[1]  = FILT121(S4,S5,S6)          pp[12] = FILT121(S3,S4,S5)
 *   pp[2]  = FILT121(S6,S7,S8)          pp[13] = FILT121(S5,S6,S7)
 *   pp[3+t]  = FILT11(S[8+t],S[9+t])      for t=0..7  (ascending window)
 *   pp[14] = FILT121(S9,S8,S7)          (standalone - descending order)
 *   pp[15+t] = FILT121(S[8+t],S[9+t],S[10+t]) for t=0..6 (ascending window)
 * Output rows use the literal, non-arithmetic pp offset sequence
 * 3,14,2,13,1,12,0,11 (`memcpy(pu1_dst, predicted_pixels+3,8)`, `+dst_strd,
 * +14`, `+2*dst_strd,+2`, ... in the vendored C) - written out as 8
 * explicit row copies below rather than forced into a loop.
 *
 * Saves %d2-%d4/%a2-%a4 (24 bytes) then a 24-byte local scratch for
 * predicted_pixels[0..21] (22 bytes used, two pad bytes) - same pre-sub
 * arg-offset base as diag_dl/dr (28=pu1_src, 32=pu1_dst, 40=dst_strd).
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_vert_r_m68k
mr_ih264_intra_pred_luma_8x8_vert_r_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#24,%sp

	clr.l	%d0
	move.b	2(%a2),%d0
	clr.l	%d1
	move.b	3(%a2),%d1
	clr.l	%d2
	move.b	4(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,0(%sp)		/* pp[0] */

	clr.l	%d0
	move.b	4(%a2),%d0
	clr.l	%d1
	move.b	5(%a2),%d1
	clr.l	%d2
	move.b	6(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,1(%sp)		/* pp[1] */

	clr.l	%d0
	move.b	6(%a2),%d0
	clr.l	%d1
	move.b	7(%a2),%d1
	clr.l	%d2
	move.b	8(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,2(%sp)		/* pp[2] */

	clr.l	%d4
.Lvr_loopA:
	clr.l	%d0
	move.b	8(%a2,%d4.l),%d0	/* S[8+t] */
	clr.l	%d1
	move.b	9(%a2,%d4.l),%d1	/* S[9+t] */
	bsr	.Lip8_filt11
	move.l	%d4,%d3
	add.l	#3,%d3
	move.b	%d0,(%sp,%d3.l)		/* pp[3+t] */
	addq.l	#1,%d4
	cmp.l	#8,%d4
	blt	.Lvr_loopA

	clr.l	%d0
	move.b	1(%a2),%d0
	clr.l	%d1
	move.b	2(%a2),%d1
	clr.l	%d2
	move.b	3(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,11(%sp)		/* pp[11] */

	clr.l	%d0
	move.b	3(%a2),%d0
	clr.l	%d1
	move.b	4(%a2),%d1
	clr.l	%d2
	move.b	5(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,12(%sp)		/* pp[12] */

	clr.l	%d0
	move.b	5(%a2),%d0
	clr.l	%d1
	move.b	6(%a2),%d1
	clr.l	%d2
	move.b	7(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,13(%sp)		/* pp[13] */

	clr.l	%d0
	move.b	9(%a2),%d0
	clr.l	%d1
	move.b	8(%a2),%d1
	clr.l	%d2
	move.b	7(%a2),%d2
	bsr	.Lip8_filt121
	move.b	%d0,14(%sp)		/* pp[14] */

	clr.l	%d4
.Lvr_loopB:
	clr.l	%d0
	move.b	8(%a2,%d4.l),%d0	/* S[8+t] */
	clr.l	%d1
	move.b	9(%a2,%d4.l),%d1	/* S[9+t] */
	clr.l	%d2
	move.b	10(%a2,%d4.l),%d2	/* S[10+t] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	#15,%d3
	move.b	%d0,(%sp,%d3.l)		/* pp[15+t] */
	addq.l	#1,%d4
	cmp.l	#7,%d4
	blt	.Lvr_loopB

	/* Output rows use the literal pp-offset sequence 3,14,2,13,1,12,0,11 -
	 * not an arithmetic progression, so these are 8 explicit row copies. */
	move.l	3(%sp),%d0
	move.l	%d0,(%a3)
	move.l	7(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	14(%sp),%d0
	move.l	%d0,(%a3)
	move.l	18(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	2(%sp),%d0
	move.l	%d0,(%a3)
	move.l	6(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	13(%sp),%d0
	move.l	%d0,(%a3)
	move.l	17(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	1(%sp),%d0
	move.l	%d0,(%a3)
	move.l	5(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	12(%sp),%d0
	move.l	%d0,(%a3)
	move.l	16(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	0(%sp),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	11(%sp),%d0
	move.l	%d0,(%a3)
	move.l	15(%sp),%d0
	move.l	%d0,4(%a3)

	add.l	#24,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_horz_d_m68k(...) - same 5-arg
 * signature, same UNUSED() args.
 *
 * pp[2t]   = FILT11(S[1+t],S[t])              for t=0..7  (8 entries)
 * pp[2t+1] = FILT121(S[2+t],S[1+t],S[t])       for t=0..6  (7 entries)
 * pp[15+m] = FILT121(S[9+m],S[8+m],S[10+m]...  -- see below, DESCENDING
 *            arg order: FILT121(S[9+m],S[8+m],S[7+m]) for m=0..6
 * Re-derived from the vendored C's 22 explicit assignments (see this
 * file's own PR description for the point-by-point check) - unlike
 * vert_r above, output row r uses a clean arithmetic pp offset
 * (14 - 2*r for r=0..7), so this one IS a loop.
 *
 * Same locals frame shape as vert_r (24-byte save + 24-byte scratch,
 * same pre-sub arg-offset base).
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_horz_d_m68k
mr_ih264_intra_pred_luma_8x8_horz_d_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#24,%sp

	clr.l	%d4
.Lhd_loopA:
	clr.l	%d0
	move.b	1(%a2,%d4.l),%d0	/* S[1+t] */
	clr.l	%d1
	move.b	0(%a2,%d4.l),%d1	/* S[t]   */
	bsr	.Lip8_filt11
	move.l	%d4,%d3
	add.l	%d3,%d3			/* 2t */
	move.b	%d0,(%sp,%d3.l)		/* pp[2t] */
	addq.l	#1,%d4
	cmp.l	#8,%d4
	blt	.Lhd_loopA

	clr.l	%d4
.Lhd_loopB:
	clr.l	%d0
	move.b	2(%a2,%d4.l),%d0	/* S[2+t] */
	clr.l	%d1
	move.b	1(%a2,%d4.l),%d1	/* S[1+t] */
	clr.l	%d2
	move.b	0(%a2,%d4.l),%d2	/* S[t]   */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	%d3,%d3			/* 2t */
	addq.l	#1,%d3			/* 2t+1 */
	move.b	%d0,(%sp,%d3.l)		/* pp[2t+1] */
	addq.l	#1,%d4
	cmp.l	#7,%d4
	blt	.Lhd_loopB

	clr.l	%d4
.Lhd_loopC:
	clr.l	%d0
	move.b	9(%a2,%d4.l),%d0	/* S[9+m] */
	clr.l	%d1
	move.b	8(%a2,%d4.l),%d1	/* S[8+m] */
	clr.l	%d2
	move.b	7(%a2,%d4.l),%d2	/* S[7+m] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	#15,%d3
	move.b	%d0,(%sp,%d3.l)		/* pp[15+m] */
	addq.l	#1,%d4
	cmp.l	#7,%d4
	blt	.Lhd_loopC

	moveq	#14,%d4
.Lhd_row:
	move.l	(%sp,%d4.l),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp,%d4.l),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3
	subq.l	#2,%d4
	bpl	.Lhd_row

	add.l	#24,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_vert_l_m68k(...) - same 5-arg
 * signature, same UNUSED() args.
 *
 * pp[t]    = FILT11(S[9+t],S[10+t])           for t=0..10 (11 entries)
 * pp[11+t] = FILT121(S[9+t],S[10+t],S[11+t])  for t=0..10 (11 entries)
 * Re-derived from the vendored C's 22 explicit assignments. Output rows
 * deinterleave: even rows 0,2,4,6 use pp[0,1,2,3], odd rows 1,3,5,7 use
 * pp[11,12,13,14] - written as 8 explicit row copies (matching vert_r's
 * treatment of its own non-arithmetic case) rather than forced into one
 * loop with a branch.
 *
 * Same locals frame shape as vert_r/horz_d (24-byte save + 24-byte
 * scratch, same pre-sub arg-offset base).
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_vert_l_m68k
mr_ih264_intra_pred_luma_8x8_vert_l_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#24,%sp

	clr.l	%d4
.Lvl_loopA:
	clr.l	%d0
	move.b	9(%a2,%d4.l),%d0	/* S[9+t] */
	clr.l	%d1
	move.b	10(%a2,%d4.l),%d1	/* S[10+t] */
	bsr	.Lip8_filt11
	move.b	%d0,(%sp,%d4.l)		/* pp[t] */
	addq.l	#1,%d4
	cmp.l	#11,%d4
	blt	.Lvl_loopA

	clr.l	%d4
.Lvl_loopB:
	clr.l	%d0
	move.b	9(%a2,%d4.l),%d0	/* S[9+t] */
	clr.l	%d1
	move.b	10(%a2,%d4.l),%d1	/* S[10+t] */
	clr.l	%d2
	move.b	11(%a2,%d4.l),%d2	/* S[11+t] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	#11,%d3
	move.b	%d0,(%sp,%d3.l)		/* pp[11+t] */
	addq.l	#1,%d4
	cmp.l	#11,%d4
	blt	.Lvl_loopB

	move.l	0(%sp),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	11(%sp),%d0
	move.l	%d0,(%a3)
	move.l	15(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	1(%sp),%d0
	move.l	%d0,(%a3)
	move.l	5(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	12(%sp),%d0
	move.l	%d0,(%a3)
	move.l	16(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	2(%sp),%d0
	move.l	%d0,(%a3)
	move.l	6(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	13(%sp),%d0
	move.l	%d0,(%a3)
	move.l	17(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	3(%sp),%d0
	move.l	%d0,(%a3)
	move.l	7(%sp),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3

	move.l	14(%sp),%d0
	move.l	%d0,(%a3)
	move.l	18(%sp),%d0
	move.l	%d0,4(%a3)

	add.l	#24,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_horz_u_m68k(...) - same 5-arg
 * signature, same UNUSED() args.
 *
 * pp[2t]   = FILT11(S[7-t],S[6-t])            for t=0..6  (7 entries)
 * pp[2t+1] = FILT121(S[7-t],S[6-t],S[5-t])    for t=0..5  (6 entries)
 * pp[13]   = FILT121(S1,S0,S0)                (standalone - S[-1] would
 *            be out of bounds, the vendored C duplicates S[0]/q instead)
 * pp[14..21] = S[0] broadcast (8 bytes) - `memset(predicted_pixels+14,
 *              ui4_q, 8)` in the vendored C, ui4_q = S[0] = left[7].
 * Output row r (0..7) uses pp[2r] - a clean loop, same shape as horz_d's
 * output but ascending instead of descending.
 *
 * Same locals frame shape as vert_r/horz_d/vert_l (24-byte save + 24-byte
 * scratch, same pre-sub arg-offset base).
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_horz_u_m68k
mr_ih264_intra_pred_luma_8x8_horz_u_m68k:
	movem.l	%d2-%d4/%a2-%a4,-(%sp)
	move.l	28(%sp),%a2		/* pu1_src */
	move.l	32(%sp),%a3		/* pu1_dst */
	move.l	40(%sp),%a4		/* dst_strd value */
	sub.l	#24,%sp

	clr.l	%d4
.Lhu_loopA:
	move.l	#7,%d3
	sub.l	%d4,%d3
	clr.l	%d0
	move.b	(%a2,%d3.l),%d0		/* S[7-t] */
	move.l	#6,%d3
	sub.l	%d4,%d3
	clr.l	%d1
	move.b	(%a2,%d3.l),%d1		/* S[6-t] */
	bsr	.Lip8_filt11
	move.l	%d4,%d3
	add.l	%d3,%d3			/* 2t */
	move.b	%d0,(%sp,%d3.l)		/* pp[2t] */
	addq.l	#1,%d4
	cmp.l	#7,%d4
	blt	.Lhu_loopA

	clr.l	%d4
.Lhu_loopB:
	move.l	#7,%d3
	sub.l	%d4,%d3
	clr.l	%d0
	move.b	(%a2,%d3.l),%d0		/* S[7-t] */
	move.l	#6,%d3
	sub.l	%d4,%d3
	clr.l	%d1
	move.b	(%a2,%d3.l),%d1		/* S[6-t] */
	move.l	#5,%d3
	sub.l	%d4,%d3
	clr.l	%d2
	move.b	(%a2,%d3.l),%d2		/* S[5-t] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	%d3,%d3			/* 2t */
	addq.l	#1,%d3			/* 2t+1 */
	move.b	%d0,(%sp,%d3.l)		/* pp[2t+1] */
	addq.l	#1,%d4
	cmp.l	#6,%d4
	blt	.Lhu_loopB

	clr.l	%d0
	move.b	1(%a2),%d0		/* S[1] */
	clr.l	%d1
	move.b	0(%a2),%d1		/* S[0] */
	move.l	%d1,%d2			/* duplicate S[0] */
	bsr	.Lip8_filt121
	move.b	%d0,13(%sp)		/* pp[13] */

	clr.l	%d0
	move.b	0(%a2),%d0		/* S[0] = q */
	move.b	%d0,14(%sp)		/* pp[14..21] = q broadcast (8 bytes, */
	move.b	%d0,15(%sp)		/* one store each - simpler than */
	move.b	%d0,16(%sp)		/* building a 4-byte broadcast pattern */
	move.b	%d0,17(%sp)		/* for what is a once-per-call special */
	move.b	%d0,18(%sp)		/* case, not a hot loop body */
	move.b	%d0,19(%sp)
	move.b	%d0,20(%sp)
	move.b	%d0,21(%sp)

	clr.l	%d4
.Lhu_row:
	move.l	(%sp,%d4.l),%d0
	move.l	%d0,(%a3)
	move.l	4(%sp,%d4.l),%d0
	move.l	%d0,4(%a3)
	adda.l	%a4,%a3
	addq.l	#2,%d4
	cmp.l	#16,%d4
	blt	.Lhu_row

	add.l	#24,%sp
	movem.l	(%sp)+,%d2-%d4/%a2-%a4
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_luma_8x8_ref_filtering_m68k(UWORD8 *pu1_left,
 *     UWORD8 *pu1_topleft, UWORD8 *pu1_top, UWORD8 *pu1_dst,
 *     WORD32 src_strd, WORD32 ngbr_avail);
 *
 * H.264 8.3.2.2.1 reference sample filtering (mandatory before all nine
 * modes above run - ih264_intra_pred_luma_8x8_mode_ref_filtering(),
 * ih264_luma_intra_pred_filters.c lines 741-848). Writes pu1_dst in the
 * same S[] layout the nine modes above consume (this file's own header
 * comment) - S[8]=topleft, S[9..24]=filtered top+topright, S[0..7]=
 * filtered left reversed - from the RAW (unfiltered) pu1_left/pu1_topleft/
 * pu1_top neighbour pointers, which are three SEPARATE pointers here
 * (not one combined S[] buffer, since this is the step that PRODUCES that
 * layout, not one that consumes it).
 *
 * Unlike the six modes above, this genuinely needs the branchy control
 * flow (left/top/top_left/top_right availability all independently
 * gate different pieces, and pu1_topleft is *also* overloaded as a NULL-
 * ness signal separate from the top_left_avail bit - see the vendored
 * C's own comment on this) - translated close to 1:1 from the vendored
 * C's own if/else structure rather than restructured, since getting the
 * combinatorial branch behaviour wrong silently produces a plausible-
 * looking but wrong CABAC-context-drifting-style bug exactly like this
 * project's own working notes warn about for the CABAC port, not a crash.
 * ngbr_avail bit layout: bit0=left, bit1=top_left, bit2=top, bit3=
 * top_right (LEFT/TOP_LEFT/TOP/TOP_RIGHT_MB_AVAILABLE_MASK, ih264_defs.h).
 *
 * The second top-smoothing pass (i=8..14, "filtered output and source in
 * same buf" per the vendored C's own comment) has a genuine sequential
 * carry dependency - each iteration's u4_xm1 must be the *pre-overwrite*
 * value of dst[9+i], not a re-read of the (already-overwritten-by-this-
 * same-loop) memory location - kept in %d5 across the loop, never
 * re-read from memory, matching the C's own u4_xm1 local exactly.
 *
 * The idx=1..6 left-smoothing loop and the src_strd-scaled left[]
 * addressing in the dst[7]/dst[0] special cases use a sliding three-
 * register address-register window (%a6/%a0/%a1, each holding a byte
 * offset, incremented by src_strd once per iteration) instead of a
 * multiply, since src_strd is a runtime value, not a compile-time
 * constant like the fixed +1/+2 offsets the other modes above use.
 *
 * FILT121 is reused via .Lip8_filt121 (register-only, IN: %d0/%d1/%d2,
 * OUT: %d0, clobbers %d0/%d3 - see that subroutine's own header comment
 * above). The 3-weight variants ((3*a+b+2)>>2, not a FILT121/FILT11
 * shape) are computed inline instead - not worth their own subroutine
 * for the handful of call sites here.
 *
 * Saves %d2-%d7/%a2-%a6 (44 bytes) - no local scratch needed (unlike the
 * six modes above, this function's intermediate values are computed and
 * consumed one at a time, nothing needs a predicted_pixels-style array).
 * Stack-arg offsets: 48(%sp)=pu1_left, 52(%sp)=pu1_topleft,
 * 56(%sp)=pu1_top, 60(%sp)=pu1_dst, 64(%sp)=src_strd, 68(%sp)=ngbr_avail.
 * Register plan: %a2=pu1_left, %a3=pu1_topleft, %a4=pu1_top, %a5=pu1_dst
 * (all persistent for the whole function). %d6=src_strd (persistent,
 * needed by the left-smoothing window). %d7=ngbr_avail (persistent,
 * tested via BTST throughout - its 4 bits are read many times rather
 * than pre-extracted into separate flag registers, since register
 * pressure is otherwise tight in the second smoothing loop and the
 * left-smoothing sliding window).
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_luma_8x8_ref_filtering_m68k
mr_ih264_intra_pred_luma_8x8_ref_filtering_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	48(%sp),%a2		/* pu1_left */
	move.l	52(%sp),%a3		/* pu1_topleft */
	move.l	56(%sp),%a4		/* pu1_top */
	move.l	60(%sp),%a5		/* pu1_dst */
	move.l	64(%sp),%d6		/* src_strd */
	move.l	68(%sp),%d7		/* ngbr_avail */

	/* ---- if(top_avail) ---- */
	btst	#2,%d7
	beq	.Lrf_notop

	btst	#3,%d7			/* top_right_avail? */
	bne.s	.Lrf_tr_have
	clr.l	%d0
	move.b	7(%a4),%d0		/* top[7] */
	move.b	%d0,17(%a5)
	move.b	%d0,18(%a5)
	move.b	%d0,19(%a5)
	move.b	%d0,20(%a5)
	move.b	%d0,21(%a5)
	move.b	%d0,22(%a5)
	move.b	%d0,23(%a5)
	move.b	%d0,24(%a5)
	bra.s	.Lrf_tr_done
.Lrf_tr_have:
	move.l	8(%a4),%d0
	move.l	%d0,17(%a5)
	move.l	12(%a4),%d0
	move.l	%d0,21(%a5)
.Lrf_tr_done:

	btst	#1,%d7			/* top_left_avail? */
	beq.s	.Lrf_dst9_noleft
	clr.l	%d0
	move.b	(%a3),%d0		/* *topleft */
	clr.l	%d1
	move.b	0(%a4),%d1		/* top[0] */
	clr.l	%d2
	move.b	1(%a4),%d2		/* top[1] */
	bsr	.Lip8_filt121
	move.b	%d0,9(%a5)
	bra.s	.Lrf_dst9_done
.Lrf_dst9_noleft:
	clr.l	%d0
	move.b	0(%a4),%d0		/* top[0] */
	move.l	%d0,%d1
	add.l	%d0,%d1
	add.l	%d0,%d1			/* 3*top[0] */
	clr.l	%d2
	move.b	1(%a4),%d2		/* top[1] */
	add.l	%d2,%d1
	add.l	#2,%d1
	asr.l	#2,%d1
	move.b	%d1,9(%a5)
.Lrf_dst9_done:

	moveq	#1,%d4
.Lrf_loop1:
	clr.l	%d0
	move.l	%d4,%d3
	subq.l	#1,%d3
	move.b	(%a4,%d3.l),%d0		/* top[i-1] */
	clr.l	%d1
	move.b	(%a4,%d4.l),%d1		/* top[i] */
	clr.l	%d2
	move.l	%d4,%d3
	addq.l	#1,%d3
	move.b	(%a4,%d3.l),%d2		/* top[i+1] */
	bsr	.Lip8_filt121
	move.l	%d4,%d3
	add.l	#9,%d3
	move.b	%d0,(%a5,%d3.l)		/* dst[9+i] */
	addq.l	#1,%d4
	cmp.l	#6,%d4
	ble	.Lrf_loop1

	clr.l	%d0
	move.b	6(%a4),%d0		/* top[6] */
	clr.l	%d1
	move.b	7(%a4),%d1		/* top[7] */
	clr.l	%d2
	move.b	17(%a5),%d2		/* dst[17] (already filled above) */
	bsr	.Lip8_filt121
	move.b	%d0,16(%a5)		/* dst[9+7] */

	clr.l	%d5
	move.b	7(%a4),%d5		/* u4_xm1 = top[7] */
	moveq	#8,%d4
.Lrf_loop2:
	move.l	%d4,%d3
	add.l	#9,%d3
	clr.l	%d0
	move.b	(%a5,%d3.l),%d0		/* d0 = dst[9+i] (unfiltered) */
	move.l	%d0,%d1
	add.l	%d1,%d1			/* 2*dst[9+i] */
	add.l	%d5,%d1			/* + u4_xm1 */
	move.l	%d4,%d2
	add.l	#10,%d2
	clr.l	%d3
	move.b	(%a5,%d2.l),%d3		/* dst[9+i+1] */
	add.l	%d3,%d1
	add.l	#2,%d1
	asr.l	#2,%d1			/* u4_x */
	move.l	%d0,%d5			/* u4_xm1 = old dst[9+i] */
	move.l	%d4,%d3
	add.l	#9,%d3
	move.b	%d1,(%a5,%d3.l)		/* dst[9+i] = u4_x */
	addq.l	#1,%d4
	cmp.l	#14,%d4
	ble	.Lrf_loop2

	clr.l	%d0
	move.b	24(%a5),%d0		/* dst[9+15] */
	move.l	%d0,%d1
	add.l	%d0,%d1
	add.l	%d0,%d1			/* 3*dst[24] */
	add.l	%d5,%d1			/* + u4_xm1 */
	add.l	#2,%d1
	asr.l	#2,%d1
	move.b	%d1,24(%a5)
.Lrf_notop:

	/* ---- if(top_left_avail) ---- */
	btst	#1,%d7
	beq	.Lrf_notopleft
	btst	#2,%d7			/* top_avail? */
	beq.s	.Lrf_tl_notoporleft
	btst	#0,%d7			/* left_avail? */
	bne.s	.Lrf_tl_bothavail
.Lrf_tl_notoporleft:
	btst	#2,%d7
	beq.s	.Lrf_tl_check_left
	clr.l	%d0
	move.b	(%a3),%d0		/* *topleft */
	move.l	%d0,%d1
	add.l	%d0,%d1
	add.l	%d0,%d1			/* 3*topleft */
	clr.l	%d2
	move.b	0(%a4),%d2		/* top[0] */
	add.l	%d2,%d1
	add.l	#2,%d1
	asr.l	#2,%d1
	move.b	%d1,8(%a5)
	bra	.Lrf_notopleft
.Lrf_tl_check_left:
	btst	#0,%d7
	beq	.Lrf_notopleft		/* neither top nor left -> no assignment */
	clr.l	%d0
	move.b	(%a3),%d0
	move.l	%d0,%d1
	add.l	%d0,%d1
	add.l	%d0,%d1
	clr.l	%d2
	move.b	0(%a2),%d2		/* left[0] */
	add.l	%d2,%d1
	add.l	#2,%d1
	asr.l	#2,%d1
	move.b	%d1,8(%a5)
	bra	.Lrf_notopleft
.Lrf_tl_bothavail:
	clr.l	%d0
	move.b	0(%a4),%d0		/* top[0] */
	clr.l	%d1
	move.b	(%a3),%d1		/* *topleft */
	clr.l	%d2
	move.b	0(%a2),%d2		/* left[0] */
	bsr	.Lip8_filt121
	move.b	%d0,8(%a5)
.Lrf_notopleft:

	/* ---- if(left_avail) ---- */
	btst	#0,%d7
	beq	.Lrf_notleft

	tst.l	%a3			/* pu1_topleft != NULL ? (overloaded
					 * pointer, NOT the top_left_avail
					 * bit - see header comment) */
	beq.s	.Lrf_dst7_notl
	clr.l	%d0
	move.b	(%a3),%d0		/* *topleft */
	clr.l	%d1
	move.b	(%a2),%d1		/* left[0] */
	clr.l	%d2
	move.l	%d6,%d3
	move.b	(%a2,%d3.l),%d2		/* left[strd] */
	bsr	.Lip8_filt121
	move.b	%d0,7(%a5)
	bra.s	.Lrf_dst7_done
.Lrf_dst7_notl:
	clr.l	%d0
	move.b	(%a2),%d0		/* left[0] */
	move.l	%d0,%d1
	add.l	%d0,%d1
	add.l	%d0,%d1			/* 3*left[0] */
	clr.l	%d2
	move.l	%d6,%d3
	move.b	(%a2,%d3.l),%d2		/* left[strd] */
	add.l	%d2,%d1
	add.l	#2,%d1
	asr.l	#2,%d1
	move.b	%d1,7(%a5)
.Lrf_dst7_done:

	suba.l	%a6,%a6			/* offset(idx-1) = 0 */
	movea.l	%d6,%a0			/* offset(idx)   = strd */
	movea.l	%d6,%a1
	adda.l	%d6,%a1			/* offset(idx+1) = 2*strd */
	moveq	#1,%d4
.Lrf_loop3:
	clr.l	%d0
	move.b	(%a2,%a6.l),%d0		/* left[(idx-1)*strd] */
	clr.l	%d1
	move.b	(%a2,%a0.l),%d1		/* left[idx*strd] */
	clr.l	%d2
	move.b	(%a2,%a1.l),%d2		/* left[(idx+1)*strd] */
	bsr	.Lip8_filt121
	move.l	#7,%d5
	sub.l	%d4,%d5			/* 7-idx */
	move.b	%d0,(%a5,%d5.l)		/* dst[7-idx] */

	movea.l	%a0,%a6			/* slide the window forward */
	movea.l	%a1,%a0
	adda.l	%d6,%a1

	addq.l	#1,%d4
	cmp.l	#6,%d4
	ble	.Lrf_loop3

	/* After the loop, %a6=6*strd, %a0=7*strd (the last slide step). */
	clr.l	%d0
	move.b	(%a2,%a6.l),%d0		/* left[6*strd] */
	clr.l	%d1
	move.b	(%a2,%a0.l),%d1		/* left[7*strd] */
	move.l	%d1,%d2
	add.l	%d1,%d2
	add.l	%d1,%d2			/* 3*left[7*strd] */
	add.l	%d0,%d2
	add.l	#2,%d2
	asr.l	#2,%d2
	move.b	%d2,0(%a5)		/* dst[0] */
.Lrf_notleft:

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_intra_pred_chroma_8x8_plane_m68k(UWORD8 *pu1_src,
 *     UWORD8 *pu1_dst, WORD32 src_strd, WORD32 dst_strd,
 *     WORD32 ngbr_avail);
 *
 * H.264 8.3.4.4 chroma Plane prediction (ih264_intra_pred_chroma_8x8_
 * mode_plane, ih264_chroma_intra_pred_filters.c) - the chroma analogue of
 * mr_ih264_intra_pred_luma_16x16_plane_m68k above: a genuine per-pixel
 * multiply+shift+clip with no 4-bytes-at-once opportunity, same reasoning,
 * same closed-form re-derivation approach (re-derived directly from the
 * vendored pointer arithmetic rather than transcribing Ittiam's own
 * shift-add chain, since MULS.L makes the closed form both correct and
 * simpler). The one real difference from the luma case: pu1_src here is
 * this project's usual chroma S[] layout - interleaved (U,V) sample
 * pairs, every logical sample doubled to two bytes (S[0..13]=left
 * reversed, S[14..15]... wait: S[16]=topleft-U/S[17]=topleft-V,
 * S[18..33]=top+top-right, S[0..15]=left reversed - see ih264_m68k_
 * optim.c's chroma vert/horz/dc header comment for the exact byte
 * layout) - so U and V need two structurally-identical passes over the
 * 8x8 grid (one full row/col fill loop per channel) rather than one,
 * each with its own h/v/a/b/c setup reading the even (U) or odd (V)
 * bytes of the same S[] buffer, and each pass's fill loop stepping the
 * output cursor by 2 bytes per column (writing only the U or only the V
 * byte of each pair) instead of 1:
 *
 *   H_u = sum_{c=0}^{3} (c+1) * (top_u[4+c] - top_u[3-c])
 *   V_u = sum_{c=0}^{3} (c+1) * (left_u[-(4+c)] - left_u[-(3-c)])
 *   a_u = 16 * (top_u[7] + left_u[-7])
 *   b_u = (34*H_u + 32) >> 6
 *   c_u = (34*V_u + 32) >> 6
 *   pred_u(row, col) = CLIP_U8((a_u + b_u*(col-3) + c_u*(row-3) + 16) >> 5)
 * (same shape for V with every _u swapped to _v, and top_v[n]/left_v[n]
 * meaning the V byte of the same UV pair top_u[n]/left_u[n] refers to.)
 *
 * In S[] terms (pu1_top = pu1_src+18, pu1_left = pu1_src+14, matching the
 * chroma vert/horz/dc functions' own bases exactly): top_u[4+c] =
 * S[18+2*(4+c)] = S[26+2c], top_u[3-c] = S[22-2c], left_u[-(4+c)] =
 * S[14-2*(4+c)] = S[6-2c], left_u[-(3-c)] = S[10+2c], top_u[7] = S[32],
 * left_u[-7] = S[0] - the _v variants are each the same offset +1 (the V
 * byte of the same pair). x/y only range 0..3 here (half of luma's 0..7,
 * since the chroma block is 8x8 not 16x16), so - like the luma version -
 * every term is a fixed constant displacement from pu1_src, no loop.
 * "halfsize" (used to fold the block-center offset into the loop's
 * initial tmp/tmpx, matching the luma version's own such folding) is 4
 * here (8/2) where luma's was 8 (16/2).
 *
 * Verification cross-checks a fuzz-test reference that instead
 * transcribes Ittiam's own literal shift-add chain, for the same reason
 * the luma version's does - see that function's header comment above.
 *
 * Saves %d2-%d7/%a2-%a3 (32 bytes): pu1_src at 36(%sp) (4+32), pu1_dst at
 * 40(%sp) (8+32), dst_strd at 48(%sp) (16+32) - identical offsets to the
 * luma Plane function above, since the arg list and save set are the
 * same shape. Register plan per pass: %a2=pu1_src (persistent across both
 * passes), %a3=dst cursor for the pass currently running (reloaded fresh
 * from pu1_dst at the start of the V pass, offset +1 for the V byte),
 * %d6=b, %d7=c (persistent within one pass), %d0=tmp, %d1=tmpx, %d2=
 * column counter, %d3=dst_strd (persistent across both passes), %d4=row
 * counter, %d5=clip scratch (and holds `a` transiently during each
 * pass's own setup) - same register shape as the luma version, just run
 * twice with different S[] read offsets and a different dst byte lane.
 * ---------------------------------------------------------------------- */
	.globl	mr_ih264_intra_pred_chroma_8x8_plane_m68k
mr_ih264_intra_pred_chroma_8x8_plane_m68k:
	movem.l	%d2-%d7/%a2-%a3,-(%sp)
	move.l	36(%sp),%a2		/* pu1_src   (4+32)  */
	move.l	40(%sp),%a3		/* pu1_dst   (8+32)  */
	move.l	48(%sp),%d3		/* dst_strd  (16+32) */

	/* ==== U pass ==== */
	clr.l	%d1
	move.b	26(%a2),%d1
	clr.l	%d2
	move.b	22(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* H_u, c=0 term (coeff 1) */

	clr.l	%d1
	move.b	28(%a2),%d1
	clr.l	%d2
	move.b	20(%a2),%d2
	sub.l	%d2,%d1
	add.l	%d1,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	30(%a2),%d1
	clr.l	%d2
	move.b	18(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	32(%a2),%d1
	clr.l	%d2
	move.b	16(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0			/* d0 = H_u */

	move.l	%d0,%d1
	muls.l	#34,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d6			/* d6 = b_u */

	clr.l	%d1
	move.b	6(%a2),%d1
	clr.l	%d2
	move.b	10(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* V_u, c=0 term */

	clr.l	%d1
	move.b	4(%a2),%d1
	clr.l	%d2
	move.b	12(%a2),%d2
	sub.l	%d2,%d1
	add.l	%d1,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	2(%a2),%d1
	clr.l	%d2
	move.b	14(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	0(%a2),%d1
	clr.l	%d2
	move.b	16(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0			/* d0 = V_u */

	move.l	%d0,%d1
	muls.l	#34,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d7			/* d7 = c_u */

	clr.l	%d5
	move.b	32(%a2),%d5		/* top_u[7] = S[32] */
	clr.l	%d1
	move.b	0(%a2),%d1		/* left_u[-7] = S[0] */
	add.l	%d1,%d5
	lsl.l	#4,%d5			/* d5 = a_u (16*...) */

	move.l	%d7,%d1
	asl.l	#2,%d1			/* 4*c_u (halfsize=4) */
	sub.l	%d1,%d5
	add.l	#16,%d5
	move.l	%d5,%d0			/* tmp = a_u - 4*c_u + 16 */

	moveq	#8,%d4			/* row counter */
.Lcp_row_u:
	add.l	%d7,%d0			/* tmp += c_u */
	move.l	%d6,%d1
	asl.l	#2,%d1			/* 4*b_u */
	neg.l	%d1			/* tmpx = -4*b_u */

	moveq	#8,%d2			/* column counter */
.Lcp_col_u:
	add.l	%d6,%d1			/* tmpx += b_u */
	move.l	%d0,%d5
	add.l	%d1,%d5
	asr.l	#5,%d5
	bmi.s	.Lcp_u_clip_lo
	cmp.l	#255,%d5
	ble.s	.Lcp_u_store
	move.l	#255,%d5
	bra.s	.Lcp_u_store
.Lcp_u_clip_lo:
	moveq	#0,%d5
.Lcp_u_store:
	move.b	%d5,(%a3)
	adda.l	#2,%a3
	subq.l	#1,%d2
	bne.s	.Lcp_col_u

	suba.l	#16,%a3			/* back to this row's start (8 cols
					 * * 2 bytes) ... */
	adda.l	%d3,%a3			/* ... then += dst_strd */
	subq.l	#1,%d4
	bne	.Lcp_row_u

	/* ==== V pass ==== */
	move.l	40(%sp),%a3		/* pu1_dst, reload fresh */
	adda.l	#1,%a3			/* + 1: V byte of the first pair */

	clr.l	%d1
	move.b	27(%a2),%d1
	clr.l	%d2
	move.b	23(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* H_v, c=0 term */

	clr.l	%d1
	move.b	29(%a2),%d1
	clr.l	%d2
	move.b	21(%a2),%d2
	sub.l	%d2,%d1
	add.l	%d1,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	31(%a2),%d1
	clr.l	%d2
	move.b	19(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	33(%a2),%d1
	clr.l	%d2
	move.b	17(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0			/* d0 = H_v */

	move.l	%d0,%d1
	muls.l	#34,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d6			/* d6 = b_v */

	clr.l	%d1
	move.b	7(%a2),%d1
	clr.l	%d2
	move.b	11(%a2),%d2
	sub.l	%d2,%d1
	move.l	%d1,%d0			/* V_v, c=0 term */

	clr.l	%d1
	move.b	5(%a2),%d1
	clr.l	%d2
	move.b	13(%a2),%d2
	sub.l	%d2,%d1
	add.l	%d1,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	3(%a2),%d1
	clr.l	%d2
	move.b	15(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#3,%d1
	add.l	%d1,%d0

	clr.l	%d1
	move.b	1(%a2),%d1
	clr.l	%d2
	move.b	17(%a2),%d2
	sub.l	%d2,%d1
	muls.l	#4,%d1
	add.l	%d1,%d0			/* d0 = V_v */

	move.l	%d0,%d1
	muls.l	#34,%d1
	add.l	#32,%d1
	asr.l	#6,%d1
	move.l	%d1,%d7			/* d7 = c_v */

	clr.l	%d5
	move.b	33(%a2),%d5		/* top_v[7] = S[33] */
	clr.l	%d1
	move.b	1(%a2),%d1		/* left_v[-7] = S[1] */
	add.l	%d1,%d5
	lsl.l	#4,%d5			/* d5 = a_v */

	move.l	%d7,%d1
	asl.l	#2,%d1
	sub.l	%d1,%d5
	add.l	#16,%d5
	move.l	%d5,%d0			/* tmp = a_v - 4*c_v + 16 */

	moveq	#8,%d4
.Lcp_row_v:
	add.l	%d7,%d0
	move.l	%d6,%d1
	asl.l	#2,%d1
	neg.l	%d1

	moveq	#8,%d2
.Lcp_col_v:
	add.l	%d6,%d1
	move.l	%d0,%d5
	add.l	%d1,%d5
	asr.l	#5,%d5
	bmi.s	.Lcp_v_clip_lo
	cmp.l	#255,%d5
	ble.s	.Lcp_v_store
	move.l	#255,%d5
	bra.s	.Lcp_v_store
.Lcp_v_clip_lo:
	moveq	#0,%d5
.Lcp_v_store:
	move.b	%d5,(%a3)
	adda.l	#2,%a3
	subq.l	#1,%d2
	bne.s	.Lcp_col_v

	suba.l	#16,%a3
	adda.l	%d3,%a3
	subq.l	#1,%d4
	bne	.Lcp_row_v

	movem.l	(%sp)+,%d2-%d7/%a2-%a3
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
