/*
 * MintVID - hand-tuned m68k version of pl_mpeg's fixed-point scaled-AAN 8x8
 * IDCT (core/pl_mpeg.h, plm_video_idct).
 *
 * This implements the exact same algorithm as the C version: two passes
 * (columns, then rows) of an 8-point butterfly using the constants 473/196/
 * 362 with (+128)>>8 rounding, operating in place on the same 64-entry
 * `int block[64]` array. It is a fresh implementation of pl_mpeg's own
 * arithmetic, not a port of RiVA's MacrosIDCT68k.m (src/) - RiVA's fixed-
 * point scaling and its own dequant/premultiply convention are calibrated
 * to RiVA's own bitstream pipeline, not pl_mpeg's, and its coefficients are
 * word-sized (16-bit) where pl_mpeg's premultiplied coefficients need the
 * full 32 bits (up to roughly +-127000 after PLM_VIDEO_PREMULTIPLIER_MATRIX,
 * before this transform even runs) - so borrowing RiVA's asm directly would
 * silently truncate and produce wrong pixels rather than fail to build.
 *
 * Both passes share one line-body shape (8 inputs -> 8 outputs), just with a
 * different byte stride between elements of a "line" and, for the row pass
 * only, a final (+128)>>8 rounding shift before each store. Written out
 * twice (not as a shared macro) so each pass reads as a direct, literal,
 * line-by-line match against the C - easier to verify by inspection than
 * chasing macro-substituted displacements, which matters here since a
 * mistake in this function corrupts every block's DC/AC energy silently
 * (drifting error, not a crash - the same failure shape CLAUDE.md calls out
 * for Cinepak).
 *
 * Register plan (both passes): %a0 = current line's base pointer (constant
 * displacements 0/S/2S/.../7S address v0..v7 within it; S is 32 bytes for
 * the column pass, 4 bytes for the row pass - see each pass's own v0..v7
 * offset comments), %a6 = remaining-lines loop counter (8 down to 0; tested
 * with cmpa.l/bne since ADDA/SUBA on an address register do not reliably
 * update the condition codes the way SUBQ+Bcc on a data register would).
 *
 * Within one line: %d0-%d3 are scratch, reused several times as each
 * intermediate (tmp1/tmp2, b6/b4, m0/b1, b3, x1/x2/x3) is computed and
 * consumed. The 8 butterfly values that feed the final 8 output sums/
 * differences (b7, x4, y3, y4, y5, y6, y7, x0) are long-lived across most of
 * the line, so each gets its own dedicated register for the whole line:
 * %d4=y3 %d5=y4 %d6=y7 %d7=x0 %a2=b7 %a3=x4 %a4=y5 %a5=y6. b4 and b6 are
 * each used twice - once early (for x4) and once late (for y7), with most
 * of the line's other work happening in between - so rather than holding
 * two more registers live across that whole gap, they are simply
 * recomputed from block[] a second time right before y7 (block[] itself is
 * not written until every line's very last 8 statements, so re-reading its
 * still-original v1/v3/v5/v7 values later in the same line is safe and
 * matches the C, which also re-reads `block[...]` directly rather than
 * caching b4/b6 across that span).
 *
 * m68k C ABI: args on the stack from 4(%sp); d0/d1/a0/a1 are caller-saved
 * (scratch), d2-d7/a2-a6 are callee-saved. This function uses d2-d7 and
 * a2-a6 (11 registers, 44 bytes) - stack-arg offset below is +44:
 * 48(%sp) = block.
 */
	.text
	.globl	plm_video_idct_m68k
	.type	plm_video_idct_m68k,@function

/* void plm_video_idct_m68k(int *block); */
plm_video_idct_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	48(%sp),%a0

	/* ============ Pass 1: columns (S=32 bytes = 8 ints) ============
	 * v0=0(a0) v1=32(a0) v2=64(a0) v3=96(a0) v4=128(a0) v5=160(a0)
	 * v6=192(a0) v7=224(a0) - i.e. block[k*8+i] for column i, k=0..7.
	 * No final shift: block[k*8+i] = <raw sum/difference>, matching the
	 * C's "Transform columns" loop exactly (its stores have no >>8). */
	move.l	#8,%a6
.Lidct_col_loop:
	move.l	32(%a0),%d0
	add.l	224(%a0),%d0		/* d0 = tmp1 = v1+v7 */
	move.l	96(%a0),%d1
	add.l	160(%a0),%d1		/* d1 = tmp2 = v3+v5 */

	move.l	32(%a0),%d2
	sub.l	224(%a0),%d2		/* d2 = b6 = v1-v7 */
	move.l	160(%a0),%d3
	sub.l	96(%a0),%d3		/* d3 = b4 = v5-v3 */

	move.l	%d0,%a2
	adda.l	%d1,%a2			/* a2 = b7 = tmp1+tmp2 */

	muls.l	#473,%d2		/* d2 = b6*473 */
	muls.l	#196,%d3		/* d3 = b4*196 */
	sub.l	%d3,%d2			/* d2 = b6*473 - b4*196 */
	add.l	#128,%d2
	asr.l	#8,%d2
	move.l	%d2,%a3
	suba.l	%a2,%a3			/* a3 = x4 */

	sub.l	%d1,%d0			/* d0 = tmp1-tmp2 (tmp1 dead after this) */
	muls.l	#362,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%a3,%d7
	sub.l	%d0,%d7			/* d7 = x0 */

	move.l	0(%a0),%d0		/* d0 = m0 = v0 */
	move.l	128(%a0),%d1		/* d1 = b1 = v4 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* d2 = x1 = m0-b1 */
	move.l	%d0,%d3
	add.l	%d1,%d3			/* d3 = x3 = m0+b1 */

	move.l	64(%a0),%d0
	add.l	192(%a0),%d0		/* d0 = b3 = v2+v6 */

	move.l	64(%a0),%d1
	sub.l	192(%a0),%d1		/* d1 = v2-v6 */
	muls.l	#362,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	sub.l	%d0,%d1			/* d1 = x2 */

	move.l	%d2,%d4
	add.l	%d1,%d4			/* d4 = y3 = x1+x2 */
	move.l	%d3,%d5
	add.l	%d0,%d5			/* d5 = y4 = x3+b3 */
	move.l	%d2,%a4
	suba.l	%d1,%a4			/* a4 = y5 = x1-x2 */
	move.l	%d3,%a5
	suba.l	%d0,%a5			/* a5 = y6 = x3-b3 */

	/* Recompute b4/b6 (block[] unwritten so far this line - safe). */
	move.l	160(%a0),%d0
	sub.l	96(%a0),%d0		/* d0 = b4 = v5-v3 */
	move.l	32(%a0),%d1
	sub.l	224(%a0),%d1		/* d1 = b6 = v1-v7 */
	muls.l	#473,%d0
	muls.l	#196,%d1
	add.l	%d1,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d7,%d6
	neg.l	%d6
	sub.l	%d0,%d6			/* d6 = y7 = -x0 - (...) */

	move.l	%a2,%d0
	add.l	%d5,%d0
	move.l	%d0,0(%a0)		/* block[0] = b7+y4 */
	move.l	%d5,%d1
	sub.l	%a2,%d1
	move.l	%d1,224(%a0)		/* block[7] = y4-b7 */

	move.l	%a3,%d0
	add.l	%d4,%d0
	move.l	%d0,32(%a0)		/* block[1] = x4+y3 */
	move.l	%d4,%d1
	sub.l	%a3,%d1
	move.l	%d1,192(%a0)		/* block[6] = y3-x4 */

	move.l	%a4,%d0
	sub.l	%d7,%d0
	move.l	%d0,64(%a0)		/* block[2] = y5-x0 */
	move.l	%d7,%d1
	add.l	%a4,%d1
	move.l	%d1,160(%a0)		/* block[5] = x0+y5 */

	move.l	%a5,%d0
	sub.l	%d6,%d0
	move.l	%d0,96(%a0)		/* block[3] = y6-y7 */
	move.l	%a5,%d1
	add.l	%d6,%d1
	move.l	%d1,128(%a0)		/* block[4] = y6+y7 */

	adda.l	#4,%a0			/* next column */
	subq.l	#1,%a6
	cmpa.l	#0,%a6
	bne	.Lidct_col_loop

	/* ============ Pass 2: rows (S=4 bytes = 1 int) ============
	 * v0=0(a0) v1=4(a0) v2=8(a0) v3=12(a0) v4=16(a0) v5=20(a0)
	 * v6=24(a0) v7=28(a0) - i.e. block[k+i] for row i, k=0..7. Every
	 * store gets the extra (+128)>>8 the C's "Transform rows" loop
	 * applies that the column pass does not. */
	move.l	48(%sp),%a0		/* back to block[0] for the row pass */
	move.l	#8,%a6
.Lidct_row_loop:
	move.l	4(%a0),%d0
	add.l	28(%a0),%d0		/* d0 = tmp1 = v1+v7 */
	move.l	12(%a0),%d1
	add.l	20(%a0),%d1		/* d1 = tmp2 = v3+v5 */

	move.l	4(%a0),%d2
	sub.l	28(%a0),%d2		/* d2 = b6 = v1-v7 */
	move.l	20(%a0),%d3
	sub.l	12(%a0),%d3		/* d3 = b4 = v5-v3 */

	move.l	%d0,%a2
	adda.l	%d1,%a2			/* a2 = b7 = tmp1+tmp2 */

	muls.l	#473,%d2
	muls.l	#196,%d3
	sub.l	%d3,%d2			/* d2 = b6*473 - b4*196 */
	add.l	#128,%d2
	asr.l	#8,%d2
	move.l	%d2,%a3
	suba.l	%a2,%a3			/* a3 = x4 */

	sub.l	%d1,%d0			/* d0 = tmp1-tmp2 */
	muls.l	#362,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%a3,%d7
	sub.l	%d0,%d7			/* d7 = x0 */

	move.l	0(%a0),%d0		/* d0 = m0 = v0 */
	move.l	16(%a0),%d1		/* d1 = b1 = v4 */
	move.l	%d0,%d2
	sub.l	%d1,%d2			/* d2 = x1 */
	move.l	%d0,%d3
	add.l	%d1,%d3			/* d3 = x3 */

	move.l	8(%a0),%d0
	add.l	24(%a0),%d0		/* d0 = b3 = v2+v6 */

	move.l	8(%a0),%d1
	sub.l	24(%a0),%d1		/* d1 = v2-v6 */
	muls.l	#362,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	sub.l	%d0,%d1			/* d1 = x2 */

	move.l	%d2,%d4
	add.l	%d1,%d4			/* d4 = y3 */
	move.l	%d3,%d5
	add.l	%d0,%d5			/* d5 = y4 */
	move.l	%d2,%a4
	suba.l	%d1,%a4			/* a4 = y5 */
	move.l	%d3,%a5
	suba.l	%d0,%a5			/* a5 = y6 */

	move.l	20(%a0),%d0
	sub.l	12(%a0),%d0		/* d0 = b4 (recomputed) */
	move.l	4(%a0),%d1
	sub.l	28(%a0),%d1		/* d1 = b6 (recomputed) */
	muls.l	#473,%d0
	muls.l	#196,%d1
	add.l	%d1,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d7,%d6
	neg.l	%d6
	sub.l	%d0,%d6			/* d6 = y7 */

	move.l	%a2,%d0
	add.l	%d5,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d0,0(%a0)		/* block[0] = (b7+y4+128)>>8 */
	move.l	%d5,%d1
	sub.l	%a2,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	move.l	%d1,28(%a0)		/* block[7] = (y4-b7+128)>>8 */

	move.l	%a3,%d0
	add.l	%d4,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d0,4(%a0)		/* block[1] = (x4+y3+128)>>8 */
	move.l	%d4,%d1
	sub.l	%a3,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	move.l	%d1,24(%a0)		/* block[6] = (y3-x4+128)>>8 */

	move.l	%a4,%d0
	sub.l	%d7,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d0,8(%a0)		/* block[2] = (y5-x0+128)>>8 */
	move.l	%d7,%d1
	add.l	%a4,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	move.l	%d1,20(%a0)		/* block[5] = (x0+y5+128)>>8 */

	move.l	%a5,%d0
	sub.l	%d6,%d0
	add.l	#128,%d0
	asr.l	#8,%d0
	move.l	%d0,12(%a0)		/* block[3] = (y6-y7+128)>>8 */
	move.l	%a5,%d1
	add.l	%d6,%d1
	add.l	#128,%d1
	asr.l	#8,%d1
	move.l	%d1,16(%a0)		/* block[4] = (y6+y7+128)>>8 */

	adda.l	#32,%a0			/* next row */
	subq.l	#1,%a6
	cmpa.l	#0,%a6
	bne	.Lidct_row_loop

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
