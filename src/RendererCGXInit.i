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
* Date:    $Date: 2017-02-13 11:37:09 +0200 (Mo, 13 Feb 2017) $             *
* Authors: Stephen Fellner                                                   *
******************************************************************************
* Cybergraphx setup / shutdown code                                          *
******************************************************************************

        ; CGX specific data
cgxbase:		dc.l	0
cgxvideobase:		dc.l	0
CGXScreenHandle:        dc.l    0
cgxWinPlayBitMap:       dc.l    0
	
cgxScreenTags8:		dc.l	SA_Colors32,Palette
			dc.l	TAG_MORE,cgxScreenTags

cgxScreenTags:		dc.l	SA_Quiet,1
			dc.l	SA_ShowTitle,0
			dc.l	SA_AutoScroll,1
			dc.l	SA_Title,ScreenTitle
			dc.l	SA_Behind,0
			dc.l	SA_Left
cgxScreenPosX:		dc.l	0
			dc.l	SA_Top
cgxScreenPosY:		dc.l	0
			dc.l	SA_Depth
			dc.l	8
			dc.l	SA_Width
cgxScreenWidth:		dc.l	320
			dc.l	SA_Height
cgxScreenHeight:	dc.l	240
			dc.l	SA_Overscan,OSCAN_TEXT
			dc.l	SA_DisplayID
cgxScreenModeID:	dc.l	0
			dc.l	TAG_END	

cgxBIDTags:
			dc.l	CYBRBIDTG_Depth
cgxBIDDepth:		dc.l	0
			dc.l	CYBRBIDTG_NominalWidth
cgxBIDWidth:		dc.l	0
			dc.l	CYBRBIDTG_NominalHeight
cgxBIDHeight:		dc.l	0
			dc.l	TAG_END

cgxlocktags:		dc.l	LBMI_BASEADDRESS
			dc.l	GfxMemBase
			dc.l	LBMI_BYTESPERROW
			dc.l	BitmapModulo
			dc.l	TAG_END


VLayerTags:	dc.l	VOA_SrcType
overlayformat:	dc.l	SRCFMT_YCbCr16
		dc.l	VOA_SrcWidth
PIPSourceWidthcgx:
		dc.l	0
		dc.l	VOA_SrcHeight
PIPSourceHeightcgx:
		dc.l	0
		dc.l	VOA_UseColorKey
usecolorkey1:	dc.l	0
		dc.l	VOA_UseBackfill
usecolorkey2:	dc.l	0
		dc.l	TAG_END

AttachVLayerTags:
		dc.l	VOA_LeftIndent,0
		dc.l	VOA_RightIndent,0
		dc.l	VOA_TopIndent,0
		dc.l	VOA_BottomIndent
bottomindent:	dc.l	0
		dc.l	TAG_END


vlayerhandle:	dc.l	0
vlayerresult:	dc.l	0
vlayerresult2:	dc.l	0

WinhandleCGXPIP:
		dc.l	0

wbwindowtagspip:
		dc.l	WA_InnerWidth
PIPWinWidthcgx:
		dc.l	0
		dc.l	WA_InnerHeight
PIPWinHeightcgx:
		dc.l	0

		dc.l	WA_MaxWidth,16383
		dc.l	WA_MaxHeight,16383

;		dc.l	WA_IDCMP,IDCMP_CLOSEWINDOW+IDCMP_RAWKEY+IDCMP_GADGETUP+IDCMP_GADGETDOWN+IDCMP_MOUSEMOVE
		dc.l	WA_IDCMP,IDCMP_CLOSEWINDOW+IDCMP_RAWKEY+IDCMP_GADGETUP+IDCMP_GADGETDOWN+IDCMP_MOUSEMOVE+IDCMP_CHANGEWINDOW+IDCMP_REFRESHWINDOW
		dc.l	TAG_MORE,CommonPIPTags



*======================================================*
*======== Cybergraphics Initalisation Routines ========*
*======================================================*
Init_cgx		moveq	#0,d0
			lea	cybergraphics_name(pc),a1
			CALLEXEC OpenLibrary			;Attempt to open cgx
			move.l	d0,cgxbase
			beq.w	Init_cgx_Error

			move.l	width(pc),d1			;minimum allowed display size = 320x240
			cmp.l	#320,d1
			bge.b	cgxBIDWidthOK
			move.l	#320,d1
cgxBIDWidthOK		move.l	d1,cgxBIDWidth
			move.l	height(pc),d1
			cmp.l	#240,d1
			bge.b	cgxBIDHeightOK
			move.l	#240,d1
cgxBIDHeightOK		move.l	d1,cgxBIDHeight

			move.b	DitherMode,d1
			beq	Chk_cgx_NoDither
			cmp.b	#DM_PIP,d1
			beq	Chk_cgx_PIP
			cmp.b	#DM_WINDOW,d1
			beq	Chk_cgx_window
			cmp.b	#DM_TRUECOLOR,d1
			beq	Chk_cgx_bgr24
			cmp.b	#DM_HICOLOR,d1
			beq	Chk_cgx_rgbhicolor

			cmp.b	#DM_GRAY,d1
			beq	Chk_cgx_gray
			bra	Chk_cgx_NoDither

Chk_cgx_PIP		bsr	Init_cgx_PIP
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Try_cgx_window

Chk_cgx_window		bsr	Init_cgx_window
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Try_cgx_PIP

Chk_cgx_bgr24		bsr	Init_cgx_bgr24			;NOTE: bgr24 and argb32 are checked together...
			tst.l	d0
			bne	Init_cgx_OK

Chk_cgx_argb32		bsr	Init_cgx_argb32
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Try_cgx_bgr24

Chk_cgx_bgra32		bsr	Init_cgx_bgra32
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Try_cgx_bgr24

Chk_cgx_rgbhicolor	bsr	Init_cgx_rgbhicolor
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Try_cgx_bgr24


Chk_cgx_gray		bsr	Init_cgx_gray
			tst.l	d0
			bne	Init_cgx_OK
			bsr	DisplayModeReject
			bra	Fail_cgx_Dither

Chk_cgx_NoDither

Try_cgx_PIP		bsr	Init_cgx_PIP
			tst.l	d0
			bne.b	Init_cgx_OK

Try_cgx_window		bsr	Init_cgx_window
			tst.l	d0
			bne.b	Init_cgx_OK

Try_cgx_bgr24		bsr	Init_cgx_bgr24
			tst.l	d0
			bne.b	Init_cgx_OK

Try_cgx_argb32		bsr	Init_cgx_argb32
			tst.l	d0
			bne.b	Init_cgx_OK

Try_cgx_bgra32		bsr	Init_cgx_bgra32
			tst.l	d0
			bne.b	Init_cgx_OK

Try_cgx_rgbhicolor	bsr	Init_cgx_rgbhicolor
			tst.l	d0
			bne.b	Init_cgx_OK


Try_cgx_gray		bsr	Init_cgx_gray
			tst.l	d0
			bne.b	Init_cgx_OK

Fail_cgx_Dither

Init_cgx_Error		CALLEXEC CacheClearU
			moveq	#0,d0
			bra.b	Init_cgx_Done

Init_cgx_OK		CALLEXEC CacheClearU
			moveq	#1,d0

Init_cgx_Done		rts


			;---------------------;
			; CGFX - PIP Init     ;
			;---------------------;
Init_cgx_PIP		bsr	Open_cgx_PIP					;Attempt to Open cgx PIP window
			tst.l	d0
			beq.b	Init_cgx_PIP_Error				;can't open -> error
			lea	mpr_jsr_offsets(pc),a1				;open ok -> cleanup & exit
			move.l	#mpr_YUV422,mpr_RenderBitMap(a1)
			move.l	#mpr_cgxLockBitMapPIP,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMapPIP,mpr_UnLockBitMap(a1)
			move.b	#DM_PIP,DitherMode
			clr.b	GrayMode
Init_cgx_PIP_OK		moveq	#1,d0
			rts
Init_cgx_PIP_Error	moveq	#0,d0
			rts

			;---------------------;
			; CGFX - WINDOW Init  ;
			;---------------------;
Init_cgx_window		bsr	Open_cgx_Window					;Attempt to Open cgx window
			tst.l	d0
			beq.b	Init_cgx_window_Error				;can't open -> error
			lea	mpr_jsr_offsets(pc),a1				;open ok -> cleanup & exit
			move.l	#mpr_cgxLockBitMapWin,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMapWin,mpr_UnLockBitMap(a1)
			move.b	#DM_WINDOW,DitherMode
			clr.b	GrayMode
Init_cgx_window_OK	moveq	#1,d0
			rts
Init_cgx_window_Error	moveq	#0,d0
			rts

			;---------------------;
			; CGFX - BGR24 Init   ;
			;---------------------;
Init_cgx_bgr24		moveq	#24,d1
			move.l	#PIXFMT_BGR24,d2
			bsr	Init_cgx_BestMode		;get best mode for specified pixel format and depth
			tst.l	d0
			beq.b	Init_cgx_bgr24_Error
			move.l	d0,cgxScreenModeID
			bsr	Open_cgx_Screen
			tst.l	d0
			beq.b	Init_cgx_bgr24_Error
			bsr	DitherInit_bgr24
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_bgr24,mpr_RenderBitMap(a1)
			move.l	#mpr_cgxLockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
Init_cgx_bgr24_OK	moveq	#1,d0
			rts
Init_cgx_bgr24_Error	moveq	#0,d0
			rts

			;---------------------;
			; CGFX - ARGB32 Init  ;
			;---------------------;
Init_cgx_argb32		moveq	#32,d1
			move.l	#PIXFMT_ARGB32,d2
			bsr	Init_cgx_BestMode
			tst.l	d0
			beq.b	Init_cgx_argb32_Error
			move.l	d0,cgxScreenModeID
			bsr	Open_cgx_Screen
			tst.l	d0
			beq.b	Init_cgx_argb32_Error
			bsr	DitherInit_argb32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_argb32,mpr_RenderBitMap(a1)
			move.l	#mpr_cgxLockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
Init_cgx_argb32_OK	moveq	#1,d0
			rts
Init_cgx_argb32_Error	moveq	#0,d0
			rts

			;---------------------;
			; CGFX - BGRA32 Init  ;
			;---------------------;
Init_cgx_bgra32		moveq	#32,d1
			move.l	#PIXFMT_BGRA32,d2
			bsr	Init_cgx_BestMode
			tst.l	d0
			beq.b	Init_cgx_bgra32_Error
			move.l	d0,cgxScreenModeID
			bsr	Open_cgx_Screen
			tst.l	d0
			beq.b	Init_cgx_bgra32_Error
			bsr	DitherInit_bgra32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_bgra32,mpr_RenderBitMap(a1)
			move.l	#mpr_cgxLockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
Init_cgx_bgra32_OK	moveq	#1,d0
			rts
Init_cgx_bgra32_Error	moveq	#0,d0
			rts

			;------------------------;
			; CGFX - RGBHICOLOR Init ;
			;------------------------;
Init_cgx_rgbhicolor	moveq	#16,d1
			move.l	#PIXFMT_RGB16PC,d2
			move.b	#$00,hicolorformat		;rgb16pC
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			moveq	#16,d1
			move.l	#PIXFMT_RGB16,d2
			move.b	#$01,hicolorformat		;rgb16
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			moveq	#15,d1
			move.l	#PIXFMT_RGB15PC,d2
			move.b	#$02,hicolorformat		;rgb15PC
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			moveq	#15,d1
			move.l	#PIXFMT_RGB15,d2
			move.b	#$03,hicolorformat		;rgb15
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			moveq	#16,d1
			move.l	#PIXFMT_BGR16PC,d2
			move.b	#$04,hicolorformat		;bgr16pC
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			moveq	#15,d1
			move.l	#PIXFMT_BGR15PC,d2
			move.b	#$05,hicolorformat		;bgr15pC
			bsr	Init_cgx_BestMode
			tst.l	d0
			bne.b	Init_cgx_rgbhicolor_main

			bra.b	Init_cgx_rgbhicolor_Error	;not supported hicolor format

Init_cgx_rgbhicolor_main:
			move.l	d0,cgxScreenModeID
			bsr	Open_cgx_Screen
			tst.l	d0
			beq.b	Init_cgx_rgbhicolor_Error
			bsr	DitherInit_rgbhicolor
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_rgbhicolor,mpr_RenderBitMap(a1)
			move.l	#mpr_cgxLockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_cgxUnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_HICOLOR,DitherMode
			clr.b	GrayMode
Init_cgx_rgbhicolor_OK	moveq	#1,d0
			rts
Init_cgx_rgbhicolor_Error
			moveq	#0,d0
			rts

			;---------------------;
			; CGFX - GRAY Init    ;
			;---------------------;
Init_cgx_gray		moveq	#8,d1
			move.l	#PIXFMT_LUT8,d2
			bsr	Init_cgx_BestMode
			tst.l	d0
			beq.b	Init_cgx_gray_Error
			move.l	d0,cgxScreenModeID
			bsr	Open_cgx_8bit
			tst.l	d0
			beq.b	Init_cgx_gray_Error
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_gray,mpr_RenderBitMap(a1)
			move.l	#mpr_DummyRTS,mpr_LockBitMap(a1)
			move.l	#mpr_DummyRTS,mpr_UnLockBitMap(a1)
			move.b	#DM_GRAY,DitherMode
			st	GrayMode
Init_cgx_gray_OK	moveq	#1,d0
			rts
Init_cgx_gray_Error	moveq	#0,d0
			rts

Init_cgx_BestMode	move.l	d1,cgxBIDDepth		;d1 - required depth
			lea	cgxBIDTags,a0
			CALLCGX BestCModeIDTagList
			cmp.l	#INVALID_ID,d0
			beq.b	Init_cgx_BM_Error

			move.l	d0,d7			;d7 - backup of ID
			move.l	d0,d1
			move.l	#CYBRIDATTR_PIXFMT,d0
			CALLCGX GetCyberIDAttr
			cmp.l	d2,d0			;check if required format!
			bne.b	Init_cgx_BM_Error

			move.l	d7,d0			;if correct ID, return it!
			rts

Init_cgx_BM_Error	moveq	#0,d0
			rts


*=======================================================*
*======= CyberGfx Window Initialisation Routines =======*
*=======================================================*
Open_cgx_Window
			move.l	Pubscr(pc),a0
			CALLINT2	LockPubScreen
			move.l	d0,PubScreen
			beq	Open_cgx_Window_Error

			sub.l	a0,a0
			move.l	PubScreen,a1
			CALLINT2	UnlockPubScreen

			move.l	PubScreen,a1
			lea	sc_BitMap(a1),a0
			move.l	a0,a5
			move.l	#CYBRMATTR_DEPTH,d0
			CALLCGX	GetCyberMapAttr
			move.l	d0,PubScreenDepth
			move.l	d0,d5
			cmp.l	#15,d5
			blt	Open_cgx_Window_Error			;screen bitmap less than 16bit -> error

			move.l	a5,a0
			move.l	#CYBRMATTR_PIXFMT,d0
			CALLCGX	GetCyberMapAttr				;check colour format of screen bitmap
			move.l	d0,PubScreenColorFmt

			cmp.l	#24,d5
			blt.b	.hicolor

			;truecolor screen
			cmp.b	#PIXFMT_BGR24,d0
			bne.b	.notBGR24
			bsr	Open_cgx_Window_Bitmap
			tst.l	d0
			beq	Open_cgx_Window_Error			;can't allocate bitmap -> error
			bsr	DitherInit_bgr24
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_bgr24,mpr_RenderBitMap(a1)		;BGR24
			bra	.color_format_ok

.notBGR24		cmp.b	#PIXFMT_ARGB32,d0
			bne.b	.notARGB32
			bsr	Open_cgx_Window_Bitmap
			tst.l	d0
			beq	Open_cgx_Window_Error			;can't allocate bitmap -> error
			bsr	DitherInit_argb32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_argb32,mpr_RenderBitMap(a1)	;ARGB32
			bra	.color_format_ok

.notARGB32		cmp.b	#PIXFMT_BGRA32,d0
			bne.b	.notBGRA32
			bsr	Open_cgx_Window_Bitmap
			tst.l	d0
			beq	Open_cgx_Window_Error			;can't allocate bitmap -> error
			bsr	DitherInit_bgra32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_bgra32,mpr_RenderBitMap(a1)	;ARGB32
			bra	.color_format_ok

.notBGRA32		bra	Open_cgx_Window_Error			;unsupported truecolor format -> error


.hicolor		;hicolor screen
			cmp.b	#PIXFMT_RGB16PC,d0
			bne.b	.notRGB16PC
			move.b	#$00,hicolorformat
			bra.b	.hicolor_ok
.notRGB16PC		cmp.b	#PIXFMT_RGB16,d0
			bne.b	.notRGB16
			move.b	#$01,hicolorformat
			bra.b	.hicolor_ok
.notRGB16		cmp.b	#PIXFMT_RGB15PC,d0
			bne.b	.notRGB15PC
			move.b	#$02,hicolorformat
			bra.b	.hicolor_ok
.notRGB15PC		cmp.b	#PIXFMT_RGB15,d0
			bne.b	.notRGB15
			move.b	#$03,hicolorformat
			bra.b	.hicolor_ok
.notRGB15		cmp.b	#PIXFMT_BGR16PC,d0
			bne.b	.notBGR16PC
			move.b	#$04,hicolorformat
			bra.b	.hicolor_ok
.notBGR16PC		cmp.b	#PIXFMT_BGR15PC,d0
			bne.b	.notBGR15PC
			move.b	#$05,hicolorformat
			bra.b	.hicolor_ok
.notBGR15PC		bra.b	Open_cgx_Window_Error			;unsupported hicolor format -> error

.hicolor_ok
			bsr	Open_cgx_Window_Bitmap
			tst.l	d0
			beq.b	Open_cgx_Window_Error			;can't allocate bitmap -> error

			bsr	DitherInit_rgbhicolor
			tst.l	d0
			beq.b	Open_cgx_Window_Error			;can't initialise hicolor render -> error
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_rgbhicolor,mpr_RenderBitMap(a1)
.color_format_ok
			move.l	width(pc),d1
			move.l	d1,WinWidth
			move.l	d1,PIPWinWidth			;needed for centerwinbeforeopen
			move.l	height(pc),d1
			move.l	d1,WinHeight
			move.l	d1,PIPWinHeight			;needed for centerwinbeforeopen

			bsr	centerwinbeforeopen

			bsr	setwindowfilename

			suba.l	a0,a0
			lea	WindowTags(pc),a1
			CALLINT2	OpenWindowTagList
			move.l	d0,MainWindow
			beq.b	Open_cgx_Window_Error

			;window open, return winhandle

			rts


Open_cgx_Window_Error
			move.l	cgxWinPlayBitMap(pc),a0
			tst.l	a0
			beq.b	.NoWinBitmap
			CALLGFX	FreeBitMap
			clr.l	cgxWinPlayBitMap
.NoWinBitmap
			moveq	#0,d0
			rts

Open_cgx_Window_Bitmap
			move.l	width(pc),d0			;width
			move.l	height(pc),d1			;height
			move.l	PubScreenDepth,d2		;depth
			move.l	PubScreenColorFmt,d3
			moveq	#24,d7
			lsl.l	d7,d3
			or.b	#BMF_DISPLAYABLE|BMF_INTERLEAVED|BMF_MINPLANES|BMF_SPECIALFMT,d3
			move.l	PubScreen,a0
			lea	sc_BitMap(a0),a0		;friend bitmap
			CALLGFX	AllocBitMap
			move.l	d0,cgxWinPlayBitMap
			beq.b	.BitmapOpenDone

			bsr	mpr_cgxLockBitMapWin		;get info on bitmap (base, modulo, etc.)
			move.l	bitlock,a0
			CALLCGX	UnLockBitMap

			move.l	cgxWinPlayBitMap(pc),d0
.BitmapOpenDone
			rts


*=======================================================*
*====== CyberGfx Screen Initialisation Routines =======*
*=======================================================*
Open_cgx_Screen		lea	cgxScreenTags(pc),a1
			bra.b	Open_cgx_Start

Open_cgx_8bit		lea	cgxScreenTags8(pc),a1

Open_cgx_Start		move.l	width(pc),cgxScreenWidth
			move.l	width(pc),ScreenWidth
			move.l	height(pc),cgxScreenHeight
			move.l	height(pc),ScreenHeight

			suba.l	a0,a0
			CALLINT2	OpenScreenTagList
			move.l	d0,ScreenHandle
			beq.b	Open_cgx_Screen_Done

			move.l	ScreenHandle(pc),a1
			lea	sc_RastPort(a1),a1
			move.l	a1,ScreenRastport

			bsr	mpr_cgxLockBitMap		;get info on bitmap (base, modulo, etc.)
			bsr	mpr_cgxUnLockBitMap

Open_cgx_Screen_Done	move.l	ScreenHandle,d0
			move.l	d0,CGXScreenHandle
			rts

*=======================================================*
*======= CyberCrapX PIP Initialisation Routines ========*
*=======================================================*
Open_cgx_PIP		
			move.l	width(pc),d1
			move.l	d1,PIPSourceWidth		;a wincenter miatt kell ez!
			move.l	d1,PIPSourceWidthcgx
			mulu.l	ZOOM_value,d1
			divu.l	#100,d1
			move.l	d1,PIPWinWidth
			move.l	d1,PIPWinWidthcgx

			move.l	height(pc),d1
			move.l	d1,PIPSourceHeight
			move.l	d1,PIPSourceHeightcgx
			mulu.l	ZOOM_value,d1
			divu.l	#100,d1
			move.l	d1,PIPWinHeight
			move.l	d1,PIPWinHeightcgx

			move.l	intbase,a6
			move.l	Pubscr(pc),a0
			jsr	_LVOLockPubScreen(a6)
			move.l	d0,PubScreen
			beq.w	lockbugcgxpip

			bsr.w	centerwinbeforeopen

			move.l	intbase,a6
			sub.l	a0,a0
			move.l	PubScreen,a1
			jsr	_LVOUnlockPubScreen(a6)

lockbugcgxpip:		
			bsr.w	setwindowfilename

			lea	cgxvideolib_name(pc),a1
			moveq	#0,d0
			CALLEXEC OpenLibrary
			move.l	d0,cgxvideobase
			beq.w	Open_cgx_PIP_Error

			move.l	cgxvideobase(pc),a6
			move.l	PubScreen,a0		;wb screen
			lea	VLayerTags(pc),a1
			jsr	_LVOCreateVLayerHandleTagList(a6)
			move.l	d0,vlayerhandle
			beq.w	Open_cgx_PIP_Error

			suba.l	a0,a0
			move.l	intbase,a6
			lea	wbwindowtagspip(pc),a1
			jsr	_LVOOpenWindowTagList(a6)
			move.l	d0,WinhandleCGXPIP
			beq.b	Open_cgx_PIP_Error
			move.l	d0,MainWindow

			clr.l	vlayerresult2

			move.l	cgxvideobase(pc),a6
			move.l	vlayerhandle(pc),a0
			move.l	WinhandleCGXPIP,a1
			lea	AttachVLayerTags(pc),a2
			jsr	_LVOAttachVLayerTagList(a6)
			move.l	d0,vlayerresult			;ha 0 akkor sikerult
			bne.b	Open_cgx_PIP_Error

			st	vlayerresult2
			
			bsr.w	mpr_cgxLockBitMapPIP
			bsr.w	mpr_cgxUnLockBitMapPIP

Open_cgx_PIP_OK		CALLEXEC CacheClearU
			moveq	#1,d0
			bra.w	Open_cgx_PIP_Done

Open_cgx_PIP_Error
			tst.l	cgxvideobase(pc)
			beq	NocgxPIP2

			tst.l	vlayerresult2
			beq.b	nemvoltlayeratt2

			move.l	vlayerhandle(pc),a0
			move.l	cgxvideobase(pc),a6
			jsr	_LVODetachVLayer(a6)

			clr.l	vlayerresult2

nemvoltlayeratt2:
			move.l	cgxvideobase(pc),a6
			move.l	vlayerhandle(pc),a0
			tst.l	a0
			beq.b	nemvoltlayer2
			jsr	_LVODeleteVLayerHandle(a6)
			clr.l	vlayerhandle

nemvoltlayer2:		move.l	intbase,a6
			move.l	WinhandleCGXPIP,a0
			tst.l	a0
			beq.b	nemvoltwin2
			jsr	_LVOCloseWindow(a6)

			clr.l	MainWindow
			clr.l	WinhandleCGXPIP			;always clear handles/pointers when closing objects!
		
nemvoltwin2:		move.l	(4).w,a6
			move.l	cgxvideobase(pc),a1
			tst.l	a1
			beq.b	noclosecgxv2
			jsr	_LVOCloseLibrary(a6)
noclosecgxv2:
			clr.l	cgxvideobase
NocgxPIP2:
			CALLEXEC CacheClearU
			moveq	#0,d0

Open_cgx_PIP_Done	rts


mpr_cgxLockBitMapWin:
		move.l	cgxWinPlayBitMap(pc),a0
		lea	cgxlocktags(pc),a1
		CALLCGX	LockBitMapTagList
		move.l	d0,bitlock
		rts

*		ALIGN	0,16
		CNOP    0,16

mpr_cgxUnLockBitMapWin
		move.l	bitlock,a0
		CALLCGX	UnLockBitMap
		move.l	cgxWinPlayBitMap(pc),a0	;src bitmap
		moveq	#0,d0			;src x
		moveq	#0,d1			;src y
		move.l	MainWindow,a2
		move.l	wd_RPort(a2),a1		;dest rastport
		moveq	#0,d2			;dest x
		move.b	wd_BorderLeft(a2),d2
		moveq	#0,d3			;dest y
		move.b	wd_BorderTop(a2),d3
		move.l	width(pc),d4		;size x
		move.l	height(pc),d5		;size y
		move.b	#$C0,d6			;minterm
		CALLGFX	BltBitMapRastPort	;blit image into window
		rts

*		ALIGN	0,16
		CNOP    0,16

mpr_cgxLockBitMap:
		move.l	cgxbase,a6
		move.l	ScreenRastport,a0
		move.l	rp_BitMap(a0),a0
		lea	cgxlocktags(pc),a1
		jsr	_LVOLockBitMapTagList(a6)
		move.l	d0,bitlock
		rts

*		ALIGN	0,16
		CNOP    0,16

mpr_cgxUnLockBitMap:
		move.l	cgxbase,a6
		move.l	bitlock,a0
		jmp	_LVOUnLockBitMap(a6)

*		ALIGN	0,16
		CNOP    0,16

mpr_cgxLockBitMapPIP:
		move.l	cgxvideobase(pc),a6
		move.l	vlayerhandle(pc),a0
		jsr	_LVOLockVLayer(a6)

		move.l	cgxvideobase(pc),a6
		move.l	vlayerhandle(pc),a0
		move.l	#VOA_BaseAddress,d0		;base
		jsr	_LVOGetVLayerAttr(a6)
		move.l	d0,GfxMemBase
		move.l	width,d0
		asl.l	#1,d0
		move.l	d0,BitmapModulo			;modulo.... (cgx sux!)
		rts

*		ALIGN	0,16
		CNOP    0,16

mpr_cgxUnLockBitMapPIP:
		move.l	cgxvideobase(pc),a6
		move.l	vlayerhandle(pc),a0
		jmp	_LVOUnlockVLayer(a6)
		
*		ALIGN	0,16
		CNOP    0,16

CloseCGXPIP:
		tst.l	cgxvideobase(pc)
		beq	.NocgxPIP

		tst.l	vlayerresult2
		beq.b	.nemvoltlayeratt

		move.l	vlayerhandle(pc),a0
		move.l	cgxvideobase(pc),a6
		jsr	_LVODetachVLayer(a6)

		clr.l	vlayerresult2

.nemvoltlayeratt:
		move.l	cgxvideobase(pc),a6
		move.l	vlayerhandle(pc),a0
		tst.l	a0
		beq.b	.nemvoltlayer
		jsr	_LVODeleteVLayerHandle(a6)
		clr.l	vlayerhandle

.nemvoltlayer:	move.l	intbase,a6
		move.l	WinhandleCGXPIP,a0
		tst.l	a0
		beq.b	.nemvoltwin
		jsr	_LVOCloseWindow(a6)

		clr.l	MainWindow
		clr.l	WinhandleCGXPIP			;always clear handles/pointers when closing objects!
		
.nemvoltwin:	move.l	(4).w,a6
		move.l	cgxvideobase(pc),a1
		tst.l	a1
		beq.b	.noclosecgxv
		jsr	_LVOCloseLibrary(a6)
.noclosecgxv:
		clr.l	cgxvideobase
		bra	CloseWindowsDone
.NocgxPIP:
                rts

CloseCGXScreen:
		tst.l	CGXScreenHandle(pc)
		beq	.noCGXscreen
		move.l	CGXScreenHandle(pc),a0
		CALLINT2	CloseScreen
		clr.l	CGXScreenHandle
		clr.l   ScreenHandle
.noCGXscreen	
                rts
