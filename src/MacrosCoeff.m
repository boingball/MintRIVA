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
* Date:    $Date: 2017-02-13 11:37:09 +0200 (Mo, 13 Feb 2017) $             * 
* Authors: Henryk Richter (bax)                                              *
******************************************************************************
* Functions for coefficient loading, DC/AC, Luma and Chroma                  *
******************************************************************************

;
; Intra DC decoding for Luma/Chroma
; Inputs: A0 - Bitstream
;         D0 - Bitstream position (in Bits)
;         A2 - VLC decoding table
; args:   \1 = extra bit for chroma, \2 = old DC
DECODE_INTRA_DC		macro
			;Get dct dc size for blocks (luma/chroma):
			CHKBITS 16,d1
			move.l	d1,d2
		ifle	\1
			moveq	#9-\1,d3
			lsr.l	d3,d2
		else
			lsr.l	#8,d2
		endc
			moveq	#1,d4
			moveq	#16,d5
			move.w	(a2,d2.w*2),d3

			;CHKBITS 7+\1,d2 ;these 4 instructions would be less in mnemonics but are slower
			;moveq	#1,d4
			;CHKBITS 16,d1
			;move.w	(a2,d2.w*2),d3

			moveq	#0,d2	;F
			move.b	d3,d2	;F
			lsr.w	#8,d3				;dct_dc_size is now in d3
			 add.b	d3,d2				;add VLC length of dc_size and dc_size itself
			 lsl.l	d3,d4				;this makes a mask (1's) over the dct_diff
			NEXTVLC d2				;to skip the required no. of bits in stream. (d2.l !)
			sub.l	d2,d5				; 
			 subq.l	#1,d4				;bits for both and-ing and eor-ing
			 lsr.w	d5,d1				;dct_diff is now right-aligned in d1
			subq.w	#1,d3				;dct_size - 1 for btst (because btst starts from 0)
			and.l	d4,d1				;mask out excess bits from dct_dc_diff
			 btst	d3,d1				;top bit set (yes=positive number, no=negative number)
			seq	d5				;top bit not set -> 0xff, else 0
			add.l	\2,d1				;add.l	dct_dc_y_past(pc),d1	;take difference from last dc coefficient
			 and.b	d5,d4				;either subtract d4 (anything between 0 and 255) or don't (latter for positive numbers)
			sub.l	d4,d1
		endm

;
; In:  D0/A0 Bitstream
;      A4    VLD table
;      \1    end of block decoding jump
; Out: D1.w  level
;      D2.b  run
;
; Trash: D3,D4
;
DECODE_AC_COEFF		MACRO

			CHKBITS 32,d3
			bge.s	.P_y_normalvlc
		ifne	\1
			add.l	d3,d3
			bmi.s	.P_y_level
			SKPBITS	2
			bra	\2	;P_reconstruction_done
.P_y_level:
			add.l	d3,d3
			moveq	#31,d1
			NSKPBITS 3,d2
			asr.l	d1,d3
			moveq	#1,d1
			moveq	#0,d2
			or.l	d3,d1
			bra	.P_y_next_coeff_out
		else
			 moveq	#31,d1
			 add.l	d3,d3			;shift bit 16+14 to sign bit
			asr.l	d1,d3			;distribute sign bit across d3 (0 or -1)
			moveq	#1,d1			;
			 moveq	#0,d2			;run=0
			 or.l	d3,d1			;level = 1/-1
			SKPBITS	2
			bra	\2
		endc
.P_y_normalvlc:
			swap	d3
			moveq	#0,d2
			moveq	#0,d1
			moveq	#1,d4
			move.w	2(a4,d3.w*4),d2		;run bitlength .w
			blt.s	.P_y_next_BLK_ESC	;run|0x80 = ESC, 6 bit prefix length

			add.b	d2,d4			;bitlength + 1
			move.b	1(a4,d3.w*4),d1		;level.b
			rol.l	d2,d3			;align sign bit to bit #15
			NEXTVLC	d4			;skip prefix bits + sign in stream
			moveq	#15,d4			;shift down for sign bit (.w)
			asr.w	d4,d3			;0xffff if sign bit set, else 0 
			eor.w	d3,d1			;if(neg) -1-d1
			lsr.w	#8,d2			;shift down to get run and delete bitlength
			sub.w	d3,d1			;if(neg)  0-d1
			bra.b	.P_y_next_coeff_out
.P_y_next_BLK_ESC:
			;d3: next bits 16....31 next bits 0...15
			rol.l	#4,d3			;d3: next bits 20..31 0...19
			move.w	#63<<8,d2		;clear upper bits in run
		;TODO: HINT on d3.b == 128 (unsigned)
			moveq	#20,d4
			NEXTVLC	d4			;skip 6 bit prefix, 6 Bit run, 8 bit level
			and.l	d3,d2			;6 bit run, 8 bit level
			lsr.w	#8,d2			;6 bit run
			move.b	d3,d1			;lower 8 Bits: level
			beq.b	.P_y_next_coeff_pos	;positive 16-bit coeff ?
			cmp.b	#128,d1
			beq.b	.P_y_next_coeff_neg	;negative 16-bit coeff ?
			extb.l	d1
			bra.b	.P_y_next_coeff_out	;If 8-bit coeff is valid, output it !
.P_y_next_coeff_pos:
			rol.l	#8,d3
			move.b	d3,d1			;upper bits of d1 assumed still clear
			SKP8
			bra.b	.P_y_next_coeff_out
.P_y_next_coeff_neg:
			rol.l	#8,d3
			move.b	d3,d1			;upper bits of d1 assumed still clear
			neg.b	d1
			SKP8
			neg.w	d1
			;
.P_y_next_coeff_out:	;one coefficient done, update run
			ENDM


