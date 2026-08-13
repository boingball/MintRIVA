/*
 * Hand-written m68k assembly for H.264's bS==4 deblocking edge filter
 * (H.264 spec 8.7.2.4, "Filtering process for edges for bS equal to 4"),
 * the strongest/most expensive of the deblocking filters and, per a
 * real-Pistorm-hardware timing breakdown (mrplay.c's "h264 stages:" line,
 * vendor/libavc_port/ih264d_stage_profile.c), the single largest piece of
 * H.264 decode cost measured so far - roughly 40-48% of libavc-core time,
 * some 7-8x the cost of all motion-compensation filters combined even
 * after those were hand-optimised.
 *
 * Both functions below filter one full macroblock edge (16 luma samples
 * along the edge), independently per sample row (vert) or column (horz):
 *
 *   Filtering decision: skip this sample entirely (no write at all) if
 *     |p0-q0| >= alpha  OR  |q1-q0| >= beta  OR  |p1-p0| >= beta.
 *   Otherwise, if |p0-q0| < (alpha>>2)+2 ("strong" region):
 *     a_p = |p2-p0|; if a_p < beta, write p0'/p1'/p2' (3-tap); else p0' only.
 *     a_q = |q2-q0|; if a_q < beta, write q0'/q1'/q2' (3-tap); else q0' only.
 *   Otherwise ("weak" region): write p0' and q0' only (the same 1-tap
 *     formula the strong region's else-branches use).
 *
 * All eight samples (p3..p0, q0..q3) are UWORD8 (0-255); every filter
 * output is a small non-negative weighted average that is guaranteed by
 * construction to stay in 0-255 (matches Ittiam's C: no CLIP_U8 call
 * anywhere in this filter, and neither is there one here). Every
 * intermediate sum fits comfortably in a 32-bit accumulator - this is
 * .l arithmetic throughout, not the .w-then-clip pattern the
 * interpolation filters in ih264_m68k_interp.S use, because there is no
 * clipping step to reason about here at all.
 *
 * Cross-dependency note (the one subtlety that isn't just "port the
 * formula"): the q-side formulas read the *original* p0/p1, and the
 * p-side formulas read the *original* q0/q1. Ittiam's C sidesteps this by
 * copying all eight samples into local variables up front. This does the
 * same thing via registers: p0/q0/p1/q1 are loaded once per sample and
 * pinned in %d2/%d3/%d4/%d5 for the samples's entire filtering, and never
 * re-read from memory - so it does not matter that q-side's memory writes
 * happen before p-side's (or vice versa); each side only ever reads the
 * pinned original copies of the other side's low samples, never the
 * other side's just-written new value.
 *
 * A second simplification versus the C: Ittiam's loop nests "4 edges of 4
 * samples" (an MB-decomposition artefact), but the address stride between
 * consecutive samples is uniform across that whole nest (each of the 16
 * total samples along the edge is exactly one src_strd - vert - or one
 * column - horz - apart from its neighbour, edge boundary or not), so
 * both functions below are just a single flat 16-iteration loop.
 *
 * void mr_ih264_deblk_luma_vert_bs4_m68k(UWORD8 *pu1_src, WORD32 src_strd,
 *     WORD32 alpha, WORD32 beta);
 * void mr_ih264_deblk_luma_horz_bs4_m68k(UWORD8 *pu1_src, WORD32 src_strd,
 *     WORD32 alpha, WORD32 beta);
 *
 * m68k C ABI: all four arguments are 4 bytes, passed on the stack in
 * order starting at 4(%sp) (confirmed against `m68k-linux-gnu-gcc -S`,
 * same as every other function in this port). Both functions save
 * %d2-%d7/%a3-%a4 (8 registers, 32 bytes), so stack offsets below are +32
 * (e.g. pu1_src at 36(%sp) = 4+32). %d0/%d1/%d7/%a0 are scratch, no save
 * needed; %d6 is the row/column counter, pinned for the whole function.
 *
 * vert reaches every sample via constant byte displacements from the
 * column cursor (-4..+3, exactly like the interpolation filters' taps).
 * horz's samples are src_strd apart instead, so it recomputes each tap
 * address fresh via %a0 from the row cursor and %a4 (src_strd held as an
 * address value for adda.l/suba.l) - the same "never chain from the
 * previous tap" discipline the vertical MC filter in ih264_m68k_interp.S
 * uses, so a slip in one tap's offset arithmetic can't propagate.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_deblk_luma_vert_bs4_m68k
	.globl	mr_ih264_deblk_luma_horz_bs4_m68k
	.globl	mr_ih264_deblk_luma_vert_bslt4_m68k
	.globl	mr_ih264_deblk_luma_horz_bslt4_m68k

/* ---------------------------------------------------------------------- */
/* Vertical edge: taps sit at constant -4/-3/-2/-1/0/1/2/3 byte            */
/* displacements from the column cursor; only the +src_strd step to the   */
/* next row needs an address computation.                                 */
/* ---------------------------------------------------------------------- */
mr_ih264_deblk_luma_vert_bs4_m68k:
	movem.l	%d2-%d7/%a3-%a4,-(%sp)
	movea.l	36(%sp),%a3		/* pu1_src   (4+32)  */
	move.l	40(%sp),%d0		/* src_strd  (8+32)  */
	movea.l	%d0,%a4			/* a4 = src_strd, held for adda/suba */
	moveq	#16,%d6			/* 4 edges * 4 rows, flattened */

.Lv_row:
	clr.l	%d2
	move.b	-1(%a3),%d2		/* p0 */
	clr.l	%d3
	move.b	(%a3),%d3		/* q0 */
	clr.l	%d4
	move.b	-2(%a3),%d4		/* p1 */
	clr.l	%d5
	move.b	1(%a3),%d5		/* q1 */

	move.l	%d2,%d0
	sub.l	%d3,%d0
	bpl.s	.Lv_abs1
	neg.l	%d0
.Lv_abs1:
	move.l	44(%sp),%d1		/* alpha    (12+32) */
	cmp.l	%d1,%d0
	bge	.Lv_skip

	move.l	%d5,%d0
	sub.l	%d3,%d0
	bpl.s	.Lv_abs2
	neg.l	%d0
.Lv_abs2:
	move.l	48(%sp),%d1		/* beta     (16+32) */
	cmp.l	%d1,%d0
	bge	.Lv_skip

	move.l	%d4,%d0
	sub.l	%d2,%d0
	bpl.s	.Lv_abs3
	neg.l	%d0
.Lv_abs3:
	cmp.l	%d1,%d0			/* %d1 still beta */
	bge	.Lv_skip

	move.l	%d2,%d0
	sub.l	%d3,%d0
	bpl.s	.Lv_abs4
	neg.l	%d0
.Lv_abs4:
	move.l	44(%sp),%d1		/* alpha */
	asr.l	#2,%d1
	addq.l	#2,%d1			/* %d1 = (alpha>>2)+2 */
	cmp.l	%d1,%d0
	bge	.Lv_weak

	/* ---- strong region: q-side (reads pinned p0/p1 only) ---- */
	clr.l	%d7
	move.b	2(%a3),%d7		/* q2 */
	move.l	%d7,%d0
	sub.l	%d3,%d0
	bpl.s	.Lv_abs5
	neg.l	%d0
.Lv_abs5:
	move.l	48(%sp),%d1		/* beta */
	cmp.l	%d1,%d0
	bge	.Lv_q_weak

	clr.l	%d1
	move.b	3(%a3),%d1		/* q3 */
	asl.l	#1,%d1			/* 2*q3 */
	move.l	%d7,%d0
	asl.l	#1,%d0
	add.l	%d7,%d0			/* 3*q2 */
	add.l	%d1,%d0			/* + 2*q3 */
	add.l	%d5,%d0			/* + q1 */
	add.l	%d3,%d0			/* + q0 */
	add.l	%d2,%d0			/* + p0 */
	addq.l	#4,%d0
	asr.l	#3,%d0
	move.b	%d0,2(%a3)		/* q2' */

	move.l	%d2,%d0
	add.l	%d3,%d0
	add.l	%d5,%d0
	add.l	%d7,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,1(%a3)		/* q1' */

	move.l	%d4,%d0
	move.l	%d2,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d3,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d5,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	add.l	%d7,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	move.b	%d0,(%a3)		/* q0' */
	bra	.Lv_q_done

.Lv_q_weak:
	move.l	%d5,%d0
	asl.l	#1,%d0
	add.l	%d3,%d0
	add.l	%d4,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,(%a3)		/* q0' */
.Lv_q_done:

	/* ---- strong region: p-side (reads pinned q0/q1 only) ---- */
	clr.l	%d7
	move.b	-3(%a3),%d7		/* p2 */
	move.l	%d7,%d0
	sub.l	%d2,%d0
	bpl.s	.Lv_abs6
	neg.l	%d0
.Lv_abs6:
	move.l	48(%sp),%d1		/* beta */
	cmp.l	%d1,%d0
	bge	.Lv_p_weak

	clr.l	%d1
	move.b	-4(%a3),%d1		/* p3 */
	asl.l	#1,%d1			/* 2*p3 */
	move.l	%d7,%d0
	asl.l	#1,%d0
	add.l	%d7,%d0			/* 3*p2 */
	add.l	%d1,%d0			/* + 2*p3 */
	add.l	%d4,%d0			/* + p1 */
	add.l	%d2,%d0			/* + p0 */
	add.l	%d3,%d0			/* + q0 */
	addq.l	#4,%d0
	asr.l	#3,%d0
	move.b	%d0,-3(%a3)		/* p2' */

	move.l	%d7,%d0
	add.l	%d4,%d0
	add.l	%d2,%d0
	add.l	%d3,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,-2(%a3)		/* p1' */

	move.l	%d7,%d0
	move.l	%d4,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d2,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d3,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	add.l	%d5,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	move.b	%d0,-1(%a3)		/* p0' */
	bra	.Lv_p_done

.Lv_p_weak:
	move.l	%d4,%d0
	asl.l	#1,%d0
	add.l	%d2,%d0
	add.l	%d5,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,-1(%a3)		/* p0' */
.Lv_p_done:
	bra	.Lv_next

.Lv_weak:
	move.l	%d5,%d0
	asl.l	#1,%d0
	add.l	%d3,%d0
	add.l	%d4,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,(%a3)		/* q0' */

	move.l	%d4,%d0
	asl.l	#1,%d0
	add.l	%d2,%d0
	add.l	%d5,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,-1(%a3)		/* p0' */

.Lv_next:
.Lv_skip:
	adda.l	%a4,%a3
	subq.l	#1,%d6
	bne	.Lv_row

	movem.l	(%sp)+,%d2-%d7/%a3-%a4
	rts

/* ---------------------------------------------------------------------- */
/* Horizontal edge: identical filter math to the vertical case above -    */
/* only the addressing differs (samples are src_strd apart, not 1 byte    */
/* apart), so every tap address below is freshly recomputed from the      */
/* current column cursor (%a3) via %a0, never chained from a previous     */
/* tap - matches the discipline mr_ih264_inter_pred_luma_vert_m68k in     */
/* ih264_m68k_interp.S already uses for the same reason.                  */
/* ---------------------------------------------------------------------- */
mr_ih264_deblk_luma_horz_bs4_m68k:
	movem.l	%d2-%d7/%a3-%a4,-(%sp)
	movea.l	36(%sp),%a3		/* pu1_src   (4+32)  */
	move.l	40(%sp),%d0		/* src_strd  (8+32)  */
	movea.l	%d0,%a4
	moveq	#16,%d6			/* 4 edges * 4 columns, flattened */

.Lh_col:
	clr.l	%d3
	move.b	(%a3),%d3		/* q0 */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	clr.l	%d5
	move.b	(%a0),%d5		/* q1 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d2
	move.b	(%a0),%d2		/* p0 */
	suba.l	%a4,%a0
	clr.l	%d4
	move.b	(%a0),%d4		/* p1 */

	move.l	%d2,%d0
	sub.l	%d3,%d0
	bpl.s	.Lh_abs1
	neg.l	%d0
.Lh_abs1:
	move.l	44(%sp),%d1		/* alpha */
	cmp.l	%d1,%d0
	bge	.Lh_skip

	move.l	%d5,%d0
	sub.l	%d3,%d0
	bpl.s	.Lh_abs2
	neg.l	%d0
.Lh_abs2:
	move.l	48(%sp),%d1		/* beta */
	cmp.l	%d1,%d0
	bge	.Lh_skip

	move.l	%d4,%d0
	sub.l	%d2,%d0
	bpl.s	.Lh_abs3
	neg.l	%d0
.Lh_abs3:
	cmp.l	%d1,%d0
	bge	.Lh_skip

	move.l	%d2,%d0
	sub.l	%d3,%d0
	bpl.s	.Lh_abs4
	neg.l	%d0
.Lh_abs4:
	move.l	44(%sp),%d1		/* alpha */
	asr.l	#2,%d1
	addq.l	#2,%d1
	cmp.l	%d1,%d0
	bge	.Lh_weak

	/* ---- strong region: q-side ---- */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0			/* a0 = q2 addr */
	clr.l	%d7
	move.b	(%a0),%d7		/* q2 */
	move.l	%d7,%d0
	sub.l	%d3,%d0
	bpl.s	.Lh_abs5
	neg.l	%d0
.Lh_abs5:
	move.l	48(%sp),%d1		/* beta */
	cmp.l	%d1,%d0
	bge	.Lh_q_weak

	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0			/* a0 = q3 addr */
	clr.l	%d1
	move.b	(%a0),%d1		/* q3 */
	asl.l	#1,%d1			/* 2*q3 */
	move.l	%d7,%d0
	asl.l	#1,%d0
	add.l	%d7,%d0			/* 3*q2 */
	add.l	%d1,%d0
	add.l	%d5,%d0
	add.l	%d3,%d0
	add.l	%d2,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0			/* a0 = q2 addr */
	move.b	%d0,(%a0)		/* q2' */

	move.l	%d2,%d0
	add.l	%d3,%d0
	add.l	%d5,%d0
	add.l	%d7,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	movea.l	%a3,%a0
	adda.l	%a4,%a0			/* a0 = q1 addr */
	move.b	%d0,(%a0)		/* q1' */

	move.l	%d4,%d0
	move.l	%d2,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d3,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d5,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	add.l	%d7,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	move.b	%d0,(%a3)		/* q0' */
	bra	.Lh_q_done

.Lh_q_weak:
	move.l	%d5,%d0
	asl.l	#1,%d0
	add.l	%d3,%d0
	add.l	%d4,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,(%a3)		/* q0' */
.Lh_q_done:

	/* ---- strong region: p-side ---- */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0			/* a0 = p2 addr */
	clr.l	%d7
	move.b	(%a0),%d7		/* p2 */
	move.l	%d7,%d0
	sub.l	%d2,%d0
	bpl.s	.Lh_abs6
	neg.l	%d0
.Lh_abs6:
	move.l	48(%sp),%d1		/* beta */
	cmp.l	%d1,%d0
	bge	.Lh_p_weak

	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0			/* a0 = p3 addr */
	clr.l	%d1
	move.b	(%a0),%d1		/* p3 */
	asl.l	#1,%d1			/* 2*p3 */
	move.l	%d7,%d0
	asl.l	#1,%d0
	add.l	%d7,%d0			/* 3*p2 */
	add.l	%d1,%d0
	add.l	%d4,%d0
	add.l	%d2,%d0
	add.l	%d3,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0			/* a0 = p2 addr */
	move.b	%d0,(%a0)		/* p2' */

	move.l	%d7,%d0
	add.l	%d4,%d0
	add.l	%d2,%d0
	add.l	%d3,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0			/* a0 = p1 addr */
	move.b	%d0,(%a0)		/* p1' */

	move.l	%d7,%d0
	move.l	%d4,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d2,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	move.l	%d3,%d1
	asl.l	#1,%d1
	add.l	%d1,%d0
	add.l	%d5,%d0
	addq.l	#4,%d0
	asr.l	#3,%d0
	movea.l	%a3,%a0
	suba.l	%a4,%a0			/* a0 = p0 addr */
	move.b	%d0,(%a0)		/* p0' */
	bra	.Lh_p_done

.Lh_p_weak:
	move.l	%d4,%d0
	asl.l	#1,%d0
	add.l	%d2,%d0
	add.l	%d5,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	move.b	%d0,(%a0)		/* p0' */
.Lh_p_done:
	bra	.Lh_next

.Lh_weak:
	move.l	%d5,%d0
	asl.l	#1,%d0
	add.l	%d3,%d0
	add.l	%d4,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	move.b	%d0,(%a3)		/* q0' */

	move.l	%d4,%d0
	asl.l	#1,%d0
	add.l	%d2,%d0
	add.l	%d5,%d0
	addq.l	#2,%d0
	asr.l	#2,%d0
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	move.b	%d0,(%a0)		/* p0' */

.Lh_next:
.Lh_skip:
	addq.l	#1,%a3
	subq.l	#1,%d6
	bne	.Lh_col

	movem.l	(%sp)+,%d2-%d7/%a3-%a4
	rts

/*
 * ------------------------------------------------------------------------
 * bS<4 luma deblocking edge filter (H.264 spec 8.7.2.3, "Filtering process
 * for edges for bS less than 4") - the weak filter used at every ordinary
 * inter-predicted edge, as opposed to bS==4's macroblock-boundary-only
 * reach. Per a real-Pistorm timing breakdown, bS==4 alone barely moved the
 * aggregate deblock cost because it is the less common of the two - this
 * is the filter that actually dominates the "deblock" bucket for ordinary
 * P/B-heavy content.
 *
 * Structurally different from bS==4 in three ways:
 *
 * 1. Boundary strength is now per-4-sample-group, not a single value for
 *    the whole edge: u4_bs packs four bS values (0-3), one per byte, MSB
 *    first (group 0 = bits 31:24 ... group 3 = bits 7:0). A zero bS means
 *    the entire 4-sample group is skipped outright - not per-sample, the
 *    whole group - matching Ittiam's outer-loop `continue`. tc0 is then a
 *    single byte looked up from the caller-supplied clip table at
 *    pu1_cliptab[bS], shared by all 4 samples in that group.
 * 2. The output is a single clipped delta applied symmetrically to p0/q0
 *    (CLIP_U8(p0+delta) / CLIP_U8(q0-delta)), plus *optional* small
 *    corrections to p1/q1 - not the strong-vs-weak 3-tap/1-tap split.
 * 3. p1'/q1' are written via C's `pu1_src_temp[pos] += CLIP3(...)`, i.e.
 *    an unsigned-byte add-with-implicit-truncation - there is no CLIP_U8
 *    on those two, unlike p0'/q0'. A `move.b` of the full sum replicates
 *    that truncation exactly; adding an extra 0/255 clamp there would be
 *    a bug, not a safety net.
 *
 * Per-sample computation (bS for this group already resolved to tc0):
 *   skip if |p0-q0|>=alpha or |q1-q0|>=beta or |p1-p0|>=beta (as bS==4)
 *   tc = tc0 + (|p2-p0|<beta ? 1 : 0) + (|q2-q0|<beta ? 1 : 0)
 *   delta = CLIP3(-tc, tc, ((q0-p0)<<2 + (p1-q1) + 4) >> 3)
 *   p0' = CLIP_U8(p0+delta) ; q0' = CLIP_U8(q0-delta)
 *   if |p2-p0|<beta: p1 += CLIP3(-tc0, tc0, (p2 + ((p0+q0+1)>>1) - 2*p1) >> 1)
 *   if |q2-q0|<beta: q1 += CLIP3(-tc0, tc0, (q2 + ((p0+q0+1)>>1) - 2*q1) >> 1)
 *
 * Write-ordering note extending the one in the bS==4 functions above: here
 * p1'/q1' need the *original* p0/q0 (not p0'/q0'), so - rather than pin
 * six registers per sample the way bS==4 pins four - every tap below is
 * simply re-read fresh from memory each time it is needed (safe, since
 * nothing is written to p0/q0/p1/q1/p2/q2's memory until the sample's own
 * writes, and p0'/q0' are always written last, after p1'/q1' have already
 * consumed the still-original p0/q0). |p2-p0| and |q2-q0| are each
 * evaluated twice as a result - once to build tc, once more (independently
 * re-derived, not cached) to gate the p1'/q1' branch - a deliberate
 * register-pressure/clarity trade for a filter already dominated by
 * memory traffic and branches, not by these few extra byte compares.
 *
 * void mr_ih264_deblk_luma_vert_bslt4_m68k(UWORD8 *pu1_src, WORD32 src_strd,
 *     WORD32 alpha, WORD32 beta, UWORD32 u4_bs, const UWORD8 *pu1_cliptab);
 * void mr_ih264_deblk_luma_horz_bslt4_m68k(... same signature ...);
 *
 * Six stack arguments, all 4 bytes, starting at 4(%sp). Both functions
 * save %d2-%d7/%a3-%a5 (9 registers, 36 bytes), so stack offsets below are
 * +36 (pu1_src at 40(%sp) = 4+36). %d0/%d1/%a0 are scratch, no save
 * needed. %d6 = outer group counter (4), %d5 = inner sample counter
 * (reloaded to 4 per group), %d4 = tc0 (read-only for the group's 4
 * samples - every other register is free scratch within one sample).
 * ------------------------------------------------------------------------
 */
	.globl	mr_ih264_deblk_luma_vert_bslt4_m68k
	.globl	mr_ih264_deblk_luma_horz_bslt4_m68k

/* ---------------------------------------------------------------------- */
/* Vertical: constant byte displacements from the row cursor, same as     */
/* bS==4's vertical function above (p2=-3,p1=-2,p0=-1,q0=0,q1=1,q2=2).    */
/* ---------------------------------------------------------------------- */
mr_ih264_deblk_luma_vert_bslt4_m68k:
	movem.l	%d2-%d7/%a3-%a5,-(%sp)
	movea.l	40(%sp),%a3		/* pu1_src     (4+36)  */
	move.l	44(%sp),%d0		/* src_strd    (8+36)  */
	movea.l	%d0,%a4
	movea.l	60(%sp),%a5		/* pu1_cliptab (24+36) */
	moveq	#4,%d6			/* group (edge) counter */

.Lvw_edge:
	move.l	%d6,%d0
	subq.l	#1,%d0
	lsl.l	#3,%d0			/* d0 = (group-1)*8: 24,16,8,0 */
	move.l	56(%sp),%d1		/* u4_bs       (20+36) */
	lsr.l	%d0,%d1
	and.l	#0xff,%d1		/* d1 = bS for this group */
	tst.l	%d1
	beq	.Lvw_edge_skip

	clr.l	%d4
	move.b	0(%a5,%d1.l),%d4	/* tc0 = pu1_cliptab[bS] */
	moveq	#4,%d5			/* 4 samples in this group */

.Lvw_row:
	/* ---- filtering decision ---- */
	clr.l	%d0
	move.b	-1(%a3),%d0		/* p0 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lvw_sk1
	neg.l	%d2
.Lvw_sk1:
	move.l	48(%sp),%d3		/* alpha (12+36) */
	cmp.l	%d3,%d2
	bge	.Lvw_skip

	clr.l	%d0
	move.b	1(%a3),%d0		/* q1 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* q1-q0 */
	bpl.s	.Lvw_sk2
	neg.l	%d2
.Lvw_sk2:
	move.l	52(%sp),%d3		/* beta  (16+36) */
	cmp.l	%d3,%d2
	bge	.Lvw_skip

	clr.l	%d0
	move.b	-2(%a3),%d0		/* p1 */
	clr.l	%d1
	move.b	-1(%a3),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lvw_sk3
	neg.l	%d2
.Lvw_sk3:
	cmp.l	%d3,%d2			/* %d3 still beta */
	bge	.Lvw_skip

	/* ---- tc = tc0 + (a_p<beta) + (a_q<beta) ---- */
	move.l	%d4,%d7			/* d7 = tc, starts at tc0 */
	clr.l	%d0
	move.b	-3(%a3),%d0		/* p2 */
	clr.l	%d1
	move.b	-1(%a3),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lvw_ap1
	neg.l	%d2
.Lvw_ap1:
	move.l	52(%sp),%d3		/* beta */
	cmp.l	%d3,%d2
	bge	.Lvw_ap1_no
	addq.l	#1,%d7
.Lvw_ap1_no:
	clr.l	%d0
	move.b	2(%a3),%d0		/* q2 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lvw_aq1
	neg.l	%d2
.Lvw_aq1:
	cmp.l	%d3,%d2			/* %d3 still beta */
	bge	.Lvw_aq1_no
	addq.l	#1,%d7
.Lvw_aq1_no:

	/* ---- val = ((q0-p0)<<2 + (p1-q1) + 4) >> 3 ; delta = clip3(-tc,tc,val) ---- */
	clr.l	%d0
	move.b	(%a3),%d0		/* q0 */
	clr.l	%d1
	move.b	-1(%a3),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* q0-p0 */
	asl.l	#2,%d2
	clr.l	%d0
	move.b	-2(%a3),%d0		/* p1 */
	clr.l	%d1
	move.b	1(%a3),%d1		/* q1 */
	sub.l	%d1,%d0			/* p1-q1 */
	add.l	%d0,%d2
	addq.l	#4,%d2
	asr.l	#3,%d2			/* d2 = val */

	move.l	%d7,%d0
	neg.l	%d0			/* d0 = -tc */
	cmp.l	%d0,%d2
	bge.s	.Lvw_dlo_ok
	move.l	%d0,%d2
.Lvw_dlo_ok:
	cmp.l	%d7,%d2
	ble.s	.Lvw_dhi_ok
	move.l	%d7,%d2
.Lvw_dhi_ok:				/* d2 = delta */

	/* ---- p1' if |p2-p0|<beta (re-derived) ---- */
	clr.l	%d0
	move.b	-3(%a3),%d0		/* p2 */
	clr.l	%d1
	move.b	-1(%a3),%d1		/* p0 */
	move.l	%d0,%d3
	sub.l	%d1,%d3
	bpl.s	.Lvw_ap2
	neg.l	%d3
.Lvw_ap2:
	move.l	52(%sp),%d0		/* beta */
	cmp.l	%d0,%d3
	bge	.Lvw_p1_skip
	clr.l	%d0
	move.b	(%a3),%d0		/* q0 */
	clr.l	%d1
	move.b	-1(%a3),%d1		/* p0 */
	add.l	%d0,%d1
	addq.l	#1,%d1
	asr.l	#1,%d1			/* (p0+q0+1)>>1 */
	clr.l	%d0
	move.b	-3(%a3),%d0		/* p2 */
	add.l	%d0,%d1
	clr.l	%d0
	move.b	-2(%a3),%d0		/* p1 (kept for the final add) */
	move.l	%d0,%d3
	asl.l	#1,%d3
	sub.l	%d3,%d1			/* d1 = val (pre-shift) */
	asr.l	#1,%d1
	move.l	%d4,%d3
	neg.l	%d3
	cmp.l	%d3,%d1
	bge.s	.Lvw_p1lo_ok
	move.l	%d3,%d1
.Lvw_p1lo_ok:
	cmp.l	%d4,%d1
	ble.s	.Lvw_p1hi_ok
	move.l	%d4,%d1
.Lvw_p1hi_ok:
	add.l	%d0,%d1			/* %d0 still = original p1 */
	move.b	%d1,-2(%a3)		/* p1' (byte truncation, no clamp) */
.Lvw_p1_skip:

	/* ---- q1' if |q2-q0|<beta (re-derived) ---- */
	clr.l	%d0
	move.b	2(%a3),%d0		/* q2 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d3
	sub.l	%d1,%d3
	bpl.s	.Lvw_aq2
	neg.l	%d3
.Lvw_aq2:
	move.l	52(%sp),%d0		/* beta */
	cmp.l	%d0,%d3
	bge	.Lvw_q1_skip
	clr.l	%d0
	move.b	-1(%a3),%d0		/* p0 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	add.l	%d0,%d1
	addq.l	#1,%d1
	asr.l	#1,%d1			/* (p0+q0+1)>>1 */
	clr.l	%d0
	move.b	2(%a3),%d0		/* q2 */
	add.l	%d0,%d1
	clr.l	%d0
	move.b	1(%a3),%d0		/* q1 (kept for the final add) */
	move.l	%d0,%d3
	asl.l	#1,%d3
	sub.l	%d3,%d1
	asr.l	#1,%d1
	move.l	%d4,%d3
	neg.l	%d3
	cmp.l	%d3,%d1
	bge.s	.Lvw_q1lo_ok
	move.l	%d3,%d1
.Lvw_q1lo_ok:
	cmp.l	%d4,%d1
	ble.s	.Lvw_q1hi_ok
	move.l	%d4,%d1
.Lvw_q1hi_ok:
	add.l	%d0,%d1			/* %d0 still = original q1 */
	move.b	%d1,1(%a3)		/* q1' */
.Lvw_q1_skip:

	/* ---- p0'/q0' last, using original p0/q0 (still untouched) + delta ---- */
	clr.l	%d0
	move.b	-1(%a3),%d0		/* p0 */
	add.l	%d2,%d0			/* p0+delta */
	bmi.s	.Lvw_p0lo
	cmp.l	#255,%d0
	ble.s	.Lvw_p0store
	move.l	#255,%d0
	bra.s	.Lvw_p0store
.Lvw_p0lo:
	moveq	#0,%d0
.Lvw_p0store:
	move.b	%d0,-1(%a3)		/* p0' */

	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	sub.l	%d2,%d1			/* q0-delta */
	bmi.s	.Lvw_q0lo
	cmp.l	#255,%d1
	ble.s	.Lvw_q0store
	move.l	#255,%d1
	bra.s	.Lvw_q0store
.Lvw_q0lo:
	moveq	#0,%d1
.Lvw_q0store:
	move.b	%d1,(%a3)		/* q0' */

.Lvw_skip:
	adda.l	%a4,%a3
	subq.l	#1,%d5
	bne	.Lvw_row
	bra	.Lvw_edge_next

.Lvw_edge_skip:
	adda.l	%a4,%a3
	adda.l	%a4,%a3
	adda.l	%a4,%a3
	adda.l	%a4,%a3

.Lvw_edge_next:
	subq.l	#1,%d6
	bne	.Lvw_edge

	movem.l	(%sp)+,%d2-%d7/%a3-%a5
	rts

/* ---------------------------------------------------------------------- */
/* Horizontal: identical filter math, samples are src_strd apart instead  */
/* of 1 byte apart - every tap address is recomputed fresh from the       */
/* column cursor (%a3) via %a0, matching the horz bS==4 function above.   */
/* ---------------------------------------------------------------------- */
mr_ih264_deblk_luma_horz_bslt4_m68k:
	movem.l	%d2-%d7/%a3-%a5,-(%sp)
	movea.l	40(%sp),%a3		/* pu1_src     (4+36)  */
	move.l	44(%sp),%d0		/* src_strd    (8+36)  */
	movea.l	%d0,%a4
	movea.l	60(%sp),%a5		/* pu1_cliptab (24+36) */
	moveq	#4,%d6			/* group counter */

.Lhw_edge:
	move.l	%d6,%d0
	subq.l	#1,%d0
	lsl.l	#3,%d0
	move.l	56(%sp),%d1		/* u4_bs (20+36) */
	lsr.l	%d0,%d1
	and.l	#0xff,%d1
	tst.l	%d1
	beq	.Lhw_edge_skip

	clr.l	%d4
	move.b	0(%a5,%d1.l),%d4	/* tc0 */
	moveq	#4,%d5

.Lhw_col:
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lhw_sk1
	neg.l	%d2
.Lhw_sk1:
	move.l	48(%sp),%d3		/* alpha */
	cmp.l	%d3,%d2
	bge	.Lhw_skip

	movea.l	%a3,%a0
	adda.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* q1 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* q1-q0 */
	bpl.s	.Lhw_sk2
	neg.l	%d2
.Lhw_sk2:
	move.l	52(%sp),%d3		/* beta */
	cmp.l	%d3,%d2
	bge	.Lhw_skip

	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p1 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lhw_sk3
	neg.l	%d2
.Lhw_sk3:
	cmp.l	%d3,%d2			/* %d3 still beta */
	bge	.Lhw_skip

	/* ---- tc = tc0 + (a_p<beta) + (a_q<beta) ---- */
	move.l	%d4,%d7
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p2 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lhw_ap1
	neg.l	%d2
.Lhw_ap1:
	move.l	52(%sp),%d3		/* beta */
	cmp.l	%d3,%d2
	bge	.Lhw_ap1_no
	addq.l	#1,%d7
.Lhw_ap1_no:
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* q2 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2
	bpl.s	.Lhw_aq1
	neg.l	%d2
.Lhw_aq1:
	cmp.l	%d3,%d2			/* %d3 still beta */
	bge	.Lhw_aq1_no
	addq.l	#1,%d7
.Lhw_aq1_no:

	/* ---- val = ((q0-p0)<<2 + (p1-q1) + 4) >> 3 ; delta = clip3(-tc,tc,val) ---- */
	clr.l	%d0
	move.b	(%a3),%d0		/* q0 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* p0 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* q0-p0 */
	asl.l	#2,%d2
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p1 */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* q1 */
	sub.l	%d1,%d0			/* p1-q1 */
	add.l	%d0,%d2
	addq.l	#4,%d2
	asr.l	#3,%d2			/* d2 = val */

	move.l	%d7,%d0
	neg.l	%d0
	cmp.l	%d0,%d2
	bge.s	.Lhw_dlo_ok
	move.l	%d0,%d2
.Lhw_dlo_ok:
	cmp.l	%d7,%d2
	ble.s	.Lhw_dhi_ok
	move.l	%d7,%d2
.Lhw_dhi_ok:				/* d2 = delta */

	/* ---- p1' if |p2-p0|<beta (re-derived) ---- */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p2 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* p0 */
	move.l	%d0,%d3
	sub.l	%d1,%d3
	bpl.s	.Lhw_ap2
	neg.l	%d3
.Lhw_ap2:
	move.l	52(%sp),%d0		/* beta */
	cmp.l	%d0,%d3
	bge	.Lhw_p1_skip
	clr.l	%d0
	move.b	(%a3),%d0		/* q0 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d1
	move.b	(%a0),%d1		/* p0 */
	add.l	%d0,%d1
	addq.l	#1,%d1
	asr.l	#1,%d1			/* (p0+q0+1)>>1 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p2 */
	add.l	%d0,%d1
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p1 (kept for final add) */
	move.l	%d0,%d3
	asl.l	#1,%d3
	sub.l	%d3,%d1
	asr.l	#1,%d1
	move.l	%d4,%d3
	neg.l	%d3
	cmp.l	%d3,%d1
	bge.s	.Lhw_p1lo_ok
	move.l	%d3,%d1
.Lhw_p1lo_ok:
	cmp.l	%d4,%d1
	ble.s	.Lhw_p1hi_ok
	move.l	%d4,%d1
.Lhw_p1hi_ok:
	add.l	%d0,%d1			/* %d0 still = original p1 */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	suba.l	%a4,%a0
	move.b	%d1,(%a0)		/* p1' */
.Lhw_p1_skip:

	/* ---- q1' if |q2-q0|<beta (re-derived) ---- */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* q2 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	move.l	%d0,%d3
	sub.l	%d1,%d3
	bpl.s	.Lhw_aq2
	neg.l	%d3
.Lhw_aq2:
	move.l	52(%sp),%d0		/* beta */
	cmp.l	%d0,%d3
	bge	.Lhw_q1_skip
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */
	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	add.l	%d0,%d1
	addq.l	#1,%d1
	asr.l	#1,%d1			/* (p0+q0+1)>>1 */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	adda.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* q2 */
	add.l	%d0,%d1
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* q1 (kept for final add) */
	move.l	%d0,%d3
	asl.l	#1,%d3
	sub.l	%d3,%d1
	asr.l	#1,%d1
	move.l	%d4,%d3
	neg.l	%d3
	cmp.l	%d3,%d1
	bge.s	.Lhw_q1lo_ok
	move.l	%d3,%d1
.Lhw_q1lo_ok:
	cmp.l	%d4,%d1
	ble.s	.Lhw_q1hi_ok
	move.l	%d4,%d1
.Lhw_q1hi_ok:
	add.l	%d0,%d1			/* %d0 still = original q1 */
	movea.l	%a3,%a0
	adda.l	%a4,%a0
	move.b	%d1,(%a0)		/* q1' */
.Lhw_q1_skip:

	/* ---- p0'/q0' last, using original p0/q0 (still untouched) + delta ---- */
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	clr.l	%d0
	move.b	(%a0),%d0		/* p0 */
	add.l	%d2,%d0			/* p0+delta */
	bmi.s	.Lhw_p0lo
	cmp.l	#255,%d0
	ble.s	.Lhw_p0store
	move.l	#255,%d0
	bra.s	.Lhw_p0store
.Lhw_p0lo:
	moveq	#0,%d0
.Lhw_p0store:
	movea.l	%a3,%a0
	suba.l	%a4,%a0
	move.b	%d0,(%a0)		/* p0' */

	clr.l	%d1
	move.b	(%a3),%d1		/* q0 */
	sub.l	%d2,%d1			/* q0-delta */
	bmi.s	.Lhw_q0lo
	cmp.l	#255,%d1
	ble.s	.Lhw_q0store
	move.l	#255,%d1
	bra.s	.Lhw_q0store
.Lhw_q0lo:
	moveq	#0,%d1
.Lhw_q0store:
	move.b	%d1,(%a3)		/* q0' */

.Lhw_skip:
	addq.l	#1,%a3
	subq.l	#1,%d5
	bne	.Lhw_col
	bra	.Lhw_edge_next

.Lhw_edge_skip:
	addq.l	#4,%a3

.Lhw_edge_next:
	subq.l	#1,%d6
	bne	.Lhw_edge

	movem.l	(%sp)+,%d2-%d7/%a3-%a5
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
