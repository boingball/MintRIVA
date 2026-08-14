/*
 * Hand-written m68k assembly for H.264 motion vector prediction's core
 * primitive: ih264d_get_motion_vector_predictor() (spec 8.4.1.2.1),
 * vendor/libavc/decoder/ih264d_mvpred.c lines 75-119. Called from
 * ih264d_mvpred_dispatch_port.c's __wrap_ih264d_mvpred_nonmbaff() /
 * __wrap_ih264d_mvpred_nonmbaffB() - see that file for why this is
 * invoked directly from a reimplemented dispatcher rather than swapped
 * in under the vendored primitive's own name (short version: every real
 * call to the vendored primitive is inside its own defining file,
 * ih264d_mvpred.c, so -Wl,--wrap=ih264d_get_motion_vector_predictor
 * cannot intercept any of them - confirmed with a minimal repro, not
 * assumed).
 *
 * void mr_ih264d_get_motion_vector_predictor_m68k(UWORD8 *ps_result,
 *     UWORD8 **ps_mv_pred, UWORD32 u1_ref_idx, UWORD32 u1_B,
 *     const UWORD8 *pu1_mv_pred_condition);
 *
 * Ported instruction-for-instruction from the real C (kept byte-for-byte,
 * renamed only, as ih264d_get_motion_vector_predictor_generic() for the
 * differential fuzz test - see check_mv_predictor() in
 * tests/mr_h264_m68k_check.c):
 *
 *   c_temp = (L->i1_ref_frame[u1_B]  == u1_ref_idx)
 *          | ((T->i1_ref_frame[u1_B]  == u1_ref_idx) << 1)
 *          | ((TR->i1_ref_frame[u1_B] == u1_ref_idx) << 2);
 *   c_temp = pu1_mv_pred_condition[c_temp];
 *   if (c_temp != -1)
 *       result->i2_mv[uc_B2+k] = ps_mv_pred[c_temp]->i2_mv[uc_B2+k];  k=0,1
 *   else
 *       result->i2_mv[uc_B2+k] = MAX(MIN(L,T), MIN(MAX(L,T),TR));     k=0,1
 *
 * where uc_B2 = u1_B<<1 and L/T/TR = ps_mv_pred[LEFT=0]/[TOP=1]/[TOP_R=2].
 * pu1_mv_pred_condition is always gau1_ih264d_mv_pred_condition (the only
 * table ever passed at any of this function's ~20 call sites, all inside
 * ih264d_mvpred.c) = {-1,0,1,-1,2,-1,-1,-1} (ih264d_tables.c).
 *
 * mv_pred_t (ih264d_structs.h) layout relied on here: 0=i2_mv[4] (WORD16,
 * 8 bytes), 8=i1_ref_frame[2] (WORD8, 2 bytes) - the two trailing UWORD8
 * fields (u1_col_ref_pic_idx, u1_pic_type) are never touched by this
 * function and are not referenced here either.
 *
 * u1_ref_idx and u1_B are UWORD8 in the real C signature but declared
 * UWORD32 here and read as full stack words - the m68k SysV ABI (confirmed
 * via `m68k-linux-gnu-gcc -O2 -m68030 -S` against the real function)
 * passes every scalar argument in its own 4-byte stack slot regardless of
 * declared width, with the meaningful byte at the slot's highest address
 * (big-endian); only that byte is read below, so whatever a caller leaves
 * in the rest of the slot is ignored exactly like the compiler-generated
 * code does.
 *
 * i1_ref_frame's bytes and pu1_mv_pred_condition's table entries are
 * sign-extended (EXTB.L) before use, matching the C's implicit int
 * promotion of WORD8 (i1_ref_frame) and the round-trip cast the real
 * function's UWORD8* table pointer forces on its WORD8-typed table (a 255
 * read as UWORD8 and narrowed back into the C's WORD8 c_temp is bit-for-
 * bit the same value as reading the byte signed in the first place, so
 * this reads it signed directly rather than reproducing the round trip).
 *
 * Verification: differential fuzz test against the independent from-spec
 * reimplementation above (check_mv_predictor() in mr_h264_m68k_check.c)
 * across randomised ref_frame/ref_idx/u1_B/mv combinations covering every
 * c_temp bitmask value (0..7) and both the shortcut and median paths.
 * Plus check_mvpred_dispatch() (mr_h264_m68k_check.c), fuzzing the full
 * dispatcher call chain against the real vendored dispatchers. Plus the
 * full make check / make check-m68k H.264-vs-ffmpeg conformance suite
 * (worst per-frame MAE must stay unchanged), and a qemu -d exec address
 * trace confirming this function's own address is what actually executes
 * during real decode (not just the wrap linking cleanly - see the
 * dispatch port file's header comment for why that distinction matters
 * here specifically).
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264d_get_motion_vector_predictor_m68k

mr_ih264d_get_motion_vector_predictor_m68k:
	movem.l	%d2-%d7/%a2-%a5,-(%sp)
	movea.l	44(%sp),%a0		/* a0 = ps_result               (4+40)  */
	movea.l	48(%sp),%a1		/* a1 = ps_mv_pred              (8+40)  */
	clr.l	%d0
	move.b	55(%sp),%d0		/* d0 = u1_ref_idx             (15+40)  */
	clr.l	%d1
	move.b	59(%sp),%d1		/* d1 = u1_B                   (19+40)  */

	movea.l	(%a1),%a2		/* a2 = ps_mv_pred[LEFT]  */
	movea.l	4(%a1),%a3		/* a3 = ps_mv_pred[TOP]   */
	movea.l	8(%a1),%a4		/* a4 = ps_mv_pred[TOP_R] */

	move.l	%d1,%d7
	lsl.l	#2,%d7			/* d7 = off0 = u1_B*4 (byte offset of
					 * i2_mv[uc_B2+0]; off1 = off0+2) */

	moveq	#8,%d6
	add.l	%d1,%d6			/* d6 = struct offset of i1_ref_frame[u1_B] */

	move.b	(%a2,%d6.l),%d2
	extb.l	%d2
	cmp.l	%d0,%d2
	seq	%d2
	and.l	#1,%d2			/* d2 = bit0 (weight 1) */

	move.b	(%a3,%d6.l),%d3
	extb.l	%d3
	cmp.l	%d0,%d3
	seq	%d3
	and.l	#2,%d3			/* d3 = bit1, already weight 2 (0xff&2==2) */
	or.l	%d3,%d2

	move.b	(%a4,%d6.l),%d3
	extb.l	%d3
	cmp.l	%d0,%d3
	seq	%d3
	and.l	#4,%d3			/* d3 = bit2, already weight 4 */
	or.l	%d3,%d2			/* d2 = c_temp index (0..7) */

	movea.l	60(%sp),%a5		/* a5 = pu1_mv_pred_condition  (20+40)  */
	move.b	(%a5,%d2.l),%d2
	extb.l	%d2			/* d2 = c_temp (-1, 0, 1, or 2) */

	cmpi.l	#-1,%d2
	beq.b	.Lmvpred_median

	/* single-match shortcut: straight 16-bit copy, no arithmetic */
	lsl.l	#2,%d2
	movea.l	(%a1,%d2.l),%a5		/* a5 = ps_mv_pred[c_temp] */
	move.w	(%a5,%d7.l),%d3
	move.w	2(%a5,%d7.l),%d4
	move.w	%d3,(%a0,%d7.l)
	move.w	%d4,2(%a0,%d7.l)
	bra.b	.Lmvpred_done

.Lmvpred_median:
	/* component 0 (byte offset off0): D0=MIN(L,T); D1=MIN(MAX(L,T),TR);
	 * result = MAX(D0,D1). All four candidate values sign-extended to
	 * 32 bits first (EXT.L) so the MIN/MAX chain matches the C's WORD32
	 * D0/D1 temporaries exactly, not a 16-bit-wrapping comparison. */
	move.w	(%a2,%d7.l),%d2
	ext.l	%d2			/* d2 = L */
	move.w	(%a3,%d7.l),%d3
	ext.l	%d3			/* d3 = T */
	move.w	(%a4,%d7.l),%d4
	ext.l	%d4			/* d4 = TR */

	move.l	%d2,%d5
	cmp.l	%d3,%d5
	ble.b	.Lmv0_d0ok
	move.l	%d3,%d5
.Lmv0_d0ok:				/* d5 = D0 = MIN(L,T) */
	move.l	%d2,%d6
	cmp.l	%d3,%d6
	bge.b	.Lmv0_d1ok
	move.l	%d3,%d6
.Lmv0_d1ok:				/* d6 = MAX(L,T) */
	cmp.l	%d4,%d6
	ble.b	.Lmv0_d1ok2
	move.l	%d4,%d6
.Lmv0_d1ok2:				/* d6 = D1 = MIN(MAX(L,T),TR) */
	cmp.l	%d6,%d5
	bge.b	.Lmv0_res
	move.l	%d6,%d5
.Lmv0_res:				/* d5 = MAX(D0,D1) */
	move.w	%d5,(%a0,%d7.l)

	/* component 1 (byte offset off0+2): identical chain */
	move.w	2(%a2,%d7.l),%d2
	ext.l	%d2
	move.w	2(%a3,%d7.l),%d3
	ext.l	%d3
	move.w	2(%a4,%d7.l),%d4
	ext.l	%d4

	move.l	%d2,%d5
	cmp.l	%d3,%d5
	ble.b	.Lmv1_d0ok
	move.l	%d3,%d5
.Lmv1_d0ok:
	move.l	%d2,%d6
	cmp.l	%d3,%d6
	bge.b	.Lmv1_d1ok
	move.l	%d3,%d6
.Lmv1_d1ok:
	cmp.l	%d4,%d6
	ble.b	.Lmv1_d1ok2
	move.l	%d4,%d6
.Lmv1_d1ok2:
	cmp.l	%d6,%d5
	bge.b	.Lmv1_res
	move.l	%d6,%d5
.Lmv1_res:
	move.w	%d5,2(%a0,%d7.l)

.Lmvpred_done:
	movem.l	(%sp)+,%d2-%d7/%a2-%a5
	rts

#endif /* MR_M68K_ASM */

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
