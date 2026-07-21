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
* Authors: Stephen Fellner, Henryk Richter (bax)                             *
******************************************************************************
* Macros for Bitstream access and parsing                                    *
* The pointer to the bitstream is assumed to be A0 and the bit offset D0     *
* Depending on the called Macro, either of A0,D0 might get incremented       *
******************************************************************************

; Macro from reading \1 bits from input stream into data register \2.
; guaranteed to set flags in \2 correctly
CHKBITS		MACRO
		bfextu	(a0){d0:\1},\2
		ENDM


; \1 Number of bits to skip (1...8)
QSKP		MACRO
		addq.l	#\1,d0	;
		ENDM

CHK8		MACRO
		bfextu	(a0){d0:8},\1
		ENDM

SKP8		MACRO
		addq.l	#1,a0
		ENDM

; Get 8 Bits into Data register \1
GET8		MACRO
		bfextu	(a0){d0:8},\1
		addq.l	#1,a0
		ENDM

; Get 16 Bits into Data register \1
GET16		MACRO
		bfextu	(a0){d0:16},\1
		addq.l	#2,a0
		ENDM

CHK16		MACRO
		bfextu	(a0){d0:16},\1
		ENDM

SKP16		MACRO
		addq.l	#2,a0
		ENDM

;Macro for parsing 32 bits from any bitposition in input stream into \1.
GET32		MACRO
		 IFNE	ASMONE
		  bfextu (a0){d0:0},\1
		 ELSE
		  bfextu (a0){d0:32},\1
		 ENDC
		 addq.l	#4,a0
		ENDM

;Macro for reading 32 bits from any bitposition in input stream into \1.
CHK32		MACRO
		 IFNE	ASMONE
		  bfextu (a0){d0:0},\1
		 ELSE
		  bfextu (a0){d0:32},\1
		 ENDC
		ENDM

;Macro for skipping 32 bits from any bitposition in input stream.
SKP32		MACRO
		addq.l	#4,a0
		ENDM

; advance to next byte
; this also synchronizes A0 to current byte position from D0
BYTEALIGN	MACRO
		addq.l	#7,d0 ; +7 bits
		lsr.l	#3,d0 ; /8 = next byte position
		add.l	d0,a0 ; add byte ptr
		moveq	#0,d0 ; 
		ENDM

	ifne	NEW_BITSTREAM

NGETBITS	MACRO
		bfextu	(a0){d0:\1},\2			;read bits
			IFLE	\1-8
				addq.l	#\1,d0
			ELSE
				moveq	#\1,\3
				add.l	\3,d0
			ENDC
		ENDM

; read \1 bits into data register \2 and advance pointer (GETBTS)
NREGBITS	MACRO
		bfextu	(a0){d0:\1},\2
		add.l	\1,d0
		ENDM


NSKPBITS	MACRO
			IFLE	\1-8
				addq.l	#\1,d0
			ELSE
				moveq	#\1,\2
				add.l	\2,d0
			ENDC
		ENDM



NGETDATA	MACRO
		bfextu	(a0){d0:\1},d1
		IFLE	\1-8
			addq.l	#\1,d0
		ELSE
			add.l	#\1,d0
		ENDC
			move.l	d1,\2
		ENDM


; align to byte, read next start code, check buffer
NEXT_START_CODE:	MACRO
			addq.l	#7,d0 ; +7 bits
			lsr.l	#3,d0 ; /8 = next byte position
			add.l	d0,a0 ; add byte ptr
			moveq	#0,d0
.nsc_loop:
			CHECK_BUFFER
			move.l	(a0),d1
			clr.b	d1
			addq.l	#1,a0
			cmp.l	#$00000100,d1
			bne.b	.nsc_loop
.nsc_end		subq.l	#1,a0
			ENDM


; align to byte, read next start code and check systems buffer
; same as NEXT_START_CODE, except for the CHECK_SYSBUFFER call
NEXT_START_CODE_SYSTEM	MACRO
			addq.l	#7,d0 ; +7 bits
			lsr.l	#3,d0 ; /8 = next byte position
			add.l	d0,a0 ; add byte ptr
			moveq	#0,d0
.nsc_loop:
			CHECK_SYSBUFFER
			move.l	(a0),d1
			clr.b	d1
			addq.l	#1,a0
			cmp.l	#$00000100,d1
			bne.b	.nsc_loop
.nsc_end		subq.l	#1,a0
			ENDM

; FIXME: no longer use this one, needs to assume byte width \1 register
; \1 Number of bits to skip, \2 scratch
NNEXTVLC	MACRO
		moveq	#0,\2	;F
		move.b	\1,\2	;F
		add.l	\2,d0	;
		ENDM

; \1 Number of bits to skip (.l, either #bla or <EA>)
NEXTVLC		MACRO
		add.l	\1,d0	;
		ENDM

;
;Macro for skipping \1 bits in input stream.
SKPBITS		MACRO
			IFLE	\1-8
				addq.l	#\1,d0
			ELSE
				add.l	#\1,d0
			ENDC
		ENDM



	else

NGETBITS	MACRO
		bfextu	(a0){d0:\1},\2			;read bits
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		move.l	d0,\3
		and.b	#7,d0
		lsr.l	#3,\3
		add.l	\3,a0
		ENDM


NREGBITS	MACRO
		bfextu	(a0){d0:\1},\2
		add.b	\1,d0
		move.l	d0,\3
		and.b	#7,d0
		lsr.l	#3,\3
		add.l	\3,a0
		ENDM


NSKPBITS	MACRO
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		move.l	d0,\2
		and.b	#7,d0
		lsr.l	#3,\2
		add.l	\2,a0
		ENDM


NGETDATA	MACRO
		bfextu	(a0){d0:\1},d1
		move.l	d1,\2
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		move.l	d0,\3
		lsr.l	#3,\3
		add.l	\3,a0
		and.b	#7,d0
		ENDM

NEXT_START_CODE		MACRO
			move.l	d0,d1
			lsr.b	#3,d0
			and.b	#7,d1
			beq.b	*+4
			addq.b	#1,d0
			add.l	d0,a0
			moveq	#0,d0
.nsc_loop		CHECK_BUFFER
			move.l	(a0),d1
			clr.b	d1
			cmp.l	#$00000100,d1
			beq.b	.nsc_end
			addq.l	#1,a0
			bra.b	.nsc_loop
.nsc_end
			ENDM

NEXT_START_CODE_SYSTEM	MACRO
			move.l	d0,d1
			lsr.b	#3,d0
			and.b	#7,d1
			beq.b	*+4
			addq.b	#1,d0
			add.l	d0,a0
			moveq	#0,d0
.nsc_loop		CHECK_SYSBUFFER
			move.l	(a0),d1
			clr.b	d1
			cmp.l	#$00000100,d1
			beq.b	.nsc_end
			addq.l	#1,a0
			bra.b	.nsc_loop
.nsc_end
			ENDM


NNEXTVLC	MACRO
		add.b	\1,d0
		move.l	d0,\2	;
		lsr.l	#3,\2	;
		and.b	#7,d0
		add.l	\2,a0
		ENDM

; 
NEXTVLC		MACRO
		add.b	\1,d0
		ror.l	#3,d0
		add.w	d0,a0
		rol.l	#3,d0
		and.b	#7,d0
		ENDM

;Macro for skipping \1 bits in input stream.
SKPBITS		MACRO
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		ror.l	#3,d0
		add.w	d0,a0
		rol.l	#3,d0
		and.b	#7,d0
		ENDM

	endc


;
; unused macros, moved down, out of scope
;
	ifne	0

;Macro for parsing byte-aligned longword (32-bit) data from input stream.
GETLONG		MACRO
		move.l	(a0)+,\1
		ENDM


;Macro for reading byte-aligned longword (32-bit) data from input stream.
CHKLONG		MACRO
		move.l	(a0),\1
		ENDM


SKIPREG		MACRO
		add.b	\1,d0
		ror.l	#3,d0
		add.w	d0,a0
		rol.l	#3,d0
		and.b	#7,d0
		ENDM


;Macro for reading data of any bitlength direct into its memory address
;(Result is returned in both \1 and d1.)
GETDATA		MACRO
		bfextu	(a0){d0:\1},d1
		move.l	d1,\2
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		ror.l	#3,d0
		add.w	d0,a0
		rol.l	#3,d0
		and.b	#7,d0
		ENDM

;Macro for parsing \1 bits from input stream into \2.
GETBITS		MACRO
		bfextu	(a0){d0:\1},\2			;read bits
		IFLE	\1-8
		addq.l	#\1,d0
		ELSE
		add.b	#\1,d0
		ENDC
		ror.l	#3,d0				;lower 16 bits are divided by 8
		add.w	d0,a0				;number of bytes to skip
		rol.l	#3,d0				;restore d0 remainder
		and.b	#7,d0				;remove bytes added to a0
		ENDM


;Macro for parsing \1 bits from input stream into \2, where \2 is a data register.
REGBITS		MACRO
		bfextu	(a0){d0:\1},\2
		add.b	\1,d0
		ror.l	#3,d0
		add.w	d0,a0
		rol.l	#3,d0
		and.b	#7,d0
		ENDM

	endc


