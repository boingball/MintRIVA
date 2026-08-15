/*
 * MintVID - hand-tuned m68k fused YUV420P -> 8-bit indexed dither.
 *
 * mr_yuv420_dither8_m68k() implements the exact same contract as the
 * portable C mr_yuv420_dither8() (core/mr_yuv_dither.c): for each output
 * row, pick source row `vscale*oy + vscale/2` (destination-pixel-centre
 * nearest-neighbour, matching mr_scale_resize_rgb24()'s DDA - see that
 * file's header), convert YUV420 -> RGB using the same five pre-multiplied
 * tables as mr_yuv420_to_rgb24_m68k, then dither straight to an 8-bit
 * palette index using the same three per-threshold LUTs as
 * mr_dither_rgb8_m68k - one pass, no intermediate RGB24 buffer.
 *
 * dst_h (= height/vscale) is computed by the C caller and passed in
 * directly, so this never needs a 32-bit divide in its own setup.
 *
 * Per output row: src_row advances by a constant `vscale` each row (exact
 * by definition, regardless of vscale's parity), but chroma_row = src_row
 * >> 1 does NOT advance by a constant amount for odd vscale (floor-division
 * parity), so this recomputes y/u/v row pointers by a fresh MULS.L each row
 * from src_row/chroma_row rather than trying to track an incremental
 * per-row stride - one 32-bit multiply per row is negligible next to the
 * per-pixel work below.
 *
 * Per pixel pair (matching the C's chroma-pair reuse): red_add/green_add/
 * blue_add depend only on (uu, vv), computed once per pair and stashed in
 * stack scratch (same technique as mr_yuv420_to_rgb24_m68k, for the same
 * reason - not enough registers to hold them plus everything else live).
 * The dither tap (Bayer threshold * 256, so it can be added straight into
 * the LUT row/value index) is precomputed once per *pixel* into another
 * scratch word, letting the three-channel LUT sum loop (mirroring
 * mr_dither_rgb8_m68k's indexed lut_r/g/b reads) run with only the clipped
 * channel value and the running sum live in registers.
 *
 * void mr_yuv420_dither8_m68k(const uint8_t *y_plane, int y_stride,
 *     const uint8_t *u_plane, int u_stride, const uint8_t *v_plane,
 *     int v_stride, int width, int dst_h, int vscale, uint8_t *out,
 *     int out_stride, int y_base, const int *luma_x298, const int *e_x409,
 *     const int *d_xm100, const int *e_xm208, const int *d_x516,
 *     const uint8_t *lut_r, const uint8_t *lut_g, const uint8_t *lut_b);
 *
 * m68k C ABI: twenty 4-byte arguments on the stack from 4(%sp). Saves
 * %d2-%d7/%a2-%a6 (44 bytes) plus 20 bytes of local scratch - stack-arg
 * offsets below are +64:
 *   68(sp)=y_plane      100(sp)=vscale       132(sp)=d_x516
 *   72(sp)=y_stride     104(sp)=out          136(sp)=lut_r
 *   76(sp)=u_plane      108(sp)=out_stride   140(sp)=lut_g
 *   80(sp)=u_stride     112(sp)=y_base       144(sp)=lut_b
 *   84(sp)=v_plane      116(sp)=luma_x298
 *   88(sp)=v_stride     120(sp)=e_x409
 *   92(sp)=width        124(sp)=d_xm100
 *   96(sp)=dst_h        128(sp)=e_xm208
 *
 * Scratch (0(sp)-19(sp)): 0=red_add(4) 4=green_add(4) 8=blue_add(4)
 * (all per-pair, longwords) 12=taprow(4, one Bayer byte per x&3 - this
 * row's bayer4[(y_base+oy)&3][0..3], refreshed once per row)
 * 16=tapword(2, this pixel's tap<<8, refreshed once per pixel).
 *
 * Register plan (whole-function, unless noted): %d0=oy, %d1=src_row
 * (running, += vscale each row), %d2=width, %d3=row_parity (running,
 * wraps mod 4). Row-persistent: %a0/%a1/%a2/%a3 = y/u/v/out row pointers.
 * Whole-function: %a5=lut_r, %a6=lut_g (lut_b reloaded via %a4 - only
 * three of the eight tables used often enough to keep dedicated). %d4=x
 * (pixel loop). %d5/%d6/%d7/%a4 = scratch within a pixel/channel, reused
 * freely - see the per-channel block below, mirrored three times per pixel
 * (red/green/blue) and twice per pair (the two lanes), plus once more for
 * the trailing odd pixel.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_yuv420_dither8_m68k

mr_yuv420_dither8_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	sub.l	#20,%sp

	move.l	92(%sp),%d2		/* width */
	tst.l	%d2
	ble	.Lyd_done
	tst.l	96(%sp)			/* dst_h */
	ble	.Lyd_done

	clr.l	%d0			/* oy = 0 */
	move.l	100(%sp),%d1		/* vscale */
	asr.l	#1,%d1			/* d1 = src_row (init = vscale>>1) */

	move.l	112(%sp),%d3		/* y_base */
	and.l	#3,%d3			/* d3 = row_parity */

	movea.l	136(%sp),%a5		/* lut_r (whole-function constant) */
	movea.l	140(%sp),%a6		/* lut_g (whole-function constant) */

.Lyd_row:
	move.l	%d0,%d6
	cmp.l	96(%sp),%d6		/* oy vs dst_h */
	bge	.Lyd_done

	/* y_row_ptr = y_plane + src_row * y_stride */
	move.l	%d1,%d6
	muls.l	72(%sp),%d6
	movea.l	68(%sp),%a0
	adda.l	%d6,%a0

	/* chroma_row = src_row >> 1; u/v row pointers from it */
	move.l	%d1,%d4
	asr.l	#1,%d4

	move.l	%d4,%d6
	muls.l	80(%sp),%d6		/* u_stride */
	movea.l	76(%sp),%a1
	adda.l	%d6,%a1

	move.l	%d4,%d6
	muls.l	88(%sp),%d6		/* v_stride */
	movea.l	84(%sp),%a2
	adda.l	%d6,%a2

	/* out_row_ptr = out + oy * out_stride */
	move.l	%d0,%d6
	muls.l	108(%sp),%d6
	movea.l	104(%sp),%a3
	adda.l	%d6,%a3

	/* taprow[0..3] = bayer4[row_parity][0..3], one longword copy */
	lea	.Lyd_bayer4,%a4
	move.l	%d3,%d6
	lsl.l	#2,%d6
	adda.l	%d6,%a4
	move.l	(%a4),%d6
	move.l	%d6,12(%sp)

	clr.l	%d4			/* x = 0 */

.Lyd_pair_test:
	move.l	%d2,%d6
	subq.l	#1,%d6
	cmp.l	%d6,%d4
	bge	.Lyd_pair_done

	move.l	%d4,%d5
	asr.l	#1,%d5			/* cx = x >> 1 */

	clr.l	%d6
	move.b	(0,%a1,%d5.l),%d6	/* uu */
	movea.l	132(%sp),%a4		/* d_x516 */
	move.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,8(%sp)		/* blue_add */

	movea.l	124(%sp),%a4		/* d_xm100 */
	move.l	(%a4,%d6.l*4),%d7	/* d7 = d_xm100[uu] (green partial) */

	clr.l	%d6
	move.b	(0,%a2,%d5.l),%d6	/* vv */
	movea.l	128(%sp),%a4		/* e_xm208 */
	add.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,4(%sp)		/* green_add */

	movea.l	120(%sp),%a4		/* e_x409 (d6 still holds vv) */
	move.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,0(%sp)		/* red_add */

	/* --- lane 0: pixel x --- */
	clr.l	%d6
	move.b	(0,%a0,%d4.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyd_l0ok
	moveq	#0,%d6
.Lyd_l0ok:
	movea.l	116(%sp),%a4		/* luma_x298 */
	move.l	(%a4,%d6.l*4),%d5	/* d5 = scaled_y */

	move.l	%d4,%d6
	and.l	#3,%d6
	clr.l	%d7
	move.b	(12,%sp,%d6.l),%d7
	lsl.w	#8,%d7
	move.w	%d7,16(%sp)		/* tapword = tap<<8 */

	move.l	%d5,%d6
	add.l	0(%sp),%d6		/* + red_add */
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	move.b	(%a5,%d6.w),%d7		/* d7 = lut_r[idx] */

	move.l	%d5,%d6
	add.l	4(%sp),%d6		/* + green_add */
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	add.b	(%a6,%d6.w),%d7		/* += lut_g[idx] */

	move.l	%d5,%d6
	add.l	8(%sp),%d6		/* + blue_add */
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	movea.l	144(%sp),%a4		/* lut_b */
	add.b	(%a4,%d6.w),%d7		/* += lut_b[idx] */

	move.b	%d7,(0,%a3,%d4.l)	/* out[x] */

	/* --- lane 1: pixel x+1 --- */
	clr.l	%d6
	move.b	(1,%a0,%d4.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyd_l1ok
	moveq	#0,%d6
.Lyd_l1ok:
	movea.l	116(%sp),%a4
	move.l	(%a4,%d6.l*4),%d5

	move.l	%d4,%d6
	addq.l	#1,%d6
	and.l	#3,%d6
	clr.l	%d7
	move.b	(12,%sp,%d6.l),%d7
	lsl.w	#8,%d7
	move.w	%d7,16(%sp)

	move.l	%d5,%d6
	add.l	0(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	move.b	(%a5,%d6.w),%d7

	move.l	%d5,%d6
	add.l	4(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	add.b	(%a6,%d6.w),%d7

	move.l	%d5,%d6
	add.l	8(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	movea.l	144(%sp),%a4
	add.b	(%a4,%d6.w),%d7

	move.b	%d7,(1,%a3,%d4.l)	/* out[x+1] */

	addq.l	#2,%d4
	bra	.Lyd_pair_test

.Lyd_pair_done:
	/* trailing odd column, if any: one more pixel, its own uu/vv */
	cmp.l	%d2,%d4
	bge	.Lyd_tail_done

	move.l	%d4,%d5
	asr.l	#1,%d5

	clr.l	%d6
	move.b	(0,%a1,%d5.l),%d6	/* uu */
	movea.l	132(%sp),%a4
	move.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,8(%sp)

	movea.l	124(%sp),%a4
	move.l	(%a4,%d6.l*4),%d7

	clr.l	%d6
	move.b	(0,%a2,%d5.l),%d6	/* vv */
	movea.l	128(%sp),%a4
	add.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,4(%sp)

	movea.l	120(%sp),%a4
	move.l	(%a4,%d6.l*4),%d7
	add.l	#128,%d7
	move.l	%d7,0(%sp)

	clr.l	%d6
	move.b	(0,%a0,%d4.l),%d6
	sub.l	#16,%d6
	bge.b	.Lyd_tok
	moveq	#0,%d6
.Lyd_tok:
	movea.l	116(%sp),%a4
	move.l	(%a4,%d6.l*4),%d5

	move.l	%d4,%d6
	and.l	#3,%d6
	clr.l	%d7
	move.b	(12,%sp,%d6.l),%d7
	lsl.w	#8,%d7
	move.w	%d7,16(%sp)

	move.l	%d5,%d6
	add.l	0(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	move.b	(%a5,%d6.w),%d7

	move.l	%d5,%d6
	add.l	4(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	add.b	(%a6,%d6.w),%d7

	move.l	%d5,%d6
	add.l	8(%sp),%d6
	asr.l	#8,%d6
	bsr	.Lyd_clip
	add.w	16(%sp),%d6
	movea.l	144(%sp),%a4
	add.b	(%a4,%d6.w),%d7

	move.b	%d7,(0,%a3,%d4.l)

.Lyd_tail_done:
	addq.l	#1,%d0
	addq.l	#1,%d3
	and.l	#3,%d3
	add.l	100(%sp),%d1		/* src_row += vscale */
	bra	.Lyd_row

.Lyd_done:
	add.l	#20,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* Clip %d6 (signed) to 0..255 in place. Only touches %d6 - safe to call
 * from anywhere mid-sequence without disturbing any other live register. */
.Lyd_clip:
	cmp.l	#0,%d6
	bge.b	.Lyd_clip_pos
	moveq	#0,%d6
	rts
.Lyd_clip_pos:
	cmp.l	#255,%d6
	ble.b	.Lyd_clip_ret
	move.l	#255,%d6
.Lyd_clip_ret:
	rts

	.even
.Lyd_bayer4:
	.byte	0,8,2,10
	.byte	12,4,14,6
	.byte	3,11,1,9
	.byte	15,7,13,5

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
