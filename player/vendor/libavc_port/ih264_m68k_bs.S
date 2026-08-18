/*
 * H.264 P-slice 16x16 boundary-strength calculation for m68k.
 *
 * This is the fixed-size hot leaf behind dec_struct_t::pf_fill_bs1[0][0].
 * It compares the current macroblock's single forward motion vector and
 * reference picture with the four 4x4 neighbours along its top and left
 * edges, setting bS=1 where the already-computed bS is zero and either the
 * reference differs or a motion-vector delta crosses the H.264 threshold.
 *
 * Ittiam's C function carries loop counters, computes 24-(i<<3), indexes
 * i&2 on every iteration and repeatedly reloads the current MV/reference.
 * The assembly pins those invariant values in registers and unrolls the
 * eight edge tests.  It uses only 68020 integer instructions and no long
 * multiply/divide, so it is safe on a real 68060 as well as PiStorm.
 *
 * mv_pred_t is 12 bytes on m68k:
 *   +0/+2 forward x/y, +4/+6 backward x/y, +8/+9 reference indices.
 * neighbouradd_t is four consecutive 32-bit pointers.
 */
#if defined(MR_M68K_ASM)
	.text
	.even
	.globl	mr_ih264d_fill_bs1_16x16mb_pslice_m68k
	.globl	mr_ih264d_fill_bs1_non16x16mb_pslice_m68k
	.globl	mr_ih264d_fill_bs1_16x16mb_bslice_m68k
	.globl	mr_ih264d_fill_bs1_non16x16mb_bslice_m68k

/* Test one neighbour.  Arguments are the predictor address register and
 * x/y offsets, neighbour-picture address register and its two offsets, the
 * destination bS bit, and the nibble mask used to detect an existing bS.
 * d3=current picture, d4/d5=current x/y MV, d6=vertical threshold,
 * d7=the edge's packed bS word; d0-d1 are scratch. */
	.macro	MR_BS1_EDGE predreg, xoff, yoff, picreg, pic0, pic1, bitno, nibmask
	move.l	%d7,%d0
	and.l	#\nibmask,%d0
	bne	.Lbs1_done_\@

	move.l	\pic0(\picreg),%d0
	cmp.l	%d3,%d0
	bne	.Lbs1_set_\@
	tst.l	\pic1(\picreg)
	bne	.Lbs1_set_\@

	move.w	\xoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_x_abs_\@
	neg.l	%d0
.Lbs1_x_abs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_set_\@

	move.w	\yoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_y_abs_\@
	neg.l	%d0
.Lbs1_y_abs_\@:
	cmp.l	%d6,%d0
	blt	.Lbs1_done_\@

.Lbs1_set_\@:
	bset	#\bitno,%d7
.Lbs1_done_\@:
	.endm

/* void fill_bs1_16x16mb_pslice(cur, top, map, bs, left, left_addr,
 *                              pic_addr, ver_mvlimit)
 * After saving 40 bytes, arguments start at 44(sp). */
mr_ih264d_fill_bs1_16x16mb_pslice_m68k:
	movem.l	%d3-%d7/%a2-%a6,-(%sp)
	movea.l	44(%sp),%a0
	movea.l	48(%sp),%a1
	movea.l	52(%sp),%a2
	movea.l	56(%sp),%a3
	movea.l	60(%sp),%a4
	movea.l	64(%sp),%a5
	movea.l	68(%sp),%a6
	move.l	72(%sp),%d6

	move.w	0(%a0),%d4
	ext.l	%d4
	move.w	2(%a0),%d5
	ext.l	%d5
	move.b	8(%a0),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	(%a2,%d0.l),%d3

	/* Top edge: four consecutive predictors, external picture addresses
	 * selected as 0/1, 0/1, 2/3, 2/3. */
	move.l	0(%a3),%d7
	MR_BS1_EDGE %a1,0,2,%a6,0,4,24,0x0f000000
	MR_BS1_EDGE %a1,12,14,%a6,0,4,16,0x000f0000
	MR_BS1_EDGE %a1,24,26,%a6,8,12,8,0x00000f00
	MR_BS1_EDGE %a1,36,38,%a6,8,12,0,0x0000000f
	move.l	%d7,0(%a3)

	/* Left edge: the leftmost predictor of each 4x4 row is 4 mv_pred_t
	 * entries (48 bytes) apart. */
	move.l	16(%a3),%d7
	MR_BS1_EDGE %a4,0,2,%a5,0,4,24,0x0f000000
	MR_BS1_EDGE %a4,48,50,%a5,0,4,16,0x000f0000
	MR_BS1_EDGE %a4,96,98,%a5,8,12,8,0x00000f00
	MR_BS1_EDGE %a4,144,146,%a5,8,12,0,0x0000000f
	move.l	%d7,16(%a3)

	movem.l	(%sp)+,%d3-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* Partitioned P macroblocks.  Unlike the 16x16 case, each of the sixteen
 * 4x4 blocks has its own current MV/reference.  The geometry is nevertheless
 * completely fixed, so expanding it as sixteen pairs of edge checks avoids
 * both of the generic C function's nested loops and all run-time i&2/index
 * arithmetic.  a0=the 4x4 current-MV grid, a1=external top predictors,
 * a2=reference map, a3=bS table, a4=external left predictors,
 * a5=external left pictures, a6=external top pictures. */

	.macro	MR_BS1_LOAD_CUR xoff, yoff, refoff
	move.w	\xoff(%a0),%d4
	ext.l	%d4
	move.w	\yoff(%a0),%d5
	ext.l	%d5
	move.b	\refoff(%a0),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	(%a2,%d0.l),%d3
	.endm

/* Internal edge: neighbour picture comes from the neighbour predictor's
 * signed reference index. */
	.macro	MR_BS1_PART_INTERNAL bsreg, predreg, xoff, yoff, refoff, bitno, nibmask
	move.l	\bsreg,%d0
	and.l	#\nibmask,%d0
	bne	.Lbs1_pi_done_\@
	move.b	\refoff(\predreg),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	(%a2,%d0.l),%d0
	cmp.l	%d3,%d0
	bne	.Lbs1_pi_set_\@
	move.w	\xoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_pi_xabs_\@
	neg.l	%d0
.Lbs1_pi_xabs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_pi_set_\@
	move.w	\yoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_pi_yabs_\@
	neg.l	%d0
.Lbs1_pi_yabs_\@:
	cmp.l	%d6,%d0
	blt	.Lbs1_pi_done_\@
.Lbs1_pi_set_\@:
	bset	#\bitno,\bsreg
.Lbs1_pi_done_\@:
	.endm

/* External macroblock edge: picture identities come from neighbouradd_t or
 * u4_pic_addrress; P-slice list 1 is always the null picture. */
	.macro	MR_BS1_PART_EXTERNAL bsreg, predreg, xoff, yoff, picreg, pic0, pic1, bitno, nibmask
	move.l	\bsreg,%d0
	and.l	#\nibmask,%d0
	bne	.Lbs1_pe_done_\@
	move.l	\pic0(\picreg),%d0
	cmp.l	%d3,%d0
	bne	.Lbs1_pe_set_\@
	tst.l	\pic1(\picreg)
	bne	.Lbs1_pe_set_\@
	move.w	\xoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_pe_xabs_\@
	neg.l	%d0
.Lbs1_pe_xabs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_pe_set_\@
	move.w	\yoff(\predreg),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_pe_yabs_\@
	neg.l	%d0
.Lbs1_pe_yabs_\@:
	cmp.l	%d6,%d0
	blt	.Lbs1_pe_done_\@
.Lbs1_pe_set_\@:
	bset	#\bitno,\bsreg
.Lbs1_pe_done_\@:
	.endm

/* Stack layout uses 44 saved bytes, putting the eight arguments at
 * 48..76(sp).  d2 holds the current row's horizontal bS word;
 * d7 is the vertical bS word being tested. */
mr_ih264d_fill_bs1_non16x16mb_pslice_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	48(%sp),%a0
	movea.l	52(%sp),%a1
	movea.l	56(%sp),%a2
	movea.l	60(%sp),%a3
	movea.l	64(%sp),%a4
	movea.l	68(%sp),%a5
	movea.l	72(%sp),%a6
	move.l	76(%sp),%d6

	/* Row 0: both top and left neighbours are external. */
	move.l	0(%a3),%d2
	MR_BS1_LOAD_CUR 0,2,8
	move.l	16(%a3),%d7
	MR_BS1_PART_EXTERNAL %d7,%a4,0,2,%a5,0,4,24,0x0f000000
	move.l	%d7,16(%a3)
	MR_BS1_PART_EXTERNAL %d2,%a1,0,2,%a6,0,4,24,0x0f000000
	MR_BS1_LOAD_CUR 12,14,20
	move.l	20(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,0,2,8,24,0x0f000000
	move.l	%d7,20(%a3)
	MR_BS1_PART_EXTERNAL %d2,%a1,12,14,%a6,0,4,16,0x000f0000
	MR_BS1_LOAD_CUR 24,26,32
	move.l	24(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,12,14,20,24,0x0f000000
	move.l	%d7,24(%a3)
	MR_BS1_PART_EXTERNAL %d2,%a1,24,26,%a6,8,12,8,0x00000f00
	MR_BS1_LOAD_CUR 36,38,44
	move.l	28(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,24,26,32,24,0x0f000000
	move.l	%d7,28(%a3)
	MR_BS1_PART_EXTERNAL %d2,%a1,36,38,%a6,8,12,0,0x0000000f
	move.l	%d2,0(%a3)

	/* Row 1: left is external; top is the corresponding block in row 0. */
	move.l	4(%a3),%d2
	MR_BS1_LOAD_CUR 48,50,56
	move.l	16(%a3),%d7
	MR_BS1_PART_EXTERNAL %d7,%a4,48,50,%a5,0,4,16,0x000f0000
	move.l	%d7,16(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,0,2,8,24,0x0f000000
	MR_BS1_LOAD_CUR 60,62,68
	move.l	20(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,48,50,56,16,0x000f0000
	move.l	%d7,20(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,12,14,20,16,0x000f0000
	MR_BS1_LOAD_CUR 72,74,80
	move.l	24(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,60,62,68,16,0x000f0000
	move.l	%d7,24(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,24,26,32,8,0x00000f00
	MR_BS1_LOAD_CUR 84,86,92
	move.l	28(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,72,74,80,16,0x000f0000
	move.l	%d7,28(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,36,38,44,0,0x0000000f
	move.l	%d2,4(%a3)

	/* Row 2: external-left address pair switches to entries 2/3. */
	move.l	8(%a3),%d2
	MR_BS1_LOAD_CUR 96,98,104
	move.l	16(%a3),%d7
	MR_BS1_PART_EXTERNAL %d7,%a4,96,98,%a5,8,12,8,0x00000f00
	move.l	%d7,16(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,48,50,56,24,0x0f000000
	MR_BS1_LOAD_CUR 108,110,116
	move.l	20(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,96,98,104,8,0x00000f00
	move.l	%d7,20(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,60,62,68,16,0x000f0000
	MR_BS1_LOAD_CUR 120,122,128
	move.l	24(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,108,110,116,8,0x00000f00
	move.l	%d7,24(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,72,74,80,8,0x00000f00
	MR_BS1_LOAD_CUR 132,134,140
	move.l	28(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,120,122,128,8,0x00000f00
	move.l	%d7,28(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,84,86,92,0,0x0000000f
	move.l	%d2,8(%a3)

	/* Row 3. */
	move.l	12(%a3),%d2
	MR_BS1_LOAD_CUR 144,146,152
	move.l	16(%a3),%d7
	MR_BS1_PART_EXTERNAL %d7,%a4,144,146,%a5,8,12,0,0x0000000f
	move.l	%d7,16(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,96,98,104,24,0x0f000000
	MR_BS1_LOAD_CUR 156,158,164
	move.l	20(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,144,146,152,0,0x0000000f
	move.l	%d7,20(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,108,110,116,16,0x000f0000
	MR_BS1_LOAD_CUR 168,170,176
	move.l	24(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,156,158,164,0,0x0000000f
	move.l	%d7,24(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,120,122,128,8,0x00000f00
	MR_BS1_LOAD_CUR 180,182,188
	move.l	28(%a3),%d7
	MR_BS1_PART_INTERNAL %d7,%a0,168,170,176,0,0x0000000f
	move.l	%d7,28(%a3)
	MR_BS1_PART_INTERNAL %d2,%a0,132,134,140,0,0x0000000f
	move.l	%d2,12(%a3)

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* B-slice 16x16 macroblocks.  H.264 permits the neighbour's list-0/list-1
 * references to match the current block either directly or swapped.  bS=1
 * only when both pairings fail their picture/MV checks.  a0/a1 pin the
 * current list-0/list-1 picture identities, d2-d5 pin its four MVs, and the
 * macro short-circuits each pairing as soon as its result is known. */

	.macro	MR_BS1_B16_EDGE predreg, p0off, p1off, p2off, p3off, picreg, pic0, pic1, bitno, nibmask
	move.l	%d7,%d0
	and.l	#\nibmask,%d0
	bne	.Lbs1_b16_done_\@

	/* Direct list pairing: (p0,p1)->(q0,q1), (p2,p3)->(q2,q3). */
	cmpa.l	\pic0(\picreg),%a0
	bne	.Lbs1_b16_direct_bad_\@
	cmpa.l	\pic1(\picreg),%a1
	bne	.Lbs1_b16_direct_bad_\@
	move.w	\p0off(\predreg),%d0
	ext.l	%d0
	sub.l	%d2,%d0
	bpl	.Lbs1_b16_d0abs_\@
	neg.l	%d0
.Lbs1_b16_d0abs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_b16_direct_bad_\@
	move.w	\p1off(\predreg),%d0
	ext.l	%d0
	sub.l	%d3,%d0
	bpl	.Lbs1_b16_d1abs_\@
	neg.l	%d0
.Lbs1_b16_d1abs_\@:
	cmp.l	%d6,%d0
	bge	.Lbs1_b16_direct_bad_\@
	move.w	\p2off(\predreg),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_b16_d2abs_\@
	neg.l	%d0
.Lbs1_b16_d2abs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_b16_direct_bad_\@
	move.w	\p3off(\predreg),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_b16_d3abs_\@
	neg.l	%d0
.Lbs1_b16_d3abs_\@:
	cmp.l	%d6,%d0
	blt	.Lbs1_b16_done_\@	/* direct pairing passed: bS stays zero */

.Lbs1_b16_direct_bad_\@:
	/* Swapped list pairing.  If either picture differs, the swapped test is
	 * already true and bS can be set without reading its four MV deltas. */
	cmpa.l	\pic1(\picreg),%a0
	bne	.Lbs1_b16_set_\@
	cmpa.l	\pic0(\picreg),%a1
	bne	.Lbs1_b16_set_\@
	move.w	\p0off(\predreg),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_b16_s0abs_\@
	neg.l	%d0
.Lbs1_b16_s0abs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_b16_set_\@
	move.w	\p1off(\predreg),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_b16_s1abs_\@
	neg.l	%d0
.Lbs1_b16_s1abs_\@:
	cmp.l	%d6,%d0
	bge	.Lbs1_b16_set_\@
	move.w	\p2off(\predreg),%d0
	ext.l	%d0
	sub.l	%d2,%d0
	bpl	.Lbs1_b16_s2abs_\@
	neg.l	%d0
.Lbs1_b16_s2abs_\@:
	cmpi.l	#4,%d0
	bge	.Lbs1_b16_set_\@
	move.w	\p3off(\predreg),%d0
	ext.l	%d0
	sub.l	%d3,%d0
	bpl	.Lbs1_b16_s3abs_\@
	neg.l	%d0
.Lbs1_b16_s3abs_\@:
	cmp.l	%d6,%d0
	blt	.Lbs1_b16_done_\@
.Lbs1_b16_set_\@:
	bset	#\bitno,%d7
.Lbs1_b16_done_\@:
	.endm

mr_ih264d_fill_bs1_16x16mb_bslice_m68k:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)
	movea.l	48(%sp),%a5		/* current predictor */
	movea.l	56(%sp),%a3		/* list-0 map base */
	movea.l	60(%sp),%a4		/* bS table */
	move.l	76(%sp),%d6		/* vertical MV threshold */
	move.w	0(%a5),%d2
	ext.l	%d2
	move.w	2(%a5),%d3
	ext.l	%d3
	move.w	4(%a5),%d4
	ext.l	%d4
	move.w	6(%a5),%d5
	ext.l	%d5
	move.b	8(%a5),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	movea.l	(%a3,%d0.l),%a0	/* current list-0 picture */
	move.b	9(%a5),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	movea.l	132(%a3,%d0.l),%a1	/* list-1 map = L0 + 33 pointers */

	movea.l	52(%sp),%a5		/* external top predictors */
	movea.l	72(%sp),%a6		/* external top picture pairs */
	move.l	0(%a4),%d7
	MR_BS1_B16_EDGE %a5,0,2,4,6,%a6,0,4,24,0x0f000000
	MR_BS1_B16_EDGE %a5,12,14,16,18,%a6,0,4,16,0x000f0000
	MR_BS1_B16_EDGE %a5,24,26,28,30,%a6,8,12,8,0x00000f00
	MR_BS1_B16_EDGE %a5,36,38,40,42,%a6,8,12,0,0x0000000f
	move.l	%d7,0(%a4)

	movea.l	64(%sp),%a5		/* external left predictors */
	movea.l	68(%sp),%a6		/* external left picture pairs */
	move.l	16(%a4),%d7
	MR_BS1_B16_EDGE %a5,0,2,4,6,%a6,0,4,24,0x0f000000
	MR_BS1_B16_EDGE %a5,48,50,52,54,%a6,0,4,16,0x000f0000
	MR_BS1_B16_EDGE %a5,96,98,100,102,%a6,8,12,8,0x00000f00
	MR_BS1_B16_EDGE %a5,144,146,148,150,%a6,8,12,0,0x0000000f
	move.l	%d7,16(%a4)

	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts

/* ---------------------------------------------------------------------- */
/* Partitioned B macroblocks.  This is the largest boundary-strength leaf:
 * 32 potential edges, two reference lists and direct/swapped pairing.  The
 * fixed block geometry is expanded below, but the expensive pair comparison
 * is one shared local subroutine rather than 32 copies; this keeps the 68060
 * instruction-cache footprint sane.  At entry to the helper:
 *   a0/a1 = current list-0/list-1 pictures
 *   a2    = reference map base, a3 = external picture pair (if d1 != 0)
 *   a4    = neighbour predictor, d2-d5 = current MVs, d6 = vertical limit
 * It returns d0=0/1 and preserves every pinned current value. */

.Lbs1_bn_check:
	tst.l	%d1
	beq	.Lbs1_bn_direct_internal
	cmpa.l	0(%a3),%a0
	bne	.Lbs1_bn_direct_bad
	cmpa.l	4(%a3),%a1
	bne	.Lbs1_bn_direct_bad
	bra	.Lbs1_bn_direct_mvs
.Lbs1_bn_direct_internal:
	move.b	8(%a4),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	(%a2,%d0.l),%d0
	cmpa.l	%d0,%a0
	bne	.Lbs1_bn_direct_bad
	move.b	9(%a4),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	132(%a2,%d0.l),%d0
	cmpa.l	%d0,%a1
	bne	.Lbs1_bn_direct_bad

.Lbs1_bn_direct_mvs:
	move.w	0(%a4),%d0
	ext.l	%d0
	sub.l	%d2,%d0
	bpl	.Lbs1_bn_d0abs
	neg.l	%d0
.Lbs1_bn_d0abs:
	cmpi.l	#4,%d0
	bge	.Lbs1_bn_direct_bad
	move.w	2(%a4),%d0
	ext.l	%d0
	sub.l	%d3,%d0
	bpl	.Lbs1_bn_d1abs
	neg.l	%d0
.Lbs1_bn_d1abs:
	cmp.l	%d6,%d0
	bge	.Lbs1_bn_direct_bad
	move.w	4(%a4),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_bn_d2abs
	neg.l	%d0
.Lbs1_bn_d2abs:
	cmpi.l	#4,%d0
	bge	.Lbs1_bn_direct_bad
	move.w	6(%a4),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_bn_d3abs
	neg.l	%d0
.Lbs1_bn_d3abs:
	cmp.l	%d6,%d0
	bge	.Lbs1_bn_direct_bad
	moveq	#0,%d0			/* direct pairing passed */
	rts

.Lbs1_bn_direct_bad:
	tst.l	%d1
	beq	.Lbs1_bn_swapped_internal
	cmpa.l	4(%a3),%a0
	bne	.Lbs1_bn_true
	cmpa.l	0(%a3),%a1
	bne	.Lbs1_bn_true
	bra	.Lbs1_bn_swapped_mvs
.Lbs1_bn_swapped_internal:
	move.b	9(%a4),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	132(%a2,%d0.l),%d0
	cmpa.l	%d0,%a0
	bne	.Lbs1_bn_true
	move.b	8(%a4),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	move.l	(%a2,%d0.l),%d0
	cmpa.l	%d0,%a1
	bne	.Lbs1_bn_true

.Lbs1_bn_swapped_mvs:
	move.w	0(%a4),%d0
	ext.l	%d0
	sub.l	%d4,%d0
	bpl	.Lbs1_bn_s0abs
	neg.l	%d0
.Lbs1_bn_s0abs:
	cmpi.l	#4,%d0
	bge	.Lbs1_bn_true
	move.w	2(%a4),%d0
	ext.l	%d0
	sub.l	%d5,%d0
	bpl	.Lbs1_bn_s1abs
	neg.l	%d0
.Lbs1_bn_s1abs:
	cmp.l	%d6,%d0
	bge	.Lbs1_bn_true
	move.w	4(%a4),%d0
	ext.l	%d0
	sub.l	%d2,%d0
	bpl	.Lbs1_bn_s2abs
	neg.l	%d0
.Lbs1_bn_s2abs:
	cmpi.l	#4,%d0
	bge	.Lbs1_bn_true
	move.w	6(%a4),%d0
	ext.l	%d0
	sub.l	%d3,%d0
	bpl	.Lbs1_bn_s3abs
	neg.l	%d0
.Lbs1_bn_s3abs:
	cmp.l	%d6,%d0
	bge	.Lbs1_bn_true
	moveq	#0,%d0
	rts
.Lbs1_bn_true:
	moveq	#1,%d0
	rts

	.macro	MR_BS1_BN_LOAD_CURRENT curoff
	movea.l	8(%a6),%a3
	.if	\curoff
	adda.l	#\curoff,%a3
	.endif
	move.w	0(%a3),%d2
	ext.l	%d2
	move.w	2(%a3),%d3
	ext.l	%d3
	move.w	4(%a3),%d4
	ext.l	%d4
	move.w	6(%a3),%d5
	ext.l	%d5
	move.b	8(%a3),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	movea.l	(%a2,%d0.l),%a0
	move.b	9(%a3),%d0
	extb.l	%d0
	lsl.l	#2,%d0
	movea.l	132(%a2,%d0.l),%a1
	.endm

/* One edge call. bsoff is the packed horizontal/vertical word, neighbour
 * comes from argument nbrarg plus nbroff, and external==1 selects the
 * picture pair at argument picarg plus picoff. */
	.macro	MR_BS1_BN_EDGE bsoff, bitno, nibmask, nbrarg, nbroff, external, picarg, picoff
	move.l	\bsoff(%a5),%d7
	move.l	%d7,%d0
	and.l	#\nibmask,%d0
	bne	.Lbs1_bn_skip_\@
	movea.l	\nbrarg(%a6),%a4
	.if	\nbroff
	adda.l	#\nbroff,%a4
	.endif
	.if	\external
	movea.l	\picarg(%a6),%a3
	.if	\picoff
	adda.l	#\picoff,%a3
	.endif
	moveq	#1,%d1
	.else
	moveq	#0,%d1
	.endif
	bsr	.Lbs1_bn_check
	tst.l	%d0
	beq	.Lbs1_bn_skip_\@
	bset	#\bitno,%d7
	move.l	%d7,\bsoff(%a5)
.Lbs1_bn_skip_\@:
	.endm

mr_ih264d_fill_bs1_non16x16mb_bslice_m68k:
	link.w	%a6,#0
	movem.l	%d2-%d7/%a2-%a5,-(%sp)
	movea.l	16(%a6),%a2		/* list-0 reference map */
	movea.l	20(%a6),%a5		/* bS table */
	move.l	36(%a6),%d6		/* vertical MV threshold */

	/* Row 0. */
	MR_BS1_BN_LOAD_CURRENT 0
	MR_BS1_BN_EDGE 16,24,0x0f000000,24,0,1,28,0
	MR_BS1_BN_EDGE 0,24,0x0f000000,12,0,1,32,0
	MR_BS1_BN_LOAD_CURRENT 12
	MR_BS1_BN_EDGE 20,24,0x0f000000,8,0,0,0,0
	MR_BS1_BN_EDGE 0,16,0x000f0000,12,12,1,32,0
	MR_BS1_BN_LOAD_CURRENT 24
	MR_BS1_BN_EDGE 24,24,0x0f000000,8,12,0,0,0
	MR_BS1_BN_EDGE 0,8,0x00000f00,12,24,1,32,8
	MR_BS1_BN_LOAD_CURRENT 36
	MR_BS1_BN_EDGE 28,24,0x0f000000,8,24,0,0,0
	MR_BS1_BN_EDGE 0,0,0x0000000f,12,36,1,32,8

	/* Row 1. */
	MR_BS1_BN_LOAD_CURRENT 48
	MR_BS1_BN_EDGE 16,16,0x000f0000,24,48,1,28,0
	MR_BS1_BN_EDGE 4,24,0x0f000000,8,0,0,0,0
	MR_BS1_BN_LOAD_CURRENT 60
	MR_BS1_BN_EDGE 20,16,0x000f0000,8,48,0,0,0
	MR_BS1_BN_EDGE 4,16,0x000f0000,8,12,0,0,0
	MR_BS1_BN_LOAD_CURRENT 72
	MR_BS1_BN_EDGE 24,16,0x000f0000,8,60,0,0,0
	MR_BS1_BN_EDGE 4,8,0x00000f00,8,24,0,0,0
	MR_BS1_BN_LOAD_CURRENT 84
	MR_BS1_BN_EDGE 28,16,0x000f0000,8,72,0,0,0
	MR_BS1_BN_EDGE 4,0,0x0000000f,8,36,0,0,0

	/* Row 2. */
	MR_BS1_BN_LOAD_CURRENT 96
	MR_BS1_BN_EDGE 16,8,0x00000f00,24,96,1,28,8
	MR_BS1_BN_EDGE 8,24,0x0f000000,8,48,0,0,0
	MR_BS1_BN_LOAD_CURRENT 108
	MR_BS1_BN_EDGE 20,8,0x00000f00,8,96,0,0,0
	MR_BS1_BN_EDGE 8,16,0x000f0000,8,60,0,0,0
	MR_BS1_BN_LOAD_CURRENT 120
	MR_BS1_BN_EDGE 24,8,0x00000f00,8,108,0,0,0
	MR_BS1_BN_EDGE 8,8,0x00000f00,8,72,0,0,0
	MR_BS1_BN_LOAD_CURRENT 132
	MR_BS1_BN_EDGE 28,8,0x00000f00,8,120,0,0,0
	MR_BS1_BN_EDGE 8,0,0x0000000f,8,84,0,0,0

	/* Row 3. */
	MR_BS1_BN_LOAD_CURRENT 144
	MR_BS1_BN_EDGE 16,0,0x0000000f,24,144,1,28,8
	MR_BS1_BN_EDGE 12,24,0x0f000000,8,96,0,0,0
	MR_BS1_BN_LOAD_CURRENT 156
	MR_BS1_BN_EDGE 20,0,0x0000000f,8,144,0,0,0
	MR_BS1_BN_EDGE 12,16,0x000f0000,8,108,0,0,0
	MR_BS1_BN_LOAD_CURRENT 168
	MR_BS1_BN_EDGE 24,0,0x0000000f,8,156,0,0,0
	MR_BS1_BN_EDGE 12,8,0x00000f00,8,120,0,0,0
	MR_BS1_BN_LOAD_CURRENT 180
	MR_BS1_BN_EDGE 28,0,0x0000000f,8,168,0,0,0
	MR_BS1_BN_EDGE 12,0,0x0000000f,8,132,0,0,0

	movem.l	(%sp)+,%d2-%d7/%a2-%a5
	unlk	%a6
	rts

#if defined(__ELF__)
	.section .note.GNU-stack,"",@progbits
#endif
#endif
