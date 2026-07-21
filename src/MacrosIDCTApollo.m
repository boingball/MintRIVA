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
* Name:    RiVA v0.53                                                        *
* Date:    $Date: 2019-05-01 10:05:55 -0359 (Mi, 01 Mai 2019) $             * 
* Authors: Henryk Richter                                                    *
******************************************************************************
* Inverse Discrete Cosine Transform, Apollo SIMD version                     *
******************************************************************************
; Notes:
; - the implementation in this file is a fixed-point scaled AAN 8x8 iDCT
; - the scaled AAN is the fastest known iDCT algorithm, but it must be pointed
;   out that the fixed-point approach is not IEEE1180 compliant
; - This iDCT was kept regardless, since speed matters on 68k-based architectures
;   way more than on recent mainstream silicon.
; - Technical: Demonstrates the use of the transpose instructions and the butterfly
;              instructions
;
; - Changes in RiVA 0.53:
;   clear DCT input array along the way to avoid clearing the whole block
;   each time beforehand (which was regardless of the actual number of nonzero 
;   coefficients)
;

;Input:
; A1 - input array, organized 8x8 (shorts)
; A6 - number of populated rows
;Output:
; first 8x4 Block
;  D0 D4
;  D1 D5
;  D2 D6
;  D3 D7
; second 8x4 Block (not actually An, rather En but encoding-wise An)
;  E0 E4  (A0 A4)
;  E1 E5  (A1 A5)
;  E2 E6  (A2 A6)
;  E3 E7  (A3 A7)
; Second output option with APOLLO_IDCTXNOSTORE==0
;  A1 - output array, organized 8x8 (shorts)
;
IDCTX_Apollo		macro

			LOADAB		   1,0
			LOADd16AB	16,1,1
			LOADd16AB	32,1,2
			LOADd16AB	48,1,3

			LOADd16AB	 8,1,4
			LOADd16AB	24,1,5
			LOADd16AB	40,1,6
			LOADd16AB	56,1,7

			TRANSHiBB	0,8	;B8:B9   = 0 1
			TRANSLoBB	0,10	;B10:B11 = 2 3
			TRANSHiBB	4,12	;B12:B13 = 4 5
			TRANSLoBB	4,14	;B14:B15 = 6 7
			
		clr.l		(a1)	; this clock cycle was a bubble (due to B12 reuse directly below)
			BFLYWBBB	12,8,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
			BFLYWBBB	14,10,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
			BFLYWBBB	15,9,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
			BFLYWBBB	11,13,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
			;
			PMUL88iBB	C4,7,7	 ;B7  tmp12=(tmp02*C4)>>8
		clr.l		4(a1)	;
			BFLYWBBB	6,4,8	 ;B8 tmp0=tmp10+tmp13 B9 tmp3=tmp10-tmp13
			BFLYWBBB	2,0,10	 ;B10 tmp7=z11+z13 B11 tmp11=z11-z13
			PSUBWBBB	6,7,7	 ;B7 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13
		clr.l		8(a1)
			BFLYWBBB	7,5,6	 ;B6 tmp1=tmp11+tmp12 B7 tmp2=tmp11-tmp12
			PADDWBBB	3,1,0	 ;B0  z50 = z10+Z12
		clr.l		12(a1)
			PMUL88iBB	FIX_1_847759065,0,0 ; B0 z5 = (z10+z12)*C2
		clr.l		16(a1)
			PMUL88iBB	FIX_2_613125930,3,3 ; B3 tmp12 = -(c2+c6) * z10
		clr.l		20(a1)
			PMUL88iBB	FIX_1_082392200,1,1 ; B1 tmp10 = z12*(c2-c6)
		clr.l		24(a1)
			PMUL88iBB	C4,11,11 ;B11 tmp11=(z11-z13)*C4>>8
		clr.l		28(a1)
			PADDWBBB	0,3,3	 ;B3  tmp12 = -(c2+c6) * z10 + z5
		clr.l		32(a1)
			PSUBWBBB	0,1,1	 ;B1  tmp10 = z12*(c2-c6) - z5
		clr.l		36(a1)
			PSUBWBBB	10,3,12  ;B12 tmp6  = tmp12-tmp7
		clr.l		40(a1)
			PSUBWBBB	12,11,11 ;B11 tmp5  = tmp11-tmp6
		clr.l		44(a1)
			PADDWBBB	11,1,1   ;B1  tmp4  = tmp10+tmp5
		clr.l		48(a1)
			; now 4 butterflies and then a register swap and transpose
			; BFLY doesn't apply here nicely, we'd rather use transpose, hence manual butterflies
			; -> for vertical stage, we can butterfly directly to outputs...
			PSUBWBBB	1,9,3	;B3 = tmp3-tmp4
			PADDWBBB	1,9,4   ;B4 = tmp3+tmp4
		clr.l		52(a1)
			PADDWBBB	10,8,0	;B0 = tmp0+tmp7
			PADDWBBB	12,6,1  ;B1 = tmp1+tmp6
		clr.l		56(a1)
			PADDWBBB	11,7,2  ;B2 = tmp2+tmp5
			PSUBWBBB	11,7,5	;B5 = tmp2-tmp5
		clr.l		60(a1)
			PSUBWBBB	12,6,6  ;B6 = tmp1-tmp6
	subq.l	#4,a6			; any coefficients in second half of block ?
			PSUBWBBB	10,8,7  ;B7 = tmp0-tmp7
	tst.l	a6			;
;	dc.w	$5fff	;HINTLE
		;nop
		;nop
		;nop
		;nop

		;32
		;4xtranspose
			TRANSHiBD	0,0	;D0: A0 A1 A2 A3  D1: B0 B1 B2 B3
			TRANSLoBD	0,2	;D2: C0 C1 C2 C3  D3: D0 D1 D2 D3
			TRANSHiBD	4,4	;D4: A4 A5 A6 A7  D5: B4 B5 B6 B7
			TRANSLoBD	4,6	;D6: C4 C5 C6 C7  D7: D4 D5 D6 D7
			;8x4 Block layout
			; D0 D4
			; D1 D5
			; D2 D6
			; D3 D7
			; -> second 8x4 block, if populated
			;
	ifeq	APOLLO_IDCTXNOSTORE
			;temporary: store block, for now (TBR)
			store		d0,(a1)
			store		d1,16(a1)
			store		d2,32(a1)
			store		d3,48(a1)
			store		d4,8(a1)
			store		d5,24(a1)
			store		d6,40(a1)
			store		d7,56(a1)
	endc

	ble	.idctx_no_second_part	; -> no, skip transform steps

		;second half (last 4 lines)
			lea		64(a1),a1
			LOADAB		   1,0
			LOADd16AB	16,1,1
			LOADd16AB	32,1,2
			LOADd16AB	48,1,3

			LOADd16AB	 8,1,4
			LOADd16AB	24,1,5
			LOADd16AB	40,1,6
			LOADd16AB	56,1,7

			TRANSHiBB	0,8	;B8:B9   = 0 1
			TRANSLoBB	0,10	;B10:B11 = 2 3
			TRANSHiBB	4,12	;B12:B13 = 4 5
			TRANSLoBB	4,14	;B14:B15 = 6 7
		clr.l		(a1)
			BFLYWBBB	12,8,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
			BFLYWBBB	14,10,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
			BFLYWBBB	15,9,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
			BFLYWBBB	11,13,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
			;
			PMUL88iBB	C4,7,7	 ;B7  tmp12=(tmp02*C4)>>8
		clr.l		4(a1)
			BFLYWBBB	6,4,8	 ;B8 tmp0=tmp10+tmp13 B9 tmp3=tmp10-tmp13
			BFLYWBBB	2,0,10	 ;B10 tmp7=z11+z13 B11 tmp11=z11-z13
			PSUBWBBB	6,7,7	 ;B7 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13
		clr.l		8(a1)
			BFLYWBBB	7,5,6	 ;B6 tmp1=tmp11+tmp12 B7 tmp2=tmp11-tmp12
			PADDWBBB	3,1,0	 ;B0  z50 = z10+Z12
		clr.l		12(a1)
			PMUL88iBB	FIX_1_847759065,0,0 ; B0 z5 = (z10+z12)*C2
		clr.l		16(a1)
			PMUL88iBB	FIX_2_613125930,3,3 ; B3 tmp12 = -(c2+c6) * z10
		clr.l		20(a1)
			PMUL88iBB	FIX_1_082392200,1,1 ; B1 tmp10 = z12*(c2-c6)
		clr.l		24(a1)
			PMUL88iBB	C4,11,11 ;B11 tmp11=(z11-z13)*C4>>8
		clr.l		28(a1)
			PADDWBBB	0,3,3	 ;B3  tmp12 = -(c2+c6) * z10 + z5
		clr.l		32(a1)
			PSUBWBBB	0,1,1	 ;B1  tmp10 = z12*(c2-c6) - z5
		clr.l		36(a1)
			PSUBWBBB	10,3,12  ;B12 tmp6  = tmp12-tmp7
		clr.l		40(a1)
			PSUBWBBB	12,11,11 ;B11 tmp5  = tmp11-tmp6
			PADDWBBB	11,1,1   ;B1  tmp4  = tmp10+tmp5
		clr.l		44(a1)

			; now 4 butterflies and then a register swap and transpose
			; BFLY doesn't apply here nicely, we'd rather use transpose, hence manual butterflies
			; -> for vertical stage, we can butterfly directly to outputs...
			PSUBWBBB	1,9,3	;B3 = tmp3-tmp4
			PADDWBBB	1,9,4   ;B4 = tmp3+tmp4
		clr.l		48(a1)			
			PADDWBBB	10,8,0	;B0 = tmp0+tmp7
			PADDWBBB	12,6,1  ;B1 = tmp1+tmp6
		clr.l		52(a1)
			PADDWBBB	11,7,2  ;B2 = tmp2+tmp5
			PSUBWBBB	11,7,5	;B5 = tmp2-tmp5
		clr.l		56(a1)
			PSUBWBBB	12,6,6  ;B6 = tmp1-tmp6
			PSUBWBBB	10,8,7  ;B7 = tmp0-tmp7
		clr.l		60(a1)

		;32
		;4xtranspose into En (Dn high)
			TRANSHiBD	0,8	;a0: A0 A1 A2 A3  a1: B0 B1 B2 B3
			TRANSLoBD	0,10	;a2: C0 C1 C2 C3  a3: D0 D1 D2 D3
			TRANSHiBD	4,12	;a4: A4 A5 A6 A7  a5: B4 B5 B6 B7
			TRANSLoBD	4,14	;a6: C4 C5 C6 C7  a7: D4 D5 D6 D7
			;8x4 Block layout
			; D0 D4
			; D1 D5
			; D2 D6
			; D3 D7
			; -> second 8x4 block, if populated
			;
	ifeq	APOLLO_IDCTXNOSTORE
			;temporary: store block, for now (TBR)
			store		a0,(a1)
			store		a1,16(a1)
			store		a2,32(a1)
			store		a3,48(a1)
                                            
			store		a4,8(a1)
			store		a5,24(a1)
			store		a6,40(a1)
			store		a7,56(a1)
	endc
		bra.s	.idctx_done		; second part done
.idctx_no_second_part:
	ifne	1
		peor	e0,e0,e0	;don't fear, actually, these are E0-E7
		peor	e1,e1,e1
		peor	e2,e2,e2
		peor	e3,e3,e3
		peor	e4,e4,e4
		peor	e5,e5,e5
		peor	e6,e6,e6
		peor	e7,e7,e7
	else
		peor	a0,a0,a0	;don't fear, actually, these are E0-E7
		peor	a1,a1,a1
		peor	a2,a2,a2
		peor	a3,a3,a3
		peor	a4,a4,a4
		peor	a5,a5,a5
		peor	a6,a6,a6
		peor	a7,a7,a7
	endc
.idctx_done:
			endm

;Input:
; first 8x4 Block
;  D0 D4
;  D1 D5
;  D2 D6
;  D3 D7
; 2nd 8x4 Block (not actually An, rather En but encoding-wise An)
;  A0 A4
;  A1 A5
;  A2 A6
;  A3 A7
; Second Input Option:
;  with APOLLO_IDCTXNOSTORE=0, A1 is used as input array
;
;Output: inverse transform result, already scaled down by 4 bit
; first 8x4 Block
;  D0 D4
;  D1 D5
;  D2 D6
;  D3 D7
; second 8x4 Block (not actually An, rather En but encoding-wise An)
;  E0 E4  (A0 A4)
;  E1 E5  (A1 A5)
;  E2 E6  (A2 A6)
;  E3 E7  (A3 A7)
IDCTY_Apollo		macro

		;first half (first 4 columns)
	ifne	APOLLO_IDCTXNOSTORE
		;D0 D1 D2 D3 A0 A1 A2 A3
		BFLYWDDB	 8, 0,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
		BFLYWDDB	10, 2,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
		BFLYWDDB	11, 1,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
		BFLYWDDB	 3, 9,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
	else
			LOADd16AB	  0,1,8
			LOADd16AB	 16,1,9
			LOADd16AB	 32,1,10
			LOADd16AB	 48,1,11
			LOADd16AB	 64,1,12
			LOADd16AB	 80,1,13
			LOADd16AB	 96,1,14
			LOADd16AB	112,1,15

			BFLYWBBB	12,8,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
			BFLYWBBB	14,10,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
			BFLYWBBB	15,9,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
			BFLYWBBB	11,13,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
	endc
			;
			PMUL88iBB	C4,7,7	 ;B7  tmp12=(tmp02*C4)>>8
			BFLYWBBB	6,4,8	 ;B8 tmp0=tmp10+tmp13 B9 tmp3=tmp10-tmp13
			BFLYWBBB	2,0,10	 ;B10 tmp7=z11+z13 B11 tmp11=z11-z13
			PSUBWBBB	6,7,7	 ;B7 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13
			PADDWBBB	3,1,0	 ;B0  z50 = z10+Z12
			PMUL88iBB	FIX_2_613125930,3,3 ; B3 tmp12 = -(c2+c6) * z10
			PMUL88iBB	FIX_1_082392200,1,1 ; B1 tmp10 = z12*(c2-c6)
			PMUL88iBB	FIX_1_847759065,0,0 ; B0 z5 = (z10+z12)*C2
			PMUL88iBB	C4,11,11 ;B11 tmp11=(z11-z13)*C4>>8
			BFLYWBBB	7,5,6	 ;B6 tmp1=tmp11+tmp12 B7 tmp2=tmp11-tmp12
			PADDWBBB	0,3,3	 ;B3  tmp12 = -(c2+c6) * z10 + z5
			PSUBWBBB	0,1,1	 ;B1  tmp10 = z12*(c2-c6) - z5
			PSUBWBBB	10,3,12  ;B12 tmp6  = tmp12-tmp7
			PSUBWBBB	12,11,11 ;B11 tmp5  = tmp11-tmp6
			PADDWBBB	11,1,1   ;B1  tmp4  = tmp10+tmp5
			; now 4 butterflies and then scale down

			BFLYWBBB	12,6,2	;B2 tmp1+tmp6 B3 tmp1-tmp6
			BFLYWBBB	1,9,4	;B4 tmp3+tmp4 B5 tmp3-tmp4
			BFLYWBBB	10,8,0	;B0 tmp0+tmp7 B1 tmp0-tmp7
			BFLYWBBB	11,7,6	;B6 tmp2+tmp5 B7 tmp2-tmp5

			PMUL88iBD	16,6,2  ; D2
			PMUL88iBD	16,5,3  ; D3 ;5
			PMUL88iBD	16,0,0  ; D0
			PMUL88iBD	16,2,1  ; D1
			PMUL88iBD	16,4,8  ; E0 ;4
			PMUL88iBD	16,7,9  ; E1
			PMUL88iBD	16,3,10 ; E2
			PMUL88iBD	16,1,11 ; E3

	ifne	APOLLO_IDCTXNOSTORE
		;D4 D5 D6 D7 A4 A5 A6 A7
		BFLYWDDB	12, 4,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
		BFLYWDDB	14, 6,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
		BFLYWDDB	15, 5,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
		BFLYWDDB	 7,13,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
	else
		;second half (last 4 columns)
		LOADd16AB	  8,1,8		;F(0)
		LOADd16AB	 24,1,9		;F(1)
		LOADd16AB	 40,1,10	;F(2)
		LOADd16AB	 56,1,11	;F(3)
		LOADd16AB	 72,1,12	;F(4)
		LOADd16AB	 88,1,13	;F(5)
		LOADd16AB	104,1,14	;F(6)
		LOADd16AB	120,1,15	;F(7)
		
			BFLYWBBB	12,8,4	;B4 tmp10 =F(0)+F(4) B5 tmp11=F(0)-F(4)
			BFLYWBBB	14,10,6	;B6 tmp13 =F(2)+F(6) B7 tmp02=F(2)-F(6)
			BFLYWBBB	15,9,0	;B0 z11=F(1)+F(7) B1 z12=F(1)-F(7)
			BFLYWBBB	11,13,2	;B2 z13=F(5)+F(3) B3 z10=F(5)-F(3)
	endc		;
			PMUL88iBB	C4,7,7	 ;B7  tmp12=(tmp02*C4)>>8
			BFLYWBBB	6,4,8	 ;B8 tmp0=tmp10+tmp13 B9 tmp3=tmp10-tmp13
			BFLYWBBB	2,0,10	 ;B10 tmp7=z11+z13 B11 tmp11=z11-z13
			PSUBWBBB	6,7,7	 ;B7 tmp12=(tmp02*C4)>>8 - tmp13 = tmp12 - tmp13
			BFLYWBBB	7,5,6	 ;B6 tmp1=tmp11+tmp12 B7 tmp2=tmp11-tmp12
			PADDWBBB	3,1,0	 ;B0  z50 = z10+Z12
			PMUL88iBB	FIX_1_847759065,0,0 ; B0 z5 = (z10+z12)*C2
			PMUL88iBB	FIX_2_613125930,3,3 ; B3 tmp12 = -(c2+c6) * z10
			PMUL88iBB	FIX_1_082392200,1,1 ; B1 tmp10 = z12*(c2-c6)
			PMUL88iBB	C4,11,11 ;B11 tmp11=(z11-z13)*C4>>8
			PADDWBBB	0,3,3	 ;B3  tmp12 = -(c2+c6) * z10 + z5
			PSUBWBBB	0,1,1	 ;B1  tmp10 = z12*(c2-c6) - z5
			PSUBWBBB	10,3,12  ;B12 tmp6  = tmp12-tmp7
			PSUBWBBB	12,11,11 ;B11 tmp5  = tmp11-tmp6
			PADDWBBB	11,1,1   ;B1  tmp4  = tmp10+tmp5
			; now 4 butterflies and then a register swap and scaledown
			BFLYWBBB	1,9,4	;B4 tmp3+tmp4 B5 tmp3-tmp4
			BFLYWBBB	10,8,0	;B0 tmp0+tmp7 B1 tmp0-tmp7
			BFLYWBBB	12,6,2	;B2 tmp1+tmp6 B3 tmp1-tmp6
			BFLYWBBB	11,7,6	;B6 tmp2+tmp5 B7 tmp2-tmp5
			PMUL88iBD	16,0,4  ; D4
			PMUL88iBD	16,2,5  ; D5
			PMUL88iBD	16,6,6  ; D6 
			PMUL88iBD	16,5,7  ; D7
			PMUL88iBD	16,4,12 ; E4
			PMUL88iBD	16,7,13 ; E5
			PMUL88iBD	16,3,14 ; E6
			PMUL88iBD	16,1,15 ; E7
			endm
