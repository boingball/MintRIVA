* YUV source formats for p96WriteYUVPixels()
	ENUM	0
	EITEM	YUVFB_NONE	; No valid YUV format (should not happen)
	EITEM	YUVFB_YUV12	; One byte luminance (Y) for each pixel,
				; squares of 2x2 pixels sharing one byte
				; of each chrominance (U,V).
	EITEM	YUVFB_YUV9	; One byte luminance (Y) for each pixel,
				; squares of 4x4 pixels sharing one byte
				; of each chrominance (U,V).
	EITEM	YUVFB_Y8	; One byte luminance (Y) only


	EITEM	YUVFB_MaxFormats

YUVFF_NONE		EQU	(1<<YUVFB_NONE)
YUVFF_YUV12		EQU	(1<<YUVFB_YUV12)
YUVFF_YUV9		EQU	(1<<YUVFB_YUV9)
YUVFF_Y8		EQU	(1<<YUVFB_Y8)

	IFND	_LVOp96WriteYUVPixels
_LVOp96WriteYUVPixels	equ	-$c6
	ENDC


	IFND	CRTCI
CRTCI	EQU	$3D4
CRTCD	EQU	$3D5
CRTC_MiscellaneousVideoControl	equ	$3f
	ENDC



PA		EQU	1

 STRUCTURE YUVInfo,0
	APTR	yi_MemoryY
	APTR	yi_MemoryU
	APTR	yi_MemoryV
	APTR	yi_MemoryD
	WORD	yi_BytesPerRowY
	WORD	yi_BytesPerRowU
	WORD	yi_BytesPerRowV
	WORD	yi_BytesPerRowD
	ULONG	yi_YUVFormat
	ULONG	yi_Flags
	LABEL	yi_SIZEOF

