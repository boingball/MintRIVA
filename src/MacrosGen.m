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
* Authors: Stephen Fellner                                                   *
******************************************************************************
* Misc Macros                                                                *
******************************************************************************


; Align a given Pointer
;  Input: D0-D6, A0-A6 or B0-B7 - pointer to align 
; Output: aligned pointer (+0...+(FRAMEBUFFER_ALIGN-1) )
; Trash:  none
;  Note:  don't use D7 as Input
ALIGN_PTR	macro
		move.l	d7,-(sp)
		moveq	#FRAMEBUFFER_ALIGN-1,d7
		add.l	\1,d7
		and.l	#$ffffffff-(FRAMEBUFFER_ALIGN-1),d7
		move.l	d7,\1
		move.l	(sp)+,d7
		endm
; Align D0 (same as above, just simpler and shorter)
ALIGN_D0	macro
		add.l	#FRAMEBUFFER_ALIGN-1,d0
		and.l	#$ffffffff-(FRAMEBUFFER_ALIGN-1),d0
		endm



;Macro for printing strings...
OUTTXT		MACRO
		move.l	d2,-(a7)
		move.l	#\1,d2
		bsr	OutputText
		move.l	(a7)+,d2
		ENDM

;Macro to print 32-bit unsigned decimal...
OUTDEC		MACRO
		move.l	d1,-(a7)
		move.l	\1,d1
		bsr	OutputDecimal
		move.l	(a7)+,d1
		ENDM

OUTDEC8		MACRO
		move.l	d1,-(a7)
		move.l	\1,d1
		and.l	#$000000ff,d1
		bsr	OutputDecimal
		move.l	(a7)+,d1
		ENDM

;Macro to print signed 32-bit decimal numbers...
OUTDECS		MACRO
		move.l	d1,-(a7)
		move.l	\1,d1
		bsr	OutputSignedDecimal
		move.l	(a7)+,d1
		ENDM

OUTDECS_W	MACRO
		move.l	d1,-(a7)
		move.l	\1,d1
		ext.l	d1
		bsr	OutputSignedDecimal
		move.l	(a7)+,d1
		ENDM

;Picike macro 16bit SIGNED! decimális szám kiírásra...
OUTDECS16	MACRO
		move.l	d1,-(a7)
		move.w	\1,d1
		and.l	#$0000ffff,d1
		bsr	OutputSignedDecimal16
		move.l	(a7)+,d1
		ENDM

;Macro to print a 32-bit integer in hex...
OUTHEX	MACRO
	move.l	d1,-(a7)
	move.l	\1,d1
	jsr	OutputLongHex
	move.l	(a7)+,d1
	ENDM


;Macro to print a 32-bit integer in binary...
OUTBIN	MACRO
	move.l	d1,-(a7)
	move.l	\1,d1
	jsr	OutputLongBinary
	move.l	(a7)+,d1
	ENDM

;Marco to print a 32-bit fraction in binary...
OUTFRAC	MACRO
	movem.l	d1/d2,-(a7)
	move.l	\1,d1
	move.l	#\2,d2
	jsr	OutputLongFraction
	movem.l	(a7)+,d1/d2
	ENDM

;Macro for displaying 16:16 int:frac numbers.
OUTNUM	MACRO
	movem.l	d0/d1,-(a7)
	move.l	\1,d0
	move.l	d0,d1
	clr.w	d0
	swap	d0
	swap	d1
	clr.w	d1
	OUTDEC	d0
	OUTFRAC	d1,4
	movem.l	(a7)+,d0/d1
	ENDM

OUTNUMN	MACRO
	movem.l	d0/d1,-(a7)
	move.l	\1,d0
	move.l	d0,d1
	clr.w	d0
	swap	d0
	swap	d1
	clr.w	d1
	OUTDEC	d0
	OUTFRAC	d1,\2
	movem.l	(a7)+,d0/d1
	ENDM

OUTNUM64	MACRO
		movem.l	d0/d1,-(a7)
		OUTDEC	\1
		OUTFRAC	\2,4
		movem.l	(a7)+,d0/d1
		ENDM

GETECLOCK	MACRO
		movem.l	a0/d0,-(a7)
		move.l	timerbase,a6
		lea	TimerStruct,a0
		jsr	_LVOReadEClock(a6)
		move.l	4(a0),\1
		movem.l	(a7)+,a0/d0
		ENDM

GETECLOCK64	MACRO
		movem.l	a0/d0,-(a7)
		lea	TimerStruct,a0
		move.l	timerbase,a6
		jsr	_LVOReadEClock(a6)
		movem.l	(a0)+,\1/\2
		movem.l	(a7)+,a0/d0
		ENDM
	

TIMERSTART	MACRO
		movem.l	d0/a0,-(a7)
		move.l	timerbase,a6
		lea	TimerStruct,a0
		jsr	_LVOReadEClock(a6)
		move.l	4(a0),e_clock_start
		movem.l	(a7)+,d0/a0
		ENDM

TIMERSTOP	MACRO
		movem.l	d0/a0,-(a7)
		move.l	timerbase,a6
		lea	TimerStruct,a0
		jsr	_LVOReadEClock(a6)
		move.l	d0,e_count_rate
		move.l	4(a0),d1
		sub.l	e_clock_correction,d1
		move.l	d1,e_clock_stop
		sub.l	e_clock_start,d1
		move.l	d1,e_clock_time
		movem.l	(a7)+,d0/a0
		ENDM
