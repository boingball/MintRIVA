/*
 * Hand-written m68k assembly for H.264 chroma motion-compensation
 * interpolation (spec 8.4.2.2.2, "Chroma sample interpolation process").
 * A host callgrind profile of the H.264 test asset showed
 * ih264_inter_pred_chroma at 6.3% of total instructions - the single
 * largest still-untouched leaf filter, bigger than any individual luma MC
 * filter and completely unaddressed until now (only apf_inter_pred_luma[]
 * got hand-asm'd earlier in this port; chroma MC has its own, separate
 * function-pointer slot, pf_inter_pred_chroma, still on Ittiam's generic
 * C).
 *
 * void mr_ih264_inter_pred_chroma_m68k(UWORD8 *pu1_src, UWORD8 *pu1_dst,
 *     WORD32 src_strd, WORD32 dst_strd, WORD32 dx, WORD32 dy, WORD32 ht,
 *     WORD32 wd);
 *
 * U and V are interleaved in memory (UVUVUV...); wd is a count of chroma
 * *sample pairs*, so each row is 2*wd bytes. Every column is a bilinear
 * blend of that sample's own top-left/top-right/bottom-left/bottom-right
 * neighbours in the SAME plane - +2 bytes reaches the next sample of the
 * same plane, exactly like the deblocking filters' chroma interleave.
 *
 * Per Ittiam's C (ih264_inter_pred_filters.c):
 *   tmp = (8-dx)*(8-dy)*s00 + dx*(8-dy)*s10 + (8-dx)*dy*s01 + dx*dy*s11
 *   dst = (tmp + 32) >> 6
 * dx,dy are always in [0,7] (chroma MVs are 1/8-pel), so the four weights
 * (8-dx)*(8-dy), dx*(8-dy), (8-dx)*dy, dx*dy are non-negative and always
 * sum to exactly 8*8=64 - a true weighted average of four samples each in
 * [0,255], so tmp is bounded to [0, 255*64] = [0,16320] and (tmp+32)>>6 is
 * bounded to exactly [0,255] for every possible input, the same
 * "guaranteed by construction, no CLIP_U8 needed" situation as the
 * deblocking filters (see ih264_m68k_deblk.S) - confirmed by the C not
 * calling CLIP_U8 either.
 *
 * The four weights are loop-invariant (dx/dy are fixed for the whole
 * call), so unlike the C - which recomputes all four products for every
 * single pixel - they are computed once up front and kept pinned in
 * registers for the entire function. This is a straightforward, semantics-
 * preserving hoist (same arithmetic, same values, just not redone 2*wd*ht
 * times) rather than any kind of reformulation.
 *
 * Addressing: rather than walking four separate pointers forward by one
 * byte per column (which would need a compensating "back up 2*wd, then
 * add src_strd" step at every row boundary, since src_strd is the full
 * plane stride, not the block width), the row/next-row/dst pointers stay
 * fixed for the whole row and every access goes through d16(An,Dn.l)
 * indexed addressing with a single column-index register - the same
 * pattern m68k GCC itself generates for CABAC's table lookups. Column
 * iteration runs backwards via dbf (index 2*wd-1 downto 0): each output
 * byte depends only on source bytes, never on another output byte in the
 * same call, so direction cannot matter, and dbf's native
 * decrement-and-test-against-(-1) is the natural fit for a runtime-known,
 * small (2, 4, 8, or 16) iteration count.
 *
 * m68k C ABI: all eight arguments are 4 bytes, passed on the stack in
 * order starting at 4(%sp) (same convention as every other function in
 * this port). Saves %d2-%d7/%a2-%a4 (9 registers, 36 bytes) - stack
 * offsets below are +36. %d0/%d1/%a0/%a1 are scratch, no save needed.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_inter_pred_chroma_m68k

mr_ih264_inter_pred_chroma_m68k:
	movem.l	%d2-%d7/%a2-%a4,-(%sp)
	movea.l	40(%sp),%a0		/* pu1_src   (4+36)  */
	movea.l	44(%sp),%a1		/* pu1_dst   (8+36)  */
	movea.l	48(%sp),%a2		/* src_strd  (12+36) */
	movea.l	52(%sp),%a3		/* dst_strd  (16+36) */
	movea.l	%a0,%a4
	adda.l	%a2,%a4			/* a4 = pu1_src + src_strd (next row) */

	/* Weights: w00=(8-dx)*(8-dy), w10=dx*(8-dy), w01=(8-dx)*dy,
	 * w11=dx*dy - computed once, kept pinned as d5/d6/d3/d1 for the
	 * rest of the function. */
	move.l	56(%sp),%d1		/* d1 = dx  (20+36) */
	move.l	60(%sp),%d2		/* d2 = dy  (24+36) */
	moveq	#8,%d5
	sub.l	%d1,%d5			/* d5 = 8-dx */
	moveq	#8,%d4
	sub.l	%d2,%d4			/* d4 = 8-dy */

	move.l	%d5,%d3
	mulu.w	%d2,%d3			/* d3 = (8-dx)*dy      = w01 */
	mulu.w	%d4,%d5			/* d5 = (8-dx)*(8-dy)  = w00 */
	move.l	%d1,%d6
	mulu.w	%d4,%d6			/* d6 = dx*(8-dy)      = w10 */
	mulu.w	%d2,%d1			/* d1 = dx*dy          = w11 */

	move.l	64(%sp),%d0		/* d0 = ht  (28+36) */
	subq.l	#1,%d0			/* preload for dbf (ht >= 1 always) */

.Lchroma_row:
	move.l	68(%sp),%d2		/* d2 = wd  (32+36) */
	add.l	%d2,%d2			/* d2 = 2*wd */
	subq.l	#1,%d2			/* preload for dbf */

.Lchroma_col:
	clr.l	%d4			/* sum = 0 */
	clr.l	%d7
	move.b	(0,%a0,%d2.l),%d7	/* s00 = pu1_src[col] */
	mulu.w	%d5,%d7			/* * w00 */
	add.l	%d7,%d4

	clr.l	%d7
	move.b	(2,%a0,%d2.l),%d7	/* s10 = pu1_src[col+2] */
	mulu.w	%d6,%d7			/* * w10 */
	add.l	%d7,%d4

	clr.l	%d7
	move.b	(0,%a4,%d2.l),%d7	/* s01 = (pu1_src+src_strd)[col] */
	mulu.w	%d3,%d7			/* * w01 */
	add.l	%d7,%d4

	clr.l	%d7
	move.b	(2,%a4,%d2.l),%d7	/* s11 = (pu1_src+src_strd)[col+2] */
	mulu.w	%d1,%d7			/* * w11 */
	add.l	%d7,%d4

	add.l	#32,%d4
	lsr.l	#6,%d4			/* result in [0,255] - see header note */
	move.b	%d4,(0,%a1,%d2.l)	/* pu1_dst[col] */

	dbf	%d2,.Lchroma_col

	adda.l	%a2,%a0			/* row pointers += stride */
	adda.l	%a2,%a4
	adda.l	%a3,%a1

	dbf	%d0,.Lchroma_row

	movem.l	(%sp)+,%d2-%d7/%a2-%a4
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
