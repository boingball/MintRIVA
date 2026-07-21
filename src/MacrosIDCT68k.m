*******************************************************************************
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program (See the included file COPYING);
** if not, write to the Free Software Foundation, Inc.,
** 675 Mass Ave, Cambridge, MA 02139, USA.
**
******************************************************************************
******************************************************************************
* Name:    RiVA v0.52                                                        *
* Date:    $Date: 2019-07-16 16:24:40 -0359 (Di, 16 Jul 2019) $             * 
* Authors: Stephen Fellner, some work by Henryk Richter                      *
******************************************************************************
* Inverse Discrete Cosine Transform, 68k version                             *
******************************************************************************
; Notes:
; - the implementation in this file is a fixed-point scaled AAN 8x8 iDCT
; - the scaled AAN is the fastest known iDCT algorithm, but it must be pointed
;   out that the fixed-point approach is not IEEE1180 compliant
; - This iDCT algorithm was kept regardless, since speed matters on 68k-based 
;   architectures -- way more than on recent mainstream silicon.
; - 3 Macros are present: first direction I/P and two macros for the second
;   direction (I and P)


;
; First iDCT loop, horizontal for Apollo builds (with scalar first stage) or
; vertical for 68k builds
;
; Input: D7    - last coefficient
;
; note: applies to I and P/B blocks
;
;
IDCTX_INTRA_68k	macro
			lea	zz_lines(pc),a1

	;The IDCT loop starts here:
		ifne	DCTLINEPOP
			lea	dct_linepopulation,a4			;highest written coefficient per line
		endc

			lea	dct_zz,a0

	; this step omits pointless transformations in the first direction
	; if the lines are empty, anyway -> don`t even try to transform anything
			moveq	#0,d0		;F
			move.b	(a1,d7.w),d0	;F B
			move.l	d0,a6		;  B

			move.l	a0,a1		; in-place iDCT (old routine used two buffers)
.idct_loop_x:
		ifne	DCTLINEPOP
			move.b	(a4)+,d0		;highest written coefficient per line
			blt	.only_0_A		;nothing written - empty line or row
			beq	.only_0			;zero means DC (per row/column) only
		endc

;Inverse DCT...
;--------------
;NOTE: All input coefficients have 4 bits of fraction. The convert_to_bitmap table will remove
;the fraction part, do proper rounding to the values and convert them to bitmap format, limiting
;the range between 0 and 255.

		; typ. 50% chance that one of the faster variants is appropriate
		move.w	1*DCTXI(a0),d0
		or.w	3*DCTXI(a0),d0
		or.w	5*DCTXI(a0),d0
		or.w	7*DCTXI(a0),d0
		beq	.only_0_2_4_6

		move.w	2*DCTXI(a0),d0
		or.w	4*DCTXI(a0),d0
		or.w	6*DCTXI(a0),d0
		beq	.only_1_3_5_7

.idct_full:	move.w	1*DCTXI(a0),d1

		move.w	7*DCTXI(a0),d0
		move.w	3*DCTXI(a0),d2
		move.w	5*DCTXI(a0),d3
		sub.w	d0,d1			;d1:tmp1 = F(1) - F(7)
		 sub.w	d2,d3			;d3:tmp3 = F(5) - F(3)

		add.w	1*DCTXI(a0),d0		;d0:tmp0 = F(1) + F(7)
		
		move.l	d1,d4
		add.w	d3,d4

		muls.w	#C6,d4			;d4:tmp4 = C6 * (tmp1 + tmp3)
		muls.w	#R,d1
		muls.w	#Q,d3

		add.w	5*DCTXI(a0),d2		;d2:tmp2 = F(5) + F(3)

		asr.l	#8,d4
		 asr.l	#8,d1

		sub.w	d4,d1			;d1:tmp6 = R * tmp1 - tmp4
		 asr.l	#8,d3

		sub.w	d4,d3			;d3:tmp5 = -Q * tmp3 - tmp4
		move.L	d0,d4
		 
		sub.w	d2,d4
		 add.w	d2,d0			;d0:tmp1 = tmp0 + tmp2 ** m0

		muls.w	#C4,d4			;d4:tmp3 = C4 * (tmp0 - tmp2)

		sub.w	d0,d1			;d1:tmp0 = tmp6 - tmp1 ** m2
		 asr.l	#8,d4

		move.l	d1,d2
		sub.w	d4,d2			;d2:tmp2 = tmp0 - tmp3 ** m1
		move.w	2*DCTXI(a0),d5

		move.w	6*DCTXI(a0),d4
		
		sub.w	d2,d3			;d3:tmp6 = tmp5 - tmp2 ** m7

		move.w	(a0),d7
		 move.l	d5,d6
		
		sub.w	d4,d5			;d5:tmp4 = F(2) - F(6)
		 add.w	d6,d4			;d4:tmp3 = F(2) + F(6)

		muls.w	#C4,d5			;d5:tmp5 = C4 * tmp4

		move.w	4*DCTXI(a0),d6

		asr.l	#8,d5

		move.l	d7,a2
		 sub.w	d6,d7			;d7:tmp7 = F(0) - F(4)
		
		sub.w	d4,d5			;d5:tmp4 = tmp5 - tmp3
		 add.w	a2,d6			;d6:tmp5 = F(0) + F(4)
		
		subq.l	#1,a6
		 move.l	d7,a2
		
		sub.w	d5,a2			;d0:tmp9 = tmp7 - tmp4
 		 add.w	d7,d5			;d5:tmp8 = tmp7 + tmp4

		move.l	d5,d7

		sub.w	d1,d7
		 add.w	d5,d1

		move.w	d7,6*DCTXO(a1)		;d7:tmp8 - tmp0 = f(6)
		move.w	d1,1*DCTXO(a1)		;d1:tmp8 + tmp0 = f(1)

		move.l	a2,d1
		sub.w	d2,d1
		 add.w	a2,d2
		move.w	d1,2*DCTXO(a1)		;d1:tmp9 - tmp2 = f(2)
		move.w	d2,5*DCTXO(a1)		;d2:tmp9 + tmp2 = f(5)

		move.l	d6,d7
		sub.w	d4,d7			;d7:tmp11 = tmp5 - tmp3
		 add.w	d6,d4			;d4:tmp10 = tmp5 + tmp3

		move.l	d4,d6
		sub.w	d0,d6
		 add.w	d4,d0
	 	move.w	d6,7*DCTXO(a1)		;d6:tmp10 - tmp1 = f(7)
		move.w	d0,(a1)			;d0:tmp10 + tmp1 = f(0)
		 move.l	d7,d0

		sub.w	d3,d0
		 add.w	d7,d3
		move.w	d0,3*DCTXO(a1)		;d0:tmp11 - tmp6 = f(3)
		move.w	d3,4*DCTXO(a1)		;d3:tmp11 + tmp6 = f(4)

		ifeq	DCTXIL-2
		 addq.l	#2,a0
		else
		 lea	DCTXIL(a0),a0
		endc
		ifeq	DCTXL-2
		 addq.l	#2,a1
		else
		 lea	DCTXL(a1),a1
		endc

		IDCT_COUNT0
		tst.l	a6
		bne	.idct_loop_x
		bra	.idctX_end

.only_1_3_5_7:
		move.w	1*DCTXI(a0),d5
		move.w	7*DCTXI(a0),d0

		subq.l	#1,a6
		 move.l	D5,D1
		
		sub.w	d0,d1		;d1: tmp1 = f(1) - f(7)
		 add.w	D5,d0		;d0: tmp0 = f(1) + f(7)

		move.w	5*DCTXI(a0),d4
		move.w	3*DCTXI(a0),d2

		move.l	D4,D3
		sub.w	d2,d3		;d3: tmp3 = f(5) - f(3)
		 add.w	d4,d2		;d2: tmp2 = f(5) + f(3)
		move.l	d1,d4
		add.l	d3,d4

		muls.w	#C6,d4
		muls.w	#Q,d3

		move.w	(a0),d7		;d7: f(0)
		 asr.l	#8,d4		;d4: tmp4 = C6 * (tmp1 + tmp3)

		muls.w	#R,d1
		ifeq	DCTXIL-2
		addq.l	#2,a0
		else
		lea		DCTXIL(a0),a0
		endc
		asr.l	#8,d3
		 asr.l	#8,d1

		sub.l	d4,d3		;d3: tmp5 = -Q * tmp3 - tmp4
		 sub.l	d4,d1		;d1: tmp6 = R * tmp1 - tmp4

		move.l	d0,d4
		 sub.w	d2,d0
		
		add.w	d2,d4		;d4: tmp1 = tmp0 + tmp2
		 sub.w	d4,d1		;d1: tmp0 = tmp6 - tmp1

		muls.w	#C4,d0

		asr.l	#8,d0		;d0: tmp3 = (tmp0 - tmp2) * C4
		 move.l	d1,d5
		
		sub.w	d0,d5		;d5: tmp2 = tmp0 - tmp3
		 move.l	d7,d6

		sub.w	d5,d3		;d3: tmp6 = tmp5 - tmp2
		 sub.w	d4,d6

		move.w	d6,7*DCTXO(a1)	;S(7) = f(0) - tmp1
		
		add.w	d7,d4
		 move.l	d7,d6

		move.w	d4,(a1)		;S(0) = f(0) + tmp1
		 sub.w	d1,d6

		move.w	d6,6*DCTXO(a1)	;S(6) = f(0) - tmp0
		
		add.w	d7,d1
		 move.l	d7,d6

		move.w	d1,1*DCTXO(a1)	;S(1) = f(0) + tmp0
		
		sub.w	d5,d6
		 add.w	d7,d5

		move.w	d6,2*DCTXO(a1)	;S(2) = f(0) - tmp2

		move.w	d5,5*DCTXO(a1)	;S(5) = f(0) + tmp2

		 move.l	d7,d6
		sub.w	d3,d6
		 add.w	d7,d3

		move.w	d6,3*DCTXO(a1)	;S(3) = f(0) - tmp6
		move.w	d3,4*DCTXO(a1)	;S(4) = f(0) + tmp6

		ifeq	DCTXL-2
		 addq.l	#2,a1
		else
		 lea.l	DCTXL(a1),a1
		endc

		IDCT_COUNT1
		tst.l	a6
		bne	.idct_loop_x
		bra	.idctX_end


.only_0_2_4_6:	
	ifeq	DCTLINEPOP
		or.w	2*DCTXI(a0),d0
		or.w	4*DCTXI(a0),d0
		or.w	6*DCTXI(a0),d0
		beq.b	.only_0
	endc
		move.w	2*DCTXI(a0),d3
		move.w	6*DCTXI(a0),d0
		subq.l	#1,a6
		 move.l	D3,D1
		
		sub.w	d0,d1			;d1:tmp1 = f(2) - f(6)
		 add.w	D3,d0			;d0:tmp0 = f(2) + f(6)

		muls.w	#C4,d1			;d1:tmp2 = tmp1 * C4

		move.w	(a0),d3
		 asr.l	#8,d1

		move.w	4*DCTXI(a0),d2
		
		sub.w	d0,d1			;d1:tmp1 = tmp2 - tmp0
		 sub.w	d2,d3			;d3:tmp3 = f(0) - f(4)
		
		add.w	(a0),d2			;d2:tmp2 = f(0) + f(4)
		 move.l	d3,d4
		add.w	d1,d4			;d4:tmp4 = tmp3 + tmp1
		
		 move.w	d4,1*DCTXO(a1)

		ifeq	DCTXIL-2
		 addq.l	#2,a0
		else
		 lea	DCTXIL(a0),a0
		endc
		 sub.w	d1,d3			;d3:tmp4 = tmp3 - tmp1

		move.w	d4,6*DCTXO(a1)
		move.w	d3,2*DCTXO(a1)
		move.w	d3,5*DCTXO(a1)

		move.l	d2,d4
		add.w	d0,d4			;d4:tmp4 = tmp2 + tmp0
		 sub.w	d0,d2			;d2:tmp4 = tmp2 - tmp0

		move.w	d4,(a1)
		move.w	d4,7*DCTXO(a1)
		move.w	d2,3*DCTXO(a1)
		move.w	d2,4*DCTXO(a1)
		
		ifeq	DCTXL-2
		 addq.l	#2,a1
		else
		 lea.l	DCTXL(a1),a1
		endc

		IDCT_COUNT2
		tst.l	a6
		bne	.idct_loop_x
		bra.b	.idctX_end

; zero coefficient
.only_0_A:
;		tst.w	(a0)	;verify that we land rightfully here (for testing/debugging)
;		beq.s	.zerook
		IDCT_COUNT3
;.zerook
		subq.l	#1,a6
		ifeq	DCTXIL-2
		 addq.l	#2,a0
		else
		 lea	DCTXIL(a0),a0
		endc
		ifeq	DCTXL-2
		 addq.l	#2,a1
		else
		 lea	DCTXL(a1),a1
		endc

		tst.l	a6
		bne	.idct_loop_x
		bra.b	.idctX_end

;
.only_0:	
		ifeq	DCTXIL-2
		 move.w	(a0)+,d0
		else
		 move.w	(a0),d0
		 lea	DCTXIL(a0),a0
		endc
		 subq.l	#1,a6
		move.w	d0,1*DCTXO(a1)
		move.w	d0,2*DCTXO(a1)
		move.w	d0,3*DCTXO(a1)
		move.w	d0,4*DCTXO(a1)
		move.w	d0,5*DCTXO(a1)
		move.w	d0,6*DCTXO(a1)
		move.w	d0,7*DCTXO(a1)
		ifeq	DCTXL-2
		 move.w	d0,(a1)+
		else
		 move.w	d0,(a1)
		 lea.l	DCTXL(a1),a1
		endc

		tst.l	a6
		bne	.idct_loop_x
.idctX_end:
		endm

;
;-------------------------------------------------------------------
; second direction iDCT for generic 68k CPUs
; this routine is Intra-Only. 
;-------------------------------------------------------------------
;
IDCTY_INTRA_68k	macro

idct_loop_y	

;Inverse DCT...
		move.w	1*DCTYO(a1),d0
		or.w	3*DCTYO(a1),d0
		or.w	5*DCTYO(a1),d0
		or.w	7*DCTYO(a1),d0
		beq	.only_0_2_4_6

		move.w	1*DCTYO(a1),d5
		move.w	7*DCTYO(a1),d0
		
		subq.l	#1,a6
		 move.l	D5,D1
 		sub.w	d0,d1			;d1:tmp1 = F(1) - F(7)
		 add.w	D5,d0			;d0:tmp0 = F(1) + F(7)

		move.w	3*DCTYO(a1),d2
		move.w	5*DCTYO(a1),d3

		move	d3,d7
		 sub.w	d2,d3			;d3:tmp3 = F(5) - F(3)

		add.w	d7,d2			;d2:tmp2 = F(5) + F(3)
		 move.l	d1,d4
		
		 add.w	d3,d4

		muls.w	#R,d1
		muls.w	#Q,d3
		muls.w	#C6,d4			;d4:tmp4 = C6 * (tmp1 + tmp3)

		asr.l	#8,d1
		 asr.l	#8,d4
		
		asr.l	#8,d3
		 sub.w	d4,d1			;d1:tmp6 = R * tmp1 - tmp4

		sub.w	d4,d3			;d3:tmp5 = -Q * tmp3 - tmp4
		 move.l	d0,d4
		
		sub.w	d2,d4
		 add.w	d2,d0			;d0:tmp1 = tmp0 + tmp2 ** m0

		muls.w	#C4,d4			;d4:tmp3 = C4 * (tmp0 - tmp2)

		sub.w	d0,d1			;d1:tmp0 = tmp6 - tmp1 ** m2
		 asr.l	#8,d4

		move.w	2*DCTYO(a1),d7

		move.l	d1,d2
		 move	d7,d5

		sub.w	d4,d2			;d2:tmp2 = tmp0 - tmp3 ** m1

		move.w	6*DCTYO(a1),d4
		
		sub.w	d2,d3			;d3:tmp6 = tmp5 - tmp2 ** m7
		 sub.w	d4,d5			;d5:tmp4 = F(2) - F(6)
		
		add.w	d7,d4			;d4:tmp3 = F(2) + F(6)
		 move.w	(a1),d7

		muls.w	#C4,d5			;d5:tmp5 = C4 * tmp4

		move.w	4*DCTYO(a1),d6

		asr.l	#8,d5
		 move.l	d7,a2
		sub.w	d4,d5			;d5:tmp4 = tmp5 - tmp3
		 sub.w	d6,d7			;d7:tmp7 = F(0) - F(4)

		add.w	a2,d6			;d6:tmp5 = F(0) + F(4)
		 move.l	d7,a2
		
		sub.w	d5,a2			;d0:tmp9 = tmp7 - tmp4
		 add.w	d7,d5			;d5:tmp8 = tmp7 + tmp4
		move.l	d5,d7
		
		sub.w	d1,d7
		 add.w	d5,d1

		DEFRAC	d7
		 DEFRAC	d1

		move.b	(a5,d1.w),1(a0)		;d1:tmp8 + tmp0 = f(1)
		move.b	(a5,d7.w),6(a0)		;d7:tmp8 - tmp0 = f(6)

		move.w	a2,d1
		 move.w	d6,d7
		sub.w	d2,d1
		 add.w	a2,d2
		DEFRAC	d1
		 DEFRAC	d2
		move.b	(a5,d1.w),2(a0)		;d1:tmp9 - tmp2 = f(2)
		move.b	(a5,d2.w),5(a0)		;d2:tmp9 + tmp2 = f(5)

		sub.w	d4,d7			;d7:tmp11 = tmp5 - tmp3
		 add.w	d6,d4			;d4:tmp10 = tmp5 + tmp3

		move.w	d4,d6
		ifeq	DCTYL-16
		 addq.l	#8,a1
		endc

		sub.w	d0,d6
		 add.w	d4,d0
		DEFRAC	d6
		 DEFRAC	d0

		move.b	(a5,d0.w),(a0)		;d0:tmp10 + tmp1 = f(0)
		move.b	(a5,d6.w),7(a0)		;d6:tmp10 - tmp1 = f(7)

		move.w	d7,d0
		ifeq	DCTYL-16
		 addq.l	#8,a1
		endc

		sub.w	d3,d0
		 add.w	d7,d3
		DEFRAC	d0
		 DEFRAC	d3

		move.l	block_count,d7

		move.b	(a5,d0.w),3(a0)		;d0:tmp11 - tmp6 = f(3)

		ifne	DCTYL-16
		 lea	DCTYL(a1),a1
		endc

		move.b	(a5,d3.w),4(a0)		;d3:tmp11 + tmp6 = f(4)

		add.l	(a4,d7.w*4),a0

		tst.l	a6
		bne	idct_loop_y
		bra	.idctY_end

.only_0_2_4_6:
		or.w	2*DCTYO(a1),d0
		or.w	4*DCTYO(a1),d0
		or.w	6*DCTYO(a1),d0
		beq.w	.only_0

		move.w	2*DCTYO(a1),d4
		move.w	6*DCTYO(a1),d0

		subq.l	#1,a6
		 move.l D4,D1
		
		sub.w	d0,d1			;d1:tmp1 = f(2) - f(6)
		 add.w	D4,d0			;d0:tmp0 = f(2) + f(6)

		muls.w	#C4,d1			;d1:tmp2 = tmp1 * C4

		move.w	(a1),d3
		 asr.l	#8,d1

		move.w	4*DCTYO(a1),d2
		
		sub.w	d0,d1			;d1:tmp1 = tmp2 - tmp0
		 sub.w	d2,d3			;d3:tmp3 = f(0) - f(4)
		
		add.w	(a1),d2			;d2:tmp2 = f(0) + f(4)
		 move.L	d3,d4
		
		add.w	d1,d4			;d4:tmp4 = tmp3 + tmp1
		 sub.w	d1,d3			;d3:tmp4 = tmp3 - tmp1
		
		DEFRAC	d4
		 DEFRAC	d3
		move.b	(a5,d4.w),d4
		move.b	d4,1(a0)
		move.b	d4,6(a0)
		
		move.b	(a5,d3.w),d3
		move.b	d3,2(a0)
		move.b	d3,5(a0)

		move.w	d2,d4
		 sub.w	d0,d2			;d2:tmp4 = tmp2 - tmp0
		
		add.w	d0,d4			;d4:tmp4 = tmp2 + tmp0
		 DEFRAC	d2
		DEFRAC	d4
		 moveq	#DCTYL,d7

		move.b	(a5,d4.w),d4
		move.b	d4,(a0)
		 add.l	d7,a1
		move.b	d4,7(a0)

		move.b	(a5,d2.w),d2
		move.l	block_count,d7
		move.b	d2,3(a0)
		move.b	d2,4(a0)
		
		add.l	(a4,d7.w*4),a0
		tst.l	a6
		bne		idct_loop_y
		bra.b	.idctY_end

.only_0		
		move.w	(a1),d0
		DEFRAC	d0
		 subq.l	#1,a6

		move.l	block_count,d7	;do something instead of stalling...
		lea	DCTYL(a1),a1

		move.w	(a5,d0.w),d1
		move.b	(a5,d0.w),d1
		move.w	d1,(a0)
		move.w	d1,2(a0)
		move.w	d1,4(a0)
		move.w	d1,6(a0)
		add.l	(a4,d7.w*4),a0
		tst.l	a6
		bne	idct_loop_y

.idctY_end:	movem.l	(a7)+,d0/a0

		endm

;
;-------------------------------------------------------------------
; second direction iDCT for generic 68k CPUs
; this routine is for P/B blocks 
;-------------------------------------------------------------------
;
IDCTY_INTER_68k	macro

.idct_y_start:
		move.l	#8,a6
.idct_loop_y:
		move.l	predict_clamp,a5

		move.w	1*DCTYO(a1),d0
		or.w	3*DCTYO(a1),d0
		or.w	5*DCTYO(a1),d0
		or.w	7*DCTYO(a1),d0
		beq	.only_0_2_4_6

;IDCT Contstants..
;C4		EQU	1448/4			; 1.414213562 << 10	/ SQR(2)      /
;C6		EQU	784/4			; 0.7653668647 << 10	/ 2*Sin(Pi/8) /
;Q		EQU	-1108/4			; 1.0823922 << 10	/ -(C2 - C6)  /
;R		EQU	2676/4			; 2.61312593 << 10	/ C2 + C6     /


.idct_full:	
		move.w	1*DCTYO(a1),d5
		move.w	7*DCTYO(a1),d0
		move.w	3*DCTYO(a1),d2	;F(3)
		move.w	5*DCTYO(a1),d3
		
		move.l	d5,d1		;
		 move	d3,d7		;F(5)
		sub.w	d2,d3		;d3:tmp3 = F(5) - F(3)
		 sub.w	d0,d1		;d1:tmp1 = F(1) - F(7)
		
		move.l	d1,d4
		 add.w	d5,d0		;d0:tmp0 = F(1) + F(7)
		add.w	d7,d2		;d2:tmp2 = F(5) + F(3)
		 add.w	d3,d4
		
		muls.w	#R,d1
		muls.w	#Q,d3
		muls.w	#C6,d4			;d4:tmp4 = C6 * (tmp1 + tmp3)

		asr.l	#8,d1
		 asr.l	#8,d4
		
		asr.l	#8,d3
		 sub.w	d4,d1			;d1:tmp6 = R * tmp1 - tmp4
		 
		sub.w	d4,d3			;d3:tmp5 = -Q * tmp3 - tmp4
		 move.l	d0,d4
		 
		sub.w	d2,d4
		 add.w	d2,d0			;d0:tmp1 = tmp0 + tmp2 ** m0
		
		muls.w	#C4,d4			;d4:tmp3 = C4 * (tmp0 - tmp2)

		sub.w	d0,d1			;d1:tmp0 = tmp6 - tmp1 ** m2
		 asr.l	#8,d4

		move.w	2*DCTYO(a1),d5

		move.l	d1,d2
		sub.w	d4,d2			;d2:tmp2 = tmp0 - tmp3 ** m1

		move.w	6*DCTYO(a1),d4

		sub.w	d2,d3			;d3:tmp6 = tmp5 - tmp2 ** m7
		 sub.w	d4,d5			;d5:tmp4 = F(2) - F(6)

		add.w	2*DCTYO(a1),d4		;d4:tmp3 = F(2) + F(6)

		muls.w	#C4,d5			;d5:tmp5 = C4 * tmp4

		move.w	(a1),d7
		 asr.l	#8,d5

		move.w	4*DCTYO(a1),d6
		 
		sub.w	d4,d5			;d5:tmp4 = tmp5 - tmp3
		 move.l	d7,a2
		sub.w	d6,d7			;d7:tmp7 = F(0) - F(4)
		 add.w	a2,d6			;d6:tmp5 = F(0) + F(4)
		move.l	d7,a2
		sub.w	d5,a2			;d0:tmp9 = tmp7 - tmp4
		 add.w	d7,d5			;d5:tmp8 = tmp7 + tmp4
		move.l	d5,d7
		sub.w	d1,d7
		 add.w	d5,d1
		moveq	#0,d5
		 DEFRAC	d1
		move.b	1(a4),d5
		DEFRAC	d7
		 add.w	d5,d1
		move.b	(a5,d1.w),1(a0)		;d1:tmp8 + tmp0 = f(1)
		move.l	a2,d1
		sub.w	d2,d1
		 move.b	6(a4),d5
		DEFRAC	d1
		 add.w	d5,d7
		move.b	(a5,d7.w),6(a0)		;d7:tmp8 - tmp0 = f(6)
		add.w	a2,d2
		DEFRAC	d2
		 move.b	2(a4),d5
		move.l	d6,d7
		sub.w	d4,d7			;d7:tmp11 = tmp5 - tmp3
		 add.w	d5,d1
		move.b	(a5,d1.w),2(a0)		;d1:tmp9 - tmp2 = f(2)
		add.w	d6,d4			;d4:tmp10 = tmp5 + tmp3
		move.l	d4,d6
		 move.b	5(a4),d5
		sub.w	d0,d6
		 add.w	d5,d2
		move.b	(a5,d2.w),5(a0)		;d2:tmp9 + tmp2 = f(5)
		DEFRAC	d6
		add.w	d4,d0
		DEFRAC	d0
		 move.b	(a4),d5
		 add.w	d5,d0
		move.b	(a5,d0.w),(a0)		;d0:tmp10 + tmp1 = f(0)
		move.l	d7,d0
		sub.w	d3,d0
		 move.b	7(a4),d5
		 add.w	d5,d6
		move.b	(a5,d6.w),7(a0)		;d6:tmp10 - tmp1 = f(7)
		DEFRAC	d0
		add.w	d7,d3
		 move.b	3(a4),d5
		 add.w	d5,d0
		move.b	(a5,d0.w),3(a0)		;d0:tmp11 - tmp6 = f(3)
		DEFRAC	d3
		 lea	DCTYL(a1),a1
		 move.b	4(a4),d5
		 add.w	d5,d3
		move.b	(a5,d3.w),4(a0)		;d3:tmp11 + tmp6 = f(4)
		 move.l	block_count,d7
		 subq.l	#1,a6
		 lea	idct_modulo_add,a5
		 addq.l	#8,a4
		 add.l	(a5,d7.w*4),a0
		 IDCT_COUNT4
		 tst.l	a6
		 bne	.idct_loop_y
		 bra	.idctY_done

.only_0_2_4_6	or.w	2*DCTYO(a1),d0
		or.w	4*DCTYO(a1),d0
		or.w	6*DCTYO(a1),d0
		beq	.only_0
		move.w	2*DCTYO(a1),d0
		move.w	d0,d1
		sub.w	6*DCTYO(a1),d1		;d1:tmp1 = f(2) - f(6)
		muls.w	#C4,d1			;d1:tmp2 = tmp1 * C4
		add.w	6*DCTYO(a1),d0		;d0:tmp0 = f(2) + f(6)
		asr.l	#8,d1
		sub.w	d0,d1			;d1:tmp1 = tmp2 - tmp0
		move.w	(a1),d2
		move.w	d2,d3
		sub.w	4*DCTYO(a1),d3		;d3:tmp3 = f(0) - f(4)
		 moveq	#0,d6
		add.w	4*DCTYO(a1),d2		;d2:tmp2 = f(0) + f(4)
		move.w	d3,d4
		add.w	d1,d4			;d4:tmp4 = tmp3 + tmp1
		DEFRAC	d4
		move.w	d4,d5
		 move.b	1(a4),d6
		 add.w	d6,d4
		 move.b	6(a4),d6
		 add.w	d6,d5
		move.b	(a5,d4.w),d4
		move.b	(a5,d5.w),d5
		move.b	d4,1(a0)
		move.b	d5,6(a0)
		sub.w	d1,d3			;d3:tmp4 = tmp3 - tmp1
		DEFRAC	d3
		move.w	d3,d5
		 move.b	2(a4),d6
		 add.w	d6,d3
		 move.b	5(a4),d6
		 add.w	d6,d5
		move.b	(a5,d3.w),d3
		move.b	(a5,d5.w),d5
		move.b	d3,2(a0)
		move.b	d5,5(a0)
		move.w	d2,d4
		add.w	d0,d4			;d4:tmp4 = tmp2 + tmp0
		DEFRAC	d4
		move.w	d4,d5
		 move.b	(a4),d6
		 add.w	d6,d4
		 move.b	7(a4),d6
		 add.w	d6,d5
		move.b	(a5,d4.w),d4
		 subq.l	#1,a6
		move.b	(a5,d5.w),d5
		 move.l	block_count,d7
		move.b	d4,(a0)
		move.b	d5,7(a0)
		sub.w	d0,d2			;d2:tmp4 = tmp2 - tmp0
		DEFRAC	d2
		move.w	d2,d5
		 move.b	3(a4),d6
		 add.w	d6,d2
		 move.b	4(a4),d6
		 add.w	d6,d5

		move.b	(a5,d2.w),d2
		 lea	DCTYL(a1),a1
		move.b	(a5,d5.w),d5
		 addq.l	#8,a4
		move.b	d2,3(a0)
		 lea	idct_modulo_add,a5
		move.b	d5,4(a0)
		 add.l	(a5,d7.w*4),a0

		 IDCT_COUNT5
		 tst.l	a6
		 bne	.idct_loop_y
		 bra	.idctY_done

; DC only - some improvements would be welcome
.only_0:	move.w	(a1),d0
		beq	.zero_ALL
		moveq	#0,d2
		DEFRAC	d0
		subq.l	#1,a6
;	bra	.nodc	; testing only, will lead to wrong picture but serves as indicator how often it is used

		; logic here:
		; compute DC component (D0), 
		; then add DC to every input byte (from A4), 
		; clip to 0/255
		; store byte

		move.w	d0,d1
		 move.b	(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),(a0)
		move.w	d1,d0
		 move.b	1(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),1(a0)
		move.w	d1,d0
		 move.b	2(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),2(a0)
		move.w	d1,d0
		 move.b	3(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),3(a0)
		move.w	d1,d0
		 move.b	4(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),4(a0)
		move.w	d1,d0
		 move.b	5(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),5(a0)
		move.w	d1,d0
		 move.b	6(a4),d2
		 add.w	d2,d0
		move.b	(a5,d0.w),6(a0)
		 move.b	7(a4),d2
		 add.w	d2,d1
		move.b	(a5,d1.w),7(a0)
		
.nodc
		move.l	block_count,d7
		lea	DCTYL(a1),a1
		 lea	idct_modulo_add,a5
		 addq.l	#8,a4
		add.l	(a5,d7.w*4),a0
		IDCT_COUNT6
		tst.l	a6
		 bne	.idct_loop_y
		 bra.b	.idctY_done

.zero_ALL:
		move.l	block_count,d7
		subq.l	#1,a6
		 move.l	(a4),(a0)
		 lea	DCTYL(a1),a1
		move.l	4(a4),4(a0)
		lea	idct_modulo_add,a5
		 addq.l	#8,a4
		 add.l	(a5,d7.w*4),a0
		IDCT_COUNT7

		tst.l	a6
		bne	.idct_loop_y
.idctY_done
		endm
