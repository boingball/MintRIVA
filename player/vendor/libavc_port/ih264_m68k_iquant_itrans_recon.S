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
	.globl	mr_ih264_iquant_itrans_recon_8x8_m68k
	.globl	mr_ih264_iquant_itrans_recon_8x8_dc_m68k

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
/*                                                                          */
/* The `ext.l %d6` below matters: the vendored C computes                  */
/* `x = i_macro + *pu1_pred_ptr; *pu1_out = CLIP_U8(x);` where `x` is      */
/* itself WORD16-typed (both the 4x4 and 8x8 DC functions declare it that  */
/* way) - so the sum is truncated to 16 bits *before* CLIP_U8 ever sees    */
/* it, not clipped from the full-width sum directly. A differential fuzz   */
/* test against the real vendored 8x8 DC function caught the missing       */
/* truncation directly (mismatched output at a deliberately out-of-spec    */
/* qp_div used only to probe this - see mr_h264_recon8x8_check.c); the     */
/* same gap exists for the 4x4 DC caller of this routine too, just never   */
/* triggered by real bitstream content (QP is spec-bounded to 0..51, and   */
/* realistic dequantised magnitudes never approach the WORD16 truncation   */
/* boundary this needs to matter) or exercised by a fuzz test until now.   */
/* ---------------------------------------------------------------------- */
.Ldc_clip_store:
	clr.l	%d6
	move.b	(%a4),%d6
	add.l	%d0,%d6
	ext.l	%d6
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

/* ---------------------------------------------------------------------- */
/* ih264_iquant_itrans_recon_8x8/_8x8_dc (High Profile transform_size_8x8_*/
/* flag path, ih264_iquant_itrans_recon.c lines 378-742) - no chroma       */
/* counterpart exists in H.264: chroma residual stays 4x4-transformed even */
/* when transform_size_8x8_flag selects 8x8 luma, so only two functions   */
/* here versus the four above.                                            */
/*                                                                          */
/* Unlike the 4x4 pair, the vendored C runs dequant as its own separate    */
/* 64-tap loop *before* the horizontal transform (rather than fusing one   */
/* row of dequant into each transform-row iteration the way the 4x4        */
/* version does), so this port mirrors that with three separate stages:    */
/* dequant all 64 taps into pi2_tmp, then an 8-row horizontal pass over    */
/* pi2_tmp in place, then an 8-column vertical pass that reads pi2_tmp and */
/* reconstructs. INV_QUANT here is qbits=6 (vs. 4x4's qbits=4) and         */
/* rnd_fact's threshold/shift are qp_div<6 ? 1<<(5-qp_div) : 0 (vs. 4x4's  */
/* qp_div<4 ? 1<<(3-qp_div) : 0) - see ih264_iquant_itrans_recon_8x8's own */
/* INV_QUANT call and rnd_fact initialiser.                                */
/*                                                                          */
/* The horizontal and vertical passes both run the *same* 8-wide 2-stage   */
/* butterfly (y0..y7 from 8 input taps, z0..z7 from y0..y7 - see the       */
/* vendored C's own side-by-side-identical formulas in both passes), only  */
/* differing in where the 8 input taps come from (dequantised-then-        */
/* row-read from pi2_tmp for the horizontal pass; column-strided reads of  */
/* the horizontal pass's own output for the vertical pass) and what        */
/* happens to the final x0..x7 = z-pairs (stored back as WORD16 into       */
/* pi2_tmp for the horizontal pass; rounded +32>>6, clipped, added to      */
/* pred and stored as the reconstructed pixel for the vertical pass, one   */
/* output row per z-pair). That shared butterfly is factored into its own  */
/* subroutine below (.Lc8r_idct1d) exactly as the C's shared math implies, */
/* rather than being duplicated per pass. The vertical pass's final        */
/* round+clip+store+advance step uses .Lc8r_recon_row (below), NOT         */
/* .Lrecon_row (above) despite the near-identical shape - the two round    */
/* the raw z-pair sum differently (.Lrecon_row truncates it to WORD16      */
/* before rounding, matching the 4x4 C's two-step `i_macro = x0+x3;` then  */
/* `i_macro = ((i_macro+32)>>6);`; the 8x8 C computes both in one          */
/* untruncated expression) - see .Lc8r_recon_row's own header comment for  */
/* why reusing .Lrecon_row here was tried first and produced wrong output. */
/*                                                                          */
/* Locals frame (relative to sp after this function's own sub.l) - plain   */
/* numeric offsets with comments, matching this file's existing style      */
/* rather than named equates: this project's CABAC-coefficient .S files    */
/* learned the hard way that bare `NAME = value` equates are exported as   */
/* globally-scoped symbols by the real AmigaOS hunk-format linker (unlike  */
/* the qemu-m68k ELF toolchain used for local verification, which scopes   */
/* them per-file) - two same-named equates with different values in        */
/* different .S files that both end up in the same mrplay link is a real,  */
/* previously-hit "multiple definition" link failure, not a hypothetical   */
/* one. Plain numeric offsets sidestep the whole hazard.                   */
/*   W0..W7 (the 8 input taps to .Lc8r_idct1d): 0,4,8,12,16,20,24,28       */
/*   Y0..Y7 (intermediate):                     32,36,40,44,48,52,56,60    */
/*   Z0..Z7 (output x-pair components):         64,68,72,76,80,84,88,92    */
/*   total local frame size: 96 bytes                                      */
/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */
/* .Lc8r_compute_rnd: rnd_fact = (qp_div_6 < 6) ? 1 << (5 - qp_div_6) : 0. */
/* Same shape as .Lcompute_rnd above but with qbits=6's own threshold/shift*/
/* (that routine bakes in qbits=4's "4"/"3" and is not reusable here).     */
/* Input: %d6 = qp_div_6 (UWORD32, unsigned compare). Output: %d7 =        */
/* rnd_fact. Clobbers %d0 only. bsr-callable, leaf.                        */
/* ---------------------------------------------------------------------- */
.Lc8r_compute_rnd:
	move.l	%d6,%d0
	cmp.l	#6,%d0
	bcc.s	.Lc8r_compute_rnd_zero
	moveq	#5,%d7
	sub.l	%d0,%d7
	moveq	#1,%d0
	lsl.l	%d7,%d0
	move.l	%d0,%d7
	rts
.Lc8r_compute_rnd_zero:
	moveq	#0,%d7
	rts

/* ---------------------------------------------------------------------- */
/* .Lc8r_invquant: one INV_QUANT() dequant, qbits=6 (vs. .Linv_quant's     */
/* qbits=4). Same register contract as .Linv_quant otherwise - see that    */
/* routine's own header comment above.                                     */
/* ---------------------------------------------------------------------- */
.Lc8r_invquant:
	muls.l	%d1,%d0
	muls.l	%d2,%d0
	add.l	%d7,%d0
	lsl.l	%d6,%d0
	asr.l	#6,%d0
	rts

/* ---------------------------------------------------------------------- */
/* .Lc8r_recon_row: round+clip+store one reconstructed pixel, advance both */
/* cursors by one row - same shape as .Lrecon_row above, deliberately NOT  */
/* reused: .Lrecon_row's leading `ext.l %d6` truncates the raw z-pair sum  */
/* to WORD16 before rounding because the 4x4 C computes i_macro in two    */
/* separate steps (`i_macro = x0 + x3;` - a WORD16-typed assignment, this  */
/* is where the truncation genuinely happens in the 4x4 case - then        */
/* `i_macro = ((i_macro + 32) >> 6);` reads the already-truncated value).  */
/* The 8x8 C (ih264_iquant_itrans_recon_8x8) computes the exact same shape */
/* of expression in ONE step instead - `i_macro = ((i_z0 + i_z7 + 32) >>   */
/* 6) + *pu1_pred_ptr;` - so the raw z-pair sum is never independently     */
/* truncated to WORD16 before the +32>>6 rounding (i_z0/i_z7 are each      */
/* individually WORD16-truncated already, by .Lc8r_idct1d's own `ext.l`,   */
/* but their SUM is not re-truncated). Reusing .Lrecon_row here produced   */
/* visibly wrong output (a differential fuzz test against the real         */
/* vendored function caught it directly) whenever a z-pair sum exceeded    */
/* the WORD16 range, which real coefficient data does often enough to      */
/* matter. Same register contract as .Lrecon_row otherwise.                */
/* ---------------------------------------------------------------------- */
.Lc8r_recon_row:
	add.l	#32,%d6
	asr.l	#6,%d6
	clr.l	%d7
	move.b	(%a4),%d7
	add.l	%d7,%d6
	bmi.s	.Lc8r_recon_row_lo
	cmp.l	#255,%d6
	ble.s	.Lc8r_recon_row_store
	move.l	#255,%d6
	bra.s	.Lc8r_recon_row_store
.Lc8r_recon_row_lo:
	moveq	#0,%d6
.Lc8r_recon_row_store:
	move.b	%d6,(%a2)
	adda.l	%a0,%a4
	adda.l	%a1,%a2
	rts

/* ---------------------------------------------------------------------- */
/* .Lc8r_idct1d: shared 8-wide 2-stage butterfly used by both the          */
/* horizontal and vertical passes of mr_ih264_iquant_itrans_recon_8x8_m68k */
/* - see the vendored C's identical y0..y7/z0..z7 formulas in both passes. */
/* IN: locals frame W0..W7 (sp+0..sp+28), already sign-extended WORD32.    */
/* OUT: locals frame Z0..Z7 (sp+64..sp+92), each truncated to WORD16 (via  */
/* ext.l) matching the C's WORD16-typed i_z0..i_z7 locals. Also writes     */
/* Y0..Y7 (sp+32..sp+60) as scratch (not meaningful to the caller after    */
/* return). Clobbers %d0-%d3. bsr-callable, leaf (no further calls).       */
/* ---------------------------------------------------------------------- */
.Lc8r_idct1d:
	/* `bsr` pushed a 4-byte return address before this subroutine's own
	 * first instruction runs, so %sp here is 4 lower than what the caller
	 * (and the W0..Z7 offset table in this file's header comment) uses -
	 * every offset below is the caller's own W0..Z7 offset plus 4. None
	 * of this file's other bsr-callable subroutines (.Lrecon_row,
	 * .Ldc_clip_store, .Linv_quant, .Lcompute_rnd and this file's own
	 * .Lc8r_invquant/.Lc8r_compute_rnd) touch %sp-relative memory at all
	 * (register-only contracts), so this +4 adjustment has no precedent
	 * to copy from elsewhere in this file - it is specific to this being
	 * the first subroutine here to read/write its caller's locals frame
	 * directly. */
	/* y0 = w0 + w4 */
	move.l	4(%sp),%d0
	add.l	20(%sp),%d0
	ext.l	%d0
	move.l	%d0,36(%sp)
	/* y1 = -w3 + w5 - w7 - (w7>>1) */
	move.l	16(%sp),%d0
	neg.l	%d0
	add.l	24(%sp),%d0
	move.l	32(%sp),%d2
	sub.l	%d2,%d0
	asr.l	#1,%d2
	sub.l	%d2,%d0
	ext.l	%d0
	move.l	%d0,40(%sp)
	/* y2 = w0 - w4 */
	move.l	4(%sp),%d0
	sub.l	20(%sp),%d0
	ext.l	%d0
	move.l	%d0,44(%sp)
	/* y3 = w1 + w7 - w3 - (w3>>1) */
	move.l	8(%sp),%d0
	add.l	32(%sp),%d0
	move.l	16(%sp),%d2
	sub.l	%d2,%d0
	asr.l	#1,%d2
	sub.l	%d2,%d0
	ext.l	%d0
	move.l	%d0,48(%sp)
	/* y4 = (w2>>1) - w6 */
	move.l	12(%sp),%d0
	asr.l	#1,%d0
	sub.l	28(%sp),%d0
	ext.l	%d0
	move.l	%d0,52(%sp)
	/* y5 = -w1 + w7 + w5 + (w5>>1) */
	move.l	8(%sp),%d0
	neg.l	%d0
	add.l	32(%sp),%d0
	move.l	24(%sp),%d2
	add.l	%d2,%d0
	asr.l	#1,%d2
	add.l	%d2,%d0
	ext.l	%d0
	move.l	%d0,56(%sp)
	/* y6 = w2 + (w6>>1) */
	move.l	28(%sp),%d0
	asr.l	#1,%d0
	add.l	12(%sp),%d0
	ext.l	%d0
	move.l	%d0,60(%sp)
	/* y7 = w3 + w5 + w1 + (w1>>1) */
	move.l	16(%sp),%d0
	add.l	24(%sp),%d0
	move.l	8(%sp),%d2
	add.l	%d2,%d0
	asr.l	#1,%d2
	add.l	%d2,%d0
	ext.l	%d0
	move.l	%d0,64(%sp)

	/* z0 = y0 + y6 */
	move.l	36(%sp),%d0
	add.l	60(%sp),%d0
	ext.l	%d0
	move.l	%d0,68(%sp)
	/* z1 = y1 + (y7>>2) */
	move.l	64(%sp),%d0
	asr.l	#2,%d0
	add.l	40(%sp),%d0
	ext.l	%d0
	move.l	%d0,72(%sp)
	/* z2 = y2 + y4 */
	move.l	44(%sp),%d0
	add.l	52(%sp),%d0
	ext.l	%d0
	move.l	%d0,76(%sp)
	/* z3 = y3 + (y5>>2) */
	move.l	56(%sp),%d0
	asr.l	#2,%d0
	add.l	48(%sp),%d0
	ext.l	%d0
	move.l	%d0,80(%sp)
	/* z4 = y2 - y4 */
	move.l	44(%sp),%d0
	sub.l	52(%sp),%d0
	ext.l	%d0
	move.l	%d0,84(%sp)
	/* z5 = (y3>>2) - y5 */
	move.l	48(%sp),%d0
	asr.l	#2,%d0
	sub.l	56(%sp),%d0
	ext.l	%d0
	move.l	%d0,88(%sp)
	/* z6 = y0 - y6 */
	move.l	36(%sp),%d0
	sub.l	60(%sp),%d0
	ext.l	%d0
	move.l	%d0,92(%sp)
	/* z7 = y7 - (y1>>2) */
	move.l	40(%sp),%d0
	asr.l	#2,%d0
	move.l	64(%sp),%d2
	sub.l	%d0,%d2
	move.l	%d2,%d0
	ext.l	%d0
	move.l	%d0,96(%sp)
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_8x8_m68k(WORD16 *pi2_src,             */
/*     UWORD8 *pu1_pred, UWORD8 *pu1_out, WORD32 pred_strd,                */
/*     WORD32 out_strd, const UWORD16 *pu2_iscal_mat,                      */
/*     const UWORD16 *pu2_weigh_mat, UWORD32 u4_qp_div_6, WORD16 *pi2_tmp, */
/*     WORD32 iq_start_idx, WORD16 *pi2_dc_ld_addr); - iq_start_idx/       */
/* pi2_dc_ld_addr are UNUSED() in the vendored C (only relevant to the 4x4 */
/* luma pair's DC-override path), kept only so the function pointer type   */
/* matches ih264_iquant_itrans_recon_ft.                                   */
/*                                                                          */
/* Saves %d2-%d7/%a2-%a6 (44 bytes) then allocates the 96-byte W/Y/Z local */
/* frame described above - stack-arg offsets are +140 (44+96):             */
/*   144(%sp)=pi2_src        164(%sp)=pu2_iscal_mat   180(%sp)=iq_start_idx*/
/*   148(%sp)=pu1_pred       168(%sp)=pu2_weigh_mat    184(%sp)=pi2_dc_ld_ */
/*   152(%sp)=pu1_out        172(%sp)=u4_qp_div_6             addr (unused)*/
/*   156(%sp)=pred_strd      176(%sp)=pi2_tmp                (unused)      */
/*   160(%sp)=out_strd                                                     */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_8x8_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	sub.l	#96,%sp

	move.l	172(%sp),%d6		/* u4_qp_div_6 */
	bsr	.Lc8r_compute_rnd	/* %d7 = rnd_fact */

	/* ---- stage 1: dequantise all 64 taps into pi2_tmp (WORD16) ---- */
	movea.l	144(%sp),%a2		/* pi2_src cursor */
	movea.l	176(%sp),%a3		/* pi2_tmp cursor */
	movea.l	164(%sp),%a4		/* pu2_iscal_mat cursor */
	movea.l	168(%sp),%a5		/* pu2_weigh_mat cursor */
	move.l	#64,%d5			/* tap counter */
.Lc8r_dqloop:
	move.w	(%a2),%d0
	ext.l	%d0
	clr.l	%d1
	move.w	(%a4),%d1
	clr.l	%d2
	move.w	(%a5),%d2
	bsr	.Lc8r_invquant
	move.w	%d0,(%a3)
	adda.l	#2,%a2
	adda.l	#2,%a3
	adda.l	#2,%a4
	adda.l	#2,%a5
	subq.l	#1,%d5
	bne	.Lc8r_dqloop

	/* ---- stage 2: horizontal transform, 8 rows, in place in pi2_tmp --- */
	movea.l	176(%sp),%a3		/* pi2_tmp, row cursor, reset to elem 0 */
	moveq	#8,%d5			/* row counter */
.Lc8r_hrow:
	move.w	0(%a3),%d0
	ext.l	%d0
	move.l	%d0,0(%sp)		/* W0 */
	move.w	2(%a3),%d0
	ext.l	%d0
	move.l	%d0,4(%sp)		/* W1 */
	move.w	4(%a3),%d0
	ext.l	%d0
	move.l	%d0,8(%sp)		/* W2 */
	move.w	6(%a3),%d0
	ext.l	%d0
	move.l	%d0,12(%sp)		/* W3 */
	move.w	8(%a3),%d0
	ext.l	%d0
	move.l	%d0,16(%sp)		/* W4 */
	move.w	10(%a3),%d0
	ext.l	%d0
	move.l	%d0,20(%sp)		/* W5 */
	move.w	12(%a3),%d0
	ext.l	%d0
	move.l	%d0,24(%sp)		/* W6 */
	move.w	14(%a3),%d0
	ext.l	%d0
	move.l	%d0,28(%sp)		/* W7 */

	bsr	.Lc8r_idct1d

	move.l	64(%sp),%d0		/* Z0 */
	add.l	92(%sp),%d0		/* Z7 */
	move.w	%d0,0(%a3)
	move.l	72(%sp),%d0		/* Z2 */
	add.l	84(%sp),%d0		/* Z5 */
	move.w	%d0,2(%a3)
	move.l	80(%sp),%d0		/* Z4 */
	add.l	76(%sp),%d0		/* Z3 */
	move.w	%d0,4(%a3)
	move.l	88(%sp),%d0		/* Z6 */
	add.l	68(%sp),%d0		/* Z1 */
	move.w	%d0,6(%a3)
	move.l	88(%sp),%d0		/* Z6 */
	sub.l	68(%sp),%d0		/* Z1 */
	move.w	%d0,8(%a3)
	move.l	80(%sp),%d0		/* Z4 */
	sub.l	76(%sp),%d0		/* Z3 */
	move.w	%d0,10(%a3)
	move.l	72(%sp),%d0		/* Z2 */
	sub.l	84(%sp),%d0		/* Z5 */
	move.w	%d0,12(%a3)
	move.l	64(%sp),%d0		/* Z0 */
	sub.l	92(%sp),%d0		/* Z7 */
	move.w	%d0,14(%a3)

	adda.l	#16,%a3			/* next row: 8 WORD16 = 16 bytes */
	subq.l	#1,%d5
	bne	.Lc8r_hrow

	/* ---- stage 3: vertical transform + recon, 8 columns ---- */
	movea.l	176(%sp),%a3		/* pi2_tmp, column cursor, elem 0 */
	movea.l	148(%sp),%a5		/* pred column base */
	movea.l	152(%sp),%a6		/* out column base */
	movea.l	156(%sp),%a0		/* pred_strd value */
	movea.l	160(%sp),%a1		/* out_strd value */
	moveq	#8,%d5			/* column counter */
.Lc8r_vcol:
	move.w	0(%a3),%d0
	ext.l	%d0
	move.l	%d0,0(%sp)		/* W0: row0 */
	move.w	16(%a3),%d0
	ext.l	%d0
	move.l	%d0,4(%sp)		/* W1: row1 */
	move.w	32(%a3),%d0
	ext.l	%d0
	move.l	%d0,8(%sp)		/* W2: row2 */
	move.w	48(%a3),%d0
	ext.l	%d0
	move.l	%d0,12(%sp)		/* W3: row3 */
	move.w	64(%a3),%d0
	ext.l	%d0
	move.l	%d0,16(%sp)		/* W4: row4 */
	move.w	80(%a3),%d0
	ext.l	%d0
	move.l	%d0,20(%sp)		/* W5: row5 */
	move.w	96(%a3),%d0
	ext.l	%d0
	move.l	%d0,24(%sp)		/* W6: row6 */
	move.w	112(%a3),%d0
	ext.l	%d0
	move.l	%d0,28(%sp)		/* W7: row7 */

	bsr	.Lc8r_idct1d

	movea.l	%a5,%a4			/* row cursor = column base (pred) */
	movea.l	%a6,%a2			/* row cursor = column base (out) */

	move.l	64(%sp),%d6		/* Z0 */
	add.l	92(%sp),%d6		/* Z7 */
	bsr	.Lc8r_recon_row
	move.l	72(%sp),%d6		/* Z2 */
	add.l	84(%sp),%d6		/* Z5 */
	bsr	.Lc8r_recon_row
	move.l	80(%sp),%d6		/* Z4 */
	add.l	76(%sp),%d6		/* Z3 */
	bsr	.Lc8r_recon_row
	move.l	88(%sp),%d6		/* Z6 */
	add.l	68(%sp),%d6		/* Z1 */
	bsr	.Lc8r_recon_row
	move.l	88(%sp),%d6		/* Z6 */
	sub.l	68(%sp),%d6		/* Z1 */
	bsr	.Lc8r_recon_row
	move.l	80(%sp),%d6		/* Z4 */
	sub.l	76(%sp),%d6		/* Z3 */
	bsr	.Lc8r_recon_row
	move.l	72(%sp),%d6		/* Z2 */
	sub.l	84(%sp),%d6		/* Z5 */
	bsr	.Lc8r_recon_row
	move.l	64(%sp),%d6		/* Z0 */
	sub.l	92(%sp),%d6		/* Z7 */
	bsr	.Lc8r_recon_row

	adda.l	#2,%a3			/* next column: 1 WORD16 */
	adda.l	#1,%a5			/* next column, pred (luma: 1 byte) */
	adda.l	#1,%a6			/* next column, out (luma: 1 byte) */
	subq.l	#1,%d5
	bne	.Lc8r_vcol

	add.l	#96,%sp
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* void mr_ih264_iquant_itrans_recon_8x8_dc_m68k(...) - same 11-arg        */
/* signature as above. Saves only %d2-%d7/%a2-%a6 (44 bytes, no extra      */
/* local frame - i_macro lives in one register throughout, same shape as   */
/* mr_ih264_iquant_itrans_recon_4x4_dc_m68k above but broadcasting to 8x8  */
/* (64 pixels: 8 columns x 8 rows via .Ldc_clip_store) instead of 4x4, and */
/* with qbits=6 dequant instead of 4:                                      */
/*   48(%sp)=pi2_src        68(%sp)=pu2_iscal_mat    84(%sp)=iq_start_idx  */
/*   52(%sp)=pu1_pred       72(%sp)=pu2_weigh_mat    88(%sp)=pi2_dc_ld_    */
/*   56(%sp)=pu1_out        76(%sp)=u4_qp_div_6              addr (unused) */
/*   60(%sp)=pred_strd      80(%sp)=pi2_tmp (unused)                      */
/*   64(%sp)=out_strd                                                      */
/* ---------------------------------------------------------------------- */
mr_ih264_iquant_itrans_recon_8x8_dc_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)

	move.l	76(%sp),%d6		/* u4_qp_div_6 */
	bsr	.Lc8r_compute_rnd	/* %d7 = rnd_fact */

	movea.l	48(%sp),%a2		/* pi2_src */
	clr.l	%d1
	movea.l	68(%sp),%a3		/* pu2_iscal_mat */
	move.w	(%a3),%d1
	clr.l	%d2
	movea.l	72(%sp),%a3		/* pu2_weigh_mat */
	move.w	(%a3),%d2
	move.w	(%a2),%d0
	ext.l	%d0
	bsr	.Lc8r_invquant

	add.l	#32,%d0
	asr.l	#6,%d0
	ext.l	%d0			/* i_macro */

	move.l	60(%sp),%a0		/* pred_strd value */
	move.l	64(%sp),%a1		/* out_strd value */
	move.l	52(%sp),%a5		/* pred column base */
	move.l	56(%sp),%a6		/* out column base */
	moveq	#8,%d5			/* column counter: 8x8 broadcast */
	bra	.Lc8r_dc_broadcast_tail

/* ---------------------------------------------------------------------- */
/* .Lc8r_dc_broadcast_tail: 8x8 broadcast tail for                         */
/* mr_ih264_iquant_itrans_recon_8x8_dc_m68k - .Ldc_broadcast_tail above    */
/* hardcodes 4 .Ldc_clip_store calls per column (built for the 4x4 DC      */
/* case) so is not reusable here; this is the same shape with 8 calls per */
/* column instead. .Ldc_clip_store itself (round already done by caller,   */
/* clip+store+advance one pixel) has no row-count assumption baked in and  */
/* is reused directly. Entry state: %d0 = i_macro, %a5 = pred column base, */
/* %a6 = out column base, %a0/%a1 = pred_strd/out_strd values, %d5 =       */
/* column counter (8). Reached via `bra`, matching .Ldc_broadcast_tail's   */
/* own calling convention.                                                 */
/* ---------------------------------------------------------------------- */
.Lc8r_dc_broadcast_tail:
	movea.l	%a5,%a4
	movea.l	%a6,%a2
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	bsr	.Ldc_clip_store
	adda.l	#1,%a5			/* next column, pred (luma: 1 byte) */
	adda.l	#1,%a6			/* next column, out (luma: 1 byte) */
	subq.l	#1,%d5
	bne	.Lc8r_dc_broadcast_tail

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
