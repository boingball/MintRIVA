********************************************************************************
* Copyright notice: This file is available with dual licensing. Depending on the
* type of project for subsequent re-use of this source code, it is up to the 
* discretion of the interested party to choose between the GNU general public
* license or the MIT license. Both licensing headers are included here.
********************************************************************************
*
* MIT License
*
* Copyright (c) 2004 Stephen Fellner
*
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal 
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE.
*
*******************************************************************************
** GPL
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
* Authors: Stephen Fellner                                                   *
******************************************************************************
* AGA C2P Code and Accupak renderer                                          *
******************************************************************************

	ifne	0
; directly after fixing DHAM8
;7.Temp:src/vampire/soft/RIVA/0.53> a.out ram:bla.mpg display=dham8 fps=1000 noskip noaudio verbose
; Generic m68k build of RiVA running.
;
; Video: 320x176, 24.000 fps
; Audio: <NONE>
;
; Number of frames played:  2143
; Number of frames skipped: 0
; Total number of frames:   2143
;
; Total playback time: 144.9995 seconds.
; Average framerate:   14.7793 fps
; Displayed framerate: 14.7793 fps
;

	endc


		; ==============================================================


*		ALIGN	0,16
		CNOP    0,16
mpr_accupak:
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	BitmapModulo,d7		;dest modulo
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		move.l	GfxMemBase,a1		;dest

		move.l	y_bitmap_width(pc),d1		;source width (y plane)
		move.l	d1,d6
		and.l	#3,d6
		add.l	d1,d6
		move.l	d6,a5			;source_y_skip

		move.l	a5,d6
		and.l	#3,d6			
		add.l	d7,d6
		move.l	d6,a6			;bitmap_add

		move.l	height(pc),d6
		asr.l	#1,d6			;/2 (yuv411 miatt)
		move.l	d6,d4

mpr_accupak_loopy

		move.l	d1,d6			;source width
		asr.l	#2,d6			;/4

mpr_accupak_loopx

		moveq	#0,d5		;u/v
		move.b	(a3),d5		;-------- -------- -------- uuuuuuuu
		lsl.l	#6,d5		;-------- -------- --uuuuuu uu------
		move.b	(a4),d5		;-------- -------- --uuuuuu vvvvvvvv

		move.l	(a2),d2		;33333... 22222... 11111... 00000...

		lsr.l	#2,d5		;-------- -------- ----uuuu uuvvvvvv

		move.l	(a2,d1.w),d3	;33333... 22222... 11111... 00000...

		lsr.b	#3,d2		;33333... 22222... 11111... ---00000
		rol.w	#5,d2		;33333... 22222... ...---00 00011111
		swap	d2		;...---00 00011111 33333... 22222...
		rol.w	#5,d2		;...---00 00011111 ...22222 ...33333
		lsl.b	#3,d2		;...---00 00011111 ...22222 33333---
		lsl.w	#3,d2		;...---00 00011111 22222333 33------
		lsl.l	#6,d2		;00000111 11222223 3333---- --------
		or.w	d5,d2		;00000111 11222223 3333uuuu uuvvvvvv

		move.l	d2,(a1)

		lsr.b	#3,d3		;33333... 22222... 11111... ---00000
		rol.w	#5,d3		;33333... 22222... ...---00 00011111
		swap	d3		;...---00 00011111 33333... 22222...
		rol.w	#5,d3		;...---00 00011111 ...22222 ...33333
		lsl.b	#3,d3		;...---00 00011111 ...22222 33333---
		lsl.w	#3,d3		;...---00 00011111 22222333 33------
		lsl.l	#6,d3		;00000111 11222223 3333---- --------
		or.w	d5,d3
		move.l	d3,(a1,d7.w)

		addq.l	#4,a2		;source y next
		addq.l	#2,a3
		addq.l	#2,a4

		addq.l	#4,a1

		subq.l	#1,d6
		bne.b	mpr_accupak_loopx

		add.l	a5,a2
		add.l	a6,a1

		subq.l	#1,d4
		bne.b	mpr_accupak_loopy

		rts


	; C2P stuff disabled in pure Apollo builds
AGAPlaneSize:			dc.l	0
C2P_x_loop:				dc.l	0
C2P_y_loop:				dc.l	0
C2P_EOL_skip:			dc.l	0
C2P_y_datacorrect:		dc.l	0
C2P_c_datacorrect:		dc.l	0
c2pMask:				dc.l	0
GrayC2PMask:			dc.l	0
DHAM8C2PMask:			dc.l	0

*-------------------------------------------------------------------*
*------ Convert Chunky Bitmap to Planar and display in chipmem -----*
*-------------------------------------------------------------------*

*		ALIGN	0,16
		CNOP    0,16

mpr_Planar8
GrayC2P		movem.l	d0-a6,-(a7)

		move.l	GfxMemBase,a1
		move.l	y_bitmap_base,a2
		lea	GrayC2PMask(pc),a6

		move.l	ScreenHeight(pc),d1
		subq.l	#1,d1
c2p_loopy	move.l	C2P_x_loop(pc),d0
		subq.l	#1,d0
		move.l	#$ffffffff,(a6)
c2p_loopx	move.l	d1,a5
		move.l	d0,a4
		tst.l	d0
		bne.b	.nomask
		move.l	c2pMask(pc),(a6)
.nomask
;Inputs:   a1 - Planar Bitmap Address (Output)
;          a2 - chunky bitmap (Input)

;C2P Start...
		move.l	(a2),d0
		move.l	8(a2),d2
		move.w	16(a2),d0
		move.w	24(a2),d2
		move.l	d0,d7
		lsl.l	#8,d7
		eor.l	d2,d7
		and.l	#$ff00ff00,d7
		eor.l	d7,d2
		lsr.l	#8,d7
		eor.l	d7,d0
		move.l	4(a2),d1
		move.l	12(a2),d3
		move.w	20(a2),d1
		move.w	28(a2),d3
		move.l	d1,d7
		lsl.l	#8,d7
		eor.l	d3,d7
		and.l	#$ff00ff00,d7
		eor.l	d7,d3
		lsr.l	#8,d7
		eor.l	d7,d1
		move.l	d0,d7
		lsl.l	#4,d7
		eor.l	d1,d7
		and.l	#$f0f0f0f0,d7
		eor.l	d7,d1
		lsr.l	#4,d7
		eor.l	d7,d0
		move.l	d2,d7
		lsl.l	#4,d7
		eor.l	d3,d7
		and.l	#$f0f0f0f0,d7
		eor.l	d7,d3
		lsr.l	#4,d7
		eor.l	d7,d2
		move.l	2(a2),d4
		move.l	10(a2),d6
		move.w	18(a2),d4
		move.w	26(a2),d6
		move.l	d4,d7
		lsl.l	#8,d7
		eor.l	d6,d7
		and.l	#$ff00ff00,d7
		eor.l	d7,d6
		lsr.l	#8,d7
		eor.l	d7,d4
		move.l	d0,a3
		move.l	6(a2),d5
		move.l	14(a2),d0
		move.w	22(a2),d5
		move.w	30(a2),d0
		move.l	d5,d7
		lsl.l	#8,d7
		eor.l	d0,d7
		and.l	#$ff00ff00,d7
		eor.l	d7,d0
		lsr.l	#8,d7
		eor.l	d7,d5
		move.l	d4,d7
		lsl.l	#4,d7
		eor.l	d5,d7
		and.l	#$f0f0f0f0,d7
		eor.l	d7,d5
		lsr.l	#4,d7
		eor.l	d7,d4
		move.l	d6,d7
		lsl.l	#4,d7
		eor.l	d0,d7
		and.l	#$f0f0f0f0,d7
		eor.l	d7,d0
		lsr.l	#4,d7
		eor.l	d7,d6
		move.l	d2,d7
		lsl.l	#2,d7
		eor.l	d6,d7
		and.l	#$cccccccc,d7
		eor.l	d7,d6
		lsr.l	#2,d7
		eor.l	d7,d2
		move.l	d1,d7
		lsl.l	#2,d7
		eor.l	d5,d7
		and.l	#$cccccccc,d7
		eor.l	d7,d5
		lsr.l	#2,d7
		eor.l	d7,d1
		move.l	d3,d7
		lsl.l	#2,d7
		eor.l	d0,d7
		and.l	#$cccccccc,d7
		eor.l	d7,d0
		lsr.l	#2,d7
		eor.l	d7,d3
		exg	d2,a3
		move.l	d2,d7
		lsl.l	#2,d7
		eor.l	d4,d7
		and.l	#$cccccccc,d7
		eor.l	d7,d4
		lsr.l	#2,d7
		eor.l	d7,d2
		move.l	d5,d7
		lsl.l	#1,d7
		eor.l	d0,d7
		and.l	#$aaaaaaaa,d7
		eor.l	d7,d0
		and.l	(a6),d0
		move.l	d0,(a1)
		lsr.l	#1,d7
		eor.l	d7,d5
		and.l	(a6),d5
c2p_offset_1:	move.l	d5,1*24(a1)
		move.l	d1,d7
		lsl.l	#1,d7
		eor.l	d3,d7
		and.l	#$aaaaaaaa,d7
		eor.l	d7,d3
		and.l	(a6),d3
c2p_offset_2:	move.l	d3,2*24(a1)
		lsr.l	#1,d7
		eor.l	d7,d1
		and.l	(a6),d1
c2p_offset_3:	move.l	d1,3*24(a1)
		move.l	d4,d7
		lsl.l	#1,d7
		eor.l	d6,d7
		and.l	#$aaaaaaaa,d7
		eor.l	d7,d6
		and.l	(a6),d6
c2p_offset_4:	move.l	d6,4*24(a1)
		lsr.l	#1,d7
		eor.l	d7,d4
		and.l	(a6),d4
c2p_offset_5:	move.l	d4,5*24(a1)
		move.l	a3,d4
		move.l	d2,d7
		lsl.l	#1,d7
		eor.l	d4,d7
		and.l	#$aaaaaaaa,d7
		eor.l	d7,d4
		and.l	(a6),d4
c2p_offset_6:	move.l	d4,6*24(a1)
		lsr.l	#1,d7
		eor.l	d7,d2
		and.l	(a6),d2
c2p_offset_7:	move.l	d2,7*24(a1)
		lea	32(a2),a2

;C2P End...
		move.l	a4,d0
		move.l	a5,d1

		lea	4(a1),a1
		dbf	d0,c2p_loopx
		add.l	C2P_EOL_skip(pc),a1
		sub.l	C2P_y_datacorrect(pc),a2
		tst.l	half_switch
		beq.b	.nextrow
		add.l	y_bitmap_width(pc),a2
.nextrow	dbf	d1,c2p_loopy

		movem.l	(a7)+,d0-a6
		rts

		CNOP    0,16
aga_storm_temp:	ds.b	3*16
aga_ff00ff00:	dc.l	$ff00ff00
aga_f0f0f0f0:	dc.l	$f0f0f0f0
		CNOP    0,16
mpr_STORM8
		movem.l	d0-a6,-(a7)
		move.l	GfxMemBase,a1
		move.l	y_bitmap_base,a2
		move.l	cb_bitmap_base,a3
		move.l	cr_bitmap_base,a4
		move.l	height(pc),d1
		subq.l	#1,d1
dham8_loopy:
		move.l	#$ffffffff,DHAM8C2PMask
		move.l	C2P_x_loop(pc),d0
		subq.l	#1,d0
dham8_loopx	
		tst.l	d0
		bne.b	.nomask
		move.l	c2pMask(pc),DHAM8C2PMask
.nomask		movem.l	d0-d2,-(a7)
*-------------------------------------------------------------------------------------------------*
*----------------------------------------- C2P ROUTINE -------------------------------------------*
*-------------------------------------------------------------------------------------------------*
;;; Input Registers: a2 origi chunky data
;;;                  a1 plane0 address

		lea		aga_storm_temp(pc),a0
		move.l	YUV_BG_Table,a5
		move.l	YUV_GR_Table,a6

		moveq	#4,d6
		move.l	#$fcfcfcfc,d5	;D5 = mask1
.yuvconvloop
		move.l	(a2)+,d0		;d0 = [Y0,Y1,Y2,Y3]
		 moveq	#0,d7
		move.w	(a3)+,d1		;d1 = [--,--,U0,U1]
		 and.l	d5,d0			;d0 = [000000--][111111--][222222--][333333--]
		move.w	(a4)+,d2		;d2 = [--,--,V0,V1]
		 move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		and.w	d5,d2			;d1 = [--------][--------][vvvvvv--][vvvvvv--]
		 lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		lsr.l	#2,d0			;d0 = [--000000][--111111][--222222][--333333]
		 move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]
		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		 ror.w	#8,d2			;d1 = [--,--,V1,V0]
		move.l	d7,d4			;d4 = [--------][------uu][uuuuvvvv][vv------]
		 or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		ror.l	#8,d0			;d0 = [--333333][--000000][--111111][--222222]
		 ror.w	#8,d1			;d1 = [--,--,U1,U0]
		or.b	d0,d4			;d4 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		 moveq	#8,d3
		ror.l	#8,d0			;d0 = [--222222][--333333][--000000][--111111]
		 move.b	d1,d3			;d3 = [--------][--------][--------][uuuuuuuu]

		move.w	(a6,d7.l*2),10(a0)	;[- - - - - - - - - - G R]
		move.w	(a5,d4.l*2),6(a0)	;[- - - - - - B G - - G R]

		lsl.l	#6,d3			;d3 = [--------][--------][--uuuuuu][uu------]
		 move.b	d0,d1
		move.b	d2,d3			;d3 = [--------][--------]]--uuuuuu][vvvvvv--]
		 ror.l	#8,d0			;d0 = [--111111][--222222][--333333][--000000]
		lsl.l	#4,d3			;d3 = [--------][------uu][uuuuvvvv][vv------]
		move.l	d3,d7			;
		 or.b	d1,d3			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		 moveq	#12,d4			;

		move.w	(a6,d3.l*2),4(a0)	;[- - - - G R B G - - G R]
		move.w	(a5,d7.l*2),(a0)	;[B G - - G R B G - - G R]

		adda.l	d4,a0
		 subq.l	#1,d6
		bne.b	.yuvconvloop
	
		lea	aga_storm_temp(pc),a0

		move.l	(a0),d0			;d0=[ 0, 1, x, x]
		move.w	24(a0),d0		;d0=[ 0, 1,16,17]
		move.l	12(a0),d2
		move.w	36(a0),d2		;d2=[ 8, 9,24,25]

		move.l	6(a0),d1
		move.w	30(a0),d1		;d1=[ 4, 5,20,21]
		move.l	18(a0),d3
		move.w	42(a0),d3		;d3=[12,13,28,29]

		move.l	aga_ff00ff00-aga_storm_temp(a0),d4
		move.l	d0,d6
		 move.l	d1,d5
		lsl.l	#8,d6			;1   16   17    00
		 lsl.l	#8,d5
		eor.l	d2,d6			;1^8 16^9 17^24 25
		eor.l	d3,d5
		and.l	d4,d6			;1^8 0    17^24 00
		 and.l	d4,d5

		eor.l	d6,d2			;d2=[ 1, 9,17,25]
		eor.l	d5,d3			;d3=[ 5,13,21,29]
		lsr.l	#8,d6			;  0, 1^8, 0, 17^24
		 lsr.l	#8,d5
		eor.l	d6,d0			;d0=[ 0, 8,16,24]
		eor.l	d5,d1			;d1=[ 4,12,20,28]

		move.l	#$f0f0f0f0,d7		;Move 4bit mask into a4

		move.l	d0,d6
		 move.l	d2,d5
		lsl.l	#4,d6
		 lsl.l	#4,d5
		eor.l	d1,d6
		eor.l	d3,d5
		and.l	d7,d6
		 and.l	d7,d5
		eor.l	d6,d1			;d1=[ e0, e4, e8,e12,e16,e20,e24,e28]<-row 4
		eor.l	d5,d3			;d3=[ e1, e5, e9,e13,e17,e21,e25,e29]<-row 5
		lsr.l	#4,d6
		 lsr.l	#4,d5
		eor.l	d6,d0			;d0=[ a0, a4, a8,a12,a16,a20,a24,a28]<-row 0
		eor.l	d5,d2			;d2=[ a1, a5, a9,a13,a17,a21,a25,a29]<-row 1

		exg		d0,a5
		exg		d1,a6

		move.l	29(a0),d0
		move.w	5(a0),d0
		move.b	17(a0),d0
		swap	d0
		move.b	41(a0),d0		;d0=[2,10,18,26]

		move.l	35(a0),d1
		move.w	11(a0),d1
		move.b	23(a0),d1
		swap	d1
		move.b	47(a0),d1		;d1=[6,14,22,30]

		move.l	28(a0),d4
		move.w	4(a0),d4
		move.b	16(a0),d4
		swap	d4
		move.b	40(a0),d4		;d4=[3,11,19,27]

		move.l	34(a0),d5
		move.w	10(a0),d5
		move.b	22(a0),d5
		swap	d5
		move.b	46(a0),d5		;d5=[7,15,23,31]

		move.l	d0,d6
		 move.l	d4,d7
		lsl.l	#4,d6
		 lsl.l	#4,d7
		eor.l	d1,d6
		eor.l	d5,d7
		and.l	aga_f0f0f0f0-aga_storm_temp(a0),d6
		and.l	aga_f0f0f0f0-aga_storm_temp(a0),d7
		eor.l	d6,d1			;d1=[ e2, e6,e10,e14,e18,e22,e26,e30]<-row 6
		eor.l	d7,d5			;d5=[ e3, e7,e11,e15,e19,e23,e27,e31]<-row 7
		lsr.l	#4,d6
		 lsr.l	#4,d7
		eor.l	d6,d0			;d0=[ a2, a6,a10,a14,a18,a22,a26,a30]<-row 2
		eor.l	d7,d4			;d4=[ a3, a7,a11,a15,a19,a23,a27,a31]<-row 3

		move.l	#$cccccccc,d7
		and.l	d7,d3			;d3=[e1f1,----,e5f5,----]...
		 move.l	d2,d6
		and.l	d7,d5			;d5=[e3f3,----,e7e7,----]...
		 lsl.l	#2,d6
		lsr.l	#2,d5			;d5=[----,e3f3,----,e7f7]...
		 eor.l	d4,d6
		or.l	d5,d3			;d3=[e1f1,e3f3,e5f5,e7f7]...<-row 5
		 and.l	d7,d6

		eor.l	d6,d4			;d4=[c1d1,c3d3,c5d5,c7d7]...<-row 3
		 lsr.l	#2,d6
		eor.l	d6,d2			;d2=[a1b1,a3b3,a5b5,a7b7]...<-row 1

		exg	d2,a5
		exg	d4,a6

		move.l	d2,d6
		 and.l	d7,d4			;d4=[e0f0,----,e4f4,----]...
		lsl.l	#2,d6
		 and.l	d7,d1			;d1=[e2f2,----,e6f6,----]...
		eor.l	d0,d6
		 lsr.l	#2,d1			;d1=[----,e2f2,----,e6f6]...
		and.l	d7,d6
		 or.l	d4,d1			;d1=[e0f0,e2f2,e4f4,e6f6]...<-row 4
		eor.l	d6,d0			;d0=[c0d0,c3d3,c4d4,c6d6]...<-row 2
		 lsr.l	#2,d6
		eor.l	d6,d2			;d2=[a0b0,a2b2,a4b4,a6b6]...<-row 0

		move.l	#$aaaaaaaa,d7

		move.l	d1,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[f0,f1,f2,f3,f4,f5,f6,f7]...<-*ROW 5*
		 lsr.l	#1,d6
		and.l	DHAM8C2PMask(pc),d3
		move.l	d3,(a1)
		 eor.l	d6,d1			;d1=[e0,e1,e2,e3,e4,e5,e6,e7]...<-*ROW 4*

		and.l	DHAM8C2PMask(pc),d1
dham8_offset_1:	move.l	d1,24*1(a1)

		exg	d1,a5
		exg	d3,a6

		move.l	d0,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[d0,d1,d2,d3,d4,d5,d6,d7]...<-*ROW 3*
		 lsr.l	#1,d6
		and.l	DHAM8C2PMask(pc),d3
dham8_offset_2:	move.l	d3,24*2(a1)
		eor.l	d6,d0			;d0=[c0,c1,c2,c3,c4,c5,c6,c7]...<-*ROW 2*
		 move.l	d2,d6
		and.l	DHAM8C2PMask(pc),d0
dham8_offset_3:	move.l	d0,24*3(a1)
		lsl.l	#1,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1			;d1=[b0,b1,b2,b3,b4,b5,b6,b7]...<-*ROW 1*
		 lsr.l	#1,d6
		and.l	DHAM8C2PMask(pc),d1
dham8_offset_4:	move.l	d1,24*4(a1)
		eor.l	d6,d2			;d2=[a0,a1,a2,a3,a4,a5,a6,a7]...<-*ROW 0*
		and.l	DHAM8C2PMask(pc),d2
dham8_offset_5:	move.l	d2,24*5(a1)
*-------------------------------------------------------------------------------------------------*
;C2P End...
		movem.l	(a7)+,d0-d2
		addq.l	#4,a1
		dbf	d0,dham8_loopx
		add.l	C2P_EOL_skip(pc),a1
		sub.l	C2P_y_datacorrect(pc),a2
		sub.l	C2P_c_datacorrect(pc),a3
		sub.l	C2P_c_datacorrect(pc),a4
		btst	#0,d1
		beq.b	.nextrow
		sub.l	c_bitmap_width(pc),a3
		sub.l	c_bitmap_width(pc),a4
.nextrow	dbf	d1,dham8_loopy
		movem.l	(a7)+,d0-a6
		rts

		CNOP    0,16
mpr_STORM6	movem.l	d0-a6,-(a7)
		move.l	GfxMemBase,a1
		move.l	y_bitmap_base,a2
		move.l	cb_bitmap_base,a3
		move.l	cr_bitmap_base,a4
		move.l	height(pc),d1
		subq.l	#1,d1
dham6_loopy	move.l	#$ffffffff,DHAM8C2PMask
		move.l	C2P_x_loop(pc),d0
		subq.l	#1,d0
dham6_loopx	tst.l	d0
		bne.b	.nomask
		move.l	c2pMask(pc),DHAM8C2PMask
.nomask		movem.l	d0-d2,-(a7)
*-------------------------------------------------------------------------------------------------*
*----------------------------------------- C2P ROUTINE -------------------------------------------*
*-------------------------------------------------------------------------------------------------*
;;; Input Registers: a2 origi chunky data
;;;                  a1 plane0 address

		lea	aga_storm_temp(pc),a0
		move.l	YUV_BG_Table,a5
		move.l	YUV_GR_Table,a6

		moveq	#4,d6			;Convert 4 x 4 = 16 pixels
.yuvconvloop
		move.l	(a2),d0			;d0 = [Y0,Y1,Y2,Y3]

		and.l	#$fcfcfcfc,d0	;d0 = [000000--][111111--][222222--][333333--]

		lsr.l	#2,d0			;d0 = [--000000][--111111][--222222][--333333]

		move.w	(a3),d1			;d1 = [--,--,U0,U1]

		move.w	(a4),d2			;d2 = [--,--,V0,V1]

		and.w	#$fcfc,d2		;d1 = [--------][--------][vvvvvv--][vvvvvv--]

		moveq	#0,d7

		move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]

		lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]

		move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]

		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]

		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]

		move.w	(a6,d7.l*2),10(a0)	;[- - - - - - - - - - G R]

		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		ror.l	#8,d0			;d0 = [--333333][--000000][--111111][--222222]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a5,d7.l*2),6(a0)	;[- - - - - - B G - - G R]
		ror.l	#8,d0			;d0 = [--222222][--333333][--000000][--111111]
		ror.w	#8,d1			;d1 = [--,--,U1,U0]
		ror.w	#8,d2			;d1 = [--,--,V1,V0]
		moveq	#8,d7
		move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]
		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a6,d7.l*2),4(a0)	;[- - - - G R B G - - G R]
		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		ror.l	#8,d0			;d0 = [--111111][--222222][--333333][--000000]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a5,d7.l*2),(a0)	;[B G - - G R B G - - G R]

		addq.l	#4,a2			;Y add
		addq.l	#2,a3			;U/Cb add
		addq.l	#2,a4			;V/Cr add
		lea	12(a0),a0

		subq.l	#1,d6
		bne.b	.yuvconvloop

		lea	aga_storm_temp(pc),a0
		move.l	#$f0f0f0f0,d7		;4bit mask in d7
		move.w	(a0),d0			;d0=[--,--, 0,--]
		move.w	6(a0),d1		;d1=[--,--, 4,--]
		move.b	12(a0),d0		;d0=[--,--, 0, 8]
		move.b	18(a0),d1		;d1=[--,--, 4,12]
		swap	d0			;d0=[ 0, 8,--,--]
		swap	d1			;d1=[ 4,12,--,--]
		move.w	24(a0),d0		;d0=[ 0, 8,16,--]
		move.w	30(a0),d1		;d1=[ 4,12,20,--]
		move.b	36(a0),d0		;d0=[ 0, 8,16,24]
		move.b	42(a0),d1		;d1=[ 4,12,20,28]
		and.l	d7,d0
		and.l	d7,d1
		lsr.l	#4,d1
		or.l	d1,d0			;d0=[0,4,8,12,16,20,24,28]<-4 bits each
		move.w	1(a0),d1
		move.w	7(a0),d2
		move.b	13(a0),d1
		move.b	19(a0),d2
		swap	d1
		swap	d2
		move.w	25(a0),d1
		move.w	31(a0),d2
		move.b	37(a0),d1		;d1=[ 1,9,17,25]
		move.b	43(a0),d2		;d2=[5,13,21,29]
		and.l	d7,d1
		and.l	d7,d2
		lsr.l	#4,d2
		or.l	d2,d1			;d1=[1,5,9,13,17,21,25,29]
		move.w	5(a0),d2
		move.w	11(a0),d3
		move.b	17(a0),d2
		move.b	23(a0),d3
		swap	d2
		swap	d3
		move.w	29(a0),d2
		move.w	35(a0),d3
		move.b	41(a0),d2		;d2=[ 2,10,18,26]
		move.b	47(a0),d3		;d3=[ 6,14,22,30]
		and.l	d7,d2
		and.l	d7,d3
		lsr.l	#4,d3
		or.l	d3,d2			;d2=[2,6,10,14,18,22,26,30]
		move.w	4(a0),d3
		move.w	10(a0),d4
		move.b	16(a0),d3
		move.b	22(a0),d4
		swap	d3
		swap	d4
		move.w	28(a0),d3
		move.w	34(a0),d4
		move.b	40(a0),d3		;d3=[ 3,11,19,27]
		move.b	46(a0),d4		;d4=[ 7,15,23,31]
		and.l	d7,d3
		and.l	d7,d4
		lsr.l	#4,d4
		or.l	d4,d3			;d3=[3,7,11,15,19,23,27,31]
		move.l	#$cccccccc,d7
		move.l	d0,d6
		lsl.l	#2,d6
		eor.l	d2,d6
		and.l	d7,d6
		eor.l	d6,d2			;d2=[c0,d0,c2,d2,c4,d4,c6,d6]...3
		lsr.l	#2,d6
		eor.l	d6,d0			;d0=[a0,b0,a2,b2,a4,b4,a6,b6]...1
		move.l	d1,d6
		lsl.l	#2,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[c1,d1,c3,d3,c5,d5,c7,d7]...4
		lsr.l	#2,d6
		eor.l	d6,d1			;d1=[a1,b1,a3,b3,a5,b5,a7,b7]...2
		move.l	#$aaaaaaaa,d7
		move.l	d0,d6
		lsl.l	#1,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1
dham6_offset_2:	move.l	d1,24*2(a1)
		lsr.l	#1,d6
		eor.l	d6,d0
dham6_offset_3:	move.l	d0,24*3(a1)
		move.l	d2,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3
		move.l	d3,(a1)
		lsr.l	#1,d6
		eor.l	d6,d2
dham6_offset_1:	move.l	d2,24*1(a1)
;--------------------------------------------------------
;c2p end
		movem.l	(a7)+,d0-d2
		addq.l	#4,a1
		dbf	d0,dham6_loopx
		add.l	C2P_EOL_skip(pc),a1
		sub.l	C2P_y_datacorrect(pc),a2
		sub.l	C2P_c_datacorrect(pc),a3
		sub.l	C2P_c_datacorrect(pc),a4
		btst	#0,d1
		beq.b	.nextrow
		sub.l	c_bitmap_width(pc),a3
		sub.l	c_bitmap_width(pc),a4
.nextrow	dbf	d1,dham6_loopy
		movem.l	(a7)+,d0-a6
		rts

		CNOP    0,16
mpr_STORM8_half	movem.l	d0-a6,-(a7)			;halfheight storm dither
		move.l	GfxMemBase,a1
		move.l	y_bitmap_base,a2
		move.l	cb_bitmap_base,a3
		move.l	cr_bitmap_base,a4
		move.l	height(pc),d1
		lsr.l	#1,d1
		subq.l	#1,d1
dham8h_loopy	move.l	#$ffffffff,DHAM8C2PMask
		move.l	C2P_x_loop(pc),d0
		subq.l	#1,d0
dham8h_loopx	tst.l	d0
		bne.b	.nomask
		move.l	c2pMask(pc),DHAM8C2PMask
.nomask		movem.l	d0-d2,-(a7)
*-------------------------------------------------------------------------------------------------*
*----------------------------------------- C2P ROUTINE -------------------------------------------*
*-------------------------------------------------------------------------------------------------*
;;; Input Registers: a2 origi chunky data
;;;                  a1 plane0 address

		lea	aga_storm_temp(pc),a0
		move.l	YUV_BG_Table,a5
		move.l	YUV_BG_Table,a6

		moveq	#4,d6			;Convert 4 x 4 = 16 pixels
		move.l	#$fcfcfcfc,d5	;D5 = mask1
.yuvconvloop
		move.l	(a2),d0			;d0 = [Y0,Y1,Y2,Y3]
		 moveq	#0,d7
		move.w	(a3),d1			;d1 = [--,--,U0,U1]
		 and.l	d5,d0			;d0 = [000000--][111111--][222222--][333333--]
		move.w	(a4),d2			;d2 = [--,--,V0,V1]
		 move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		and.w	d5,d2			;d1 = [--------][--------][vvvvvv--][vvvvvv--]
		 lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		lsr.l	#2,d0			;d0 = [--000000][--111111][--222222][--333333]
		 move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]

		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]

		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]

		move.w	(a6,d7.l*2),10(a0)	;[- - - - - - - - - - G R]

		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]

		ror.l	#8,d0			;d0 = [--333333][--000000][--111111][--222222]

		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]

		move.w	(a5,d7.l*2),6(a0)	;[- - - - - - B G - - G R]

		ror.l	#8,d0			;d0 = [--222222][--333333][--000000][--111111]
		ror.w	#8,d1			;d1 = [--,--,U1,U0]
		ror.w	#8,d2			;d1 = [--,--,V1,V0]
		moveq	#8,d7
		move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]
		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]

		move.w	(a6,d7.l*2),4(a0)	;[- - - - G R B G - - G R]

		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		ror.l	#8,d0			;d0 = [--111111][--222222][--333333][--000000]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a5,d7.l*2),(a0)	;[B G - - G R B G - - G R]

		addq.l	#4,a2			;Y add
		addq.l	#2,a3			;U/Cb add
		addq.l	#2,a4			;V/Cr add
		lea	12(a0),a0
		subq.l	#1,d6
		bne.b	.yuvconvloop

		lea	aga_storm_temp(pc),a0
		move.l	(a0),d0
		move.w	24(a0),d0		;d0=[ 0, 1,16,17]
		move.l	12(a0),d2
		move.w	36(a0),d2		;d2=[ 8, 9,24,25]
		move.l	d0,d6
		lsl.l	#8,d6
		eor.l	d2,d6
		and.l	#$ff00ff00,d6
		eor.l	d6,d2			;d2=[ 1, 9,17,25]
		lsr.l	#8,d6
		eor.l	d6,d0			;d0=[ 0, 8,16,24]
		move.l	6(a0),d1
		move.w	30(a0),d1		;d1=[ 4, 5,20,21]
		move.l	18(a0),d3
		move.w	42(a0),d3		;d3=[12,13,28,29]
		move.l	d1,d6
		lsl.l	#8,d6
		eor.l	d3,d6
		and.l	#$ff00ff00,d6
		eor.l	d6,d3			;d3=[ 5,13,21,29]
		lsr.l	#8,d6
		eor.l	d6,d1			;d1=[ 4,12,20,28]
		move.l	#$f0f0f0f0,d7		;Move 4bit mask into a4
		move.l	d0,d6
		lsl.l	#4,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1			;d1=[ e0, e4, e8,e12,e16,e20,e24,e28]<-row 4
		lsr.l	#4,d6
		eor.l	d6,d0			;d0=[ a0, a4, a8,a12,a16,a20,a24,a28]<-row 0
		move.l	d2,d6
		lsl.l	#4,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[ e1, e5, e9,e13,e17,e21,e25,e29]<-row 5
		lsr.l	#4,d6
		eor.l	d6,d2			;d2=[ a1, a5, a9,a13,a17,a21,a25,a29]<-row 1
		exg	d0,a5
		exg	d1,a6
		move.l	29(a0),d0
		move.w	5(a0),d0
		move.b	17(a0),d0
		swap	d0
		move.b	41(a0),d0		;d0=[2,10,18,26]
		move.l	35(a0),d1
		move.w	11(a0),d1
		move.b	23(a0),d1
		swap	d1
		move.b	47(a0),d1		;d1=[6,14,22,30]
		move.l	d0,d6
		lsl.l	#4,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1			;d1=[ e2, e6,e10,e14,e18,e22,e26,e30]<-row 6
		lsr.l	#4,d6
		eor.l	d6,d0			;d0=[ a2, a6,a10,a14,a18,a22,a26,a30]<-row 2
		move.l	28(a0),d4
		move.w	4(a0),d4
		move.b	16(a0),d4
		swap	d4
		move.b	40(a0),d4		;d4=[3,11,19,27]
		move.l	34(a0),d5
		move.w	10(a0),d5
		move.b	22(a0),d5
		swap	d5
		move.b	46(a0),d5		;d5=[7,15,23,31]
		move.l	d4,d6
		lsl.l	#4,d6
		eor.l	d5,d6
		and.l	d7,d6
		eor.l	d6,d5			;d5=[ e3, e7,e11,e15,e19,e23,e27,e31]<-row 7
		lsr.l	#4,d6
		eor.l	d6,d4			;d4=[ a3, a7,a11,a15,a19,a23,a27,a31]<-row 3
		move.l	#$cccccccc,d7
		and.l	d7,d3			;d3=[e1f1,----,e5f5,----]...
		and.l	d7,d5			;d5=[e3f3,----,e7e7,----]...
		lsr.l	#2,d5			;d5=[----,e3f3,----,e7f7]...
		or.l	d5,d3			;d3=[e1f1,e3f3,e5f5,e7f7]...<-row 5
		move.l	d2,d6
		lsl.l	#2,d6
		eor.l	d4,d6
		and.l	d7,d6
		eor.l	d6,d4			;d4=[c1d1,c3d3,c5d5,c7d7]...<-row 3
		lsr.l	#2,d6
		eor.l	d6,d2			;d2=[a1b1,a3b3,a5b5,a7b7]...<-row 1
		exg	d2,a5
		exg	d4,a6
		move.l	d2,d6
		lsl.l	#2,d6
		eor.l	d0,d6
		and.l	d7,d6
		eor.l	d6,d0			;d0=[c0d0,c3d3,c4d4,c6d6]...<-row 2
		lsr.l	#2,d6
		eor.l	d6,d2			;d2=[a0b0,a2b2,a4b4,a6b6]...<-row 0
		and.l	d7,d4			;d4=[e0f0,----,e4f4,----]...
		and.l	d7,d1			;d1=[e2f2,----,e6f6,----]...
		lsr.l	#2,d1			;d1=[----,e2f2,----,e6f6]...
		or.l	d4,d1			;d1=[e0f0,e2f2,e4f4,e6f6]...<-row 4
		move.l	#$aaaaaaaa,d7
		move.l	d1,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[f0,f1,f2,f3,f4,f5,f6,f7]...<-*ROW 5*
		and.l	DHAM8C2PMask(pc),d3
		move.l	d3,(a1)
		lsr.l	#1,d6
		eor.l	d6,d1			;d1=[e0,e1,e2,e3,e4,e5,e6,e7]...<-*ROW 4*
		and.l	DHAM8C2PMask(pc),d1
dham8h_offset_1	move.l	d1,24*1(a1)
		exg	d1,a5
		exg	d3,a6
		move.l	d0,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[d0,d1,d2,d3,d4,d5,d6,d7]...<-*ROW 3*
		and.l	DHAM8C2PMask(pc),d3
dham8h_offset_2	move.l	d3,24*2(a1)
		lsr.l	#1,d6
		eor.l	d6,d0			;d0=[c0,c1,c2,c3,c4,c5,c6,c7]...<-*ROW 2*
		and.l	DHAM8C2PMask(pc),d0
dham8h_offset_3	move.l	d0,24*3(a1)
		move.l	d2,d6
		lsl.l	#1,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1			;d1=[b0,b1,b2,b3,b4,b5,b6,b7]...<-*ROW 1*
		and.l	DHAM8C2PMask(pc),d1
dham8h_offset_4	move.l	d1,24*4(a1)
		lsr.l	#1,d6
		eor.l	d6,d2			;d2=[a0,a1,a2,a3,a4,a5,a6,a7]...<-*ROW 0*
		and.l	DHAM8C2PMask(pc),d2
dham8h_offset_5	move.l	d2,24*5(a1)
*-------------------------------------------------------------------------------------------------*
;C2P End...
		movem.l	(a7)+,d0-d2
		addq.l	#4,a1
		dbf	d0,dham8h_loopx
		add.l	C2P_EOL_skip(pc),a1
		add.l	y_bitmap_width(pc),a2
		sub.l	C2P_y_datacorrect(pc),a2
		sub.l	C2P_c_datacorrect(pc),a3
		sub.l	C2P_c_datacorrect(pc),a4
		dbf	d1,dham8h_loopy
		movem.l	(a7)+,d0-a6
		rts

		CNOP    0,16
mpr_STORM6_half	movem.l	d0-a6,-(a7)
		move.l	GfxMemBase,a1
		move.l	y_bitmap_base,a2
		move.l	cb_bitmap_base,a3
		move.l	cr_bitmap_base,a4
		move.l	height(pc),d1
		lsr.l	#1,d1
		subq.l	#1,d1
dham6h_loopy	move.l	#$ffffffff,DHAM8C2PMask
		move.l	C2P_x_loop(pc),d0
		subq.l	#1,d0
dham6h_loopx	tst.l	d0
		bne.b	.nomask
		move.l	c2pMask(pc),DHAM8C2PMask
.nomask		movem.l	d0-d2,-(a7)
*-------------------------------------------------------------------------------------------------*
*----------------------------------------- C2P ROUTINE -------------------------------------------*
*-------------------------------------------------------------------------------------------------*
;;; Input Registers: a2 origi chunky data
;;;                  a1 plane0 address

		lea	aga_storm_temp(pc),a0
		move.l	YUV_BG_Table,a5
		move.l	YUV_BG_Table,a6

		moveq	#4,d6			;Convert 4 x 4 = 16 pixels
.yuvconvloop
		move.l	(a2),d0			;d0 = [Y0,Y1,Y2,Y3]
		and.l	#$fcfcfcfc,d0		;d0 = [000000--][111111--][222222--][333333--]
		lsr.l	#2,d0			;d0 = [--000000][--111111][--222222][--333333]
		move.w	(a3),d1			;d1 = [--,--,U0,U1]
		move.w	(a4),d2			;d2 = [--,--,V0,V1]
		and.w	#$fcfc,d2		;d1 = [--------][--------][vvvvvv--][vvvvvv--]
		moveq	#0,d7
		move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]
		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a6,d7.l*2),10(a0)	;[- - - - - - - - - - G R]
		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		ror.l	#8,d0			;d0 = [--333333][--000000][--111111][--222222]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a5,d7.l*2),6(a0)	;[- - - - - - B G - - G R]
		ror.l	#8,d0			;d0 = [--222222][--333333][--000000][--111111]
		ror.w	#8,d1			;d1 = [--,--,U1,U0]
		ror.w	#8,d2			;d1 = [--,--,V1,V0]
		moveq	#8,d7
		move.b	d1,d7			;d7 = [--------][--------][--------][uuuuuuuu]
		lsl.l	#6,d7			;d7 = [--------][--------][--uuuuuu][uu------]
		move.b	d2,d7			;d7 = [--------][--------]]--uuuuuu][vvvvvv--]
		lsl.l	#4,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a6,d7.l*2),4(a0)	;[- - - - G R B G - - G R]
		eor.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vv------]
		ror.l	#8,d0			;d0 = [--111111][--222222][--333333][--000000]
		or.b	d0,d7			;d7 = [--------][------uu][uuuuvvvv][vvyyyyyy]
		move.w	(a5,d7.l*2),(a0)	;[B G - - G R B G - - G R]

		addq.l	#4,a2			;Y add
		addq.l	#2,a3			;U/Cb add
		addq.l	#2,a4			;V/Cr add
		lea	12(a0),a0
		subq.l	#1,d6
		bne.b	.yuvconvloop

		lea	aga_storm_temp(pc),a0
		move.l	#$f0f0f0f0,d7		;4bit mask in d7
		move.w	(a0),d0			;d0=[--,--, 0,--]
		move.w	6(a0),d1		;d1=[--,--, 4,--]
		move.b	12(a0),d0		;d0=[--,--, 0, 8]
		move.b	18(a0),d1		;d1=[--,--, 4,12]
		swap	d0			;d0=[ 0, 8,--,--]
		swap	d1			;d1=[ 4,12,--,--]
		move.w	24(a0),d0		;d0=[ 0, 8,16,--]
		move.w	30(a0),d1		;d1=[ 4,12,20,--]
		move.b	36(a0),d0		;d0=[ 0, 8,16,24]
		move.b	42(a0),d1		;d1=[ 4,12,20,28]
		and.l	d7,d0
		and.l	d7,d1
		lsr.l	#4,d1
		or.l	d1,d0			;d0=[0,4,8,12,16,20,24,28]<-4 bits each
		move.w	1(a0),d1
		move.w	7(a0),d2
		move.b	13(a0),d1
		move.b	19(a0),d2
		swap	d1
		swap	d2
		move.w	25(a0),d1
		move.w	31(a0),d2
		move.b	37(a0),d1		;d1=[ 1,9,17,25]
		move.b	43(a0),d2		;d2=[5,13,21,29]
		and.l	d7,d1
		and.l	d7,d2
		lsr.l	#4,d2
		or.l	d2,d1			;d1=[1,5,9,13,17,21,25,29]
		move.w	5(a0),d2
		move.w	11(a0),d3
		move.b	17(a0),d2
		move.b	23(a0),d3
		swap	d2
		swap	d3
		move.w	29(a0),d2
		move.w	35(a0),d3
		move.b	41(a0),d2		;d2=[ 2,10,18,26]
		move.b	47(a0),d3		;d3=[ 6,14,22,30]
		and.l	d7,d2
		and.l	d7,d3
		lsr.l	#4,d3
		or.l	d3,d2			;d2=[2,6,10,14,18,22,26,30]
		move.w	4(a0),d3
		move.w	10(a0),d4
		move.b	16(a0),d3
		move.b	22(a0),d4
		swap	d3
		swap	d4
		move.w	28(a0),d3
		move.w	34(a0),d4
		move.b	40(a0),d3		;d3=[ 3,11,19,27]
		move.b	46(a0),d4		;d4=[ 7,15,23,31]
		and.l	d7,d3
		and.l	d7,d4
		lsr.l	#4,d4
		or.l	d4,d3			;d3=[3,7,11,15,19,23,27,31]
		move.l	#$cccccccc,d7
		move.l	d0,d6
		lsl.l	#2,d6
		eor.l	d2,d6
		and.l	d7,d6
		eor.l	d6,d2			;d2=[c0,d0,c2,d2,c4,d4,c6,d6]...3
		lsr.l	#2,d6
		eor.l	d6,d0			;d0=[a0,b0,a2,b2,a4,b4,a6,b6]...1
		move.l	d1,d6
		lsl.l	#2,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3			;d3=[c1,d1,c3,d3,c5,d5,c7,d7]...4
		lsr.l	#2,d6
		eor.l	d6,d1			;d1=[a1,b1,a3,b3,a5,b5,a7,b7]...2
		move.l	#$aaaaaaaa,d7
		move.l	d0,d6
		lsl.l	#1,d6
		eor.l	d1,d6
		and.l	d7,d6
		eor.l	d6,d1
dham6h_offset_2	move.l	d1,24*2(a1)
		lsr.l	#1,d6
		eor.l	d6,d0
dham6h_offset_3	move.l	d0,24*3(a1)
		move.l	d2,d6
		lsl.l	#1,d6
		eor.l	d3,d6
		and.l	d7,d6
		eor.l	d6,d3
		move.l	d3,(a1)
		lsr.l	#1,d6
		eor.l	d6,d2
dham6h_offset_1	move.l	d2,24*1(a1)
;--------------------------------------------------------
;c2p end
		movem.l	(a7)+,d0-d2
		addq.l	#4,a1
		dbf	d0,dham6h_loopx
		add.l	C2P_EOL_skip(pc),a1
		add.l	y_bitmap_width(pc),a2
		sub.l	C2P_y_datacorrect(pc),a2
		sub.l	C2P_c_datacorrect(pc),a3
		sub.l	C2P_c_datacorrect(pc),a4
		dbf	d1,dham6h_loopy
		movem.l	(a7)+,d0-a6
		rts

