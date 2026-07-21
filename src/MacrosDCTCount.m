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
* IDCT Count Macros for testing / debugging                                  *
******************************************************************************

; requires external definition of IDCT_COUNT - 0 for disabled or 1 enabled

	ifne	IDCT_COUNT
IDCT_COUNT0		macro
			addq.l	#1,(idct_counters)
			endm
IDCT_COUNT1		macro
			addq.l	#1,(idct_counters+4)
			endm
IDCT_COUNT2		macro
			addq.l	#1,(idct_counters+8)
			endm
IDCT_COUNT3		macro
			addq.l	#1,(idct_counters+12)
			endm
IDCT_COUNT4		macro
			addq.l	#1,(idct_counters+16)
			endm
IDCT_COUNT5		macro
			addq.l	#1,(idct_counters+20)
			endm
IDCT_COUNT6		macro
			addq.l	#1,(idct_counters+24)
			endm
IDCT_COUNT7		macro
			addq.l	#1,(idct_counters+28)
			endm
	else
IDCT_COUNT0		macro
			endm
IDCT_COUNT1		macro
			endm
IDCT_COUNT2		macro
			endm
IDCT_COUNT3		macro
			endm
IDCT_COUNT4		macro
			endm
IDCT_COUNT5		macro
			endm
IDCT_COUNT6		macro
			endm
IDCT_COUNT7		macro
			endm
	endc


	ifne	IDCT_COUNT
DCT_COUNTERS	macro
idct_counters:		dc.l	0,0,0,0,0,0,0,0
		endm
	else
DCT_COUNTERS	macro
		endm
	endc


