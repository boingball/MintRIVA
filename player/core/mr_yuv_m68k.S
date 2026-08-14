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
 * true of the C's structure, kept here) and stashed in a small stack-
 * local scratch area (not registers - there are not enough m68k registers
 * left over once row/column counters, the four source/dest row pointers,
 * and the per-pixel working values are all accounted for) so both pixels
 * of the pair can reuse them without recomputing.
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
 * order starting at 4(%sp). Saves %d2/%d4-%d6/%a2-%a4 (7 registers, 28
 * bytes) plus a fixed 12-byte local scratch area (red/green/blue_add) -
 * stack offsets below are +40. %d0/%d1/%a0/%a1 are scratch, no save
 * needed: d0=row, d1=column x, a0=src_y row pointer, a1=dst row pointer.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_yuv420_to_rgb24_m68k

mr_yuv420_to_rgb24_m68k:
	movem.l	%d2/%d4-%d6/%a2-%a4,-(%sp)
	sub.l	#12,%sp			/* 0(sp)=red_add 4(sp)=green_add 8(sp)=blue_add */

	movea.l	52(%sp),%a0		/* a0 = y_plane row ptr   (12+40) */
	movea.l	44(%sp),%a1		/* a1 = dst row ptr       (4+40)  */
	movea.l	60(%sp),%a2		/* a2 = u_plane row ptr   (20+40) */
	movea.l	68(%sp),%a3		/* a3 = v_plane row ptr   (28+40) */
	move.l	76(%sp),%d2		/* d2 = width             (36+40) */

	clr.l	%d0			/* row = 0 */

.Lyuv_row:
	cmp.l	80(%sp),%d0		/* height (40+40) */
	bge	.Lyuv_done

	clr.l	%d1			/* x = 0 */
	move.l	%d2,%d4
	subq.l	#1,%d4			/* d4 = width-1 (pair-loop bound) */

.Lyuv_pair_test:
	cmp.l	%d4,%d1
	bge	.Lyuv_pair_done

	move.l	%d1,%d5
	asr.l	#1,%d5			/* d5 = cx = x>>1 */

	clr.l	%d6
	move.b	(0,%a2,%d5.l),%d6	/* d6 = uu */

	movea.l	108(%sp),%a4		/* d_x516 (68+40) */
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,8(%sp)		/* blue_add */

	movea.l	100(%sp),%a4		/* d_xm100 (60+40) */
	move.l	(%a4,%d6.l*4),%d4	/* d4 = d_xm100[uu] */

	clr.l	%d6
	move.b	(0,%a3,%d5.l),%d6	/* d6 = vv */

	movea.l	104(%sp),%a4		/* e_xm208 (64+40) */
	add.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,4(%sp)		/* green_add */

	movea.l	96(%sp),%a4		/* e_x409 (56+40) */
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,0(%sp)		/* red_add */

	/* pixel 0 (column x) */
	clr.l	%d6
	move.b	(0,%a0,%d1.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_c0ok
	moveq	#0,%d6
.Lyuv_c0ok:
	movea.l	92(%sp),%a4		/* luma_x298 (52+40) */
	move.l	(%a4,%d6.l*4),%d5	/* d5 = scaled_y */

	move.l	%d1,%d6
	add.l	%d1,%d6
	add.l	%d1,%d6			/* d6 = x*3 */

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_r0a
	moveq	#0,%d4
	bra.b	.Lyuv_r0done
.Lyuv_r0a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_r0done
	move.l	#255,%d4
.Lyuv_r0done:
	move.b	%d4,(0,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_g0a
	moveq	#0,%d4
	bra.b	.Lyuv_g0done
.Lyuv_g0a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_g0done
	move.l	#255,%d4
.Lyuv_g0done:
	move.b	%d4,(1,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_b0a
	moveq	#0,%d4
	bra.b	.Lyuv_b0done
.Lyuv_b0a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_b0done
	move.l	#255,%d4
.Lyuv_b0done:
	move.b	%d4,(2,%a1,%d6.l)

	/* pixel 1 (column x+1) */
	clr.l	%d6
	move.b	(1,%a0,%d1.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_c1ok
	moveq	#0,%d6
.Lyuv_c1ok:
	movea.l	92(%sp),%a4
	move.l	(%a4,%d6.l*4),%d5

	move.l	%d1,%d6
	add.l	%d1,%d6
	add.l	%d1,%d6
	addq.l	#3,%d6			/* d6 = (x+1)*3 */

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_r1a
	moveq	#0,%d4
	bra.b	.Lyuv_r1done
.Lyuv_r1a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_r1done
	move.l	#255,%d4
.Lyuv_r1done:
	move.b	%d4,(0,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_g1a
	moveq	#0,%d4
	bra.b	.Lyuv_g1done
.Lyuv_g1a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_g1done
	move.l	#255,%d4
.Lyuv_g1done:
	move.b	%d4,(1,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_b1a
	moveq	#0,%d4
	bra.b	.Lyuv_b1done
.Lyuv_b1a:
	cmp.l	#255,%d4
	ble.b	.Lyuv_b1done
	move.l	#255,%d4
.Lyuv_b1done:
	move.b	%d4,(2,%a1,%d6.l)

	addq.l	#2,%d1
	move.l	%d2,%d4
	subq.l	#1,%d4			/* restore pair-loop bound (d4 was scratch above) */
	bra	.Lyuv_pair_test

.Lyuv_pair_done:
	/* trailing odd column, if any: one more pixel, its own uu/vv */
	cmp.l	%d2,%d1
	bge	.Lyuv_tail_done

	move.l	%d1,%d5
	asr.l	#1,%d5
	clr.l	%d6
	move.b	(0,%a2,%d5.l),%d6	/* uu */
	movea.l	108(%sp),%a4
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,8(%sp)
	movea.l	100(%sp),%a4
	move.l	(%a4,%d6.l*4),%d4
	clr.l	%d6
	move.b	(0,%a3,%d5.l),%d6	/* vv */
	movea.l	104(%sp),%a4
	add.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,4(%sp)
	movea.l	96(%sp),%a4
	move.l	(%a4,%d6.l*4),%d4
	add.l	#128,%d4
	move.l	%d4,0(%sp)

	clr.l	%d6
	move.b	(0,%a0,%d1.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyuv_tcok
	moveq	#0,%d6
.Lyuv_tcok:
	movea.l	92(%sp),%a4
	move.l	(%a4,%d6.l*4),%d5

	move.l	%d1,%d6
	add.l	%d1,%d6
	add.l	%d1,%d6

	move.l	%d5,%d4
	add.l	0(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_tra
	moveq	#0,%d4
	bra.b	.Lyuv_trdone
.Lyuv_tra:
	cmp.l	#255,%d4
	ble.b	.Lyuv_trdone
	move.l	#255,%d4
.Lyuv_trdone:
	move.b	%d4,(0,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	4(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_tga
	moveq	#0,%d4
	bra.b	.Lyuv_tgdone
.Lyuv_tga:
	cmp.l	#255,%d4
	ble.b	.Lyuv_tgdone
	move.l	#255,%d4
.Lyuv_tgdone:
	move.b	%d4,(1,%a1,%d6.l)

	move.l	%d5,%d4
	add.l	8(%sp),%d4
	asr.l	#8,%d4
	cmp.l	#0,%d4
	bge.b	.Lyuv_tba
	moveq	#0,%d4
	bra.b	.Lyuv_tbdone
.Lyuv_tba:
	cmp.l	#255,%d4
	ble.b	.Lyuv_tbdone
	move.l	#255,%d4
.Lyuv_tbdone:
	move.b	%d4,(2,%a1,%d6.l)

.Lyuv_tail_done:
	/* if (service && (row & 15) == 15) service(service_opaque) */
	move.l	%d0,%d4
	and.l	#15,%d4
	cmp.l	#15,%d4
	bne.b	.Lyuv_no_service
	tst.l	84(%sp)			/* service      (44+40) */
	beq.b	.Lyuv_no_service
	movea.l	84(%sp),%a4
	move.l	88(%sp),-(%sp)		/* service_opaque (48+40) */
	jsr	(%a4)
	addq.l	#4,%sp
.Lyuv_no_service:

	adda.l	56(%sp),%a0		/* y_stride   (16+40) */
	adda.l	48(%sp),%a1		/* dst_stride (8+40)  */

	move.l	%d0,%d4
	and.l	#1,%d4
	beq.b	.Lyuv_no_chroma_adv
	adda.l	64(%sp),%a2		/* u_stride (24+40) */
	adda.l	72(%sp),%a3		/* v_stride (32+40) */
.Lyuv_no_chroma_adv:

	addq.l	#1,%d0
	bra	.Lyuv_row

.Lyuv_done:
	add.l	#12,%sp
	movem.l	(%sp)+,%d2/%d4-%d6/%a2-%a4
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
