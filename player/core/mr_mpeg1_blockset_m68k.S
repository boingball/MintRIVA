/*
 * MintVID - hand-tuned m68k MPEG-1 motion-compensation block set.
 *
 * These 8 functions implement pl_mpeg's PLM_MB_CASE bodies (core/pl_mpeg.h,
 * plm_video_process_macroblock) - the per-macroblock copy/half-pel-average
 * step run for every P/B-picture macroblock (16x16 luma, 8x8 chroma), which
 * is the hottest part of MPEG-1 inter-frame decode: unlike the IDCT, which
 * only runs on blocks with non-zero coefficients, this runs unconditionally
 * for every inter-coded block. RiVA (this repo's 68k MPEG-1 reference, see
 * src/MacrosMotion.m) hand-tuned the same class of operation; this is a
 * fresh implementation matching pl_mpeg's own indexing/rounding exactly
 * rather than a transcription, since RiVA's block-buffer layout and vector
 * conventions do not match pl_mpeg's.
 *
 * All 8 share one contract:
 *   void NAME(uint8_t *dest, const uint8_t *src, int dw, int block_size);
 * dest/src are already offset to d[di]/s[si] by the C caller
 * (plm_video_process_macroblock, core/pl_mpeg.h) - the struct-layout-
 * dependent bounds check and si/di arithmetic stays in portable C (cheap,
 * once per macroblock call); only the O(block_size^2) inner loop below is
 * hand-asm. block_size is always 8 or 16 (chroma/luma) but nothing here
 * assumes a fixed value. dw is the shared row stride of both dest and src
 * (pl_mpeg always calls PLM_BLOCK_SET with DEST_WIDTH == SOURCE_WIDTH), so
 * "rowskip" (dw - block_size, the byte gap to the start of the next row) is
 * the same for both pointers.
 *
 * m68k C ABI: args on the stack from 4(%sp); d0/d1/a0/a1 are caller-saved
 * (scratch), d2-d7/a2-a6 are callee-saved. Each function here only needs
 * d0-d7/a0/a1, so only d2-d7 (24 bytes) need saving - stack-arg offsets
 * below are +24: 28(%sp)=dest, 32(%sp)=src, 36(%sp)=dw, 40(%sp)=block_size.
 *
 * Register plan (all 8 functions): %a0 = dest cursor, %a1 = src cursor
 * (post-incremented through each row), %d2 = block_size (reloaded into %d1
 * as the column counter each row), %d3 = dw (also used as the index
 * register for the "+dw"/"+dw+1" neighbour reads via m68k's indexed
 * addressing mode, e.g. `(%a1,%d3.l)` = src[dw], `1(%a1,%d3.l)` =
 * src[dw+1] - folds the stride multiply into the addressing mode instead of
 * a separate pointer/add), %d4 = rowskip, %d0 = row counter, %d5/%d6/%d7 =
 * per-pixel scratch (sums/averages). Pixel bytes are unsigned, so every
 * load zero-extends via `clr.l %dn; move.b ...,%dn` (never a sign-extending
 * move), matching this project's other hand-asm pixel loops.
 */
	.text

/* ---- mode 0: full-pel copy (interpolate=0, odd_h=0, odd_v=0) ----------- */
	.globl	plm_bs_copy_m68k
	.type	plm_bs_copy_m68k,@function
/* void plm_bs_copy_m68k(uint8_t *dest, const uint8_t *src, int dw, int block_size);
 * dest[i] = src[i]. */
plm_bs_copy_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0		/* dest */
	move.l	32(%sp),%a1		/* src */
	move.l	36(%sp),%d3		/* dw */
	move.l	40(%sp),%d2		/* block_size */
	move.l	%d3,%d4
	sub.l	%d2,%d4			/* rowskip = dw - block_size */

	move.l	%d2,%d0
.Lcopy_row:
	move.l	%d2,%d1
.Lcopy_col:
	move.b	(%a1)+,(%a0)+
	subq.l	#1,%d1
	bne.s	.Lcopy_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lcopy_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 1: vertical half-pel average (odd_v only) -------------------- */
	.globl	plm_bs_avg_v_m68k
	.type	plm_bs_avg_v_m68k,@function
/* dest[i] = (src[i] + src[i+dw] + 1) >> 1. */
plm_bs_avg_v_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lavgv_row:
	move.l	%d2,%d1
.Lavgv_col:
	clr.l	%d5
	move.b	(%a1),%d5		/* src[0] */
	clr.l	%d6
	move.b	(%a1,%d3.l),%d6		/* src[dw] */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lavgv_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lavgv_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 2: horizontal half-pel average (odd_h only) ------------------ */
	.globl	plm_bs_avg_h_m68k
	.type	plm_bs_avg_h_m68k,@function
/* dest[i] = (src[i] + src[i+1] + 1) >> 1. */
plm_bs_avg_h_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lavgh_row:
	move.l	%d2,%d1
.Lavgh_col:
	clr.l	%d5
	move.b	(%a1),%d5		/* src[0] */
	clr.l	%d6
	move.b	1(%a1),%d6		/* src[1] */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lavgh_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lavgh_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 3: horizontal+vertical quarter-pel average -------------------- */
	.globl	plm_bs_avg_hv_m68k
	.type	plm_bs_avg_hv_m68k,@function
/* dest[i] = (src[i] + src[i+1] + src[i+dw] + src[i+dw+1] + 2) >> 2. */
plm_bs_avg_hv_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lavghv_row:
	move.l	%d2,%d1
.Lavghv_col:
	clr.l	%d5
	move.b	(%a1),%d5		/* src[0] */
	clr.l	%d6
	move.b	1(%a1),%d6		/* src[1] */
	add.l	%d6,%d5
	clr.l	%d6
	move.b	(%a1,%d3.l),%d6		/* src[dw] */
	add.l	%d6,%d5
	clr.l	%d6
	move.b	1(%a1,%d3.l),%d6	/* src[dw+1] */
	add.l	%d6,%d5
	addq.l	#2,%d5
	asr.l	#2,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lavghv_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lavghv_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 4: bidirectional blend, full-pel source (interpolate) -------- */
	.globl	plm_bs_interp_copy_m68k
	.type	plm_bs_interp_copy_m68k,@function
/* dest[i] = (dest[i] + src[i] + 1) >> 1. */
plm_bs_interp_copy_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lic_row:
	move.l	%d2,%d1
.Lic_col:
	clr.l	%d5
	move.b	(%a0),%d5		/* dest[0] (existing) */
	clr.l	%d6
	move.b	(%a1),%d6		/* src[0] */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lic_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lic_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 5: bidirectional blend, vertical half-pel source -------------- */
	.globl	plm_bs_interp_avg_v_m68k
	.type	plm_bs_interp_avg_v_m68k,@function
/* dest[i] = (dest[i] + ((src[i] + src[i+dw] + 1) >> 1) + 1) >> 1. */
plm_bs_interp_avg_v_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Liv_row:
	move.l	%d2,%d1
.Liv_col:
	clr.l	%d6
	move.b	(%a1),%d6		/* src[0] */
	clr.l	%d7
	move.b	(%a1,%d3.l),%d7		/* src[dw] */
	add.l	%d7,%d6
	addq.l	#1,%d6
	asr.l	#1,%d6			/* inner half-pel average */
	clr.l	%d5
	move.b	(%a0),%d5		/* dest[0] (existing) */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Liv_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Liv_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 6: bidirectional blend, horizontal half-pel source ------------ */
	.globl	plm_bs_interp_avg_h_m68k
	.type	plm_bs_interp_avg_h_m68k,@function
/* dest[i] = (dest[i] + ((src[i] + src[i+1] + 1) >> 1) + 1) >> 1. */
plm_bs_interp_avg_h_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lih_row:
	move.l	%d2,%d1
.Lih_col:
	clr.l	%d6
	move.b	(%a1),%d6		/* src[0] */
	clr.l	%d7
	move.b	1(%a1),%d7		/* src[1] */
	add.l	%d7,%d6
	addq.l	#1,%d6
	asr.l	#1,%d6			/* inner half-pel average */
	clr.l	%d5
	move.b	(%a0),%d5		/* dest[0] (existing) */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lih_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lih_row

	movem.l	(%sp)+,%d2-%d7
	rts

/* ---- mode 7: bidirectional blend, h+v quarter-pel source ---------------- */
	.globl	plm_bs_interp_avg_hv_m68k
	.type	plm_bs_interp_avg_hv_m68k,@function
/* dest[i] = (dest[i] + ((src[i]+src[i+1]+src[i+dw]+src[i+dw+1]+2)>>2) + 1) >> 1. */
plm_bs_interp_avg_hv_m68k:
	movem.l	%d2-%d7,-(%sp)
	move.l	28(%sp),%a0
	move.l	32(%sp),%a1
	move.l	36(%sp),%d3
	move.l	40(%sp),%d2
	move.l	%d3,%d4
	sub.l	%d2,%d4

	move.l	%d2,%d0
.Lihv_row:
	move.l	%d2,%d1
.Lihv_col:
	clr.l	%d6
	move.b	(%a1),%d6		/* src[0] */
	clr.l	%d7
	move.b	1(%a1),%d7		/* src[1] */
	add.l	%d7,%d6
	clr.l	%d7
	move.b	(%a1,%d3.l),%d7		/* src[dw] */
	add.l	%d7,%d6
	clr.l	%d7
	move.b	1(%a1,%d3.l),%d7	/* src[dw+1] */
	add.l	%d7,%d6
	addq.l	#2,%d6
	asr.l	#2,%d6			/* inner quarter-pel average */
	clr.l	%d5
	move.b	(%a0),%d5		/* dest[0] (existing) */
	add.l	%d6,%d5
	addq.l	#1,%d5
	asr.l	#1,%d5
	move.b	%d5,(%a0)+
	addq.l	#1,%a1
	subq.l	#1,%d1
	bne.s	.Lihv_col
	adda.l	%d4,%a0
	adda.l	%d4,%a1
	subq.l	#1,%d0
	bne.s	.Lihv_row

	movem.l	(%sp)+,%d2-%d7
	rts
