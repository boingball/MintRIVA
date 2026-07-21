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
* Motion Compensation support structures/macros, 68k generic                 *
******************************************************************************

; Motion compensation control
;  MC is done in 8x8 blocks, this controls the block location in reference frames and destination,
;  either a temporary buffer (when DCT coeffs are present), or directly to the frame buffer (B skip mode)
;  The first part of this structure is compatible to the RIvA block offset table. The second part controls
;  the pointer advance after one row
;
  STRUCTURE	MC_DATA,0
      SHORT	MC_Y_BLOCKOFF0		;offset in source buffer for first 8x8 block (upper left block )
      SHORT	MC_Y_BLOCKOFF1		;offset in source buffer for 2nd 8x8 block   (upper right block)
      SHORT	MC_Y_BLOCKOFF2		;offset in source buffer for 3rd 8x8 block   (down left block  )
      SHORT	MC_Y_BLOCKOFF3		;offset in source buffer for 4th 8x8 block   (down right block )

      LABEL	MC_BLOCKOFF2LINESTRIDE	;distance from block offset to line stride
      SHORT	MC_Y_LINESTRIDE0	;offsets in destination buffer after one row of 8 pixels MINUS 8
      SHORT	MC_Y_LINESTRIDE1	;
      SHORT	MC_Y_LINESTRIDE2	;
      SHORT	MC_Y_LINESTRIDE3	;

      LABEL	MC_BLOCKOFF2DESTOFF	;distance from block offset to destination offset
      SHORT	MC_Y_DESTOFF0		;offsets in destination buffer after one block 
      SHORT	MC_Y_DESTOFF1		;(assuming output ptr is beyond the end of the current 8x8 block)
      SHORT	MC_Y_DESTOFF2		;
      SHORT	MC_Y_DESTOFF3		;

      LONG	MC_DestPTR_Y		; destination pointer Y (either framebuffer = direct mode or Macroblock buffer = buffered mode)
      LONG	MC_DestPTR_Cb		; destination pointer U
      LONG	MC_DestPTR_Cr		; destination pointer V
      LONG	MC_SrcPTR_Cb		; source pointer Cb (srcY is passed in register but the other two are stored here)
      LONG	MC_SrcPTR_Cr		; source pointer Cr
      SHORT	MC_C_LINESTRIDE	;
 
      LABEL	MC_DATA_SIZE

;mc_offsets_buffered:		ds.b	MC_DATA_SIZE
;mc_offsets_direct:		ds.b	MC_DATA_SIZE
;see HardcodeIDCTOffsets


	ifne	0

	; incorrect debug variant (like original RiVA): DON'T ENABLE
ADJUST_CVECTOR		macro
	move.l	d3,d1
	move.l	d4,d2
	asr.l	#1,d1
	asr.l	#1,d2
	endm

	else

; Chroma Vector adjustment ( actually, just /2 - the rest is for show )
; In:  D3 - x displacement in halfpel units
;      D4 - y displacement in halfpel units
; Out: D1 - chroma vector x in chroma halfpel units
;      D2 - chroma vector y in chroma halfpel units
; Trash: d0,d5,d6,d7
ADJUST_CVECTOR		macro
	; short path would be divs #2,d1 ; divs #2,d2 after the move(s)
			move.l	d3,d1	;save halfpel luma displacements
			move.l	d4,d2	;vertical displacement
		ifne	0
			divs	#2,d1
			divs	#2,d2
		else
			asr.l	#1,d1	;chroma h halfpel = luma/2
			scs	d6	;catch lowest bit
			slt	d5	;catch positive / negative
			asr.l	#1,d2	;chroma v halfpel = luma/2
			scs	d7
			slt	d0
			and.b	d5,d6	;don't add lowest bit for positive numbers
			and.b	d0,d7
			ext.w	d6	;words
			ext.w	d7
			sub.w	d6,d1	;add + 1 for negative numbers with carry
			sub.w	d7,d2
		endc
			ext.l	d1	;long (for add.l)
			ext.l	d2
			endm
	endc

