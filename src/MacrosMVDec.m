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
* Motion Vector Difference (MVD) decoding                                    *
******************************************************************************
;
;inputs:
; arg1 (\1) - input data register with 11 Bits from stream
; arg2 (\2) - output data register (assumed unclean, \2 != \1 )
; D4        - f_code
; a1        - MV decoding table
; A0/D0     - bitstream
;Outputs:
; \2        - decoded MVD
;Trash:
;           - d5,d6,d7 (d7 is OK as input)
;
DECODE_MV		macro
			move.w	#0,\2
			QSKP	1
			cmp.w	#%10000000000,\1		;check for zero vec before table access
			bhs.s	.p_no_vert_r
			; alternative to QSKP above and subq.b #1,d7 below
			;blt.s	.p_have_vec_v
			;QSKP	1				;skip zero bit (=1)
			;bra.s	.p_no_vert_r
;.p_have_vec_v
			move.w	(a1,\1.w*2),\2
			moveq	#0,d7
			move.b	\2,d7
			lsr.w	#8,\2				;get symbol (8 bit signed)
			subq.b	#1,d7
			extb.l	\2				;\2 = vertical_forward_code
			NEXTVLC d7
			tst.w	d4				;if !fcode, don't load more bits
			beq.b	.p_no_vert_r			;
			;
			;tradeoff: if we store symbol as val<<1|sign in Huff table, we lose speed with
			;          short f_codes (ext.b dn vs. explicit sign change) but would gain some 
			;          cycles with longer f_codes
			;          on the other hand, the longer f_codes require sign extraction anyway
			;          before working on the values
			;
			;
			move.l	\2,d6	
			NREGBITS d4,d5,d7			;d5 = motion_vertical_forward_r (interleaved)
			moveq	#31,d7
			asr.l	d7,d6				;sign bit of \2 in all slots of d6
			moveq	#1,d7				;
			or.l	d6,d7				;+1 for positive \2, -1 for negative \2
			eor.l	d6,d5				;-1-d5 if(\2 < 0 )
			sub.l	d7,\2				;positive \2 -1, negative \2 +1
			sub.l	d6,d5				;0-d5 if(\2 < 0 )
			lsl.l	d4,\2				;\2<<f_code
			add.l	d7,\2				;+1 for positive \2, -1 for negative \2
			add.l	d5,\2				;motion LSB
.p_no_vert_r
			endm
