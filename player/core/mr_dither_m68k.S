/*
 * MintVID - hand-tuned m68k RGB24 -> indexed palette dither (4 pixels at a
 * time, matching the 4x4 Bayer pattern's period).
 *
 * mr_dither_rgb8_m68k() implements the exact same contract as the portable
 * C mr_dither_rgb8() (core/mr_dither.c): per pixel, look up a dithering
 * threshold from the 4x4 Bayer matrix (cyclic in both x and y with period
 * 4), then sum three precomputed per-threshold/per-channel LUT bytes
 * (lut_r[t][r] + lut_g[t][g] + lut_b[t][b] - see that file's build_lut()
 * for how those tables fold the threshold offset, selected 4/5/8-plane cube
 * quantisation and channel weighting into a single byte each, so this loop is
 * but loads and adds, no per-pixel multiply/divide).
 *
 * What this buys over `gcc -O2 -m68030` on the portable C: GCC re-derives
 * `t = bayer4[y&3][x&3]` from a genuine 2D array index every pixel (an AND
 * plus a real 2D lookup) and re-finds each LUT sub-table's base address
 * (t*256, i.e. t<<8, plus the table's own base) three times per pixel, once
 * per channel. Since t only takes 4 distinct values across any row and
 * cycles with period 4, this instead precomputes the row's 4 threshold
 * bytes into a local table once per row and reads that same table by a
 * *constant* displacement (0/1/2/3(%a6)) inside four unrolled per-lane
 * pixel bodies - no per-pixel AND, no per-pixel 2D indexing arithmetic. The
 * LUT loads themselves use m68k's indexed addressing mode (`t<<8 + value`
 * folded into one `(%aN,%dM.w)` operand) instead of a separately computed
 * pointer.
 *
 * lut_r/lut_g/lut_b are `static uint8_t [16][256]` in mr_dither.c - kept
 * `static` there (internal linkage) and passed here as plain pointers
 * rather than given external linkage, so this file only ever sees them as
 * opaque 4096-byte tables, exactly like every other cross-file boundary in
 * this project that keeps vendored/private data static and passes a
 * pointer instead.
 *
 * m68k C ABI: args on the stack from 4(%sp); d0/d1/a0/a1 are caller-saved
 * (scratch), d2-d7/a2-a6 are callee-saved (saved here via movem since most
 * of them are used as working registers throughout).
 */
	.text
	.globl	mr_dither_rgb8_m68k
	.type	mr_dither_rgb8_m68k,@function

/*
 * void mr_dither_rgb8_m68k(const uint8_t *rgb, int w, int h, int rgb_stride,
 *                          uint8_t *out, int out_stride, int y_base,
 *                          const uint8_t *lut_r, const uint8_t *lut_g,
 *                          const uint8_t *lut_b);
 *
 * Saves %d2-%d7/%a2-%a6 (44 bytes) - stack-arg offsets below are +44:
 *   48(%sp)=rgb         64(%sp)=out          80(%sp)=lut_g
 *   52(%sp)=w           68(%sp)=out_stride   84(%sp)=lut_b
 *   56(%sp)=h           72(%sp)=y_base
 *   60(%sp)=rgb_stride  76(%sp)=lut_r
 *
 * Register plan: %a0 = src pixel cursor (post-increment through the row's
 * RGB24 triplets), %a1 = dst pixel cursor (post-increment through the
 * row's index bytes), %a2/%a3/%a4 = lut_r/lut_g/lut_b bases (whole
 * function), %a5 = local Bayer table base (whole function), %a6 = this
 * row's 4-byte threshold sub-array (recomputed once per row). %d0 = row
 * parity 0..3 (whole function, wraps every row), %d1 = w (whole function),
 * %d2 = h row counter (whole function), %d3/%d5/%d6/%d7 = scratch (group
 * counter between rows, tap/index/sum within a pixel - never needed
 * simultaneously across those two roles). %d4 unused but still saved, to
 * match this project's uniform movem set.
 */
mr_dither_rgb8_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	move.l	56(%sp),%d2		/* h */
	tst.l	%d2
	ble	.Ldither_done
	move.l	52(%sp),%d1		/* w */
	tst.l	%d1
	ble	.Ldither_done

	move.l	48(%sp),%a0		/* rgb row cursor */
	move.l	64(%sp),%a1		/* out row cursor */
	move.l	76(%sp),%a2		/* lut_r */
	move.l	80(%sp),%a3		/* lut_g */
	move.l	84(%sp),%a4		/* lut_b */
	lea	.Lbayer4,%a5		/* local 4x4 Bayer table base */

	move.l	72(%sp),%d0		/* y_base */
	and.l	#3,%d0			/* d0 = starting row parity */

.Ldither_row:
	move.l	%d0,%d3
	lsl.l	#2,%d3			/* d3 = row_parity * 4 */
	lea	(%a5,%d3.l),%a6		/* a6 = this row's 4-byte tap array */

	move.l	%d1,%d3
	lsr.l	#2,%d3			/* d3 = w >> 2 (4-pixel groups) */
	beq	.Ldither_row_tail

.Ldither_row_group:
	/* lane 0: tap = 0(%a6) */
	clr.l	%d5
	move.b	(%a6),%d5
	lsl.w	#8,%d5
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	move.b	(%a2,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a3,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a4,%d6.w),%d7
	move.b	%d7,(%a1)+

	/* lane 1: tap = 1(%a6) */
	clr.l	%d5
	move.b	1(%a6),%d5
	lsl.w	#8,%d5
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	move.b	(%a2,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a3,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a4,%d6.w),%d7
	move.b	%d7,(%a1)+

	/* lane 2: tap = 2(%a6) */
	clr.l	%d5
	move.b	2(%a6),%d5
	lsl.w	#8,%d5
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	move.b	(%a2,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a3,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a4,%d6.w),%d7
	move.b	%d7,(%a1)+

	/* lane 3: tap = 3(%a6) */
	clr.l	%d5
	move.b	3(%a6),%d5
	lsl.w	#8,%d5
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	move.b	(%a2,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a3,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a4,%d6.w),%d7
	move.b	%d7,(%a1)+

	subq.l	#1,%d3
	bne	.Ldither_row_group

.Ldither_row_tail:
	move.l	%d1,%d3
	and.l	#3,%d3			/* d3 = w & 3, remaining pixels (0..2) */
	beq.s	.Ldither_row_next
	moveq	#0,%d4			/* d4 = runtime lane index 0/1/2 */
.Ldither_row_tail_px:
	clr.l	%d5
	move.b	(%a6,%d4.l),%d5
	lsl.w	#8,%d5
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	move.b	(%a2,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a3,%d6.w),%d7
	clr.l	%d6
	move.b	(%a0)+,%d6
	add.w	%d5,%d6
	add.b	(%a4,%d6.w),%d7
	move.b	%d7,(%a1)+
	addq.l	#1,%d4
	subq.l	#1,%d3
	bne.s	.Ldither_row_tail_px

.Ldither_row_next:
	/* a0/a1 have walked exactly 3*w / w bytes through this row via the
	 * pixel loops' own post-increments; advance the rest of the way to
	 * the next row (rgb_stride/out_stride may be wider than the packed
	 * row data, e.g. caller padding). */
	move.l	%d1,%d5
	add.l	%d1,%d5
	add.l	%d1,%d5			/* d5 = 3*w */
	move.l	60(%sp),%d6		/* rgb_stride */
	sub.l	%d5,%d6
	adda.l	%d6,%a0
	move.l	68(%sp),%d6		/* out_stride */
	sub.l	%d1,%d6
	adda.l	%d6,%a1

	addq.l	#1,%d0
	and.l	#3,%d0			/* wrap row parity 0..3 */
	subq.l	#1,%d2
	bne	.Ldither_row

.Ldither_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

	.even
.Lbayer4:
	.byte	0,8,2,10
	.byte	12,4,14,6
	.byte	3,11,1,9
	.byte	15,7,13,5
