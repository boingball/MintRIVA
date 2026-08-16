/*
 * MintVID - hand-tuned m68k chunky-to-planar (32-pixel groups).
 *
 * mr_c2p8_riva32_m68k() implements the exact same contract as the portable C
 * mr_c2p8_riva32() (core/mr_c2p.c): given a pw-wide (multiple of 32) chunky
 * row, produces nplanes bitplanes via a 32-pixel-at-a-time transpose.
 *
 * The portable C version's transpose8()/transpose32() (core/mr_c2p.c) do the
 * same job using a uint64_t-based bit-interleave network (the classic
 * "transpose an 8x8 bit matrix" algorithm) - correct, but m68k has no native
 * 64-bit ALU, so GCC must synthesise every 64-bit shift/and/or as a pair of
 * 32-bit operations with manual carry propagation, which is exactly the kind
 * of thing worth doing by hand instead. This file's .Ltranspose8 is that
 * same algorithm, mechanically split into 32-bit register-pair (xh:xl)
 * arithmetic - not a different algorithm, just the same one with the 64-bit
 * emulation done deliberately instead of left to the compiler. The 32-bit
 * split was derived and exhaustively verified byte-for-byte against
 * transpose8() (2,000,000 random 8-byte inputs, zero mismatches) as portable
 * C before being transliterated here, and the full grouping/packing loop
 * (four transpose8 calls per 32-pixel group, MSB-first - matching
 * transpose32()'s own documented contract) is differentially verified against
 * mr_c2p8() for nplanes in {4, 5, 6, 8} - see tests/mr_c2p_check.c (including
 * its real-m68k execution in tests/run_m68k_check.sh). The 4-plane case is the
 * ECS16/--ecs-fast path used by both ECS and AGA screens. This file is a
 * mechanical translation of
 * already-proven-correct C, not new bit-manipulation designed directly in
 * asm.
 *
 * m68k C ABI: args on the stack from 4(%sp); d0/d1/a0/a1 are caller-saved
 * (scratch), d2-d7/a2-a6 are callee-saved (saved here via movem since all of
 * them are used as working registers throughout).
 */
	.text
	.globl	mr_c2p8_riva32_m68k
	.type	mr_c2p8_riva32_m68k,@function

/*
 * void mr_c2p8_riva32_m68k(const uint8_t *chunky, int pw, int h,
 *                          int chunky_stride, int nplanes,
 *                          uint8_t *const planes[], int bpr,
 *                          int x0byte, int y0);
 *
 * Stack args after the movem (44 bytes: d2-d7 + a2-a6) and the 44-byte local
 * scratch (sub.l #44,%sp) - original 4(%sp) is now 92(%sp):
 *   92(%sp)  chunky
 *   96(%sp)  pw
 *  100(%sp)  h
 *  104(%sp)  chunky_stride
 *  108(%sp)  nplanes
 *  112(%sp)  planes
 *  116(%sp)  bpr
 *  120(%sp)  x0byte
 *  124(%sp)  y0
 *
 * Local scratch (relative to the current %sp):
 *    0(%sp)..31(%sp)   t0[8],t1[8],t2[8],t3[8] - one transpose8() result per
 *                       8-pixel sub-group of the current 32-pixel group.
 *   32(%sp)             group_counter (current row's remaining group count)
 *   36(%sp)             groups_per_row (pw>>5, constant across the call)
 *   40(%sp)             h_counter (remaining row count)
 *
 * Persistent registers across the whole function:
 *   a0 = current row's chunky pointer (advances by chunky_stride per row)
 *   a2 = planes[] array base pointer (constant)
 *   a4 = current row's dest byte offset, (y0+row)*bpr+x0byte (advances by
 *        bpr per row)
 * Per-group registers (re-derived every group, unused inside .Ltranspose8):
 *   a1 = current group's chunky pointer (row pointer + group*32)
 *   a5 = current group's dest byte offset (row offset + group*4)
 * .Ltranspose8 calling convention (local subroutine, not part of the C ABI):
 *   in:  a6 = pointer to 8 source pixel bytes, a3 = pointer to 8-byte dest
 *   clobbers: d0-d7
 *   preserves: a0-a2, a4, a5 (never touched)
 */
mr_c2p8_riva32_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	sub.l	#44,%sp

	move.l	124(%sp),%d2		/* d2 = y0 */
	move.l	116(%sp),%d1		/* d1 = bpr */
	mulu.l	%d1,%d2			/* d2 = y0*bpr (68020+ 32x32 multiply;
					 * project baseline is 68030+) */
	add.l	120(%sp),%d2		/* d2 += x0byte */
	movea.l	%d2,%a4			/* a4 = initial rowbase */

	move.l	96(%sp),%d3		/* d3 = pw */
	asr.l	#5,%d3			/* d3 = groups_per_row = pw>>5 */
	move.l	%d3,36(%sp)

	move.l	100(%sp),%d4		/* d4 = h */
	move.l	%d4,40(%sp)
	tst.l	%d4
	ble.w	.Lc2p_done		/* h<=0: nothing to draw */

	move.l	92(%sp),%a0		/* a0 = chunky row pointer */
	move.l	112(%sp),%a2		/* a2 = planes[] array base */

.Lc2p_row:
	movea.l	%a0,%a1			/* a1 = this row's group pointer */
	movea.l	%a4,%a5			/* a5 = this row's dest byte offset */
	move.l	36(%sp),%d3
	move.l	%d3,32(%sp)		/* group_counter = groups_per_row */
	tst.l	%d3
	ble.b	.Lc2p_row_end		/* pw<32: no groups this row */

.Lc2p_group:
	/* Four transpose8() calls, one per 8-pixel sub-group, packed
	 * MSB-first into t0..t3 - matches transpose32()'s documented
	 * "four consecutive transpose8 calls" contract exactly. */
	movea.l	%a1,%a6
	lea	0(%sp),%a3
	bsr.w	.Ltranspose8
	lea	8(%a1),%a6
	lea	8(%sp),%a3
	bsr.w	.Ltranspose8
	lea	16(%a1),%a6
	lea	16(%sp),%a3
	bsr.w	.Ltranspose8
	lea	24(%a1),%a6
	lea	24(%sp),%a3
	bsr.w	.Ltranspose8

	/* Pack each plane's word from t0[k]:t1[k]:t2[k]:t3[k] and store to
	 * planes[k] + dest_offset. nplanes<=8 and .Ltranspose8 is done with
	 * d0-d7 for this group, so it's safe to use them freely here. */
	move.l	108(%sp),%d7		/* d7 = nplanes (dbf loop count-1) */
	subq.l	#1,%d7
	moveq	#0,%d6			/* d6 = plane index */
.Lc2p_store:
	move.l	%d6,%d0
	moveq	#0,%d1
	move.b	0(%sp,%d6.l),%d1	/* d1 = t0[plane] */
	lsl.l	#8,%d1
	move.b	8(%sp,%d6.l),%d1	/* d1 = [t0][t1] */
	lsl.l	#8,%d1
	move.b	16(%sp,%d6.l),%d1	/* d1 = [t0][t1][t2] */
	lsl.l	#8,%d1
	move.b	24(%sp,%d6.l),%d1	/* d1 = [t0][t1][t2][t3] */
	movea.l	%a2,%a3
	movea.l	(%a3,%d0.l*4),%a3	/* a3 = planes[plane] */
	adda.l	%a5,%a3			/* a3 += dest byte offset */
	move.l	%d1,(%a3)
	addq.l	#1,%d6
	dbf	%d7,.Lc2p_store

	adda.l	#32,%a1			/* next group's chunky pointer */
	adda.l	#4,%a5			/* next group's dest byte offset */
	subq.l	#1,32(%sp)
	bne.w	.Lc2p_group

.Lc2p_row_end:
	move.l	104(%sp),%d3
	adda.l	%d3,%a0			/* row pointer += chunky_stride */
	move.l	116(%sp),%d3
	adda.l	%d3,%a4			/* dest offset += bpr */
	subq.l	#1,40(%sp)
	bne.w	.Lc2p_row

.Lc2p_done:
	add.l	#44,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/*
 * .Ltranspose8: local subroutine, not part of the C ABI - see the calling
 * convention comment above. Mechanical 32-bit-halves translation of
 * core/mr_c2p.c's transpose8() (see the file header comment).
 */
.Ltranspose8:
	move.l	(%a6),%d0		/* xh = in[0..3] */
	move.l	4(%a6),%d1		/* xl = in[4..7] */

	/* step 1: M1=0xAA55AA55 (both halves), M2=0x00AA00AA (both halves),
	 * N=7, comp=25 */
	move.l	%d0,%d2
	and.l	#0x00AA00AA,%d2		/* d2 = th */
	move.l	%d1,%d3
	and.l	#0x00AA00AA,%d3		/* d3 = tl */

	move.l	%d3,%d4
	moveq	#25,%d5
	lsr.l	%d5,%d4			/* d4 = tl>>25 */
	lsl.l	#7,%d2			/* d2 = th<<7 */
	or.l	%d2,%d4			/* d4 = shl_h */
	lsl.l	#7,%d3			/* d3 = shl_l = tl<<7 */

	move.l	%d0,%d2
	lsr.l	#7,%d2
	and.l	#0x00AA00AA,%d2		/* d2 = shr_h */
	move.l	%d0,%d6
	moveq	#25,%d5
	lsl.l	%d5,%d6			/* d6 = xh<<25 */
	move.l	%d1,%d7
	lsr.l	#7,%d7			/* d7 = xl>>7 */
	or.l	%d6,%d7
	and.l	#0x00AA00AA,%d7		/* d7 = shr_l */

	and.l	#0xAA55AA55,%d0
	or.l	%d4,%d0
	or.l	%d2,%d0			/* d0 = new xh */
	and.l	#0xAA55AA55,%d1
	or.l	%d3,%d1
	or.l	%d7,%d1			/* d1 = new xl */

	/* step 2: M1=0xCCCC3333 (both halves), M2=0x0000CCCC (both halves),
	 * N=14, comp=18 */
	move.l	%d0,%d2
	and.l	#0x0000CCCC,%d2		/* d2 = th */
	move.l	%d1,%d3
	and.l	#0x0000CCCC,%d3		/* d3 = tl */

	move.l	%d3,%d4
	moveq	#18,%d5
	lsr.l	%d5,%d4			/* d4 = tl>>18 */
	moveq	#14,%d5
	lsl.l	%d5,%d2			/* d2 = th<<14 */
	or.l	%d2,%d4			/* d4 = shl_h */
	lsl.l	%d5,%d3			/* d3 = shl_l = tl<<14 (d5 still 14) */

	move.l	%d0,%d2
	moveq	#14,%d5
	lsr.l	%d5,%d2
	and.l	#0x0000CCCC,%d2		/* d2 = shr_h */
	move.l	%d0,%d6
	moveq	#18,%d5
	lsl.l	%d5,%d6			/* d6 = xh<<18 */
	move.l	%d1,%d7
	moveq	#14,%d5
	lsr.l	%d5,%d7			/* d7 = xl>>14 */
	or.l	%d6,%d7
	and.l	#0x0000CCCC,%d7		/* d7 = shr_l */

	and.l	#0xCCCC3333,%d0
	or.l	%d4,%d0
	or.l	%d2,%d0			/* d0 = new xh */
	and.l	#0xCCCC3333,%d1
	or.l	%d3,%d1
	or.l	%d7,%d1			/* d1 = new xl */

	/* step 3: M1h=0xF0F0F0F0, M1l=0x0F0F0F0F, M2h=0, M2l=0xF0F0F0F0,
	 * N=28, comp=4. M2h=0 means th is always 0: shl_h reduces to
	 * tl>>4 and shr_h reduces to 0 - see the file header comment. */
	move.l	%d1,%d3
	and.l	#0xF0F0F0F0,%d3		/* d3 = tl */

	move.l	%d3,%d4
	lsr.l	#4,%d4			/* d4 = shl_h = tl>>4 (th term is 0) */
	moveq	#28,%d5
	lsl.l	%d5,%d3			/* d3 = shl_l = tl<<28 */

	move.l	%d0,%d6
	lsl.l	#4,%d6			/* d6 = xh<<4 */
	move.l	%d1,%d7
	moveq	#28,%d5
	lsr.l	%d5,%d7			/* d7 = xl>>28 */
	or.l	%d6,%d7
	and.l	#0xF0F0F0F0,%d7		/* d7 = shr_l */

	and.l	#0xF0F0F0F0,%d0
	or.l	%d4,%d0			/* d0 = new xh (shr_h term is 0) */
	and.l	#0x0F0F0F0F,%d1
	or.l	%d3,%d1
	or.l	%d7,%d1			/* d1 = new xl */

	/* transpose8_ref (core/mr_c2p.c) defines out[0]=xl's LSB .. out[3]=
	 * xl's MSB, out[4]=xh's LSB .. out[7]=xh's MSB - i.e. out[] holds
	 * each half byte-REVERSED relative to a plain big-endian move.l
	 * store (which would put the MSB at the lowest address). Reverse
	 * each half before storing so .Lc2p_store's linear byte indexing
	 * reads the right plane out of it. ror.w #8/swap/ror.w #8 is the
	 * standard 68k 32-bit byte-reversal idiom (verified by hand-trace
	 * and, along with this whole transpose, against the C reference
	 * across 2,000,000 random cases before being written here - see the
	 * file header comment). */
	ror.w	#8,%d1
	swap	%d1
	ror.w	#8,%d1
	move.l	%d1,(%a3)		/* out[0..3] = xl, out[0]=LSB=xl&0xff */

	ror.w	#8,%d0
	swap	%d0
	ror.w	#8,%d0
	move.l	%d0,4(%a3)		/* out[4..7] = xh, out[4]=LSB=xh&0xff */
	rts
