
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
* Date:    $Date: 2019-05-01 10:05:55 -0359 (Mi, 01 Mai 2019) $             * 
* Authors: Henryk Richter (bax)                                              *
******************************************************************************
* Motion Compensation interpolation Macros, Apollo AMMX                      *
******************************************************************************

; wrapper for Apollo "touch" instruction
dotouch	 macro
		touch	\1
	 endm

; Initialization for Interpolation, used by the 68k variant to load constants
; - not used right now for AMMX 
;
; in:     -
; out: D0 - mask for lower two bits ($03030303)
;      D6 - mask for upper six bits ($FCFCFCFC)
MOT_8x1_HALFHORVER_INIT	macro
		endm

MOT_8x1_HALFHOR_INIT	macro
		endm

; purpose: 8x1 MC for hor+ver interpolation case
; in:  D0 - mask for lower two bits ($03030303) (unused)
;      D6 - mask for upper six bits ($FCFCFCFC) (unused)
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pixel pointer second line
; out:
;      A1 - unchanged
;      A2 - next position to write to  (+8)
;      A3 - unchanged
; trash:
;      E8-E13
; notes: 
;  (i+j+k+l+2)/4 = (s+t+1)/2 - (ij|kl)&st
;   with  s=(i+j+1)/2, t=(k+l+1)/2, ij = i^j, kl = k^l, st = s^t.
;
MOT_8x1_HALFHORVER	macro
	LOAD	 (A1),E8  	;LOADAB   1,0     ;dc.w  $FE51,$0001             ; i
	LOAD	1(A1),E9  	;LOADd16AB 1,1,1  ;dc.w   $FE69,$0101,$0001      ; j  flype suggested FE61 but (d16,an) is %101 with a1 %001
	PEOR	E8,E9,E12  	;PEORBBB  0,1,4   ;dc.w	$FFC0,$140A		; i^j  FFE0
	LOAD	 (A3),E10  	;LOADAB   3,2	 ;dc.w   $FE53,$0201            ; k
	LOAD	1(A3),E11  	;LOADd16AB 1,3,3  ;dc.w   $FE6B,$0301,$0001      ; l
        touch  (a3,a5.l)
	PEOR	E10,E11,E13  	;PEORBBB  2,3,5   ;dc.w	$FFC2,$350A		; k^l
	PAVGB	E10,E11,E10  	;PAVGBBB  2,3,2   ;dc.w	$FFC2,$320C		; t=(i+j+1)>>1
	PAVGB 	E8,E9,E8  	;PAVGBBB  0,1,0   ;dc.w	$FFC0,$100C		; s=(i+j+1)>>1
	POR	E12,E13,E12  	;PORBBB   4,5,4   ;dc.w	$FFC4,$5409		; (k^l)|(i^j)
	PEOR	E8,E10,E9  	;PEORBBB  0,2,1   ;dc.w	$FFC0,$210A		; s^t
	PAVGB	E8,E10,E8  	;PAVGBBB  0,2,0   ;dc.w	$FFC0,$200C		; (s+t+1)>>1
	PAND	E12,E9,E9  	;PANDBBB  4,1,1   ;dc.w	$FFC4,$1108		; (s^t)&( (k^l)|(i^j) )
	PAND.W	#$0101,E9,E9	;PANDiBB  $0101,1,1 ;dc.w $FFFC,$1108,$0101	; - keep LSB only
	PSUBUSB	E9,E8,E8  	;PSUBBBBB 1,0,0   ;dc.w	$FFC1,$0016		; (s+t+1)>>1 - (.....)
	STORE    E8,(A2)+	;STOREApB 2,1     ;dc.w	$FE9A,$1004		; 
			endm


; purpose: 8x1 MC for hor+ver interpolation case which averages with the destination contents 
; in:  D0 - mask for lower two bits ($03030303)
;      D6 - mask for upper six bits ($FCFCFCFC)
;      A1 - input pixel pointer first line
;      A2 - output pointer 
;      A3 - input pixel pointer second line
;      A5 - input stride (a3=a1+a5.l)
; out:
;      A1 - unchanged
;      A2 - next position to write to  (+8)
;      A3 - unchanged
; trash:
;      E8-E13
; arguments: 
MOT_8x1_HALFHORVER_ADD	macro
	LOAD	 (A1),E8  	;LOADAB   1,0     ;dc.w  $FE51,$0001             ; i
	LOAD	1(A1),E9  	;LOADd16AB 1,1,1  ;dc.w   $FE69,$0101,$0001      ; j  flype suggested FE61 but (d16,an) is %101 with a1 %001
	PEOR	E8,E9,E12  	;PEORBBB  0,1,4   ;dc.w	$FFC0,$140A		; i^j  FFE0
	LOAD	 (A3),E10  	;LOADAB   3,2	 ;dc.w   $FE53,$0201            ; k
	LOAD	1(A3),E11  	;LOADd16AB 1,3,3  ;dc.w   $FE6B,$0301,$0001      ; l
        touch  (a3,a5.l)
	PEOR	E10,E11,E13  	;PEORBBB  2,3,5   ;dc.w	$FFC2,$350A		; k^l
	PAVGB	E10,E11,E10  	;PAVGBBB  2,3,2   ;dc.w	$FFC2,$320C		; t=(i+j+1)>>1
	PAVGB 	E8,E9,E8  	;PAVGBBB  0,1,0   ;dc.w	$FFC0,$100C		; s=(i+j+1)>>1
	POR	E12,E13,E12  	;PORBBB   4,5,4   ;dc.w	$FFC4,$5409		; (k^l)|(i^j)
	PEOR	E8,E10,E9  	;PEORBBB  0,2,1   ;dc.w	$FFC0,$210A		; s^t
	PAND	E12,E9,E9  	;PANDBBB  4,1,1   ;dc.w	$FFC4,$1108		; (s^t)&( (k^l)|(i^j) )
	PAVGB	E8,E10,E8  	;PAVGBBB  0,2,0   ;dc.w	$FFC0,$200C		; (s+t+1)>>1
	PAND.W	#$0101,E9,E9	;PANDiBB  $0101,1,1 ;dc.w $FFFC,$1108,$0101	; - keep LSB only
	PSUBUSB	E9,E8,E8  	;PSUBBBBB 1,0,0   ;dc.w	$FFC1,$0016		; (s+t+1)>>1 - (.....)

		; until here, it's the same as MOT_8x1_HALFHORVER minus the store
	PAVGB  (A2),E8,E9	;PAVGABB  2,0,1   ;dc.w    $FED2,$010C		; 
	STORE    E9,(A2)+	;STOREApB 2,1     ;dc.w	$FE9A,$1004		; 
			endm

MOT_8x1_HALFHOR	macro
	LOAD	(A1),E8		;LOADAB   1,0       ;dc.w    $FE51,$0001             ; LOAD    (A1),E8
	PAVGB	1(A1),E8,E9	;PAVGd16ABB 1,1,0,1 ;dc.w    $FEE9,$010C,$0001       ; PAVG.B 1(A1),E8,E9
	STORE	E9,(a2)+	;STOREApB 2,1       ;dc.w    $FE9A,$1004             ; STORE    E9,(A2)+
		endm

MOT_8x1_HALFVER	macro
	LOAD    (A1),E8   	;LOADAB   1,0	;dc.w    $FE51,$0001            ; 
	PAVGB   (A3),E8,E9	;PAVGABB  3,0,1  ;dc.w    $FED3,$010C		; 
	STORE   E9,(A2)+ 	;STOREApB 2,1    ;dc.w	 $FE9A,$1004		; 
		endm


MOT_8x1_HALFHOR_ADD	macro
	LOAD    (A1),E8   	; LOADAB     1,0     ;dc.w    $FE51,$0001         ; 
	PAVGB  1(A1),E8,E9	; PAVGd16ABB 1,1,0,1 ;dc.w    $FEE9,$010C,$0001   ; 
	PAVGB   (A2),E9,E9	; PAVGABB    2,1,1   ;dc.w    $FED2,$110C	  ; 
	STORE    E9,(A2)+ 	; STOREApB 2,1    ;dc.w	 $FE9A,$1004		  ; 
			endm

MOT_8x1_HALFVER_ADD	macro
	LOAD    (A1),E8   	; LOADAB     1,0     ;dc.w    $FE51,$0001       ; 
	PAVGB   (A3),E8,E9	; PAVGABB    3,0,1   ;dc.w    $FED3,$010C	; 
	PAVGB   (A2),E9,E9	; PAVGABB    2,1,1   ;dc.w    $FED2,$110C	; 
	STORE   E9,(A2)+ 	; STOREApB   2,1     ;dc.w    $FE9A,$1004	; 
		endm

MOT_8x1_FULL_ADD	macro
	LOAD    (A1),E8		; LOADAB     1,0     ;dc.w    $FE51,$0001       ; 
	PAVGB   (A2),E8,E9	; PAVGABB    2,0,1   ;dc.w    $FED2,$010C	; 
	STORE   E9,(A2)+ 	; STOREApB   2,1     ;dc.w    $FE9A,$1004	; 
			endm


