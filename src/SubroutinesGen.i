	
*-----------------------------------------------------------------------------------*
*------------------------- Output Text ---------------------------------------------*
*-----------------------------------------------------------------------------------*
*----------------- Input : d2 = String Pointer -------------------------------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputText
	tst.l	StdOut
	beq.b	.nostdout
	movem.l	d0-d3/a0-a6,-(a7)
	move.l	d2,a1
.txtloop
	move.b	(a1)+,d0
	bne.b	.txtloop
	move.l	a1,d3
	exg		d2,d3
	sub.l	d3,d2
	exg		d2,d3		;D3-ban a string hossza...
	subq	#1,d3
	and.l	#$00000fff,d3	;Max. 4096 karakter.
	move.l	StdOut,d1	;D1-ben a stdout...
	move.l	dosbase,a6	;A6-ban a dosbase...
	jsr		_LVOWrite(a6)
	movem.l	(a7)+,d0-d3/a0-a6
.nostdout
	rts

*-----------------------------------------------------------------------------------*
*------------------------- Output 32-bit Unsigned Decimal --------------------------*
*-----------------------------------------------------------------------------------*
*----------------- Input : d1 = Unsigned Decimal number (longword) -----------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputDecimal
	movem.l	d0-d3/a2,-(a7)
	moveq.l	#10,d2				;Offset for string (set to end)
	lea		decstring(pc),a2
	clr.b	11(a2)				;Make string terminate code ($0) at end of string
	move.l	d1,d0				;Make copy of input
.decgenloop
	divu.l	#10,d0				;Divide number by 10
	mulu.l	#10,d0				;and multiply by 10 to round it off.
	move.l	d1,d3
	sub.l	d0,d3				;The difference between the full and the rounded value is out digit.
	add.b	#$30,d3				;Add ASCII offset to number ($30 = '0')
	move.b	d3,(a2,d2)			;Put characters from end of buffer
	divu.l	#10,d0
	subq.l	#1,d2
	beq.b	.decgenend			;get out of loop if no more digits
	move.l	d0,d1
	bne.b	.decgenloop
.decgenend
	addq.l	#1,d2
	add.l	a2,d2
	jsr		OutputText(pc)			;Yipppeeee !!! We can output the string ! ;)))
	movem.l	(a7)+,d0-d3/a2
	rts

decstring
	ds.b	11			;This is where the string is built ;)

*-----------------------------------------------------------------------------------*
*---------------------- Output 32-bit Signed Decimal String ------------------------*
*-----------------------------------------------------------------------------------*
*------------------ Input : d1 = Signed Decimal number (longword) ------------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputSignedDecimal
	movem.l	d0-d3/a2,-(a7)
	clr.b	decsign				;Clear -ve flag
	btst	#31,d1				;Test sign bit
	beq.b	.signeddecgenstart		;Start decimal generation if +ve
	not.b	decsign				;Set -ve flag.
	neg.l	d1				;Convert Signed value to Unsigned.
.signeddecgenstart
	moveq	#10,d2				;Offset for string (set to end)
	lea		signeddecstring(pc),a2
	clr.b	11(a2)				;Make string terminate code ($0) at end of string
	move.l	d1,d0				;Make copy of input
.signeddecgenloop
	divu.l	#10,d0				;Divide number by 10
	mulu.l	#10,d0				;and multiply by 10 to round it off.
	move.l	d1,d3
	sub.l	d0,d3				;The difference between the full and the rounded value is out digit.
	add.b	#$30,d3				;Add ASCII offset to number ($30 = '0')
	move.b	d3,(a2,d2)			;Put characters from end of buffer
	subq.l	#1,d2
	divu.l	#10,d0
	beq.b	.signeddecgenend		;get out of loop if no more digits
	move.l	d0,d1
	bne.b	.signeddecgenloop
.signeddecgenend
	tst.b	decsign
	beq.b	.nosign
	move.b	#$2d,(a2,d2)
	subq.l	#1,d2
.nosign
	addq.l	#1,d2
	add.l	a2,d2
	jsr		OutputText(pc)			;Yipppeeee !!! We can output the string ! ;)))
	movem.l	(a7)+,d0-d3/a2
	rts

signeddecstring
	ds.b	12			;This is where the string is built ;)

decsign
	ds.b	1

*-----------------------------------------------------------------------------------*
*------------------------ Output 32-bit Hexadecimal String -------------------------*
*-----------------------------------------------------------------------------------*
*-------------------------- Input : d1 = 32-bit hex data ---------------------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputLongHex
	movem.l	d0-d2/a2,-(a7)
	lea		longhexstring(pc),a2
	clr.b	9(a2)
	moveq.l	#7,d0
.longhexgenloop
	move.b	d1,d2
	and.b	#$0f,d2
	cmp.b	#$0a,d2
	blt.b	.notlonghexletter
	add.b	#$07,d2
.notlonghexletter
	add.b	#$30,d2
	move.b	d2,(a2,d0)
	lsr.l	#4,d1
	dbf		d0,.longhexgenloop
	move.l	a2,d2
	jsr		OutputText(pc)
	movem.l	(a7)+,d0-d2/a2
	rts
	
longhexstring
	ds.b	9

*-----------------------------------------------------------------------------------*
*--------------------------- Output 32-bit Binary String ---------------------------*
*-----------------------------------------------------------------------------------*
*------------------------- Input : d1 = 32-bit binary data -------------------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputLongBinary
	movem.l	d0-d2/a2,-(a7)
	lea		longbinstring(pc),a2
	clr.b	35(a2)
	moveq.l	#31,d0
.bingenloop
	btst	d0,d1
	beq.b	.zerobit
	move.b	#$31,(a2)+
	bra.b	.nextbit
.zerobit
	move.b	#$30,(a2)+
.nextbit
	subq.l	#1,d0
	bmi.b	.endbinloop
	move.w	d0,d2
	addq.l	#1,d2
	lsr.w	#3,d2
	lsl.w	#3,d2
	subq.l	#1,d2
	cmp.w	d0,d2
	bne.b	.nospace
	move.b	#$20,(a2)+
.nospace
	bra.b	.bingenloop
.endbinloop
	move.l	#longbinstring,d2
	jsr		OutputText(pc)
	movem.l	(a7)+,d0-d2/a2
	rts

longbinstring
	ds.b	35

*-----------------------------------------------------------------------------------*
*----------------- Output 32-bit Fraction (1/input) Binary String ------------------*
*-----------------------------------------------------------------------------------*
*--------- Inputs : d1 = 32-bit fraction --- d2 = number of decimal places ---------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputLongFraction
	tst.l	StdOut
	beq.b	frac_err
	movem.l	d0-a6,-(a7)
	lea		1+frac_string(pc),a2
	moveq	#28,d7			;register for right shifting...
	move.l	d2,d6			;loop register
	and.l	#15,d2			;discard any negative values
	cmp.b	#8,d2			;make sure length isn't more than 8
	ble.b	frac_size_ok
	moveq	#8,d2			;if more asked, use maximum
frac_size_ok
	lsr.l	#4,d1
frac_loop
	mulu.l	#10,d1
	move.l	d1,d3			;save this for finding remainder
	lsr.l	d7,d3			;find 1st digit
	move.l	d3,d4
	add.b	#$30,d4
	move.b	d4,(a2)+
	lsl.l	d7,d3			;multiply back to subtract remainder from orig.
	sub.l	d3,d1
	subq.l	#1,d6
	bne.b	frac_loop
	move.l	d2,d3
	addq.l	#1,d3			;length in d3
	move.l	StdOut,d1
	move.l	#frac_string,d2
	move.l	dosbase,a6
	jsr		_LVOWrite(a6)		;print string...
	movem.l	(a7)+,d0-a6
frac_err
	rts

frac_string:
	dc.b	".",0,0,0,0,0,0,0,0,0

*-----------------------------------------------------------------------------------*
*-------------------------- Output Signed Decimal 16 bit ---------------------------*
*---------------- Written By Stephen Fellner (COBRA) on 26-Feb-1998 ----------------*
*- This Subroutine converts a longword (32-bit) operand into a signed decimal ------*
*- null-terminated string and passes the string on to a string output subroutine ---*
*-----------------------------------------------------------------------------------*
*------------------ Input : d1 = Signed Decimal number (longword) ------------------*
*-----------------------------------------------------------------------------------*

	EVEN

OutputSignedDecimal16
	movem.l	d0-d2/d3/a2,-(a7)
	lea		signeddec16string(pc),a2
	move.l	#$20202020,d2
	move.l	d2,(a2)+
	move.l	d2,(a2)+			;fill with spaces
	move.b	#0,dec16sign			;Clear -ve flag
	btst	#15,d1				;Test sign bit
	beq.b	.signeddec16genstart		;Start decimal generation if +ve
	move.b	#1,dec16sign			;Set -ve flag.
	neg.w	d1				;Convert Signed value to Unsigned.
.signeddec16genstart	
	moveq	#8,d2				;Offset for string (set to end)
	lea		signeddec16string(pc),a2
	move.b	#0,(a2,d2)			;Make string terminate code ($0) at end of string
	subq.l	#1,d2
	move.l	d1,d0				;Make copy of input
.signeddec16genloop
	divu.l	#10,d0				;Divide number by 10
	mulu.l	#10,d0				;and multiply by 10 to round it off.
	move.l	d1,d3
	sub.l	d0,d3				;The difference between the full and the rounded value is out digit.
	add.l	#$30,d3				;Add ASCII offset to number ($30 = '0')
	move.b	d3,(a2,d2)			;Put characters from end of buffer
	subq.l	#1,d2
	divu.l	#10,d0
	beq.b	.signeddec16genend		;get out of loop if no more digits
	move.l	d0,d1
	bne.b	.signeddec16genloop
.signeddec16genend
	tst.b	dec16sign
	beq.b	.no16sign
	move.b	#$2d,(a2,d2)
	subq.l	#1,d2
.no16sign
	;addq.l	#1,d2
	;add.l	a2,d2
	move.l	#signeddec16string,d2
	jsr		OutputText(pc)			;Yipppeeee !!! We can output the string ! ;)))
	movem.l	(a7)+,d0-d2/d3/a2
	rts

signeddec16string
	ds.b	15			;This is where the string is built ;)

dec16sign
	ds.b	1
