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

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
