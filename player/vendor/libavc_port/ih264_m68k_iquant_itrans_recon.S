/*
 * Hand-written m68k assembly for H.264's 4x4 inverse quantisation + inverse
 * transform + reconstruction (Ittiam's ih264_iquant_itrans_recon_4x4/_dc and
 * ih264_iquant_itrans_recon_chroma_4x4/_dc in ih264_iquant_itrans_recon.c).
 * These are plain function-pointer fields on dec_struct_t (pf_iquant_itrans_
 * recon_luma_4x4[/_dc], pf_iquant_itrans_recon_chroma_4x4[/_dc]) assigned
 * from ih264d_function_selector_port.c, a different file than any
 * definition, so - like apf_inter_pred_luma[] - they are overridden
 * directly; no GNU ld --wrap trick is needed here.
 *
 * The four functions share two kinds of structure, so this file factors
 * that sharing into `bsr`-callable subroutines and shared tail blocks
 * (reached via plain `bra`, since they are the literal tail of more than
 * one entry point) rather than writing each function out fully flat:
 *
 *  - ih264_iquant_itrans_recon_4x4 and its chroma counterpart both run a
 *    horizontal inverse-quant pass (4 rows x 4 taps, each tap dequantised
 *    via the INV_QUANT macro) into a caller-provided WORD16 pi2_tmp[16]
 *    scratch buffer, then a vertical inverse-transform + reconstruction
 *    pass (4 columns, butterfly the tmp values, round+clip+add-pred,
 *    store). They differ only in how column 0's first row DC tap is
 *    obtained (luma: always dequantise then conditionally overwrite from
 *    pi2_dc_ld_addr; chroma: always take pi2_dc_src directly, skipping
 *    dequant for that one tap) and in the output/pred pointer step per
 *    column (luma: 1 byte; chroma: 2 bytes, interleaved UV). The dequant
 *    (`.Linv_quant`) and vertical-pass (`.Lvert_recon_tail`/`.Lrecon_row`)
 *    logic is shared between them.
 *
 *  - ih264_iquant_itrans_recon_4x4_dc and its chroma counterpart both just
 *    broadcast one already-computed DC pixel value, clipped and added to
 *    each of the 16 prediction samples, with the same 1-byte-vs-2-byte
 *    column step distinction. Shared via `.Ldc_broadcast_tail`/
 *    `.Ldc_clip_store`.
 *
 * INV_QUANT (ih264_trans_macros.h): value = ((raw*iscal*weight + rnd) <<
 * qp_div_6) >> 4, all in WORD32 (matching -fwrapv's defined-overflow
 * multiply/shift semantics - see ih264_m68k_optim.c's own header comment
 * for why this project builds with -fwrapv). rnd_fact = qp_div_6 < 4 ?
 * 1<<(3-qp_div_6) : 0. u4_qp_div_6 is UWORD32, so its "< 4" compare uses
 * the unsigned condition codes (BCC/BCS), not BGE/BLT.
 *
 * WORD16 truncation: x0..x3 (the horizontal pass's per-row butterfly
 * outputs) and i_macro (the final rounded/pre-clip pixel value) are all
 * C `WORD16` variables assigned from a wider `int` expression - genuinely
 * truncating on overflow, not just "doesn't matter in practice", since a
 * malformed/adversarial bitstream can drive q0..q3 large enough that
 * their sums exceed 16 bits before rounding. `ext.l %dn` after computing
 * a WORD16-typed sum/difference into %dn's low word replicates that exact
 * truncate-then-sign-extend semantic (EXT.L reads only the current low 16
 * bits of %dn and sign-extends them to 32, discarding whatever was above)
 * - this is applied at every point the C source assigns to a WORD16.
 * q0..q3 themselves stay WORD32 throughout (never truncated) exactly as
 * declared in the vendored C, since x2/x3's `q1 >> 1`/`q3 >> 1` operate on
 * the *full* un-truncated dequantised value.
 *
 * m68k C ABI: all arguments are 4 bytes wide, passed on the stack in
 * declaration order starting at 4(%sp) pre-push. Each function documents
 * its own post-push offsets in its header comment below, since the four
 * signatures differ in argument count (11 for the luma pair - they carry
 * iq_start_idx/pi2_dc_ld_addr - vs 10 for the chroma pair, which carries
 * pi2_dc_src instead) and two of the four allocate an extra 16-byte local
 * frame for the horizontal pass's q0..q3 scratch.
 *
 * Assembled only when MR_M68K_ASM is enabled, same convention as every
 * other file in this directory. Host builds preprocess this file to an
 * empty translation unit.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264_iquant_itrans_recon_4x4_m68k
	.globl	mr_ih264_iquant_itrans_recon_4x4_dc_m68k
	.globl	mr_ih264_iquant_itrans_recon_chroma_4x4_m68k
	.globl	mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k

/* ---------------------------------------------------------------------- */
/* .Lcompute_rnd: rnd_fact = (qp_div_6 < 4) ? 1 << (3 - qp_div_6) : 0.     */
/* Input: %d6 = qp_div_6 (UWORD32, unsigned compare). Output: %d7 =       */
/* rnd_fact. Clobbers %d0 only. bsr-callable, leaf (no further calls).    */
/* ---------------------------------------------------------------------- */
.Lcompute_rnd:
	move.l	%d6,%d0
	cmp.l	#4,%d0
	bcc.s	.Lcompute_rnd_zero
	moveq	#3,%d7
	sub.l	%d0,%d7
	moveq	#1,%d0
	lsl.l	%d7,%d0
	move.l	%d0,%d7
	rts
.Lcompute_rnd_zero:
	moveq	#0,%d7
	rts

/* ---------------------------------------------------------------------- */
/* .Linv_quant: one INV_QUANT() dequant. Input: %d0 = raw coefficient     */
/* (sign-extended WORD16 -> WORD32), %d1 = iscal (zero-extended UWORD16), */
/* %d2 = weight (zero-extended UWORD16); %d6 = qp_div_6, %d7 = rnd_fact   */
/* (both persistent, set once by the caller/.Lcompute_rnd, not consumed). */
/* Output: %d0 = dequantised value (WORD32, untruncated). Clobbers %d0    */
/* only - %d1/%d2/%d6/%d7 all survive unread-back, so a caller that still */
/* needs its %d1/%d2 inputs after the call must have already used them.   */
/* bsr-callable, leaf. qbits is fixed at 4 for every 4x4 call site here,  */
/* so it is baked in as an immediate rather than passed.                  */
/* ---------------------------------------------------------------------- */
.Linv_quant:
	muls.l	%d1,%d0
	muls.l	%d2,%d0
	add.l	%d7,%d0
	lsl.l	%d6,%d0
	asr.l	#4,%d0
	rts

/* ---------------------------------------------------------------------- */
/* .Lrecon_row: round+clip+store one reconstructed pixel, advance both    */
/* cursors by one row. Input: %d6 = raw x_a+x_b sum (WORD16-untruncated), */
/* %a4 = pred cursor, %a2 = out cursor, %a0 = pred_strd (value, not a     */
/* pointer - added via ADDA), %a1 = out_strd (value). Output: writes      */
/* *%a2, advances %a4 += pred_strd, %a2 += out_strd. Clobbers %d6/%d7.    */
/* bsr-callable, leaf.                                                    */
/* ---------------------------------------------------------------------- */
.Lrecon_row:
	ext.l	%d6			/* truncate x_a+x_b to WORD16 (i_macro's
					 * first C assignment), matching the
					 * vendored `i_macro = x0 + x3;` step */
	add.l	#32,%d6
	asr.l	#6,%d6
	clr.l	%d7
	move.b	(%a4),%d7
	add.l	%d7,%d6
	bmi.s	.Lrecon_row_lo
	cmp.l	#255,%d6
	ble.s	.Lrecon_row_store
	move.l	#255,%d6
	bra.s	.Lrecon_row_store
.Lrecon_row_lo:
	moveq	#0,%d6
.Lrecon_row_store:
	move.b	%d6,(%a2)
	adda.l	%a0,%a4
	adda.l	%a1,%a2
	rts

/* ---------------------------------------------------------------------- */
/* .Ldc_clip_store: round(already done by caller)+clip+store one pixel,   */
/* advance both cursors by one row. Input: %d0 = i_macro (already         */
/* rounded/truncated WORD16 value, persistent - NOT clobbered), %a4 =     */
/* pred cursor, %a2 = out cursor, %a0 = pred_strd (value), %a1 = out_strd */
/* (value). Clobbers %d6. bsr-callable, leaf.                             */
/* ---------------------------------------------------------------------- */
.Ldc_clip_store:
	clr.l	%d6
	move.b	(%a4),%d6
	add.l	%d0,%d6
	bmi.s	.Ldc_clip_lo
	cmp.l	#255,%d6
	ble.s	.Ldc_clip_store_st
	move.l	#255,%d6
	bra.s	.Ldc_clip_store_st
.Ldc_clip_lo:
	moveq	#0,%d6
.Ldc_clip_store_st:
	move.b	%d6,(%a2)
	adda.l	%a0,%a4
	adda.l	%a1,%a2
	rts

/* ---------------------------------------------------------------------- */
/* .Lvert_recon_tail: shared vertical-pass tail for the two full-4x4-     */
/* transform entry points (luma and chroma). Entry state: %a3 = pi2_tmp   */
/* (element 0, horizontal pass already populated all 16 WORD16s), %a5 =   */
/* pu1_pred column base, %a6 = pu1_out column base, %a0 = pred_strd       */
/* (value), %a1 = out_strd (value), %d4 = column step in bytes (1 luma /  */
/* 2 chroma, interleaved), %d5 = column counter (4). Reached via `bra`    */
/* (not `bsr`) since it is the literal tail of the calling function - the */
/* RTS at .Lrecon_done returns to whatever originally called that         */
/* function. Falls through to .Lrecon_done, which restores the callee-    */
/* saved registers AND frees the 16-byte q0..q3 local frame both full-4x4 */
/* callers allocate (DC callers never reach this label).                  */
/* ---------------------------------------------------------------------- */
.Lvert_recon_tail:
	movea.l	%a5,%a4			/* row cursor = column base */
	movea.l	%a6,%a2

	move.w	(%a3),%d0
	ext.l	%d0
	move.w	16(%a3),%d1		/* pi2_tmp_ptr[8] (byte offset 8*2) */
	ext.l	%d1
	move.l	%d0,%d2
	add.l	%d1,%d2
	ext.l	%d2			/* x0 */
	sub.l	%d1,%d0
	ext.l	%d0			/* x1 */

	move.w	8(%a3),%d1		/* pi2_tmp_ptr[4] */
	ext.l	%d1
	move.l	%d1,%d6			/* backup tmp[4]; d6 free here (its
					 * .Lrecon_row scratch role hasn't
					 * started yet this column) */
	move.w	24(%a3),%d3		/* pi2_tmp_ptr[12] */
	ext.l	%d3
	asr.l	#1,%d1
	sub.l	%d3,%d1
	ext.l	%d1			/* x2 */
	asr.l	#1,%d3
	add.l	%d6,%d3
	ext.l	%d3			/* x3 */

	move.l	%d2,%d6
	add.l	%d3,%d6
	bsr	.Lrecon_row		/* row0: x0+x3 */

	move.l	%d0,%d6
	add.l	%d1,%d6
	bsr	.Lrecon_row		/* row1: x1+x2 */

	move.l	%d0,%d6
	sub.l	%d1,%d6
	bsr	.Lrecon_row		/* row2: x1-x2 */

	move.l	%d2,%d6
	sub.l	%d3,%d6
	bsr	.Lrecon_row		/* row3: x0-x3 */

	adda.l	#2,%a3			/* pi2_tmp_ptr++ (1 WORD16 element) */
	adda.l	%d4,%a5
	adda.l	%d4,%a6
	subq.l	#1,%d5
	bne	.Lvert_recon_tail

.Lrecon_done:
	add.l	#16,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* .Ldc_broadcast_tail: shared broadcast tail for the two DC-only entry   */
/* points. Entry state: %d0 = i_macro (already rounded/truncated,         */
/* persistent - .Ldc_clip_store never touches it), %a5 = pu1_pred column  */
/* base, %a6 = pu1_out column base, %a0 = pred_strd (value), %a1 =        */
/* out_strd (value), %d4 = column step in bytes (1 luma / 2 chroma), %d5  */
/* = column counter (4). Reached via `bra`. Neither DC entry point        */
/* allocates the 16-byte local frame (they never need q0..q3 scratch      */
/* beyond a single register), so this epilogue is a plain movem/rts, not  */
/* .Lrecon_done's frame-freeing variant.                                  */
/* ---------------------------------------------------------------------- */
.Ldc_broadcast_tail:
	movea.l	%a5,%a4
	movea.l	%a6,%a2
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	adda.l	%d4,%a5
	adda.l	%d4,%a6
	subq.l	#1,%d5
	bne	.Ldc_broadcast_tail

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_4x4_m68k(WORD16 *pi2_src,            */
/*     UWORD8 *pu1_pred, UWORD8 *pu1_out, WORD32 pred_strd,               */
/*     WORD32 out_strd, const UWORD16 *pu2_iscal_mat,                     */
/*     const UWORD16 *pu2_weigh_mat, UWORD32 u4_qp_div_6, WORD16 *pi2_tmp,*/
/*     WORD32 iq_start_idx, WORD16 *pi2_dc_ld_addr);                      */
/*                                                                          */
/* Saves %d2-%d7/%a2-%a6 (44 bytes) then allocates a further 16-byte      */
/* local frame for the horizontal pass's q0..q3 scratch (freed by         */
/* .Lrecon_done) - stack-arg offsets below are +60 (44+16):               */
/*   64(%sp)=pi2_src        84(%sp)=pu2_iscal_mat   100(%sp)=iq_start_idx */
/*   68(%sp)=pu1_pred       88(%sp)=pu2_weigh_mat    104(%sp)=pi2_dc_ld_  */
/*   72(%sp)=pu1_out        92(%sp)=u4_qp_div_6              addr         */
/*   76(%sp)=pred_strd      96(%sp)=pi2_tmp                              */
/*   80(%sp)=out_strd                                                     */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_4x4_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	sub.l	#16,%sp

	move.l	92(%sp),%d6		/* u4_qp_div_6 */
	bsr	.Lcompute_rnd		/* %d7 = rnd_fact */

	move.l	64(%sp),%a2		/* pi2_src_ptr row cursor */
	move.l	96(%sp),%a3		/* pi2_tmp_ptr row cursor */
	move.l	84(%sp),%a4		/* pu2_iscal_mat row cursor */
	move.l	88(%sp),%a5		/* pu2_weigh_mat row cursor */
	moveq	#4,%d5			/* row counter */

.Lq4_row:
	/* tap 0: always dequantise, then (row 0 only, iq_start_idx!=0)
	 * overwrite from pi2_dc_ld_addr - matches the vendored "compute
	 * then conditionally override" order exactly. */
	clr.l	%d1
	clr.l	%d2
	move.w	(%a4),%d1
	move.w	(%a5),%d2
	move.w	(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,(%sp)		/* q0 */
	cmp.l	#4,%d5
	bne.s	.Lq4_row0_done
	move.l	100(%sp),%d0		/* iq_start_idx */
	tst.l	%d0
	beq.s	.Lq4_row0_done
	move.l	104(%sp),%a0		/* pi2_dc_ld_addr (free here - not
					 * used elsewhere yet this row) */
	move.w	(%a0),%d0
	ext.l	%d0
	move.l	%d0,(%sp)		/* overwrite q0 */
.Lq4_row0_done:

	clr.l	%d1
	clr.l	%d2
	move.w	2(%a4),%d1
	move.w	2(%a5),%d2
	move.w	2(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,4(%sp)		/* q1 */

	clr.l	%d1
	clr.l	%d2
	move.w	4(%a4),%d1
	move.w	4(%a5),%d2
	move.w	4(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,8(%sp)		/* q2 */

	clr.l	%d1
	clr.l	%d2
	move.w	6(%a4),%d1
	move.w	6(%a5),%d2
	move.w	6(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,12(%sp)		/* q3 */

	move.l	(%sp),%d0		/* q0 */
	move.l	8(%sp),%d1		/* q2 */
	move.l	%d0,%d2
	add.l	%d1,%d2
	ext.l	%d2			/* x0 */
	sub.l	%d1,%d0
	ext.l	%d0			/* x1 */

	move.l	4(%sp),%d1		/* q1 */
	movea.l	%d1,%a0			/* backup - dc-override use of a0
					 * this row (if any) is done by now */
	move.l	12(%sp),%d3		/* q3 */
	asr.l	#1,%d1
	sub.l	%d3,%d1
	ext.l	%d1			/* x2 */
	asr.l	#1,%d3
	add.l	%a0,%d3
	ext.l	%d3			/* x3 */

	move.l	%d2,%d4
	add.l	%d3,%d4
	move.w	%d4,(%a3)		/* tmp[0] = x0+x3 */
	move.l	%d0,%d4
	add.l	%d1,%d4
	move.w	%d4,2(%a3)		/* tmp[1] = x1+x2 */
	move.l	%d0,%d4
	sub.l	%d1,%d4
	move.w	%d4,4(%a3)		/* tmp[2] = x1-x2 */
	move.l	%d2,%d4
	sub.l	%d3,%d4
	move.w	%d4,6(%a3)		/* tmp[3] = x0-x3 */

	adda.l	#8,%a2
	adda.l	#8,%a3
	adda.l	#8,%a4
	adda.l	#8,%a5
	subq.l	#1,%d5
	bne	.Lq4_row

	move.l	96(%sp),%a3		/* pi2_tmp, reset to element 0 */
	move.l	68(%sp),%a5		/* pu1_pred column base */
	move.l	72(%sp),%a6		/* pu1_out column base */
	move.l	76(%sp),%a0		/* pred_strd value */
	move.l	80(%sp),%a1		/* out_strd value */
	moveq	#1,%d4			/* column step: 1 byte (luma) */
	moveq	#4,%d5
	bra	.Lvert_recon_tail

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_4x4_dc_m68k(...) - same 11-arg       */
/* signature as above. Saves only %d2-%d7/%a2-%a6 (44 bytes, no extra     */
/* local frame - i_macro lives in one register throughout):               */
/*   48(%sp)=pi2_src        68(%sp)=pu2_iscal_mat    84(%sp)=iq_start_idx */
/*   52(%sp)=pu1_pred       72(%sp)=pu2_weigh_mat    88(%sp)=pi2_dc_ld_   */
/*   56(%sp)=pu1_out        76(%sp)=u4_qp_div_6              addr         */
/*   60(%sp)=pred_strd      80(%sp)=pi2_tmp (unused)                     */
/*   64(%sp)=out_strd                                                     */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_4x4_dc_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)

	move.l	84(%sp),%d0		/* iq_start_idx */
	tst.l	%d0
	bne.s	.Ldcl_use_addr

	move.l	76(%sp),%d6		/* u4_qp_div_6 */
	bsr	.Lcompute_rnd		/* %d7 = rnd_fact */
	move.l	48(%sp),%a2		/* pi2_src */
	clr.l	%d1
	clr.l	%d2
	move.l	68(%sp),%a3		/* pu2_iscal_mat */
	move.w	(%a3),%d1
	move.l	72(%sp),%a3		/* pu2_weigh_mat */
	move.w	(%a3),%d2
	move.w	(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	bra.s	.Ldcl_have_q0

.Ldcl_use_addr:
	move.l	88(%sp),%a2		/* pi2_dc_ld_addr */
	move.w	(%a2),%d0
	ext.l	%d0

.Ldcl_have_q0:
	add.l	#32,%d0
	asr.l	#6,%d0
	ext.l	%d0			/* i_macro */

	move.l	60(%sp),%a0		/* pred_strd value */
	move.l	64(%sp),%a1		/* out_strd value */
	move.l	52(%sp),%a5		/* pred column base */
	move.l	56(%sp),%a6		/* out column base */
	moveq	#1,%d4			/* column step: 1 byte (luma) */
	moveq	#4,%d5
	bra	.Ldc_broadcast_tail

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_chroma_4x4_m68k(WORD16 *pi2_src,     */
/*     UWORD8 *pu1_pred, UWORD8 *pu1_out, WORD32 pred_strd,               */
/*     WORD32 out_strd, const UWORD16 *pu2_iscal_mat,                     */
/*     const UWORD16 *pu2_weigh_mat, UWORD32 u4_qp_div_6, WORD16 *pi2_tmp,*/
/*     WORD16 *pi2_dc_src); - 10 args (no iq_start_idx slot; pi2_dc_src   */
/* takes pi2_dc_ld_addr's place and is read *instead of* dequantising row */
/* 0's first tap, not as a post-hoc override like the luma variant).      */
/* Same 44+16 byte prefix as the luma full-transform function above -     */
/* offsets: 64=pi2_src, 68=pu1_pred, 72=pu1_out, 76=pred_strd,            */
/* 80=out_strd, 84=pu2_iscal_mat, 88=pu2_weigh_mat, 92=u4_qp_div_6,        */
/* 96=pi2_tmp, 100=pi2_dc_src.                                            */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_chroma_4x4_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	sub.l	#16,%sp

	move.l	92(%sp),%d6		/* u4_qp_div_6 */
	bsr	.Lcompute_rnd		/* %d7 = rnd_fact */

	move.l	64(%sp),%a2		/* pi2_src_ptr row cursor */
	move.l	96(%sp),%a3		/* pi2_tmp_ptr row cursor */
	move.l	84(%sp),%a4		/* pu2_iscal_mat row cursor */
	move.l	88(%sp),%a5		/* pu2_weigh_mat row cursor */
	moveq	#4,%d5			/* row counter */

.Lqc_row:
	cmp.l	#4,%d5
	bne.s	.Lqc_row_normal
	/* row 0's tap 0: pi2_dc_src[0] directly, dequant skipped entirely */
	move.l	100(%sp),%a0		/* pi2_dc_src */
	move.w	(%a0),%d0
	ext.l	%d0
	move.l	%d0,(%sp)		/* q0 */
	bra.s	.Lqc_row_k1
.Lqc_row_normal:
	clr.l	%d1
	clr.l	%d2
	move.w	(%a4),%d1
	move.w	(%a5),%d2
	move.w	(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,(%sp)		/* q0 */
.Lqc_row_k1:
	clr.l	%d1
	clr.l	%d2
	move.w	2(%a4),%d1
	move.w	2(%a5),%d2
	move.w	2(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,4(%sp)		/* q1 */

	clr.l	%d1
	clr.l	%d2
	move.w	4(%a4),%d1
	move.w	4(%a5),%d2
	move.w	4(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,8(%sp)		/* q2 */

	clr.l	%d1
	clr.l	%d2
	move.w	6(%a4),%d1
	move.w	6(%a5),%d2
	move.w	6(%a2),%d0
	ext.l	%d0
	bsr	.Linv_quant
	move.l	%d0,12(%sp)		/* q3 */

	move.l	(%sp),%d0		/* q0 */
	move.l	8(%sp),%d1		/* q2 */
	move.l	%d0,%d2
	add.l	%d1,%d2
	ext.l	%d2			/* x0 */
	sub.l	%d1,%d0
	ext.l	%d0			/* x1 */

	move.l	4(%sp),%d1		/* q1 */
	movea.l	%d1,%a0
	move.l	12(%sp),%d3		/* q3 */
	asr.l	#1,%d1
	sub.l	%d3,%d1
	ext.l	%d1			/* x2 */
	asr.l	#1,%d3
	add.l	%a0,%d3
	ext.l	%d3			/* x3 */

	move.l	%d2,%d4
	add.l	%d3,%d4
	move.w	%d4,(%a3)		/* tmp[0] = x0+x3 */
	move.l	%d0,%d4
	add.l	%d1,%d4
	move.w	%d4,2(%a3)		/* tmp[1] = x1+x2 */
	move.l	%d0,%d4
	sub.l	%d1,%d4
	move.w	%d4,4(%a3)		/* tmp[2] = x1-x2 */
	move.l	%d2,%d4
	sub.l	%d3,%d4
	move.w	%d4,6(%a3)		/* tmp[3] = x0-x3 */

	adda.l	#8,%a2
	adda.l	#8,%a3
	adda.l	#8,%a4
	adda.l	#8,%a5
	subq.l	#1,%d5
	bne	.Lqc_row

	move.l	96(%sp),%a3		/* pi2_tmp, reset to element 0 */
	move.l	68(%sp),%a5		/* pu1_pred column base */
	move.l	72(%sp),%a6		/* pu1_out column base */
	move.l	76(%sp),%a0		/* pred_strd value */
	move.l	80(%sp),%a1		/* out_strd value */
	moveq	#2,%d4			/* column step: 2 bytes (chroma UV) */
	moveq	#4,%d5
	bra	.Lvert_recon_tail

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k(WORD16 *pi2_src,  */
/*     UWORD8 *pu1_pred, UWORD8 *pu1_out, WORD32 pred_strd,               */
/*     WORD32 out_strd, const UWORD16 *pu2_iscal_mat,                     */
/*     const UWORD16 *pu2_weigh_mat, UWORD32 u4_qp_div_6, WORD16 *pi2_tmp,*/
/*     WORD16 *pi2_dc_src); - simplest of the four: pi2_dc_src[0] is the  */
/* whole story, no dequant ever runs (pi2_src/pu2_iscal_mat/pu2_weigh_    */
/* mat/u4_qp_div_6/pi2_tmp are UNUSED() in the vendored C). Same 44-byte  */
/* (movem only) prefix as the luma DC function - offsets: 48=pi2_src      */
/* (unused), 52=pu1_pred, 56=pu1_out, 60=pred_strd, 64=out_strd,          */
/* 68=pu2_iscal_mat (unused), 72=pu2_weigh_mat (unused), 76=u4_qp_div_6   */
/* (unused), 80=pi2_tmp (unused), 84=pi2_dc_src.                          */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_chroma_4x4_dc_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)

	move.l	84(%sp),%a2		/* pi2_dc_src */
	move.w	(%a2),%d0
	ext.l	%d0
	add.l	#32,%d0
	asr.l	#6,%d0
	ext.l	%d0			/* i_macro */

	move.l	60(%sp),%a0		/* pred_strd value */
	move.l	64(%sp),%a1		/* out_strd value */
	move.l	52(%sp),%a5		/* pred column base */
	move.l	56(%sp),%a6		/* out column base */
	moveq	#2,%d4			/* column step: 2 bytes (chroma UV) */
	moveq	#4,%d5
	bra	.Ldc_broadcast_tail

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
