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
* Date:    $Date: 2018-02-13 15:01:53 -0359 (Di, 13 Feb 2018) $             * 
* Authors: Henryk Richter (bax)                                              *
******************************************************************************
* Interim definition of some AMMX Mnemonics as Macros                        *
* (until VASM support is available)                                          *
*                                                                            *
* DON'T USE IN NEW PROJECTS, unless targeting ASM-One or Devpac              *
* WARNING: The Bn registers are no longer available for AMMX, although they  *
*          are still part of the naming in this file. Instead of Bn, you     *
*          should actually read "En", where B0-B7 maps to E8-E15             *
*          Sorry for any confusion (due to internal core changes)            *
*                                                                            *
* Available AMMX registers and their indices                                 *
*  D0-D7,  E0-E7  = 0000...0111,1000...1111 in register addressing           *
*  E8-E15,E16-E23 = 0000...0111,1000...1111 in register addressing           *
*  selection between D0-E7 and E8-E23 is done by the respective "B" bit in   *
*  the first word of the encoding                                            *
******************************************************************************
AMMX2	EQU	1	;if defined 1, use new encodings for padd/psub (set 0 otherwise)

; LOAD Aregnum,Bregnum = LOAD (An),Bn
LOADAB		macro
		dc.w	$FE50+\1
		dc.w	$0001+(\2*$100)
		endm

; LOAD Aregnum,Bregnum = LOAD (An)+,Bn
LOADApB		macro
		dc.w	$FE58+\1
		dc.w	$0001+(\2*$100)
		endm
		
; LOAD d16,Aregnum,Bregnum = LOAD d16(An),Bn
LOADd16AB	macro
		dc.w    $FE68+\2
		dc.w	$0001+(\3*$100)
		dc.w	\1
		endm


; STORE d16,Aregnum,Bregnum = STORE Bn,d16(An)
STOREd16AB	macro
		dc.w    $FEA8+\2
		dc.w	$0004+(\3*$1000)
		dc.w	\1
		endm

; STORE d8,Aregnum,Dregnum,Bregnum = STORE Bn,d8(An,Dn)
STOREd8ADB	macro
		dc.w    $FEB0+\2
		dc.w	$0004+(\4*$1000)
		dc.b	(\3*$10)
		dc.b	\1
		endm

; STORE Aregnum,Bregnum = STORE Bn,(An)+
STOREApB	macro
		dc.w	$FE98+\1
		dc.w	$0004+(\2*$1000)
		endm

; BFLYWBBD Da,Db,Bc:Bd (numbers, Bc even) = BFLYW Da,Db,Bc:Bd
; Bc = Da+Db, Bd = Db-Da
BFLYWDDB	macro
		dc.w	$fe40+\1
		dc.w	$001d+(\2*$1000)+(\3*$100)
		endm

; BFLYWBBD Ba,Bb,Dc:Dd (numbers, Dc even) = BFLYW Ba,Bb,Dc:Dd
; Dc = Ba+Bb, Dd = Bb-Ba
BFLYWBBD	macro
		dc.w	$ff80+\1
		dc.w	$001d+(\2*$1000)+(\3*$100)
		endm


; BFLYWDDD Da,Db,Dc:Dd (numbers, Dc even) = BFLYW Da,Db,Dc:Dd
; Dc = Da+Db, Dd = Db-Da
BFLYWDDD	macro
		dc.w	$fe00+\1
		dc.w	$001d+(\2*$1000)+(\3*$100)
		endm

; BFLYWBBB Ba,Bb,Bc:Bd (numbers, Bc even) = BFLYW Ba,Bb,Bc:Bd
; Bc = Ba+Bb, Bd = Bb-Ba
BFLYWBBB	macro
		dc.w	$ffc0+\1
		dc.w	$001d+(\2*$1000)+(\3*$100)
		endm

; Transpose High (first 2 words in input range)
; Inputs (consecutive D0-D4 or D5-D7, example for D0-D3)
;  D0 A B C D
;  D1 E F G H
;  D2 I J K L
;  D3 M N O P
; Outputs (ex. for B0:B1):
;  B0 A E I M
;  B1 B F J N
; TRANSHiDB n,m = TRANSHi Dn-Dn+3,Bm:Bm+1 (like TRANSHi D0-D3,B0:B1)
TRANSHiDB	macro
		dc.w	$fe40+\1	;\1 = 0 or 4 for D0-D3 or D4-D7
		dc.w	$0002+(\2*$100) ;\2 = 0,2,4,6,8,... for B0:B1,B2:B3,...
		endm

TRANSHiBD	macro
		dc.w	$ff00+\1	;\1 = 0,4,8,12 for B0-3,B4-B7,B8-11,B12-B15
		dc.w	$0002+(\2*$100) ;\2 = 0,2,4,6 for D0:D1,D2:D3,...,B16:B17,B18:B19
		endm

TRANSHiBB	macro
		dc.w	$ff40+\1	;\1 = 0 or 4 for B0-B3 or B4-B7
		dc.w	$0002+(\2*$100) ;\2 = 0,2,4,6,8,... for B0:B1,B2:B3,...
		endm


; Transpose Low (2nd 2 words in input range, 16 bit words)
; Inputs (consecutive D0-D4 or D5-D7, example for D0-D3)
;  D0 A B C D
;  D1 E F G H
;  D2 I J K L
;  D3 M N O P
; Outputs (ex. for B0:B1):
;  B0 C G K O 
;  B1 D H L P
; TRANSLoDB n,m = TRANSLo Dn-Dn+3,Bm:Bm+1 (like TRANSLo D0-D3,B0:B1)
TRANSLoDB	macro
		dc.w	$fe40+\1	;\1 = 0 or 4 for D0-D3 or D4-D7
		dc.w	$0003+(\2*$100) ;\2 = 0,2,4,6,8,... for B0:B1,B2:B3,...
		endm

TRANSLoBD	macro
		dc.w	$ff00+\1	;\1 = 0,4,8,12 for B0-3,B4-B7,B8-11,B12-B15
		dc.w	$0003+(\2*$100) ;\2 = 0,2,4,6 for D0:D1,D2:D3,...,B16:B17,B18:B19
		endm

TRANSLoBB	macro
		dc.w	$ff40+\1	;\1 = 0 or 4 for D0-D3 or D4-D7
		dc.w	$0003+(\2*$100) ;\2 = 0,2,4,6,8,... for B0:B1,B2:B3,...
		endm



; PANDBBB Bm,Bn,Bo = PAND Bm,Bn,Bo
PANDBBB		macro
		dc.w    $FFC0+\1
		dc.w	$0008+(\2*$1000)+(\3*$100)
		endm

; PANDiBB imm16,Bn,Bo = PAND #imm16,Bn,Bo - splat.w imm16
PANDiBB		macro
		dc.w	$FFFC
		dc.w	$0008+(\2*$1000)+(\3*$100) ; 
		dc.w	\1
		endm


; PORBBB Bm,Bn,Bo = POR Bm,Bn,Bo
PORBBB		macro
		dc.w    $FFC0+\1
		dc.w	$0009+(\2*$1000)+(\3*$100)
		endm

; PORDBB Dm,Dn,Bo = POR Dm,Dn,Bo
PORDDB		macro
		dc.w    $FE40+\1
		dc.w	$0009+(\2*$1000)+(\3*$100)
		endm

; PORBBD Bm,Bn,Do = POR Bm,Bn,Do
PORBBD		macro
		dc.w    $FF80+\1
		dc.w	$0009+(\2*$1000)+(\3*$100)
		endm

; PEORBBB Bm,Bn,Bo = PEOR Bm,Bn,Bo
PEORBBB		macro
		dc.w    $FFC0+\1
		dc.w	$000A+(\2*$1000)+(\3*$100)
		endm

; PAVGBBB Bm,Bn,Bo = PAVG Bm,Bn,Bo
PAVGBBB		macro
		dc.w    $FFC0+\1
		dc.w	$000C+(\2*$1000)+(\3*$100)
		endm

; PAVGABB (An),Bm,Bo = PAVG.B (An),Bm,Bo
PAVGABB		macro
		dc.w    $FED0+\1
		dc.w	$000C+(\2*$1000)+(\3*$100)
		endm

; PAVGd16ABB d16(An),Bm,Bo = PAVG.B d16(An),Bm,Bo
PAVGd16ABB	macro
		dc.w    $FEE8+\2
		dc.w	$000C+(\3*$1000)+(\4*$100)
		dc.w	\1
		endm

	ifne	AMMX2
; PSUBWBBB Bm,Bn,Bo = PSUB.W Bm,Bn,Bo
PSUBWBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0013+(\2*$1000)+(\3*$100)
		endm

; PSUBBBBB Bm,Bn,Bo = PSUB.B Bm,Bn,Bo
PSUBBBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0012+(\2*$1000)+(\3*$100)
		endm

; PADDWBBB Bm,Bn,Bo = PADD.W Bm,Bn,Bo
PADDWBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0011+(\2*$1000)+(\3*$100)
		endm

; PADDWDBB Dm,Bn,Bo = PADD.W Dm,Bn,Bo
PADDWDBB	macro
		dc.w	$FEC0+\1
		dc.w	$0011+(\2*$1000)+(\3*$100)
		endm

; PADDWiBB imm16,Bn,Bo = PADD.W #imm16,Bn,Bo
PADDWiBB	macro
		dc.w    $FFFC                      ;
		dc.w	$0011+(\2*$1000)+(\3*$100) ; 
		dc.w    \1                         ;
		endm

	else
; PSUBWBBB Bm,Bn,Bo = PSUB.W Bm,Bn,Bo
PSUBWBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0017+(\2*$1000)+(\3*$100)
		endm

; PSUBBBBB Bm,Bn,Bo = PSUB.B Bm,Bn,Bo
PSUBBBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0016+(\2*$1000)+(\3*$100)
		endm

; PADDWBBB Bm,Bn,Bo = PADD.W Bm,Bn,Bo
PADDWBBB	macro
		dc.w	$FFC0+\1
		dc.w	$0015+(\2*$1000)+(\3*$100)
		endm

; PADDWDBB Dm,Bn,Bo = PADD.W Dm,Bn,Bo
PADDWDBB	macro
		dc.w	$FEC0+\1
		dc.w	$0015+(\2*$1000)+(\3*$100)
		endm

; PADDWiBB imm16,Bn,Bo = PADD.W #imm16,Bn,Bo
PADDWiBB	macro
		dc.w    $FFFC                      ;
		dc.w	$0015+(\2*$1000)+(\3*$100) ; 
		dc.w    \1                         ;
		endm
	endc

; PMUL88iBB imm16,Bn,Bo = PMUL88.W #imm16,Bn,Bo
PMUL88iBB	macro
		dc.w    $FFFC                      ;
		dc.w	$0018+(\2*$1000)+(\3*$100) ; 
		dc.w    \1                         ;
		endm

; PMUL88iBD imm16,Bn,Do = PMUL88.W #imm16,Bn,Do
PMUL88iBD	macro
		dc.w    $FFBC                      ;
		dc.w	$0018+(\2*$1000)+(\3*$100) ; 
		dc.w    \1                         ;
		endm

; PACKUSWBBBA Bm,Bn,(Ao) = PACKUSWB Bm,Bn,(Ao)
PACKUSWBBBA	macro
		dc.w    $FED0+\3
		dc.w	$0006+(\1*$1000)+(\2*$100)
		endm

; PACKUSWBBBD Bm,Bn,Do = PACKUSWB Bm,Bn,Do
PACKUSWBBBD	macro
		dc.w    $FEC0+\3
		dc.w	$0006+(\1*$1000)+(\2*$100)
		endm

; PACK3216DDD Dm,Dn,Do = PACK3216 Dm,Dn,Do
PACK3216DDD	macro
		dc.w    $FE00+\3
		dc.w	$0007+(\1*$1000)+(\2*$100)
		endm

; PACK3216DDA Dm,Dn,(Ao) = PACK3216 Dm,Dn,(Ao)
PACK3216DDA 	macro
		dc.w    $FE10+\3
		dc.w	$0007+(\1*$1000)+(\2*$100)
		endm

; PACK3216DDAp Dm,Dn,(Ao)+ = PACK3216 Dm,Dn,(Ao)+
PACK3216DDAp 	macro
		dc.w    $FE18+\3
		dc.w	$0007+(\1*$1000)+(\2*$100)
		endm

; VPERMiBBB imm32,Bm,Bn,Bo = VPERM #imm32,Bm,Bn,Bo
VPERMiBBB	macro
		dc.w	$FFFF
		dc.w	$0000+(\3*$1000)+(\4*$100)+\2
		dc.l	\1
		endm

; VPERMiBDB imm32,Bm,Dn,Bo = VPERM #imm32,Bm,Dn,Bo
VPERMiBDB	macro
		dc.w	$FF7F
		dc.w	$0000+(\3*$1000)+(\4*$100)+\2
		dc.l	\1
		endm


