/*
 * Explicit H.264 weighted prediction for m68k (spec 8.4.2.3.2).
 *
 * These four functions replace libavc's scalar C leaves for weighted P and
 * B prediction.  The 360p PiStorm profile used to choose this work measured
 * about 1,500 calls to these leaves per decoded frame, so keeping the packed
 * weight/offset unpacking outside the pixel loops and using tight pointer
 * walks matters considerably more here than it would in a cold helper.
 *
 * Arithmetic is deliberately 68060-safe.  A source sample is an unsigned
 * byte and every weight is a signed 16-bit value, so a signed word multiply
 * produces the exact required signed 32-bit product.  Do not replace the
 * MULS.W instructions with the register-pair-result MULS.L form: the latter
 * is not implemented by the 68060 and is serviced through an emulation trap.
 *
 * Chroma follows libavc's ABI exactly: wd counts interleaved U/V pairs,
 * weights are packed as V in bits 31:16 and U in bits 15:0, and offsets are
 * packed as V in bits 15:8 and U in bits 7:0.  All shifts are arithmetic,
 * matching WORD32 right shift on the target.  Saturation is performed inline
 * so the hot pixel loops never call a clipping helper.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_weighted_pred_luma_m68k
	.globl	mr_ih264_weighted_pred_chroma_m68k
	.globl	mr_ih264_weighted_bi_pred_luma_m68k
	.globl	mr_ih264_weighted_bi_pred_chroma_m68k

/* void weighted_pred_luma(src, dst, src_stride, dst_stride, log_wd,
 *                         wt, offset, ht, wd)
 * a6 argument offsets: 8,12,16,20,24,28,32,36,40. */
mr_ih264_weighted_pred_luma_m68k:
	link.w	%a6,#0
	movem.l	%d2-%d7/%a2-%a3,-(%sp)
	movea.l	8(%a6),%a2
	movea.l	12(%a6),%a3
	move.l	36(%a6),%d7		/* rows */
	move.l	40(%a6),%d6		/* bytes per row */
	tst.l	%d7
	ble	.Lwpl_done
	tst.l	%d6
	ble	.Lwpl_done

	move.l	24(%a6),%d5		/* log_wd / shift */
	move.l	28(%a6),%d4
	ext.l	%d4			/* signed low-word weight */
	move.l	32(%a6),%d3
	extb.l	%d3			/* signed low-byte offset */
	tst.l	%d5
	beq.s	.Lwpl_bias_ready
	lsl.l	%d5,%d3			/* offset << log_wd */
	moveq	#1,%d0
	lsl.l	%d5,%d0
	lsr.l	#1,%d0			/* 1 << (log_wd - 1) */
	add.l	%d0,%d3
.Lwpl_bias_ready:
.Lwpl_row:
	movea.l	%a2,%a0
	movea.l	%a3,%a1
	move.l	%d6,%d2
.Lwpl_col:
	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d4,%d0
	add.l	%d3,%d0
	asr.l	%d5,%d0			/* register count 0 is a no-op */
	cmp.l	#255,%d0
	bls.s	.Lwpl_store		/* unsigned catches the common 0..255 case */
	tst.l	%d0
	bmi.s	.Lwpl_clip_lo
	move.l	#255,%d0
	bra.s	.Lwpl_store
.Lwpl_clip_lo:
	moveq	#0,%d0
.Lwpl_store:
	move.b	%d0,(%a1)+
	subq.l	#1,%d2
	bne.s	.Lwpl_col
	adda.l	16(%a6),%a2
	adda.l	20(%a6),%a3
	subq.l	#1,%d7
	bne.s	.Lwpl_row
.Lwpl_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a3
	unlk	%a6
	rts

/* void weighted_pred_chroma(src, dst, src_stride, dst_stride, log_wd,
 *                           packed_wt, packed_offset, ht, pair_wd) */
mr_ih264_weighted_pred_chroma_m68k:
	link.w	%a6,#-4			/* -4(a6): remaining rows */
	movem.l	%d2-%d7/%a2-%a3,-(%sp)
	movea.l	8(%a6),%a2
	movea.l	12(%a6),%a3
	move.l	36(%a6),-4(%a6)
	move.l	40(%a6),%d6		/* U/V pairs per row */
	tst.l	-4(%a6)
	ble	.Lwpc_done
	tst.l	%d6
	ble	.Lwpc_done

	move.l	24(%a6),%d5
	move.l	28(%a6),%d0
	move.w	%d0,%d4
	ext.l	%d4			/* U weight */
	swap	%d0
	move.w	%d0,%d3
	ext.l	%d3			/* V weight */

	move.l	32(%a6),%d0
	move.l	%d0,%d2
	extb.l	%d2			/* U offset/bias */
	asr.l	#8,%d0
	extb.l	%d0
	move.l	%d0,%d1			/* V offset/bias */
	tst.l	%d5
	beq.s	.Lwpc_bias_ready
	lsl.l	%d5,%d2
	lsl.l	%d5,%d1
	moveq	#1,%d0
	lsl.l	%d5,%d0
	lsr.l	#1,%d0
	add.l	%d0,%d2
	add.l	%d0,%d1
.Lwpc_bias_ready:
.Lwpc_row:
	movea.l	%a2,%a0
	movea.l	%a3,%a1
	move.l	%d6,%d7
.Lwpc_pair:
	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d4,%d0
	add.l	%d2,%d0
	asr.l	%d5,%d0
	cmp.l	#255,%d0
	bls.s	.Lwpc_u_store
	tst.l	%d0
	bmi.s	.Lwpc_u_lo
	move.l	#255,%d0
	bra.s	.Lwpc_u_store
.Lwpc_u_lo:
	moveq	#0,%d0
.Lwpc_u_store:
	move.b	%d0,(%a1)+

	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d3,%d0
	add.l	%d1,%d0
	asr.l	%d5,%d0
	cmp.l	#255,%d0
	bls.s	.Lwpc_v_store
	tst.l	%d0
	bmi.s	.Lwpc_v_lo
	move.l	#255,%d0
	bra.s	.Lwpc_v_store
.Lwpc_v_lo:
	moveq	#0,%d0
.Lwpc_v_store:
	move.b	%d0,(%a1)+
	subq.l	#1,%d7
	bne.s	.Lwpc_pair

	adda.l	16(%a6),%a2
	adda.l	20(%a6),%a3
	subq.l	#1,-4(%a6)
	bne.s	.Lwpc_row
.Lwpc_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a3
	unlk	%a6
	rts

/* void weighted_bi_pred_luma(src1, src2, dst, stride1, stride2,
 *    dst_stride, log_wd, wt1, wt2, offset1, offset2, ht, wd)
 * a6 argument offsets: 8..56. */
mr_ih264_weighted_bi_pred_luma_m68k:
	link.w	%a6,#-4			/* -4(a6): remaining rows */
	movem.l	%d2-%d7/%a2-%a5,-(%sp)
	movea.l	8(%a6),%a2
	movea.l	12(%a6),%a3
	movea.l	16(%a6),%a4
	move.l	52(%a6),-4(%a6)
	move.l	56(%a6),%d6
	tst.l	-4(%a6)
	ble	.Lwbpl_done
	tst.l	%d6
	ble	.Lwbpl_done

	move.l	32(%a6),%d5
	addq.l	#1,%d5			/* final right shift */
	move.l	36(%a6),%d4
	ext.l	%d4
	move.l	40(%a6),%d3
	ext.l	%d3

	move.l	44(%a6),%d0
	extb.l	%d0
	move.l	48(%a6),%d1
	extb.l	%d1
	add.l	%d1,%d0
	addq.l	#1,%d0
	asr.l	#1,%d0			/* (offset1 + offset2 + 1) >> 1 */
	lsl.l	%d5,%d0
	moveq	#1,%d2
	lsl.l	%d5,%d2
	lsr.l	#1,%d2			/* 1 << log_wd */
	add.l	%d0,%d2			/* complete rounding/offset bias */

.Lwbpl_row:
	movea.l	%a2,%a0
	movea.l	%a3,%a1
	movea.l	%a4,%a5
	move.l	%d6,%d7
.Lwbpl_col:
	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d4,%d0
	moveq	#0,%d1
	move.b	(%a1)+,%d1
	muls.w	%d3,%d1
	add.l	%d1,%d0
	add.l	%d2,%d0
	asr.l	%d5,%d0
	cmp.l	#255,%d0
	bls.s	.Lwbpl_store
	tst.l	%d0
	bmi.s	.Lwbpl_clip_lo
	move.l	#255,%d0
	bra.s	.Lwbpl_store
.Lwbpl_clip_lo:
	moveq	#0,%d0
.Lwbpl_store:
	move.b	%d0,(%a5)+
	subq.l	#1,%d7
	bne.s	.Lwbpl_col
	adda.l	20(%a6),%a2
	adda.l	24(%a6),%a3
	adda.l	28(%a6),%a4
	subq.l	#1,-4(%a6)
	bne.s	.Lwbpl_row
.Lwbpl_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a5
	unlk	%a6
	rts

/* Bi-pred chroma.  Registers d4/d3 hold source-1/source-2 U weights,
 * d2/d6 the corresponding V weights, d5 the shift, and -8/-12(a6) the
 * precomputed U/V biases. */
mr_ih264_weighted_bi_pred_chroma_m68k:
	link.w	%a6,#-12			/* rows, U bias, V bias */
	movem.l	%d2-%d7/%a2-%a5,-(%sp)
	move.l	52(%a6),-4(%a6)
	tst.l	-4(%a6)
	ble	.Lwbpc_done
	tst.l	56(%a6)
	ble	.Lwbpc_done

	move.l	32(%a6),%d5
	addq.l	#1,%d5

	/* U bias. */
	move.l	44(%a6),%d0
	extb.l	%d0
	move.l	48(%a6),%d1
	extb.l	%d1
	add.l	%d1,%d0
	addq.l	#1,%d0
	asr.l	#1,%d0
	lsl.l	%d5,%d0
	moveq	#1,%d7
	lsl.l	%d5,%d7
	lsr.l	#1,%d7
	add.l	%d7,%d0
	move.l	%d0,-8(%a6)

	/* V bias. */
	move.l	44(%a6),%d0
	asr.l	#8,%d0
	extb.l	%d0
	move.l	48(%a6),%d1
	asr.l	#8,%d1
	extb.l	%d1
	add.l	%d1,%d0
	addq.l	#1,%d0
	asr.l	#1,%d0
	lsl.l	%d5,%d0
	add.l	%d7,%d0
	move.l	%d0,-12(%a6)

	/* Packed signed U/V weights. */
	move.l	36(%a6),%d0
	move.w	%d0,%d4
	ext.l	%d4			/* wt1 U */
	swap	%d0
	move.w	%d0,%d2
	ext.l	%d2			/* wt1 V */
	move.l	40(%a6),%d0
	move.w	%d0,%d3
	ext.l	%d3			/* wt2 U */
	swap	%d0
	move.w	%d0,%d6
	ext.l	%d6			/* wt2 V */

	movea.l	8(%a6),%a2
	movea.l	12(%a6),%a3
	movea.l	16(%a6),%a4
.Lwbpc_row:
	movea.l	%a2,%a0
	movea.l	%a3,%a1
	movea.l	%a4,%a5
	move.l	56(%a6),%d7		/* U/V pairs */
.Lwbpc_pair:
	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d4,%d0
	moveq	#0,%d1
	move.b	(%a1)+,%d1
	muls.w	%d3,%d1
	add.l	%d1,%d0
	add.l	-8(%a6),%d0
	asr.l	%d5,%d0
	cmp.l	#255,%d0
	bls.s	.Lwbpc_u_store
	tst.l	%d0
	bmi.s	.Lwbpc_u_lo
	move.l	#255,%d0
	bra.s	.Lwbpc_u_store
.Lwbpc_u_lo:
	moveq	#0,%d0
.Lwbpc_u_store:
	move.b	%d0,(%a5)+

	moveq	#0,%d0
	move.b	(%a0)+,%d0
	muls.w	%d2,%d0
	moveq	#0,%d1
	move.b	(%a1)+,%d1
	muls.w	%d6,%d1
	add.l	%d1,%d0
	add.l	-12(%a6),%d0
	asr.l	%d5,%d0
	cmp.l	#255,%d0
	bls.s	.Lwbpc_v_store
	tst.l	%d0
	bmi.s	.Lwbpc_v_lo
	move.l	#255,%d0
	bra.s	.Lwbpc_v_store
.Lwbpc_v_lo:
	moveq	#0,%d0
.Lwbpc_v_store:
	move.b	%d0,(%a5)+
	subq.l	#1,%d7
	bne.s	.Lwbpc_pair

	adda.l	20(%a6),%a2
	adda.l	24(%a6),%a3
	adda.l	28(%a6),%a4
	subq.l	#1,-4(%a6)
	bne	.Lwbpc_row
.Lwbpc_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a5
	unlk	%a6
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
