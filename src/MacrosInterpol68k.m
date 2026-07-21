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
* Authors: Henryk Richter (bax)                                              *
******************************************************************************
* Motion Compensation interpolation macros, 68k version                      *
******************************************************************************
; A b B
; h j
; C   D
;
; (available pixels: A,B,C,D, subpixels b,h,j )
; b = (A+B+1)/2 = ((A|B)&1) + A>>1 + B>>1
; h = (A+C+1)/2 = ((A|C)&1) + A>>1 + C>>1
;
; j = (A+B+C+D+2)/4 = (q+r+1)/2 - (AB|CD)&qr
;  with q=(A+B+1)/2,  r=(C+D+1)/2,  AB=A^B,  CD=C^D,  qr=q^r
;
; 

; wrapper for Apollo "touch" instruction
dotouch	 macro
	 endm


; in:     -
; out: D0 - mask for lower two bits ($03030303)
;      D6 - mask for upper six bits ($FCFCFCFC)
MOT_8x1_HALFHORVER_INIT	macro
		move.l	#$03030303,d0	;
		move.l	#$FCFCFCFC,d6	;
		endm

MOT_8x1_HALFHOR_INIT	macro
		move.l	#$01010101,d0	;
		move.l	#$FEFEFEFE,d6	;
		endm


; purpose: 8x1 MC for hor+ver interpolation case
; in:  D0 - mask for lower two bits ($03030303)
;      D6 - mask for upper six bits ($FCFCFCFC)
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pixel pointer second line
; out:
;      A1 - unchanged
;      A2 - next position to write to  (+8)
;      A3 - unchanged
; trash:
;      D1,D2,D3,D7
; arguments: 
;
;
MOT_8x1_HALFHORVER	macro
			move.l	(a1),d1		; P00 P01 P02 P03
			move.l	1(a1),d2	; P01 P02 P03 P04
		 	move.l	d1,d3		; P00 P01 P02 P03
			 and.l	d6,d1		; upper 6 bits P00 P01 P02 P03
		 	and.l	d0,d3		; lower 2 bits P00 P01 P02 P03
			 move.l	d2,d7		; P01 P02 P03 P04
			and.l	d6,d7		; upper 6 bits P01 P02 P03 P04
			 and.l	d0,d2		; lower 2 bits P01 P02 P03 P04
			add.l	d2,d3		; lower 2 bits: P00+P01
			 move.l	(a3),d2		; P10 P11 P12 P13
			lsr.l	#2,d1		; P00>>2 ...
			 lsr.l	#2,d7		; P01>>2 ...
			add.l	d1,d7		; P00>>2 + P01>>2 ...
			 move.l	d2,d1		; P10 P11 P12 P13
			and.l	d6,d2		; upper 6 bits P10 P11 P12 P13
			 and.l	d0,d1		; lower 2 bits P10 P11 P12 P13
			lsr.l	#2,d2		; P10>>2 ...
			 add.l	d1,d3		; lower 2 bits: P00+P01+P10
			move.l	1(a3),d1	; P11 P12 P13 P14
			add.l	d2,d7		; upper 6 bits: P00>>2 + P01>>2 + P10>>2 ...
			 move.l	d1,d2		; P11 P12 P13 P14
			and.l	d6,d1		; upper 6 Bits: P11 P12 P13 P14
			 and.l	d0,d2		; lower 2 Bits: P11 P12 P13 P14
			lsr.l	#2,d1		; P11>>2 ...
			 add.l	d2,d3		; lower 2 Bits: P00+P01+P10+P11
			add.l	d1,d7		; upper 6 Bits: P00>>2 + P01>>2 + P10>>2 + P11>>2 ....
			;
			add.l	#$02020202,d3	; lower 2 Bits: P00+P01+P10+P11+2
			lsr.l	#2,d3		; lower 2 Bits: (P00+P01+P10+P11+2)>>2
			and.l	d0,d3		; leave lower two bits only 
			add.l	d3,d7
			 move.l	d0,d3		; 030303... (next 4 pixels)
			move.l	d7,(a2)+	; 
			move.l	4(a1),d1	; P00 P01 P02 P03 (next 4 pixels)
			move.l	5(a1),d2	; P01 P02 P03 P04 (next 4 pixels)
			and.l	d1,d3		; lower 2 bits P00 P01 P02 P03
			 move.l	d2,d7		; P01 P02 P03 P04
			and.l	d6,d1		; upper 6 bits P00 P01 P02 P03
			 and.l	d0,d2		; lower 2 bits P01 P02 P03 P04
			and.l	d6,d7		; upper 6 bits P01 P02 P03 P04
			 add.l	d2,d3		; lower 2 bits: P00+P01
			move.l	4(a3),d2	; P10 P11 P12 P13
			lsr.l	#2,d1		; P00>>2 ...
			 lsr.l	#2,d7		; P01>>2 ...
			add.l	d1,d7		; P00>>2 + P01>>2 ...
			 move.l	d2,d1		; P10 P11 P12 P13
			and.l	d0,d1		; lower 2 bits P10 P11 P12 P13
			 and.l	d6,d2		; upper 6 bits P10 P11 P12 P13
			lsr.l	#2,d2		; P10>>2 ...
			 add.l	d1,d3		; lower 2 bits: P00+P01+P10
			move.l  5(a3),d1	; P11 P12 P13 P14
			add.l	d2,d7		; upper 6 bits: P00>>2 + P01>>2 + P10>>2 ...
			 move.l	d1,d2		; P11 P12 P13 P14
			and.l	d6,d1		; upper 6 Bits: P11 P12 P13 P14
			 and.l	d0,d2		; lower 2 Bits: P11 P12 P13 P14
			lsr.l	#2,d1		; P11>>2 ...
			 add.l	d2,d3		; lower 2 Bits: P00+P01+P10+P11
			add.l	d1,d7		; upper 6 Bits: P00>>2 + P01>>2 + P10>>2 + P11>>2 ....
			add.l	#$02020202,d3	; lower 2 Bits: P00+P01+P10+P11+2
			lsr.l	#2,d3		; lower 2 Bits: (P00+P01+P10+P11+2)>>2
			and.l	d0,d3		; leave lower two bits only
			add.l	d3,d7
			move.l	d7,(a2)+	; 
			endm

; purpose: 8x1 MC for hor+ver interpolation case which averages with the destination contents 
; in:  D0 - mask for lower two bits ($03030303)
;      D6 - mask for upper six bits ($FCFCFCFC)
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pixel pointer second line
; out:
;      A1 - unchanged
;      A2 - next position to write to  (+8)
;      A3 - unchanged
; trash:
;      D1,D2,D3,D7
; arguments: 
MOT_8x1_HALFHORVER_ADD	macro
			move.l	(a1),d1		; P00 P01 P02 P03
			 move.l	d0,d3		; 030303...
			move.l	1(a1),d2	; P01 P02 P03 P04
			and.l	d1,d3		; lower 2 bits P00 P01 P02 P03
			 move.l	d2,d7		;F P01 P02 P03 P04
			and.l	d6,d1		; upper 6 bits P00 P01 P02 P03
			 and.l	d0,d2		; lower 2 bits P01 P02 P03 P04
			and.l	d6,d7		;F upper 6 bits P01 P02 P03 P04
			 add.l	d2,d3		; lower 2 bits: P00+P01
			move.l	(a3),d2		; P10 P11 P12 P13
			 lsr.l	#2,d1		; P00>>2 ...
			lsr.l	#2,d7		; P01>>2 ...
			add.l	d1,d7		; P00>>2 + P01>>2 ...
			move.l	d2,d1		; P10 P11 P12 P13
			 and.l	d6,d2		; upper 6 bits P10 P11 P12 P13
			and.l	d0,d1		; lower 2 bits P10 P11 P12 P13
			 lsr.l	#2,d2		; P10>>2 ...
			add.l	d1,d3		; lower 2 bits: P00+P01+P10
			move.l	1(a3),d1	; P11 P12 P13 P14
			add.l	d2,d7		; upper 6 bits: P00>>2 + P01>>2 + P10>>2 ...
			 move.l	d1,d2		; P11 P12 P13 P14
			and.l	d6,d1		; upper 6 Bits: P11 P12 P13 P14
			 and.l	d0,d2		; lower 2 Bits: P11 P12 P13 P14
			lsr.l	#2,d1		; P11>>2 ...
			 add.l	d2,d3		; lower 2 Bits: P00+P01+P10+P11
			add.l	d1,d7		; upper 6 Bits: P00>>2 + P01>>2 + P10>>2 + P11>>2 ....
			add.l	#$02020202,d3	; lower 2 Bits: P00+P01+P10+P11+2
			move.l	(a2),d2		; d0 d1 d2 d3
			 lsr.l	#2,d3		; lower 2 Bits: (P00+P01+P10+P11+2)>>2
			move.l	d2,d1		; d0 d1 d2 d3
			and.l	#$fefefefe,d1	; upper 7 bits d0 d1 d2 d3
			and.l	d0,d3		; leave lower two bits only
			 lsr.l	#1,d1		; d0>>1 d1>>1 d2>>1 d3>>1
			add.l	d3,d7		; r0 r1 r2 r3
			;
			or.l	d7,d2		; (d0|r0) (d1|r1) (d2|r2) (d3|r3)
			and.l	#$fefefefe,d7   ; upper 7 bits r0 r1 r2 r3
			lsr.l	#1,d7		; r0>>1 r1>>1 r2>>1 r3>>1
			and.l	#$01010101,d2	; (d0|res)&1
			add.l	d1,d7		; (r0>>1)+(d0>>1)
			 ;
			add.l	d2,d7		;
			move.l	d7,(a2)+	;
			;
			move.l	4(a1),d1	; P00 P01 P02 P03 (next 4 pixels)
			move.l	5(a1),d2	; P01 P02 P03 P04 (next 4 pixels)
			move.l	d0,d3		; 030303... (next 4 pixels)
			 and.l	d1,d3		; lower 2 bits P00 P01 P02 P03
			and.l	d6,d1		; upper 6 bits P00 P01 P02 P03
			move.l	d2,d7		; P01 P02 P03 P04
			 and.l	d6,d7		; upper 6 bits P01 P02 P03 P04
			 and.l	d0,d2		; lower 2 bits P01 P02 P03 P04
			add.l	d2,d3		; lower 2 bits: P00+P01
			move.l	4(a3),d2		; P10 P11 P12 P13
			 lsr.l	#2,d1		; P00>>2 ...
			 lsr.l	#2,d7		; P01>>2 ...
			add.l	d1,d7		; P00>>2 + P01>>2 ...
			move.l	d2,d1		; P10 P11 P12 P13
			 and.l	d0,d1		; lower 2 bits P10 P11 P12 P13
			 and.l	d6,d2		; upper 6 bits P10 P11 P12 P13
			lsr.l	#2,d2		; P10>>2 ...
			add.l	d1,d3		; lower 2 bits: P00+P01+P10
			 move.l 5(a3),d1	; P11 P12 P13 P14
			 add.l	d2,d7		; upper 6 bits: P00>>2 + P01>>2 + P10>>2 ...
			move.l	d1,d2		; P11 P12 P13 P14
			and.l	d6,d1		; upper 6 Bits: P11 P12 P13 P14
			 and.l	d0,d2		; lower 2 Bits: P11 P12 P13 P14
			 lsr.l	#2,d1		; P11>>2 ...
			add.l	d2,d3		; lower 2 Bits: P00+P01+P10+P11
			add.l	d1,d7		; upper 6 Bits: P00>>2 + P01>>2 + P10>>2 + P11>>2 ....
			 add.l	#$02020202,d3	; lower 2 Bits: P00+P01+P10+P11+2
			 move.l	(a2),d2		; d0 d1 d2 d3
			lsr.l	#2,d3		; lower 2 Bits: (P00+P01+P10+P11+2)>>2
			move.l	d2,d1		; d0 d1 d2 d3
			and.l	#$fefefefe,d1	; upper 7 bits d0 d1 d2 d3
			 and.l	d0,d3		; leave lower two bits only
			 lsr.l	#1,d1		; d0>>1 d1>>1 d2>>1 d3>>1
			add.l	d3,d7		; r0 r1 r2 r3
			;
			 or.l	d7,d2		; (d0|r0) (d1|r1) (d2|r2) (d3|r3)
			 and.l	#$fefefefe,d7   ; upper 7 bits r0 r1 r2 r3
			lsr.l	#1,d7		; r0>>1 r1>>1 r2>>1 r3>>1
			and.l	#$01010101,d2	; (d0|res)&1
			 add.l	d1,d7		; (r0>>1)+(d0>>1)
			 ;
			add.l	d2,d7		;B
			move.l	d7,(a2)+	;B 

			endm


MOT_8x1_HALFHOR		macro
			move.l	1(a1),d2	; P01 P02 P03 P04
			move.l	(a1)+,d1	; P00 P01 P02 P03
			move.l	d1,d3
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			or.l	d2,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d2		; upper 7 bits P01 P02 P03 P04
			lsr.l	#1,d1		; 
			 lsr.l	#1,d2		;
			and.l	d0,d3		; keep the 1
			 add.l	d1,d2		; P00+P01 .. .. ..
			move.l	1(a1),d7	; P05 P06 P07 P08
			move.l	(a1),d1		; P04 P05 P06 P07
			 add.l	d3,d2		; (P00+P01+1)>>1 .. .. ..
			move.l	d1,d3
			 move.l	d2,(a2)+
			or.l	d7,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d7		; upper 7 bits P01 P02 P03 P04
			 lsr.l	#1,d1		; 
			lsr.l	#1,d7		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d7		; P00+P01 .. .. ..
			 subq.l	#4,a1
			add.l	d3,d7		; (P00+P01+1)>>1 .. .. ..
			move.l	d7,(a2)+
			endm

; purpose: 8x1 MC for horizontal interpolation case which averages with the destination contents 
;  in: D0 - mask for lower bits $01010101
;      D6 - mask for upper bits $FEFEFEFE
;      A1 - input pixel pointer first line
;      A2 - output pointer 
; out:
;      A1 - input pixel (unchanged)
;      A2 - next position to write to  (+4)
; trash:
;      D1,D2,D3,D7
MOT_8x1_HALFHOR_ADD	macro
			move.l	1(a1),d2	; P01 P02 P03 P04
			move.l	(a1)+,d1	; P00 P01 P02 P03
			move.l	d1,d3
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			or.l	d2,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d2		; upper 7 bits P01 P02 P03 P04
			lsr.l	#1,d1		; 
			 lsr.l	#1,d2		;
			and.l	d0,d3		; keep the 1
			 add.l	d1,d2		; P00+P01 .. .. ..
			move.l	(a2),d7		; d0 d1 d2 d3
			 add.l	d3,d2		; (P00+P01+1)>>1 .. .. .. = r0 r1 r2 r3
			move.l	d2,d1		; d0 d1 d2 d3
			 and.l	d6,d2		; upper 7 bits r0 r1 r2 r3
			or.l	d7,d1		; d0|r0 d1|r1 d2|r2 d3|r3
			 and.l	d6,d7		; upper 7 bits d0 d1 d2 d3
			lsr.l	#1,d2		; r0>>1 r1>>1 r2>>1 r3>>1
			 lsr.l	#1,d7		; d0>>1 d1>>1 d2>>1 d3>>1
			and.l	d0,d1		; keep lowest bit only 
			 add.l	d7,d2		;
			add.l	d1,d2
			 move.l	(a1),d1		; P04 P05 P06 P07
			move.l	d2,(a2)+
			 move.l	d1,d3
			;
			move.l	1(a1),d7	; P05 P06 P07 P08
			or.l	d7,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d7		; upper 7 bits P01 P02 P03 P04
			 lsr.l	#1,d1		; 
			lsr.l	#1,d7		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d7		; P00+P01 .. .. ..
			 subq.l	#4,a1
			add.l	d3,d7		; (P00+P01+1)>>1 .. .. .. = r4 r5 r6 r7
			 move.l	(a2),d2		;d4 d5 d6 d7
			move.l	d2,d1		;d4 d5 d6 d7
			 and.l	d6,d2		;upper 7 bits d4 d5 d6 d7
			or.l	d7,d1		;d4|r4 d5|r5 d6|r6 d7|r7
			 and.l	d6,d7		;upper 7 bits r4 r5 r6 r7
			lsr.l	#1,d2
			 lsr.l	#1,d7
			and.l	d0,d1		;keep 1
			 add.l	d2,d7
			add.l	d1,d7
			move.l	d7,(a2)+
			endm


; purpose: 8x1 MC for vertical interpolation case
; in:  D0 - mask for lower two bits ($01010101)
;      D6 - mask for upper six bits ($FeFeFeFe)
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pixel pointer second line
; out:
;      A1 - input pixel (unchanged)
;      A2 - next position to write to  (+4)
;      A3 - input pixel (unchanged)
; trash:
;      D1,D2,D3,D7
MOT_8x1_HALFVER		macro
			move.l	(a1)+,d1	; P00 P01 P02 P03
			move.l	(a3)+,d2	; P10 P11 P12 P13
			 move.l	d1,d3		; P00 P01 P02 P03
			or.l	d2,d3		; P00|P10 P01|P11 P02|P12 P03|P13 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d2		; upper 7 bits P10 P11 P12 P13
			 lsr.l	#1,d1		; >>1
			lsr.l	#1,d2		; >>1
			 and.l	d0,d3		; keep the 1
			add.l	d1,d2		; P00+P01 .. .. ..
			 move.l	(a1),d1		; P04 P05 P06 P07
			add.l	d3,d2		; (P00+P10+1)>>1 .. .. ..
			 move.l	(a3),d7		; P14 P15 P16 P17
			move.l	d2,(a2)+	; store
			 move.l	d1,d3		; P04 P05 P06 P07
			or.l	d7,d3		; P04|P14 P05|P15 P06|P16 P07|P17 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P04 P05 P06 P07
			and.l	d6,d7		; upper 7 bits P14 P15 P16 P17
			 lsr.l	#1,d1		; 
			lsr.l	#1,d7		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d7		; P04+P14 .. .. ..
			 subq.l	#4,a1
			add.l	d3,d7		; (P04+P14+1)>>1 .. .. ..
			 subq.l	#4,a3
			move.l	d7,(a2)+	; store
			endm

; purpose: 8x1 MC for vertical interpolation case which averages with the destination contents 
;  in: D0 - mask for lower bits $01010101
;      D6 - mask for upper bits $FEFEFEFE
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pointer second line
; out:
;      A1 - input pixel (unchanged)
;      A2 - next position to write to  (+4)
; trash:
;      D1,D2,D3,D7
; clone of MOT_8x1_HALFHOR_ADD
MOT_8x1_HALFVER_ADD	macro
			move.l	(a1)+,d1	; P00 P01 P02 P03
			move.l	(a3)+,d2	; P01 P02 P03 P04
			 move.l	d1,d3
			or.l	d2,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d2		; upper 7 bits P01 P02 P03 P04
			 lsr.l	#1,d1		; 
			lsr.l	#1,d2		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d2		; P00+P01 .. .. ..
			 move.l	(a2),d7		; d0 d1 d2 d3
			add.l	d3,d2		; (P00+P01+1)>>1 .. .. .. = r0 r1 r2 r3
			 move.l	d7,d1		; d0 d1 d2 d3
			or.l	d2,d1		; d0|r0 d1|r1 d2|r2 d3|r3
			 and.l	d6,d2		; upper 7 bits r0 r1 r2 r3
			and.l	d6,d7		; upper 7 bits d0 d1 d2 d3
			 lsr.l	#1,d2		; r0>>1 r1>>1 r2>>1 r3>>1
			lsr.l	#1,d7		; d0>>1 d1>>1 d2>>1 d3>>1
			 and.l	d0,d1		; keep lowest bit only 
			add.l	d7,d2		;
			 move.l	(a3),d7		; P05 P06 P07 P08
			add.l	d1,d2
			 move.l	(a1),d1		; P04 P05 P06 P07
			move.l	d2,(a2)+
			 move.l	d1,d3
			or.l	d7,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d7		; upper 7 bits P01 P02 P03 P04
			 lsr.l	#1,d1		; 
			lsr.l	#1,d7		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d7		; P00+P01 .. .. ..
			 subq.l	#4,a1
			add.l	d3,d7		; (P00+P01+1)>>1 .. .. .. = r4 r5 r6 r7
			 move.l	(a2),d2		;d4 d5 d6 d7
			move.l	d2,d1		;d4 d5 d6 d7
			 and.l	d6,d2		;upper 7 bits d4 d5 d6 d7
			or.l	d7,d1		;d4|r4 d5|r5 d6|r6 d7|r7
			 and.l	d6,d7		;upper 7 bits r4 r5 r6 r7
			lsr.l	#1,d2
			 lsr.l	#1,d7
			and.l	d0,d1		;keep 1
			 add.l	d2,d7
			add.l	d1,d7
			 subq.l	#4,a3
			move.l	d7,(a2)+
			endm

; purpose: 8x1 MC for fullpel position wit add
;  in: D0 - mask for lower bits $01010101
;      D6 - mask for upper bits $FEFEFEFE
;      A1 - input pixel pointer first line
;      A2 - output pointer 
; out:
;      A1 - input pixel (unchanged)
;      A2 - next position to write to  (+4)
; trash:
;      D1,D2,D3,D7
; note: clone of MOT_8x1_HALFVER
MOT_8x1_FULL_ADD	macro
			move.l	(a1)+,d1	; P00 P01 P02 P03
			movem.l	(a2),d2/d7	; P01 P02 P03 P04,P05 P06 P07 P08
			 move.l	d1,d3
			or.l	d2,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			 and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			and.l	d6,d2		; upper 7 bits P01 P02 P03 P04
			 lsr.l	#1,d1		; 
			lsr.l	#1,d2		;
			 and.l	d0,d3		; keep the 1
			add.l	d1,d2		; P00+P01 .. .. ..
			 move.l	(a1),d1		; P04 P05 P06 P07
			add.l	d3,d2		; (P00+P01+1)>>1 .. .. ..
			 move.l	d1,d3
			move.l	d2,(a2)+
			 or.l	d7,d3		; P00|P01 P01|P02 P02|P03 P03|P04 -> meaning: we need to add "1" whenever any of the operands has it's LSB set
			and.l	d6,d1		; upper 7 bits P00 P01 P02 P03
			 and.l	d6,d7		; upper 7 bits P01 P02 P03 P04
			lsr.l	#1,d1		; 
			 lsr.l	#1,d7		;
			and.l	d0,d3		; keep the 1
			 add.l	d1,d7		; P00+P01 .. .. ..
			subq.l	#4,a1
			 add.l	d3,d7		; (P00+P01+1)>>1 .. .. ..
			move.l	d7,(a2)+
			endm


