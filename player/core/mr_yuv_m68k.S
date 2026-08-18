/*
 * Hand-written m68k assembly for YUV420 -> RGB24 conversion
 * (core/mr_yuv.c's mr_yuv420_to_rgb24), the "rgb-output" stage that runs
 * once per decoded frame before display. A WinUAE/Pistorm log measured
 * this at 6-16ms/frame - real, recurring CPU cost, and unlike the H.264
 * decode path this one is this project's own code (not vendored), so it
 * can be edited directly rather than needing a submodule workaround.
 *
 * m68k-linux-gnu-gcc -O2 -S showed this loop calling emit_pixel() via an
 * indirect jsr (through a register holding its address) *twice per pixel*
 * with five stack-pushed arguments each time, not inlined despite being a
 * small static function - a real, measurable per-pixel call overhead this
 * asm removes by inlining the same arithmetic directly into the loop body.
 *
 * Algorithm (unchanged from the C - see mr_yuv.c's own header comment):
 * U/V are 4:2:0 subsampled, so each chroma sample pair (uu, vv) is shared
 * by two horizontally-adjacent luma samples and covers two picture rows
 * (chroma row = luma row >> 1). Per pixel:
 *   scaled_y = 298 * (Y - 16), Y-16 clamped to >= 0
 *   R = clip8((scaled_y + 409*(V-128)          + 128) >> 8)
 *   G = clip8((scaled_y - 100*(U-128) - 208*(V-128) + 128) >> 8)
 *   B = clip8((scaled_y + 516*(U-128)          + 128) >> 8)
 * All five products are pre-multiplied into 256-entry tables by
 * build_tables() in mr_yuv.c (no fast integer multiplier on 68030 - a
 * table read beats MULU.L/MULS.L for values applied to an 8-bit input);
 * this function only ever indexes those tables, never multiplies.
 *
 * red_add/green_add/blue_add depend only on (uu, vv), which are constant
 * across the two luma samples in a pair - computed once per pair (already
 * true of the C's structure, kept here) and stashed in a small stack-local
 * scratch area so both pixels of the pair can reuse them.  The four image
 * pointers stream through their rows with postincrement addressing.  This
 * avoids recomputing x>>1, x*3 and (x+1)*3 for every pair; only small
 * stride-minus-consumed-byte corrections remain at the end of each row.
 * The luma and blue-add table bases stay resident in a5/a6 for the entire
 * conversion.  They account for three of the six table-base loads in every
 * complete pair, so keeping them out of the stack-fed a4 scratch path removes
 * another three recurring memory loads without changing the arithmetic.
 *
 * Channel clipping keeps the common 0..255 case to one unsigned compare
 * and branch.  Values outside that range are then split by sign into 0 or
 * 255.  This replaces the former pair of signed comparisons on all three
 * channels of every output pixel without changing a single result byte.
 *
 * void mr_yuv420_to_rgb24_m68k(uint8_t *dst, int dst_stride,
 *     const uint8_t *y_plane, int y_stride, const uint8_t *u_plane,
 *     int u_stride, const uint8_t *v_plane, int v_stride, int width,
 *     int height, mr_yuv_service_fn service, void *service_opaque,
 *     const int *luma_x298, const int *e_x409, const int *d_xm100,
 *     const int *e_xm208, const int *d_x516);
 *
 * Only ever called from mr_yuv420_to_rgb24() itself (mr_yuv.c), which
 * already validates the null/size preconditions the C version checks -
 * this function trusts its caller and does not re-check them.
 *
 * m68k C ABI: all seventeen arguments are 4 bytes, passed on the stack in
 * order starting at 4(%sp). Saves %d2/%d4-%d6/%a2-%a6 (9 registers, 36
 * bytes) plus a fixed 12-byte local scratch area (red/green/blue_add) -
 * stack offsets below are +48. %d0/%d1/%a0/%a1 don't need saving *for our
 * caller's sake* (d0=row, d1=pair countdown, a0=src_y stream pointer,
 * a1=dst stream pointer) - but this function is also a caller itself (the
 * periodic service() callback below), and values held in those same four
 * registers that are still needed *after* that call must be explicitly
 * preserved around it, since an ordinary C callee is entitled to clobber
 * any of them. See the .Lyuv_tail_done comment - missing this the first
 * time caused a real-hardware crash.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_yuv420_to_rgb24_m68k

mr_yuv420_to_rgb24_m68k:
	movem.l	%d2/%d4-%d6/%a2-%a6,-(%sp)
	sub.l	#12,%sp			/* 0(sp)=red_add 4(sp)=green_add 8(sp)=blue_add */

	movea.l	60(%sp),%a0		/* a0 = y_plane stream ptr  (12+48) */
	movea.l	52(%sp),%a1		/* a1 = dst stream ptr      (4+48)  */
	movea.l	68(%sp),%a2		/* a2 = u_plane stream ptr  (20+48) */
	movea.l	76(%sp),%a3		/* a3 = v_plane stream ptr  (28+48) */
	move.l	84(%sp),%d2		/* d2 = width               (36+48) */
	movea.l	100(%sp),%a5		/* a5 = luma_x298           (52+48) */
	movea.l	116(%sp),%a6		/* a6 = d_x516              (68+48) */

	clr.l	%d0			/* row = 0 */

.Lyuv_row:
	cmp.l	88(%sp),%d0		/* height (40+48) */
	bge	.Lyuv_done

	move.l	%d2,%d1
	lsr.l	#1,%d1			/* d1 = complete two-pixel pairs */
	beq	.Lyuv_pair_done

.Lyuv_pair_loop:
	clr.l	%d6
	move.b	(%a2)+,%d6		/* d6 = uu; advance one chroma sample */

	move.l	(%a6,%d6.l*4),%d4	/* cached d_x516[uu] */
	add.l	#128,%d4
	move.l	%d4,8(%sp)		/* blue_add */

	movea.l	108(%sp),%a4		/* d_xm100 (60+48) */
	move.l	(%a4,%d6.l*4),%d4	/* d4 = d_xm100[uu] */

	clr.l	%d6
	move.b	(%a3)+,%d6		/* d6 = vv; advance one chroma sample */

	movea.l	112(%sp),%a4		/* e_xm208 (64+48) */
	add.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,4(%sp)		/* green_add */

	movea.l	104(%sp),%a4		/* e_x409 (56+48) */
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,0(%sp)		/* red_add */

	/* pixel 0 (column x) */
	clr.l	%d6
	move.b	(%a0)+,%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_c0ok
	moveq	#0,%d6
.Lyuv_c0ok:
	move.l	(%a5,%d6.l*4),%d5	/* d5 = cached luma_x298[c] */

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	/* pixel 1 (column x+1) */
	clr.l	%d6
	move.b	(%a0)+,%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_c1ok
	moveq	#0,%d6
.Lyuv_c1ok:
	move.l	(%a5,%d6.l*4),%d5

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	subq.l	#1,%d1
	bne	.Lyuv_pair_loop

.Lyuv_pair_done:
	/* trailing odd column, if any: one more Y and one more U/V sample */
	btst	#0,%d2
	beq	.Lyuv_tail_done

	clr.l	%d6
	move.b	(%a2)+,%d6		/* uu */
	move.l	(%a6,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,8(%sp)
	movea.l	108(%sp),%a4
	move.l	(%a4,%d6.l*4),%d4
	clr.l	%d6
	move.b	(%a3)+,%d6		/* vv */
	movea.l	112(%sp),%a4
	add.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,4(%sp)
	movea.l	104(%sp),%a4
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,0(%sp)

	clr.l	%d6
	move.b	(%a0)+,%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_tcok
	moveq	#0,%d6
.Lyuv_tcok:
	move.l	(%a5,%d6.l*4),%d5

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#255,%d4
	bls.b	2f
	tst.l	%d4
	bmi.b	1f
	move.l	#255,%d4
	bra.b	2f
1:	moveq	#0,%d4
2:
	move.b	%d4,(%a1)+

.Lyuv_tail_done:
	/* if (service && (row & 15) == 15) service(service_opaque) */
	move.l	%d0,%d4
	and.l	#15,%d4
	cmp.l	#15,%d4
	bne.b	.Lyuv_no_service
	tst.l	92(%sp)			/* service      (44+48) */
	beq.b	.Lyuv_no_service
	movea.l	92(%sp),%a4		/* load fn ptr before pushing anything else */
	/* d0/a0/a1 are "scratch, caller must not assume preserved" by this
	 * port's own convention (see the header comment) - which is exactly
	 * why they're not in the save-registers list above. That's only
	 * safe for values *this* function doesn't need after a call it
	 * makes itself; row/a0(y ptr)/a1(dst ptr) are read again right
	 * after this jsr returns, so an ordinary C service() - fully
	 * entitled to clobber d0/d1/a0/a1 like any m68k caller-saved
	 * registers - corrupts them silently. Missing this caused a real-
	 * hardware crash (illegal instruction + visible screen corruption)
	 * the very first time this ran with audio servicing actually wired
	 * up instead of NULL/a trivial test stub (see check_yuv_service()
	 * in mr_h264_m68k_check.c, added specifically to keep this covered). */
	movem.l	%d0/%a0-%a1,-(%sp)
	move.l	108(%sp),-(%sp)		/* service_opaque: 96 + 12 (movem push) */
	jsr	(%a4)
	addq.l	#4,%sp
	movem.l	(%sp)+,%d0/%a0-%a1
.Lyuv_no_service:

	/* a0/a1 now point just past width Y bytes / width*3 RGB bytes. */
	move.l	64(%sp),%d4		/* y_stride - width */
	sub.l	%d2,%d4
	adda.l	%d4,%a0
	move.l	%d2,%d5
	add.l	%d2,%d5
	add.l	%d2,%d5			/* d5 = width*3 */
	move.l	56(%sp),%d4		/* dst_stride - width*3 */
	sub.l	%d5,%d4
	adda.l	%d4,%a1

	/* Each chroma sample serves two image rows.  a2/a3 have consumed
	 * ceil(width/2) bytes: rewind after an even row to reuse that chroma
	 * row, or add stride-consumed after an odd row to reach the next one. */
	move.l	%d2,%d5
	addq.l	#1,%d5
	lsr.l	#1,%d5			/* d5 = ceil(width/2) */
	btst	#0,%d0
	bne.b	.Lyuv_chroma_next
	suba.l	%d5,%a2
	suba.l	%d5,%a3
	bra.b	.Lyuv_chroma_done
.Lyuv_chroma_next:
	move.l	72(%sp),%d4
	sub.l	%d5,%d4
	adda.l	%d4,%a2
	move.l	80(%sp),%d4
	sub.l	%d5,%d4
	adda.l	%d4,%a3
.Lyuv_chroma_done:

	addq.l	#1,%d0
	bra	.Lyuv_row

.Lyuv_done:
	add.l	#12,%sp
	movem.l	(%sp)+,%d2/%d4-%d6/%a2-%a6
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
