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
* AGA setup / shutdown code                                                  *
******************************************************************************

			EVEN
AGAScreenTags8:		dc.l	SA_Colors32,Palette
			dc.l	TAG_MORE,AGAScreenTags

AGAScreenTags:		dc.l	SA_Quiet,1
			dc.l	SA_Title,ScreenTitle
			dc.l	SA_Overscan,OSCAN_TEXT
			dc.l	SA_Interleaved,1
			dc.l	SA_Behind,0
			dc.l	SA_AutoScroll,1
			dc.l	SA_Left
AGAScreenPosX:		dc.l	0
			dc.l	SA_Top
AGAScreenPosY:		dc.l	0
			dc.l	SA_Width
AGAScreenWidth:		dc.l	320
			dc.l	SA_Height
AGAScreenHeight:	dc.l	240
			dc.l	SA_Depth
AGADepth:		dc.l	8
			dc.l	SA_DisplayID
AGAScreenModeID:	dc.l	0
			dc.l	TAG_END



*=================================================*
*========== AGA Initialisation Routines ==========*
*=================================================*
Init_AGA:
		move.b	DitherMode,d1
		cmp.b	#DM_GRAY,d1
		beq	Init_AGA_Gray
		cmp.b	#DM_DHAM6,d1
		beq	Init_AGA_DHAM6

Init_AGA_DHAM8:
		bsr	ColorAGASetup
		tst.l	d0
		beq	Init_AGA_End				;fail setup
		move.l	#8,AGADepth
		suba.l	a0,a0
		lea	AGAScreenTags8(pc),a1
		CALLINT2	OpenScreenTagList
		move.l	d0,ScreenHandle
		beq	Init_AGA_End
		move.l	ScreenHandle(pc),a1
		lea	sc_RastPort(a1),a1
		move.l	a1,ScreenRastport
		move.l	rp_BitMap(a1),a1
		move.l	bm_Planes(a1),a1
		move.l	a1,GfxMemBase
		move.b	#DM_DHAM8,DitherMode
		lea		mpr_jsr_offsets(pc),a1
		tst.l	half_switch
		beq.b	.full
		move.l	#mpr_STORM8_half,mpr_RenderBitMap(a1)
		bra.b	.renderok
.full	move.l	#mpr_STORM8,mpr_RenderBitMap(a1)
.renderok
		move.l	#mpr_DummyRTS,mpr_LockBitMap(a1)
		move.l	#mpr_DummyRTS,mpr_UnLockBitMap(a1)
		bsr		CreateYUVtoBGGRTable			;need conversion table...
		tst.l	d0
		beq.b	Init_AGA_DHAM8_Error			;if not enough RAM for table, exit
		st		doublewidth
		bsr		OpenWindow				;Must Open Window before setting HAM control bits!!!
		bsr		DHAM8DitherInit				;Calc stuff, control bits, etc.
		clr.b	GrayMode
		bra		Init_AGA_End

Init_AGA_DHAM8_Error
		moveq	#0,d0
		rts

Init_AGA_DHAM6
		bsr	ColorAGASetup
		tst.l	d0
		beq		Init_AGA_End				;fail setup
		move.l	#6,AGADepth
		suba.l	a0,a0
		lea		AGAScreenTags8(pc),a1
		CALLINT2	OpenScreenTagList
		move.l	d0,ScreenHandle
		beq		Init_AGA_End
		move.l	ScreenHandle(pc),a1
		lea		sc_RastPort(a1),a1
		move.l	a1,ScreenRastport
		move.l	rp_BitMap(a1),a1
		move.l	bm_Planes(a1),a1
		move.l	a1,GfxMemBase
		move.b	#DM_DHAM6,DitherMode
		lea		mpr_jsr_offsets(pc),a1
		tst.l	half_switch
		beq.b	.full
		move.l	#mpr_STORM6_half,mpr_RenderBitMap(a1)
		bra.b	.renderok
.full	move.l	#mpr_STORM6,mpr_RenderBitMap(a1)
.renderok	
		move.l	#mpr_DummyRTS,mpr_LockBitMap(a1)
		move.l	#mpr_DummyRTS,mpr_UnLockBitMap(a1)
		bsr		CreateYUVtoBGGRTable			;need conversion table...
		tst.l	d0
		beq.b	Init_AGA_DHAM6_Error			;if not enough RAM for table, exit
		st		doublewidth
		bsr		OpenWindow				;Must Open Window before setting HAM control bits!!!
		bsr		DHAM6DitherInit			;Calc stuff, control bits, etc.
		clr.b	GrayMode
		bra		Init_AGA_End

Init_AGA_DHAM6_Error
		moveq	#0,d0
		rts


ColorAGASetup	move.l	width(pc),d1			;ez ugyanaz minden színes aga cuccra
		lsl.l	#1,d1
		move.l	d1,d2
		and.l	#$ffffffc0,d1
		and.b	#63,d2
		beq.b	.agawidth_ok
		add.l	#64,d1
.agawidth_ok	move.l	d1,AGAScreenWidth		;Width must be multiple of 64!!!
		move.l	d1,ScreenWidth
		lsr.l	#3,d1				;calculate planesize
		move.l	d1,AGAPlaneSize
		move.l	width(pc),d1
		lsl.l	#1,d1				;2x width
		move.l	d1,d2
		lsr.l	#5,d1
		and.b	#31,d2
		beq.b	.c2p_x_loop_ok
		addq.l	#1,d1
.c2p_x_loop_ok	move.l	d1,C2P_x_loop			;number of x loops for 32-bit c2p

		move.l	height(pc),d1
		tst.l	half_switch
		beq.b	.nothalf
		lsr.l	#1,d1				;half height
.nothalf
		move.l	d1,AGAScreenHeight
		move.l	d1,ScreenHeight

		move.l	y_bitmap_width(pc),d0
		lsl.l	#1,d0
		and.l	#$1f,d0
		eor.b	#$1f,d0				;Itt számolja ki hogy mennyive több adatot
		addq.b	#1,d0				;olvasott ki a sor végén, amit majd ki kell
		and.b	#$1f,d0				;vonnia minden sor végén az input pointerbõl.
		move.l	d0,C2P_y_datacorrect		;sorok végén ennyit kell kivonni az input pointerbõl
		lsr.l	#1,d0
		move.l	d0,C2P_c_datacorrect

		move.l	width,d0
		lsl.l	#1,d0
		and.l	#$1f,d0
		moveq.l	#0,d1
		bfset	d1{0:d0}
		move.l	d1,c2pMask			;c2p után használandó maszk

		move.l	#PAL_MONITOR_ID,BIDMonitorID
		move.l	#640,BIDWidth			;bugos graphics.library/BestModeIDA
		move.l	#256,BIDHeight
		lea		BIDTags(pc),a0
		CALLGFX	BestModeIDA
		cmp.l	#INVALID_ID,d0
		beq		ColorAGASetupError
		or.l	#HAM_KEY,d0
		tst.l	half_switch
		beq.b	.agaidok
		or.l	#LORESSDBL_KEY,d0
.agaidok	
		move.l	d0,AGAScreenModeID
		moveq	#1,d0
		rts
ColorAGASetupError
		moveq	#0,d0
		rts


Init_AGA_Gray	move.l	width(pc),d1
		move.l	d1,d2
		and.l	#$ffffffc0,d1
		and.b	#63,d2
		beq.b	.agawidth_ok
		add.l	#64,d1
.agawidth_ok	move.l	d1,AGAScreenWidth			;width must be multiple of 64!!
		move.l	d1,ScreenWidth
		move.l	height(pc),d1
		tst.l	half_switch
		beq.b	.nothalf
		lsr.l	#1,d1
.nothalf	move.l	d1,AGAScreenHeight
		move.l	d1,ScreenHeight

		tst.l	VGA_switch
		beq	.novga
		move.l	#VGA_MONITOR_ID,BIDMonitorID
		bra.b	.mon_id_done
.novga		move.l	#PAL_MONITOR_ID,BIDMonitorID
.mon_id_done	move.l	#8,AGADepth

		move.l	AGAScreenWidth(pc),BIDWidth
		move.l	height(pc),BIDHeight

		lea	BIDTags(pc),a0
		CALLGFX	BestModeIDA
		cmp.l	#INVALID_ID,d0
		beq	Init_AGA_End

		tst.l	half_switch
		beq.b	.nosdbl
		or.l	#LORESSDBL_KEY,d0
.nosdbl		move.l	d0,AGAScreenModeID

		move.l	#8,AGADepth
		suba.l	a0,a0
		lea	AGAScreenTags8(pc),a1
		CALLINT2	OpenScreenTagList
		move.l	d0,ScreenHandle
		beq.b	Init_AGA_End
		move.l	ScreenHandle(pc),a1
		lea	sc_RastPort(a1),a1
		move.l	a1,ScreenRastport
		move.l	rp_BitMap(a1),a1
		move.l	bm_Planes(a1),a1
		move.l	a1,GfxMemBase

		st	GrayMode
		move.b	#DM_GRAY,DitherMode

		lea	mpr_jsr_offsets(pc),a1

		move.l	#mpr_Planar8,mpr_RenderBitMap(a1)
		move.l	#mpr_DummyRTS,mpr_LockBitMap(a1)
		move.l	#mpr_DummyRTS,mpr_UnLockBitMap(a1)
		bsr	AGAGrayDitherInit

Init_AGA_End	move.l	ScreenHandle(pc),d0		;return ScreenHandle
		rts



AGAGrayDitherInit
		move.l	AGAScreenWidth(pc),d1
		lsr.l	#3,d1			;calculate planesize
		move.l	d1,AGAPlaneSize

		move.l	width(pc),d1
		move.l	d1,d2
		lsr.l	#5,d1
		and.b	#31,d2
		beq.b	.c2p_x_loop_ok
		addq.l	#1,d1
.c2p_x_loop_ok	move.l	d1,C2P_x_loop		;number of x loops for 32-bit c2p

		move.l	AGAPlaneSize(pc),d1
		move.l	d1,d2
		move.l	C2P_x_loop(pc),d3
		lsl.l	#2,d3			;no. of bytes in loop
		sub.l	d3,d2
		mulu.w	#7,d1
		add.l	d2,d1
		move.l	d1,C2P_EOL_skip

		move.l	AGAPlaneSize,d1
		move.w	d1,d2
		move.w	d2,2+c2p_offset_1
		add.w	d1,d2
		move.w	d2,2+c2p_offset_2
		add.w	d1,d2
		move.w	d2,2+c2p_offset_3
		add.w	d1,d2
		move.w	d2,2+c2p_offset_4
		add.w	d1,d2
		move.w	d2,2+c2p_offset_5
		add.w	d1,d2
		move.w	d2,2+c2p_offset_6
		add.w	d1,d2
		move.w	d2,2+c2p_offset_7
		CALLEXEC CacheClearU

		move.l	y_bitmap_width(pc),d0
		and.l	#$1f,d0
		eor.b	#$1f,d0				;Itt számolja ki hogy mennyive több adatot
		addq.b	#1,d0				;olvasott ki a sor végén, amit majd ki kell
		and.b	#$1f,d0				;vonnia minden sor végén az input pointerbõl.
		move.l	d0,C2P_y_datacorrect		;sorok végén ennyit kell kivonni az input pointerbõl

		move.l	width,d0
		and.l	#$1f,d0
		moveq.l	#0,d1
		bfset	d1{0:d0}
		move.l	d1,c2pMask			;c2p után használandó maszk

		rts


DHAM8DitherInit
		move.l	AGAPlaneSize(pc),d1
		move.l	d1,d2
		move.l	C2P_x_loop(pc),d3
		lsl.l	#2,d3			;no. of bytes in loop
		sub.l	d3,d2
		mulu.w	#7,d1
		add.l	d2,d1
		move.l	d1,C2P_EOL_skip

		move.l	AGAPlaneSize,d1
		move.w	d1,d2
		move.w	d2,2+dham8_offset_1
		move.w	d2,2+dham8h_offset_1
		add.w	d1,d2
		move.w	d2,2+dham8_offset_2
		move.w	d2,2+dham8h_offset_2
		add.w	d1,d2
		move.w	d2,2+dham8_offset_3
		move.w	d2,2+dham8h_offset_3
		add.w	d1,d2
		move.w	d2,2+dham8_offset_4
		move.w	d2,2+dham8h_offset_4
		add.w	d1,d2
		move.w	d2,2+dham8_offset_5
		move.w	d2,2+dham8h_offset_5
		CALLEXEC CacheClearU

		move.l	GfxMemBase,a1			;Setup HAM control bits!
		move.l	AGAPlaneSize(pc),d2
		mulu.l	#6,d2
		move.l	AGAPlaneSize(pc),d3
		mulu.l	#7,d3
		move.l	AGAScreenHeight(pc),d1		;Set Y count value
.cloopy	move.l	AGAScreenWidth(pc),d0
		lsr.l	#5,d0				;Set X count value
.cloopx	move.l	#$DDDDDDDD,(a1,d2.l)		;Write Data To Plane #6
		move.l	#$77777777,(a1,d3.l)		;Write Data To Plane #7
		addq.l	#4,a1				;Increase a1 by 1 word (4 bytes)
		subq.l	#1,d0				;Decrease X count...
		bne.b	.cloopx				;If X=0 branch to .cloopx
		move.l	AGAScreenWidth(pc),d4
		add.l	AGAScreenWidth(pc),a1
		lsr.l	#3,d4
		sub.l	d4,a1
		sub.l	#1,d1				;Decrease Y count
		bne	.cloopy
		rts


DHAM6DitherInit	move.l	AGAPlaneSize(pc),d1
		move.l	d1,d2
		move.l	C2P_x_loop(pc),d3
		lsl.l	#2,d3			;no. of bytes in loop
		sub.l	d3,d2
		mulu.w	#5,d1
		add.l	d2,d1
		move.l	d1,C2P_EOL_skip

		move.l	AGAPlaneSize,d1
		move.w	d1,d2
		move.w	d2,2+dham6_offset_1
		move.w	d2,2+dham6h_offset_1
		add.w	d1,d2
		move.w	d2,2+dham6_offset_2
		move.w	d2,2+dham6h_offset_2
		add.w	d1,d2
		move.w	d2,2+dham6_offset_3
		move.w	d2,2+dham6h_offset_3
		CALLEXEC CacheClearU

		move.l	GfxMemBase,a1			;Setup HAM control bits!
		move.l	AGAPlaneSize(pc),d2
		mulu.l	#4,d2
		move.l	AGAPlaneSize(pc),d3
		mulu.l	#5,d3
		move.l	AGAScreenHeight(pc),d1		;Set Y count value
.cloopy	move.l	AGAScreenWidth(pc),d0
		lsr.l	#5,d0				;Set X count value
.cloopx	move.l	#$DDDDDDDD,(a1,d2.l)		;Write Data To Plane #6
		move.l	#$77777777,(a1,d3.l)		;Write Data To Plane #7
		addq.l	#4,a1				;Increase a1 by 1 word (4 bytes)
		subq.l	#1,d0				;Decrease X count...
		bne.b	.cloopx				;If X=0 branch to .cloopx
		move.l	AGAPlaneSize(pc),d4
		lsl.l	#2,d4
		add.l	d4,a1
		add.l	AGAPlaneSize(pc),a1
		subq.l	#1,d1				;Decrease Y count
		bne	.cloopy
		rts



