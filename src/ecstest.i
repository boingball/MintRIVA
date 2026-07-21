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
* ECS audio capability test                                                  *
* author: Henryk Richter <henryk.richter@gmx.net>                            *
* $Date: 2016-10-28 22:59:19 +0200 (Fr, 28. Okt 2016) $                      * 
*                                                                            *
* Notes:                                                                     *
*  - the following assumptions are carried out                               *
*     ECS Denise register must be present - else OCS, no 44 kHz              *
*     Picasso96 must have the 31 kHz ENV variable                            *
*     it is assumed that P96 actually runs                                   *
*  - Enable Picasso96 31 kHz from CLI:                                       *
*     SetEnv Picasso96/AmigaVideo 31kHz                                      *
*     SetEnv EnvArc:Picasso96/AmigaVideo 31kHz                               *
******************************************************************************

	; assumes to be included after exec, else build standalone
	ifnd _LVOOpenLibrary

; build standalone test (1) or use as include (0)
ECSTESTTEST	EQU	1
	else
ECSTESTTEST	EQU	0
	endc

	ifne	ECSTESTTEST

        include lvo/exec_lib.i
        include lvo/dos_lib.i
	include dos/dos.i

CALLDOS2	MACRO
		move.l	dosbase,a6
		jsr	_LVO\1(a6)
		ENDM

	;standalone ECS test, checks and writes string to console
ECSTEST_STANDALONE:
		move.l	4.w,a6
		lea	dosname(pc),a1
		moveq	#0,d0
		jsr	_LVOOpenLibrary(a6)
		move.l	d0,dosbase

		CALLDOS2	Output
		move.l	d0,stdout

		bsr	ECSAudioTest

		lea	.ecson(pc),a0
		tst.l	d0
		bgt.s	.print
		lea	.ecsoff(pc),a0
		beq.s	.print
		lea	.ecsunavail(pc),a0
.print:
		move.l	stdout(pc),d1
		move.l	a0,d2
		moveq	#-1,d3
.len
		addq.l	#1,d3
		tst.b	(a0)+
		bne.s	.len
		CALLDOS2	Write

		move.l	dosbase(pc),a1
		move.l	4.w,a6
		jsr	_LVOCloseLibrary(a6)

		rts

.ecson:		dc.b	'ECS audio available',10,0
.ecsoff:	dc.b	'ECS audio probably inactive',10,0
.ecsunavail:	dc.b	'no ECS/AGA chipset detected',10,0
dosname:	dc.b	'dos.library',0
		cnop	0,2
stdout:		dc.l	0
dosbase:	dc.l	0
	endc	;TEST


; inputs: -
; outputs: D0 <= 0 - no safe usage for 44 kHz audio (<0 = not detected, 0= not P96 on)
;          D0 >  0 - ECS audio probably available
; notes:   uses CALLDOS2 macro
ECSAudioTest:
	movem.l	d1-a6,-(sp)

	; read the register: OCS denise doesn't have it and
	; returns garbage, hence make sure we have some garbage
	; in case of OCS
	moveq	#32,d1
	moveq	#-1,d0		;def: no ECS detected
	moveq	#0,d3		;
.detloop:
	move.w	$DFF006,d2	;get something over the bus
	divu.w	#1,d2		;waste some time
	or.w	$DFF07C,d3
	dbf	d1,.detloop

	;
	; actual Denise/Lisa check
	;
	cmp.b	#$fc,d3		;Denise 8373 ?
	beq.s	.ecsdenise
	cmp.b	#$f8,d3		;Lisa ?
	bne.s	.noecs
.ecsdenise:

	;
	; Now verify whether P96 environment variable is set
	;
	lea		.p96path(pc),a1
	move.l		a1,d1
	move.l		#MODE_OLDFILE,d2
	CALLDOS2 	Open
	move.l		d0,d1
	beq.s		.noecson	;d0 = 0 - no P96 variable set, no safe use of ECS audio
	move.l		d0,a2

	lea		.p96head(pc),a3
	move.l		a3,d2
	moveq		#2,d3		;just the 15/31
	CALLDOS2	Read

	move.l		a2,d1
	CALLDOS2        Close

	; look into the environment variable
	moveq		#0,d0		;d0 = 0 - no P96 variable set, no safe use of ECS audio
	cmp.b		#'3',(a3)
	bne.s		.noecson
	cmp.b		#'1',1(a3)
	bne.s		.noecson

	moveq		#1,d0		;ECS probably available
.noecson:

.noecs:
	movem.l	(sp)+,d1-a6
	rts
.p96path:	dc.b	"Env:Picasso96/AmigaVideo",0
.p96head:	dc.b	0,0,0,0
		cnop	0,2

