******************************************************************************
* Name:    RiVA v0.50 Registered                                             *
* Date:    $Date: 2019-07-23 06:22:02 -0359 (Di, 23 Jul 2019) $              * 
* Authors: Stephen Fellner (COBRA) and László Török (pH03N1x)                *
******************************************************************************
*                           RiVA Revision History                            *
*                                                                            *
* 0.1     (18 Jun 1999)                                                      *
*         - First public release                                             *
*                                                                            *
* 0.11    (5 Jul 1999)                                                       *
*         - ADDED FPS limiting code, intelligent frame-skipping, FPS and     *
*           NOSKIP options.                                                  *
*         - Completely redesigned Video Stream parser                        *
*         - Major code structure redesign! (better internal error            *
*           detection)                                                       *
*         - ADDED Automatic screen-centering                                 *
*         - IMPROVED Playback quality (more contrast - as it's supposed      *
*           to be)                                                           *
*         - IMPROVED Looping - now loops properly with as much skipping      *
*           as you like! it skips over end of anim and skips into the        *
*           exact(!) place needed to keep the loop playback constant.        *
*         - IMPROVED Frametime code (optimisations)                          *
*         - ADDED ESC-Quit detect on screens too + fixed PIP close bug.      *
*         - ADDED HiColor dither (faster colour playback)                    *
*         - ADDED DITHER option with help/request (using 'DITHER ?')         *
*         - Complete redesign of DitherMode selection logic                  *
*         - ADDED On-the-fly dither selection!!! (using SPACE bar)           *
*         - IMPROVED CybergraphX support                                     *
*                                                                            *
* 0.12    (8 Jul 1999)                                                       *
*         - PIP bugfix by László Török                                       *
*                                                                            *
* 0.20    (27 Aug 1999)                                                      *
*         - ADDED P frame support                                            *
*         - Major changes in internal code structure                         *
*         - FIXED all known decoder bugs                                     *
*         - FIXED some old bugs which caused incorrect decoding of P frames  *
*           in some very rare MPEG files                                     *
*         - FIXED XING framerate (Was 8 fps, now 15 fps)                     *
*         - Implemented new frame-skipping routines to handle P and B frames *
*                                                                            *
* 0.21    (4 Sep 1999)                                                       *
*         - Fixed bug which caused crashes on some 030-based systems without *
*           a gfxcard                                                        *
*                                                                            *
* 0.30    (2 Apr 2000)                                                       *
*         - OPTIMIZED IDCT routines (up to 15% global speedup)               *
*         - OPTIMIZED B-frame skipping (up to 60% global speedup!)           *
*         - ADDED AsyncIO (smooth real-time load/playback)                   *
*         - Major internal code re-design (for system stream code)           *
*         - ADDED System Stream support (FINALLY !!! :-)                     *
*         - FIXED many major and minor bugs...                               *
*                                                                            *
* 0.33    (4 Dec 2000)                                                       *
*         - ADDED CyberGraphX VLayer support                                 *
*         - ADDED support for more 15/16bit modes                            *
*         - ADDED Experimental PicassoIV PlanarAssist and Accupak options    *
*           (Great speedup, especially on Z2 machines!)                      *
*         - ADDED ZOOM option for PIP/VLayer.                                *
*         - ADDED PIP/VLayer on-the-fly zoom slection (1-6 and +/-)          *
*         - ADDED Colour AGA support (AGA8 and AGA6)                         *
*         - ADDED HALFHEIGHT option for faster AGA playback                  *
*         - ADDED ASL File Requester                                         *
*         - ADDED Workbench start support (no more shell-only usage)         *
*         - ADDED on-the-fly half/full height selection (use SPACE bar)      *
*           (Note this only works in AGA modes!)                             *
*         - ADDED Workbench Argument reading. You can drag'n'drop files onto *
*           the RiVA icon and RiVA will start playing them.                  *
*         - ADDED Workbench tooltype parsing (RiVA can now be configured via *
*           its icon's tooltypes)                                            *
*                                                                            *
* 0.34    (8 Dec 2000)                                                       *
*         - CHANGED AsyncIO code (New AsyncIO rulez!)                        *
*         - Re-implemented LOOP option (with new AsyncIO)                    *
*         - ADDED Seek option (F1-F10 allows seeking into bigger files)      *
*         - FIXED AGA dither selection (now defaults to AGA8)                *
*                                                                            *
* 0.35    (30 Dec 2000)                                                      *
*         - FIXED bug in closedown code (buffers not freed, etc. LAME!)      *
*         - ADDED experimental audio support!!!!                             *
*                                                                            *
* 0.36    (8 Feb 2001)                                                       *
*         - FIXED FPS counter bug introduced in 0.34                         *
*         - FIXED Seek bug in new AsyncIO                                    *
*         - OPTIMIZED IDCT algorithm (a bit more speed... again... :)        *
*         - ADDED NOAUDIO option (to disable experimental audio :)           *
*                                                                            *
* 0.37    (9 Feb 2001)                                                       *
*         - IMPROVED Audio Support (PAL/NTSC autodetect, now supports any    *
*           frequency (ie. 32kHz, 44.1kHz, 48kHz)                            *
*         - FIXED lots of Audio bugs... :)                                   *
*                                                                            *
* 0.38    (10 Feb 2001)                                                      *
*         - Temporarily removed F1-F10 seek and LOOP options (until Audio    *
*           support is finalized... sorry guys :)                            *
*         - Some cleanups (removed some old obsolete stuff...)               *
*         - REMOVED NOIDCT switch (not really needed)                        *
*                                                                            *
* 0.39    (10 Feb 2001)                                                      *
*         - Implemented Audio Scaling (audio is scaled to fit specified      *
*           framerate)                                                       *
*         - ADDED extra key input check (if video can't keep up)             *
*                                                                            *
* 0.40    (15 Feb 2001)                                                      *
*         - ADDED NOVIDEO and SAVEAUDIO options                              *
*         - IMPROVED Audio support (uses both left & right channels)         *
*         - FIXED a bug introduced in 0.39 which caused crashes on streams   *
*           with no audio                                                    *
*         - ADDED MONOSURROUND option                                        *
*         - Updated icon tooltype parsing                                    *
*         - Changed 'DITHER' keyword to 'DISPLAY' in tooltype (both can be   *
*           used as command-line argument)                                   *
*         - FIXED VLayer ZOOM on Cgfx                                        *
*         - CHANGED names of AGA6 and AGA8 display modes to DHAM6 and DHAM8  *
*         - FIXED videodecoder bug (did not exit in certain cases!)          *
*                                                                            *
* 0.41    (22 Feb 2001)                                                      *
*         - FIXED Enforcer Hits                                              *
*         - ADDED PUBSCREEN option                                           *
*                                                                            *
* 0.42    (27 Mar 2001)                                                      *
*         - ADDED B frame support                                            *
*         - ADDED NOP and NOB options                                        *
*         - Disabled custom intra/nonintra matrix loading (some MPEGs give   *
*           custom matrix but are still encoded with default matrix)         *
*         - IMPROVED timer and framerate calculators (now handles very large *
*           values and is more accurate)                                     *
*         - ADDED 'Displayed Framerate' calculation                          *
*         - ADDED full-pixel vectors (Thanks to Pavel for sample MPEG file)  *
*                                                                            *
* 0.43    (6 May 2001)                                                       *
*         - ADDED AUDIOQUALITY and AUDIOFREQDIV options/tooltypes            *
*         - ADDED DEFAULTDIR command line option and tooltype                *
*         - Made 'DITHER ?' work without filename                            *
*         - FIXED NOVIDEO option (no video decoding done at all)             *
*         - ADDED NORENDER option (decodes video but does not render)        *
*                                                                            *
* 0.44    (21 May 2001)                                                      *
*         - FIXED a bug in system stream parser (László Török)               *
*                                                                            *
* 0.45    (29 Jan 2002)                                                      *
*         - Improved compatibility with some buggy MPEGs                     *
*         - FIXED a bug related to EOF indication                            *
*                                                                            *
* 0.46    (20 Mar 2002)                                                      *
*         - FIXED Audio sync problems                                        *
*                                                                            *
* 0.47    (7 Aug 2002)                                                       *
*         - ADDED Fullscreen borderless PIP/Vlayer (toggle with Enter)       *
*         - ADDED FULLPIP command line option and tooltype                   *
*         - Clicking mouse button will now exit in screen playback           *
*                                                                            *
* 0.48    (20 Apr 2004)                                                      *
*         - ADDED AHI support                                                *
*         - ADDED AHI tooltype and command line option                       *
*         - FIXED a bug which caused riva to deadlock when audio was enabled *
*           and the NOVIDEO option was used                                  *
*         - FIXED a bug which caused timing problems on systems with very    *
*           high EClock frequency (e.g. AmigaOne)                            *
*                                                                            *
* 0.49    (9 May 2004)                                                       *
*         - OPTIMIZED motion vector code                                     *
*         - OPTIMIZED PIP render routine                                     *
*         - ADDED fast Accupak render routine (PicassoIV only)               *
*         - Blank pointer when playing on screen                             *
*         - IMPROVED verbose output                                          *
*                                                                            *
* 0.50    (30 May 2004)                                                      *
*         - REMOVED GRAYPIP and GRAY24 modes (not really useful)             *
*         - ADDED Window Playback ('DISPLAY=WINDOW' option) + toggle between *
*           Window/Fullscreen with Enter key                                 *
*         - ADDED BGRA32 TrueColor support (for GeForce, etc.)               *
******************************************************************************
* Notes: - AudioControlMsgPort and AudioControlReplyPort are due to be       *
*          removed as they're not used... (I think it's not needed :)        *
******************************************************************************
* Todo: - VideoCD Track reading                                              *
*       - Audio fixes                                                        *
*       - Multiple file support                                              *
*       - GUI                                                                *
*       - hw mpeg audio support (mpeg.device-compatible 4 Delfina, etc.)     *
******************************************************************************
; TODO: 
;       - Discuss YUV->RGB conversion with Gunnar, possibly need to go to BT.709 
;         instead of BT.601
;       - VERBOSE2 to output FPS only
;       - 14 Bit PAULA
;       - <left> <right> keys for forward/back
;       - clear screen at start
;       - Double click for switching between window and fullscreen
;       - fix switching from 24/32 bit WB to fullscreen (strides wrong in 320 mode)
;       - BestModeID replacement to get screens wide enough
;       - play video from RAM:, remove CF and RiVA stops

;
;Done:
;       - refuse to run on old cores/68k in case of pure apollo build
;       - Space for Play/Pause
;
;
;Changelog:
; March 2016
; Flype: YCbCr4:2:2 support for Apollo SAGA
; 10-Oct-2016
; buggs: make assmble working with VASM (see if 1 below)
;         vasmm68k_mot -Fhunkexe -devpac -nowarn=32 -I/opt/amigaos-68k/os-include/ RiVA-0.52a2.s
;         vasmm68k_mot RiVA-0.52a2.s -devpac -Fhunkexe -I/opt/amigaos-68k/os-include 
;          notes: had to patch bugs in includes (workbench.i, asl.i, mpega.i) 
;          listing option is -L bla.lst
;        - enable YUV422 rendering in RGB656 only when an apollo core was detected,
;          else keep old routines
;        - exchange P motion compensation by new routines with proper rounding
; 11-Oct-2016
; buggs: - work on B motion compensation
; 13-Oct-2016
; buggs: - finished rewriting motion compensation routines
; 14-Oct-2016
; buggs: - small improvements, requires vasm for Apollo from now on
;          build vasm by: make CPU=m68k SYNTAX=mot
; 15-Oct-2016
; buggs: - preparations for new motion compensation handling: manual
;          buffer alignment, structure definition for efficient and
;          flexible MC workflow
; 16-Oct-2016
; buggs: - finally enabled the new flexible motion handling (for P-frames)
;        - skip iDCT buffer zeroing if not necessary
;        - skip iDCT altogether when CBP is zero for a block
;        - Intra iDCT uses min/max instructions instead of table access
;          (see APOLLO_CLIP directive)
; 17-Oct-2016
; buggs: - new directive for Apollo motion switch (PAVGB)
;        - replaced simple interpolation Macros by PAVGB based ones
;        - improvements to INTER DCT.
; 18-Oct-2016
;        - minor improvements to iDCT scheduling
;        - stereo audio support
; 19-Oct-2016
;        - starts in fullscreen by default on Apollo
; 20-Oct-2016
;        - replaced AMMX macros by AMMX2 in Motcomp
; 21-Oct-2016
;        - moved generic 68k motion interpolation to separate
;          include file
;        - moved interpolation structure and chroma
;          adjustment to separate include file
;        - more cleanup in the header
;        - finished first working 2D interpolation macros
;          for regular forward/backward HORVER and bidirectional
;          HORVER_ADD
;          NOTE: PSUB will change again, so $FFC0,$1016
;                will need changing back to $FFC1,$0016
;                in both macros
; 22-Oct-2016
; buggs/biggun/flype:
;        - Added the APOLLO_IDCT directive
;        - Use AMMX in ".idct_0" routine (core 3518 minimum).
; 23-Oct-2016
; buggs  - fixed the AMMX iDCT clip/add macro
; 24-Oct-2016
; biggun - Use MOVEP in CopyAudio routine (tested ok).
; 25-Oct-2016
; flype  - modified PERM64 instead of PERM32 in SAGA YUYV renderer.
;        - added Vertical Centering in SAGA YUYV renderer.
;        - added Triple-Buffering in SAGA YUYV renderer.
;          => allocates required memory for Triple-Buffering feature.
;          => allocates is done just after the R5G6B5 OpenScreen().
;          => align allocated memory to 32-bytes.
;          => fill it with BLACK YUYV colorkey ($0080).
;          => use in YUYV renderer (this bypass the System allocated GfxMemBase).
;          => free allocated memory in Close_Display().
; 29-Oct-2016
; buggs  - finished initial reworking of DCT
;        - changed all AMMX instructions into macro calls
;        - integrated AMMX iDCT (vertical stage)
;        - enabled horizontal stage shortcuts to avoid
;          pointless transform steps
;        - fixed YUYV display output for videos which
;          have not the same width as the screen
; 30-Oct-2016
; buggs  - cleanup day
;        - remove iCGX/HAM/C2P stuff for pure Apollo builds
;        - fix chroma in last row (occasional side effect
;          of "proper" chroma vector adjustment)
;        - fix for image sizes that are not 320 or 640,
;          including selection of proper screen mode (currently
;          effectively forces 320 or 640 screen)
; 31-Oct-2016
; buggs  - add experimental support for touch instruction
;        - remove accupak
; 01-Nov-2016
;        - removed touch in most places, gain/loss ratio was
;          not favorable
; 04-Nov-2016
;        - reworked coefficient decoding, moved everything
;          into Macros
;        - shortened code size due to re-use of Y decoding
;          for Cb,Cr
; 05-Nov-2016
;        - started to disable auto-selection of Apollo-specific
;          code as it gets more and more pointless - Apollo
;          builds won't run on native 68k _at all_
;        - removed a lot of direct memory accesses, use A5 as
;          base register wherever possible
; 07-Nov-2016
;        - even more basereg style memory accesses
;        - reworked target location calculation in iDCTs
; 08-Nov-2016
;        - rewrote motion vector decoding for P- and B-Frames
;          for less code, less branches, less stalls and more speed
; 10-Nov.2016
;        - DC only iDCT improved with AMMX (a bit more speed but
;          more importantly less code size and elimination of the
;          last table-based clipping application)
;        - moved some more data out to BSS section, reduced the
;          size of some variables to words and bytes for higher
;          cache efficiency
; 16-Nov-2016
;        - new method of frame timing in YUYV mode, use timer
;          interrupts and picture queues
;        - apply proper rounding for audio period calculation
;          (AudioPeriod) for audio.device
;        - A/V sync
;        - Play/Pause by <SPACE>
;        - experimental: SEEK by <F1>-<F10>
; 17-Nov-2016
;        - more work on sync, bigger a/v input buffers
;        - time calculation like old versions, replaced
;          actual_time by audio_time when sync on audio
;          is active
;        - some improvements to RGB hicolor conversion routine
;          (unfinished right now)
; 19-Nov-2016
;        - increased audio buffer size to avoid problems
;          with some large videos
;        - some more work on improved 16 bit YCbCr2RGB
;        - internal restructuring: reduced code section size
;          but more relocations (right now, more work is in order)
; 21-Nov-2016
;        - fixed AHI, implemented AHI sync
;        - finished restructuring, way less code section size,
;          parts of the relocs are gone now, file size down below
;          40k on Apollo builds
; 02-Dec-2016
;        - rewrote iDCT for new Apollo instructions (BFLY,TRANSxx)
;        - fixed end of sequence marker handling (preliminary)
;        - fixed screen mode issues (when screen was too large)
; 30-Jan-2017
;        - implemented RGB24 as AMMX (for Vampire cards)
;        - switch from windowed mode to fullscreen will select
;          16 BPP (=YUYV) on Apollo
; 05-Feb-2017
;        - slight improvements to RGB565 windowed mode color
;          conversion
; 14-Feb-2017
;        - cleanup: CreatePalette (as suggested by MattHey and Don Adan)
;        - proper rounding in audio 8 Bit conversion (less noise)
; 03-Mar-2017
;        - fix P-IV YUV PIP
;        - fix BGRA,ARGB,BGR,HiColor screen offsets in FS mode for cases
;          where the video was wider than the screen (68k, AMMX is still pending)
; 19-Apr-2017
;        - improvement: AMMX DCT clear is done directly in iDCT stage,
;          gains around 1 FPS in 640x360 2 MBit/s
; 06-Aug-2017
;        - fixed 32 Bit ARGB chroma rendering
;        - replaced paddb by paddusb mnemonics (requires updated VASM)
; 23-Jul-2018
;        - EXTRA_DELAY_FRAME option for revised A/V sync
; 30-Oct-2018
;        - adjusted width/height of doublescan decision to latest VampireGFX
; 01-Feb-2018
;        - numerous changes in motion and general ops to improve performance a bit
; Jul-2019
;        - fixed DHAM6,DHAM8 for 68k builds
;        - improved 68060 scheduling for DCT and Interpolation
;        - fixed A/V sync
;        - implemented 14 Bit Paula and 16 Bit Pamela audio support (HQAUDIO)
;        - more resiliency in timer code (Vampire multiframe buffering mode)
;
;
;   expected ARGB32 render time
;    11 cycles preamble (pmul & stuff) for 16 pixels = 0.6875 cycles/pixel ~1clock per pixel
;    5-7 cycles per 2 pixels = 3-4 cycles/pixel
;    total                   = 5 cycles per pixel
;   at 640x360 = 230400 pixels -> 640*360*5*538/78000000 = 7.94 s overhead for 538 frames, x11
;   -> plus overhead for BltBitmapRastport()
;
; list = p96AllocModeListTagList(Tags)
; d0                             a0
; p96FreeModeList(ModeList) a0
; STRUCTURE P96Mode,LN_SIZE
; ...
;        UWORD   p96m_Width
;        UWORD   p96m_Height
;        UWORD   p96m_Depth
;        ULONG   p96m_DisplayID

;
APOLLOCHECK		EQU	0	;0=DO the check, -1=always apollo mode, 1=always 68k mode
;
;
; APOLLO SPECIFIC options (APOLLO_CLIP is the "master switch")
;
;
APOLLO_CLIP		EQU	1	;controls various Apollo specific functions
	ifne	APOLLO_CLIP
APOLLO_SETSR		EQU	1	;set SR Bit #11 to announce AMMX usage
APOLLO_MOT		EQU	1	;use Apollo for MOTION
APOLLO_IDCT		EQU	1 	;use Apollo for IDCT = AMMX, Column-Row processing instead of Row-Column
APOLLO_IDCTX		EQU	1	;perform first part of iDCT in MMX, too
APOLLO_IDCTXNOSTORE	EQU	1	;don`t store intermediate iDCT results
APOLLO_IDCT_UNROLL	EQU	1	;unroll Apollo iDCT and perform clip/add directly (old option, implicit in IDCTX)
APOLLO_MOVEP		EQU	1	;use MOVEP whenever possible 
APOLLO_YUYV             EQU	1	;use SAGA YUYV Triple-Buffer (bypass system GfxMemBase)
APOLLO_P96ONLY		EQU	1	;don't include HAM, C2P, CyberGraphX on Apollo
APOLLO_FULLSCREEN_DEF	EQU	1	;default to fullscreen on Apollo
APOLLO_DCTCLEAR		EQU	1	;use store for clearing dct field
APOLLO_DCTZERO		EQU	1	;works after using other instructions
APOLLO_NSAGABUFS	EQU	16	;16 video buffers for SAGA YUYV output (>0 enables new buffered handling, 2^n, n >= 2)
APOLLO_NSAGABUFK	EQU	2	;number of buffers to keep before overwriting (>=1, but much smaller than NSAGABUFS)
APOLLO_NSAGA_TD		EQU	1	;Timer start delay to wait on audio at the beginning
SYNC_ON_AUDIO		EQU	1	;calculate elapsed replay time from audio thread (correct A/V drift every second)
DCTLINEPOP		EQU	0	;number of coefficients per line -> no longer relevant for IDCTX AMMX iDCT
APOLLO_P96KLUDGE	EQU	1	;temporary fix for 320/640 handling (dff1f4) 
AUDIO_ROUND		EQU	1	;correctly round samples before 8 Bit conversion
CUSTOM_BESTSCREENMODE	EQU	1	;re-implement screen mode selection to work around P96 bugs
;EXTRA_DELAY_FRAME	EQU	1	;timing: increase frame counter by this (helps AV sync)
HIGHBOOST		EQU	1	;apply highboost filter in HQ Paula mode
HQ_TRIPLEBUF		EQU	1	;use triple buffering in HQ Paula mode
HQ_PAMELA16		EQU	1	;enable Pamela 16 for HQ Audio
	else
APOLLO_SETSR		EQU	0	;set SR Bit #11 to announce AMMX usage
APOLLO_MOT		EQU	0	;use Apollo for MOTION
APOLLO_IDCT		EQU	0 	;use Apollo for IDCT = AMMX, Column-Row processing instead of Row-Column
APOLLO_IDCT_UNROLL	EQU	0	;
APOLLO_IDCTX		EQU	0	;perform first part of iDCT in MMX, too
APOLLO_IDCTXNOSTORE	EQU	0	;
APOLLO_MOVEP		EQU	0	;use MOVEP whenever possible 
APOLLO_YUYV             EQU	0	;use SAGA YUYV Triple-Buffer (bypass system GfxMemBase)
APOLLO_P96ONLY		EQU	0	;don't include HAM, C2P, CyberGraphX on Apollo
APOLLO_P96KLUDGE	EQU	0	;temporary fix for 320/640 handling (dff1f4) 
APOLLO_FULLSCREEN_DEF	EQU	0	;default to fullscreen on Apollo
APOLLO_DCTCLEAR		EQU	0	;use store for clearing dct field
APOLLO_DCTZERO		EQU	0	;works after using other instructions
APOLLO_NSAGABUFS	EQU	0	;video buffers for SAGA YUYV output (>0 enables new buffered handling, 2^n, n >= 2)
APOLLO_NSAGABUFK	EQU	0	;number of buffers to keep before overwriting (>=1, but much smaller than NSAGABUFS)
SYNC_ON_AUDIO		EQU	0	;disabled sync on audio for 68k builds
DCTLINEPOP		EQU	1	;skip more pointless transform checks and steps by tracking iDCT line population
AUDIO_ROUND		EQU	0	;
CUSTOM_BESTSCREENMODE	EQU	1	;re-implement screen mode selection to work around P96 bugs
HIGHBOOST		EQU	0	;apply highboost filter in HQ Paula mode
HQ_TRIPLEBUF		EQU	0	;use triple buffering in HQ Paula mode
HQ_PAMELA16		EQU	0	;
	endc
;
;
; GENERIC OPTIONS
;
;
WRENDER_DIRECT		EQU	0	;render directly into screen in windowed mode
FRAMEBUFFER_ALIGN	EQU	32	;align framebuffers to multiple of this, 2^n
IDCT_COUNT		equ	0	; TEST: check how important the IDCT sub-functions are (leave 0 for release builds)
DCTCLEAR_BYPASS		EQU	1
IDCT_BYPASS		EQU	1
NEW_BITSTREAM		EQU	1	;with NOA0DIRECT, replace bitstream functions by something new
NOA0DIRECT		EQU	1	;don't access A0 directly in bitstream processing (preparation for better bitstream functions)
BT709			EQU	0	;use ITU-R BT.709 constants instead of BT.601 for software YCbCr to RGB conversion
CODE_ALIGN		EQU	0	;align code to n*16 (1) or not (0) - latter (=0) saves space
DEBUG_TIMING		EQU	0	;conditional print of timing (debug only)

DEF_AUDIOQUALITY	EQU	1	;default audio quality (w/o tooltypes or cmdline args) 0=low,1=default,2=high
DEF_AUDIOFREQDIV	EQU	1	;default audio frequency division (w/o tooltypes or cmdline args), 1=no div, 2=factor 2, 4=factor 4
;Skip_CurrentPic

OPT_ASYNCPRI		EQU	0	;ASYNCIO task priority
OPT_AUDIOPRI		EQU	2	;Audio task priority

	ifne	APOLLO_CLIP
		MACHINE ac68080	
	else
		MACHINE	MC68040
	endc


	include	exec/types.i
	include	exec/io.i
	include	lvo/exec_lib.i
	include	lvo/dos_lib.i
	include	lvo/intuition_lib.i
	include	lvo/asl_lib.i
	include	lvo/timer_lib.i
	include	lvo/graphics_lib.i
	;include lvo/picasso96_lib.i
	include picasso96/Picasso96API_lib.i
	include	lvo/icon_lib.i
	include	lvo/mpega_lib.i
	include	exec/exec.i
	include	dos/dos.i
	include	dos/dostags.i
	include	intuition/intuition.i
	include graphics/gfxbase.i
	include	libraries/asl.i
	include	libraries/mpega.i
	include	workbench/workbench.i
	include	workbench/startup.i
	include	picasso96/picasso96.i
	ifeq	APOLLO_P96ONLY
	 include cybergraphics/cgxvideo.i
	 include cybergraphics/cgxvideo_lib.i
	 include cybergraphics/cybergraphics.i
	 include cybergraphics/cybergraphics_lib.i
	endc
	include	devices/audio.i
	include	dos/dosextens.i
	include ahi/ahi.i
	include exec/lists.i
	include vampire/vampire.i

	include	MacrosGen.m
	include	MacrosMPEG.m
	include	p96YUVStuff.i
	include	mpegaadapter.i

	; Structures/Macros
	include	"MacrosAMMX.m"
	include	"MacrosMotion.m"
	include	"MacrosDCTCount.m"
	include "MacrosCoeff.m"
	include "MacrosMVDec.m"
	ifne	APOLLO_MOT
	 include "MacrosInterpolApollo.m"
	else	;APOLLO_MOT
	 include "MacrosInterpol68k.m"
	endc

	ifne	APOLLO_IDCTX
	 include	 "MacrosIDCTApollo.m"
	else
	 include	 "MacrosIDCT68k.m"
;	 include	 "MacrosIDCTApolloOld.m"
	endc

		bra	main

versioninfo	dc.b	"$VER: RiVA v0.54 (23-Jul-2019) "
	ifne	APOLLO_CLIP
		dc.b	"Apollo"
	else
		dc.b	"68k"
	endc
		dc.b	0

*-----------------------------------------------------------------------------*
ASMONE			EQU	0
DEBUG			EQU	0

SHOW_PICINFO		EQU	0
SHOW_MBINFO		EQU	0
SHOW_COEFFS		EQU	0
SHOW_RENDERINFO		EQU	0
*-----------------------------------------------------------------------------*
	ifnd	AFB_68080
AFB_68080	EQU	10
	endc

; Status:
;  - abstracted Hardware output initialization/shutdown
;  - still references globals in 14 Bit Amplifier
;  - no actual check whether this runs on Amiga hardware
P16INTREQ	EQU	$dff29c
P16INTENA	EQU	$dff29a
P16INTENAR	EQU	$dff21c
P16DMACON	EQU	$dff296
P16ADKCON	EQU	$dff29e
P16AUD0		EQU	$dff2a0
P16ALEN0	EQU	$dff2a4
P16APER0	EQU	$dff2a6
P16AVOL0	EQU	$dff2a8
P16ALEN1	EQU	$dff2b4
P16APER1	EQU	$dff2b6
P16AVOL1	EQU	$dff2b8

P16VECTOR	EQU	$50 	;relative to VBR
PAMELA16_JUMPSTART	EQU	1	;init dff0a6-d6,dff0a8-d8

intena		EQU	$09a
intenar		EQU	$01c
*-----------------------------------------------------------------------------*

KILOBYTE		EQU	1024

;Dither Modes:
DM_PIP			EQU	1
DM_WINDOW		EQU	2
DM_TRUECOLOR		EQU	3
DM_HICOLOR		EQU	4
DM_ACCUPAK		EQU	5
DM_GRAY			EQU	6
DM_DHAM8		EQU	7
DM_DHAM6		EQU	8
DM_NUMBER_OF		EQU	8

picture_start_code:		EQU	$00000100
user_data_start_code:		EQU	$000001b2
sequence_header_code:		EQU	$000001b3
sequence_error_code:		EQU	$000001b4
extension_start_code:		EQU	$000001b5
sequence_end_code:		EQU	$000001b7
group_start_code:		EQU	$000001b8
pack_start_code:		EQU	$000001ba
system_header_start_code:	EQU	$000001bb
padding_stream_code:		EQU	$000001be

PARSE_START	EQU	0
PARSE_VIDEO	EQU	1
PARSE_AUDIO	EQU	2

;Picture Coding types:
I_FRAME		EQU	1
P_FRAME		EQU	2
B_FRAME		EQU	3
D_FRAME		EQU	4

;Macroblock Type (what the bits mean):
MB_QUANT	EQU	4
MB_MOTION_FWD	EQU	3
MB_MOTION_BWD	EQU	2
MB_PATTERN	EQU	1
MB_INTRA	EQU	0

EOB		EQU	128
BLK_ESC		EQU	129

;Constants for YUV to RGB conversion
	ifne	BT709
	; use ITU-R BT.709 constants as most source material (HD) is in this color space these
	; days and AFAIK the usual transcoders don't bother converting from 709 to 601
FIX_0_3359	EQU	48	;scaled by 256
FIX_0_6985	EQU	119
FIX_1_3711	EQU	402
FIX_1_7337	EQU	475
;FIX_0_3359	EQU	47	;scaled by 255
;FIX_0_6985	EQU	118
;FIX_1_3711	EQU	400
;FIX_1_7337	EQU	473
	else
FIX_0_3359	EQU	86
FIX_0_6985	EQU	179
FIX_1_3711	EQU	351
FIX_1_7337	EQU	444
	endc

;IDCT Contstants, not enough to get even close to IEEE1180 --> but on 68k, compromises are in order...
C4		EQU	1448/4			; 1.414213562 << 10	/ SQR(2)      /
C6		EQU	784/4			; 0.7653668647 << 10	/ 2*Sin(Pi/8) /
Q		EQU	-1108/4			; 1.0823922 << 10	/ -(C2 - C6)  /
R		EQU	2676/4			; 2.61312593 << 10	/ C2 + C6     /
FIX_1_082392200	EQU	1108/4			; -Q
FIX_2_613125930	EQU	-2676/4			; -R
FIX_1_847759065 EQU	1892/4			; C2

SIZE_MB_address		EQU	2*2048		;fast MB_number -> MB_BitmapAddress table
SIZE_MB_type_P		EQU	2*64		;Huffman tables...
SIZE_MB_type_B		EQU	2*64		;
SIZE_block_pattern	EQU	2*512		;
SIZE_motion_vector	EQU	2*2048		;
SIZE_DCT_size_lum	EQU	2*128		;
SIZE_DCT_size_chrom	EQU	2*256		;
SIZE_DCT_coeff		EQU	4*65536		;
SIZE_convert_to_bitmap	EQU	4096		;table for quick 0-255 clamping (for IDCT output)
SIZE_predict_clamp	EQU	4096
SIZE_YUVtoHiColorTable	EQU	2*262144	;fast YUV->Hicolor table
SIZE_YUVtoBGGRTable	EQU	2*524288	;fast YUV->BG/GR table (for STORM)
;-------------------------------------------
SIZE_LookupTables	EQU	SIZE_MB_address+SIZE_MB_type_P+SIZE_MB_type_B+SIZE_block_pattern+SIZE_motion_vector+SIZE_DCT_size_lum+SIZE_DCT_size_chrom+SIZE_DCT_coeff+SIZE_convert_to_bitmap+SIZE_predict_clamp

IDCT_FRAC	EQU	4			;fraction size to use for idct algorithm (no. of bits) range: 0 - 6

ABUF_SIZE	EQU	512*KILOBYTE		;audio buffer size
ABUF_OVERLAP	EQU	1*KILOBYTE		;audio doublebuf overlap size
ABUF_MARGIN	EQU	ABUF_SIZE-ABUF_OVERLAP	;audio buffer margin

VBUF_SIZE	EQU	512*KILOBYTE		;video buffer size
VBUF_OVERLAP	EQU	1*KILOBYTE		;video doublebuf overlap size
VBUF_MARGIN	EQU	VBUF_SIZE-VBUF_OVERLAP	;video buffer margin

SBUF_SIZE	EQU	48*KILOBYTE		;system buffer size
SBUF_OVERLAP	EQU	1*KILOBYTE		;system buffer overlap size
SBUF_MARGIN	EQU	SBUF_SIZE-SBUF_OVERLAP	;system buffer margin

;Asyncio parameters description:
;+-------------+----------------------------------------------------------------------------------+-------------+
;|/////////////|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|/////////////|
;+-------------+----------------------------------------------------------------------------------+-------------+
;:<- OVERLAP ->:                                                                                  :<- OVERLAP ->:
;:<------------------------------------------- MARGIN ------------------------------------------->:             :
;:<------------------------------------------------ SIZE ------------------------------------------------------>:

**************
*** MACROS ***
**************
	ifne	1
ALIGN	MACRO
	IFNE	*&(\2-1)
	ds.b	\2-*&(\2-1)
	ENDC
	ENDM
	endc

	ifne	NEW_BITSTREAM

CHECK_BUFFER	MACRO
		movem.l	d0,-(sp)
		lsr.l	#3,d0
		add.l	a0,d0
		cmp.l	vbuf_max,d0
		movem.l	(sp)+,d0
		blt.b	.bufok
		bsr	ReadVideoBuffer
.keychk
		bsr	KeyInputCheck
		tst.b	PauseFlag
		beq.s	.nopause
		VBLDELAY	10
		bra.s	.keychk
.nopause
		tst.l	AbortFlag
		beq.s	.bufok

		bra	MPEGExit
.bufok
		ENDM

CHECK_SYSBUFFER	MACRO
		movem.l	d0,-(sp)
		lsr.l	#3,d0
		add.l	a0,d0
		cmp.l	sbuf_max,d0
		movem.l	(sp)+,d0
		blt.b	.sbufok
		bsr	ReadSystemBuffer
.sbufok
		ENDM

	else

CHECK_BUFFER	MACRO
		cmp.l	vbuf_max,a0
		blt.b	.bufok
		bsr	ReadVideoBuffer
		bsr	KeyInputCheck
		tst.l	AbortFlag
		bne	MPEGExit
.bufok
		ENDM

CHECK_SYSBUFFER	MACRO
		cmp.l	sbuf_max,a0
		blt.b	.sbufok
		bsr	ReadSystemBuffer
.sbufok
		ENDM

	endc

DEFRAC		MACRO
		 IFNE	IDCT_FRAC
		 asr.w	#IDCT_FRAC,\1
		 ENDC
		ENDM

CALLEXEC	MACRO
		move.l	4.w,a6
		jsr	_LVO\1(a6)
		ENDM

CALLDOS2	MACRO
		move.l	dosbase,a6
		jsr	_LVO\1(a6)
		ENDM

CALLINT2	MACRO
		move.l	intbase,a6
		jsr	_LVO\1(a6)
		ENDM

CALLGFX		MACRO
		move.l	gfxbase,a6
		jsr	_LVO\1(a6)
		ENDM

CALLP96		MACRO
		move.l	p96base,a6
		jsr	_LVOp96\1(a6)
		ENDM

	ifeq	APOLLO_P96ONLY
CALLCGX		MACRO
		move.l	cgxbase,a6
		jsr	_LVO\1(a6)
		ENDM
	endc

CALLASL		MACRO
		move.l	aslbase,a6
		jsr	_LVO\1(a6)
		ENDM

CALLICON2	MACRO
		move.l	iconbase,a6
		jsr	_LVO\1(a6)
		ENDM

CALLMPEGA	MACRO
		move.l	mpegabase,a6
		jsr	LVO_MPEGA_\1(a6)
		ENDM

; delay in seconds
DELAY		MACRO
		movem.l	d0-a6,-(a7)
		move.l	#50*\1,d1
		CALLDOS2	Delay
		movem.l	(a7)+,d0-a6
		ENDM

; delay in VBLANK units (1/50 s)
VBLDELAY	MACRO
		movem.l	d0-a6,-(a7)
		move.l	#\1,d1
		CALLDOS2	Delay
		movem.l	(a7)+,d0-a6
		ENDM

MakeLookupTable	MACRO
		lea	huff_\1,a3
		move.l	lookup_\1,a4
		move.l	#\2,a5
		bsr	GenerateVLCTable
		ENDM

FILL		MACRO
		REPT	\1
		dc.b	\2
		ENDR
		ENDM

; args: 
;  src/dest register \1
;  scratch register  \2
RAND8_XORSHIFT	macro
		 move.l	\1,\2	;save input
		 lsl.w	#7,\1	;shift left 7
		eor.w	\2,\1	;first xor
		 move.l	\1,\2	;save input
		 lsr.w	#5,\1	;shift right 5
		eor.w	\2,\1	;next xor
		 move.l	\1,\2	;save input
		 lsl.w	#3,\1	;shift left 3
		eor.w	\2,\1	;last xor
		and.w	#$3f,\1
		add.w	#$7f,\1
		endm

RAND8		macro
		mulu.l	#$01010101,\1
		add.l	#31415927,\1
		endm

	ifne	DEBUG_TIMING

DOUTTXT		MACRO
		move.l	d2,-(a7)
		move.l	#\1,d2
		bsr	OutputText
		move.l	(a7)+,d2
		ENDM

;Macro to print 32-bit unsigned decimal...
DOUTDEC		MACRO
		move.l	d1,-(a7)
		move.l	\1,d1
		bsr	OutputDecimal
		move.l	(a7)+,d1
		ENDM

	else	;DEBUG_TIMING

DOUTTXT	macro
	;
	endm

DOUTDEC	macro
	;
	endm

	endc	;DEBUG_TIMING

; Video Buffer for SAGA YUYV output
  STRUCTURE	VIDBUF,MN_SIZE
  	LONG	VIDBUF_TimeStampH	;timestamp high longword
	LONG	VIDBUF_TimeStampL	;timestamp low longword
	APTR	VIDBUF_Y		;Y
	APTR	VIDBUF_U		;Cb (placeholder, unused right now)
	APTR	VIDBUF_V		;Cr (placeholder, unused right now)
	LONG	VIDBUF_Width		;
	LONG	VIDBUF_Height		;
	SHORT	VIDBUF_FMT		;0=YUYV
	SHORT	VIDBUF_UNUSED		;
        LABEL	VIDBUF_SIZE		;

;AsyncIO Control Message
  STRUCTURE	AIOC,MN_SIZE
      BYTE	AIOC_COMMAND			;AsyncIO Control Commands are identified by a 16bit code
      BYTE	AIOC_REPLY			;Reply codes
      LONG	AIOC_DATA			;Extra data (if applicable) for AsyncIO Control Commands
      APTR	AIOC_VIDBUFMIN			;Base of VIDEO buffer to be returned by AsyncIO Task
      APTR	AIOC_VIDBUFMAX			;End of VIDEO buffer returned by AsyncIO Task
      APTR	AIOC_FILEPOSITION		;Current Position in file
      LABEL	AIOC_SIZE


;AsyncIO Commands
AIOCMD_SEEK	EQU	1			;Seek to given location in file
AIOCMD_READ	EQU	2			;Read into buffers
AIOCMD_EXIT	EQU	3			;Exit

;AsyncIO Replies
AIOREP_OK	EQU	1			;if command was successful
AIOREP_ERROR	EQU	2			;if command was unsuccessful (ie. end of file, etc.)
AIOREP_EOF	EQU	3			;if buffer read but end of file

;Audio Control Message
  STRUCTURE	AUDC,MN_SIZE
        BYTE	AUDC_COMMAND			;Control command
        BYTE	AUDC_REPLY			;Reply code
        LABEL	AUDC_SIZE

AUDCMD_OK	EQU	1
AUDCMD_ABORT	EQU	2

;Audio Buffer Message
  STRUCTURE	AUDB,MN_SIZE
        BYTE	AUDB_COMMAND
        BYTE	AUDB_REPLY
        APTR	AUDB_BUFFER_MIN
        APTR	AUDB_BUFFER_MAX
        LABEL	AUDB_SIZE

AUDREP_OK	EQU	1
AUDREP_ERROR	EQU	2


;------------------------------------------------------------------------------------------------------------------------;
;----------------------------------------------- START OF MAIN PROGRAM --------------------------------------------------;
;------------------------------------------------------------------------------------------------------------------------;
		even
MemInit:	
	; safety: make sure the BSS section is zeroed
		lea	STARTBSS,a0
		move.l	a0,a4				; base pointer in BSS
		lea	ENDBSS,a1
.clrbss
		clr.b	(a0)+
		cmpa.l	a1,a0
		blt.s	.clrbss

	; init some variables to a sane state
		move.l	#"RiVA",PIPtitle-STARTBSS(a4)		;"RiVA"
		move.l	#$202D2000,PIPtitle-STARTBSS+4(a4)	;" - ",0
		move.l	#100,WinNormalZoom-STARTBSS(a4)
		move.l  #100,ZOOM_value-STARTBSS(a4)
		move.l	#1,TimerClosed-STARTBSS(a4)
		lea	block_ref_y1,a1
		move.l	a1,block_buffer_tmp_y1

	ifne    APOLLO_NSAGABUFS
		move.l	#-1,VidBuf_TMRSig-STARTBSS(a4)		;no timer signal yet
	endc

		;obsolete: see above
;		clr.l	FrameBufferAllocSize-STARTBSS(a4)	;better safe than sorry, mark framebuffers unallocated
;		clr.l	WBProgName-STARTBSS(a4)			;

	ifne	APOLLO_SETSR
		; set Bit #11 in SR to announce the use of AMMX
		move.l	4.w,a6
		jsr	_LVODisable(a6)
		jsr	_LVOSuperState(a6)
		move	sr,d1
		or.w	#$800,d1
		move	d1,sr
		jsr	_LVOUserState(a6)
		jsr	_LVOEnable(a6)
	endc
		rts


	ifne	CODE_ALIGN
		CNOP    0,16
	else
		CNOP	0,4
	endc
main:
		bsr	MemInit

		lea	VDEC_BASE,a5			;internal variables base (not fully implemented right now, sorry -> that's why several reloads of A5 take place)

		suba.l	a1,a1
		CALLEXEC FindTask
		move.l	d0,MainTask			;find our task
		move.l	d0,a4

	ifeq	APOLLOCHECK
		CALLEXEC Disable
		;test if we are running on Apollo
		move.l	TC_TRAPCODE(a4),-(sp)
		move.l	#TrapCatch,TC_TRAPCODE(a4)	
		;dc.w	$1054				;move.l	 (a4),b0 - old test
		;AMMX test: perm B0 to D0 and see if we get the desired byte swap
		move.l	#$DEADBEEF,d0
		dc.w	$FE3F,$0000,$807C,$5476	;VPERM   #$807C5476,D0,D0,D0 (actually embeds or.w #$5476,d0 when interpreted as 2 word op)
		nop				;just in case the trap is triggered somewhere else 

		cmp.l	#$ADDEEFBE,d0
		sne	TrapCaught			;behave as if instruction was not executed in case we get the wrong result
		move.l	(sp)+,TC_TRAPCODE(a4)
		CALLEXEC Enable

		tst.w	TrapCaught(pc)
		seq	apollo_active-VDEC_BASE(a5)	;apollo_active.b != 0 means the move.l D0,B0 and/or VPERM were not correctly executed 
	else
		ifgt	APOLLOCHECK
			sf	apollo_active-VDEC_BASE(a5)
		else
			st	apollo_active-VDEC_BASE(a5)
		endc
	endc

		tst.l	pr_CLI(a4)
		bne.w	CliStart
		bra.s	WBStart

;**********************************************************
; Check whether an Apollo instruction causes a Trap
; assumes 10 byte of code (2 + 8)
;**********************************************************
TrapCaught:	dc.w	0
TrapCatch:
		st	TrapCaught
		ADDQ    #4,SP
		ADDQ.L	#8,2(sp)	;VPERM  #....,....,...
		rte


; ----------------------------- We land here when started from WB ----------------------------------------
WBStart:	lea	pr_MsgPort(a4),a0
		CALLEXEC WaitPort
		lea	pr_MsgPort(a4),a0
		CALLEXEC GetMsg
		move.l	d0,WBStartMsg-VDEC_BASE(A5)	;wbnel ;)

		lea	WBProgName-VDEC_BASE(A5),a1	;itt lesz a nev
		move.l	#"PROG",(a1)+
		move.l	#"DIR:",(a1)+
		move.l	LN_NAME(a4),a0
.nameloop:	move.b	(a0)+,d0
		move.b	d0,(a1)+
		bne.b	.nameloop

CliStart	CALLEXEC CacheClearU		;Flush CPU Cache...

OpenDOS:
		lea	dos_name,a2		;string base

		moveq	#39,d0
		lea	dos_name-dos_name(a2),a1
		CALLEXEC OpenLibrary			;Open dos.library
		move.l	d0,dosbase-VDEC_BASE(a5)
		beq.w	Error				;UNABLE TO OPEN DOS.LIBRARY !

		lea	intuition_name-dos_name(a2),a1
		moveq	#39,d0
		CALLEXEC OpenLibrary			;Open Intuition library
		move.l	d0,intbase-VDEC_BASE(A5)
		beq.w	Error

		lea	asl_name-dos_name(a2),a1
		moveq	#39,d0
		CALLEXEC OpenLibrary
		move.l	d0,aslbase-VDEC_BASE(A5)

		CALLDOS2	Output				;Get stdout for shell output
		move.l	d0,StdOut-VDEC_BASE(A5)

		tst.l	WBProgName-VDEC_BASE(A5)
		beq	ReadArgs			;if not WB, read cli args

.ReadTooltypes
.OpenIconLib	lea	icon_name-dos_name(a2),a1
		moveq	#39,d0
		CALLEXEC OpenLibrary
		move.l	d0,iconbase-VDEC_BASE(A5)

.OpenIcon	lea	WBProgName-VDEC_BASE(A5),a0
		CALLICON2 GetDiskObject
		move.l	d0,RiVADiskObject-VDEC_BASE(A5)
		beq	NoDiskObject			;no tooltypes
		move.l	d0,a0
		move.l	do_ToolTypes(a0),a0
		move.l	a0,d7				;d7 = tt

		lea	tt_P96-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_p96
		st	p96_switch-VDEC_BASE(A5)
.tt_no_p96

	ifeq	APOLLO_P96ONLY
		move.l	d7,a0
		lea	tt_AGA-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_aga
		st	AGA_switch-VDEC_BASE(A5)
.tt_no_aga
		move.l	d7,a0
		lea	tt_CGX-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_cgx
		st	cgx_switch-VDEC_BASE(A5)
.tt_no_cgx
	endc

		move.l	d7,a0
		lea	tt_FULLPIP-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_fullpip
		not.l	WinFullToggle-VDEC_BASE(A5)

		lea	PIPWinTagBLess,a1
		move.l	#1,PIPWinTagBLess-PIPWinTagBLess(a1)
		clr.l	PIPWinTagDrag-PIPWinTagBLess(a1)
		clr.l	PIPWinTagClose-PIPWinTagBLess(a1)
		clr.l	PIPWinTagDepth-PIPWinTagBLess(a1)
		clr.l	PIPWinTagSize-PIPWinTagBLess(a1)
		clr.l	WindowTitle-PIPWinTagBLess(a1)

		move.l	#1000,ZOOM_value-VDEC_BASE(A5)
.tt_no_fullpip
		move.l	d7,a0
		lea	tt_PA-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_pa
		st	PlanarAssistance-VDEC_BASE(A5)
.tt_no_pa
		move.l	d7,a0
		lea	tt_NOSKIP-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_noskip
		st	NOSKIP_switch-VDEC_BASE(A5)
.tt_no_noskip
		move.l	d7,a0
		lea	tt_HALF-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_half
		st	half_switch-VDEC_BASE(A5)
.tt_no_half
		move.l	d7,a0
		lea	tt_LOOP-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_loop
		st	LOOP_switch-VDEC_BASE(A5)
.tt_no_loop
		move.l	d7,a0
		lea	tt_NOAUDIO-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_noaudio
		clr.l	GlobalAudioEnable
.tt_no_noaudio
		move.l	d7,a0
		lea	tt_NOVIDEO-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_novideo
		st	NOVIDEO_switch-VDEC_BASE(A5)
.tt_no_novideo
		move.l	d7,a0
		lea	tt_NORENDER-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_norender
		st	NORENDER_switch-VDEC_BASE(A5)
.tt_no_norender
		move.l	d7,a0
		lea	tt_AHI-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_ahi
		st	AHI_switch-VDEC_BASE(A5)
.tt_no_ahi
		move.l	d7,a0
		lea	tt_HQAUDIO-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_hqa
		sf	AHI_switch-VDEC_BASE(A5)
		st	P14_switch-VDEC_BASE(A5)
.tt_no_hqa

		move.l	d7,a0
		lea	tt_MONOSURROUND-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_monosurround
		st	MONOSURROUND_switch-VDEC_BASE(A5)
.tt_no_monosurround
		move.l	d7,a0
		lea	tt_ZOOM-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_zoom
		lea	ZOOM_value-VDEC_BASE(A5),a1
		bsr	rva_StrToLong
		tst.l	ZOOM_value-VDEC_BASE(A5)
		bne.b	.tt_no_zoom
		move.l	#100,ZOOM_value-VDEC_BASE(A5)
.tt_no_zoom
		move.l	d7,a0
		lea	tt_FPS-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_fps
		lea	FPS_value-VDEC_BASE(A5),a1
		bsr	rva_StrToLong
.tt_no_fps
		move.l	d7,a0
		lea	tt_AUDIOQUALITY-dos_name(a2),a1
		CALLICON2 FindToolType
		lea	MP12_Mono_Quality(pc),a3	;MPEGA details
		tst.l	d0
		beq.b	.tt_no_audioquality
		lea	ttval_AUDIOQUALITY-VDEC_BASE(A5),a1
		bsr     rva_StrToLong
		move.l	ttval_AUDIOQUALITY-VDEC_BASE(A5),d1
		beq.s	.is_q0
		bra.s	.not_q0
.tt_no_audioquality:
		moveq	#DEF_AUDIOQUALITY,d1
		bne.b	.not_q0
.is_q0:
		clr.w	MP12_Mono_Quality-MP12_Mono_Quality(a3)		;0 = low
		clr.w	MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		clr.w	MP3_Mono_Quality-MP12_Mono_Quality(a3)
		clr.w	MP3_Stereo_Quality-MP12_Mono_Quality(a3)
		bra.b	.quality_done
.not_q0		cmp.b	#2,d1
		beq.b	.not_q2
		move.w	#2,MP12_Mono_Quality-MP12_Mono_Quality(a3)		;2 = high
		move.w	#2,MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Mono_Quality-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Stereo_Quality-MP12_Mono_Quality(a3)
		bra.b	.quality_done
.not_q2		move.w	#1,MP12_Mono_Quality-MP12_Mono_Quality(a3)		;1 = default
		move.w	#1,MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Mono_Quality-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Stereo_Quality-MP12_Mono_Quality(a3)
.quality_done

		move.l	d7,a0
		lea	tt_AUDIOFREQDIV-dos_name(a2),a1
		CALLICON2 FindToolType
		;lea	MP12_Mono_Quality(pc),a3	;MPEGA details (see above)

		tst.l	d0
		beq.s	.tt_no_audiofreqdiv
		lea	ttval_AUDIOFREQDIV-VDEC_BASE(A5),a1
		bsr     rva_StrToLong
		move.l	ttval_AUDIOFREQDIV-VDEC_BASE(a5),d1
		cmp.b	#1,d1
		bne.b	.not_d1
		bra.s	.is_d1
.tt_no_audiofreqdiv:
		moveq	#DEF_AUDIOFREQDIV,d1
		cmp.b	#1,d1
		bne.s	.not_d1

		; extra check: if audio frequency division is 1, the sound might be 44 kHz, so check whether we can safely enable it
		bsr	ECSAudioTest
		tst.l	d0				;ECS with proper P96 settings found ?
		bgt.s	.is_d1				;yes, accept

		moveq	#2,d1
		bra.s	.not_d1
.is_d1:
		clr.l	audio_freqdiv			;0 = -d1
		move.w	#1,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		bra.b	.freqdiv_done
.not_d1		cmp.b	#2,d1
		bne.b	.not_d2
		move.l	#1,audio_freqdiv-MP12_Mono_Quality(a3)		;1 = -d2
		move.w	#2,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		bra.b	.freqdiv_done
.not_d2		move.l	#2,audio_freqdiv-MP12_Mono_Quality(a3)		;2 = -d4
		move.w	#4,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
.freqdiv_done

		move.l	d7,a0
		lea	tt_PUBSCREEN-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0			;d0 = string after PUBSCREEN=
		beq.b	.tt_no_pubscrn
		move.l	d0,Pubscr-MP12_Mono_Quality(a3)	;see above
.tt_no_pubscrn
		move.l	d7,a0
		lea	tt_DEFAULTDIR-dos_name(a2),a1
		CALLICON2 FindToolType
		tst.l	d0
		beq.b	.tt_no_defaultdir
		move.l	d0,ASLDrawerString-MP12_Mono_Quality(a3) ;see above
.tt_no_defaultdir
		move.l	  d7,a0
		lea	  tt_BORDERLESS-dos_name(a2),a1
		CALLICON2 FindToolType
		move.l    d0,BORDERLESS_switch

	ifne	APOLLO_FULLSCREEN_DEF
		;default: HiColor Fullscreen - maps to YUV on Apollo
		move.b  #DM_HICOLOR,DitherMode-VDEC_BASE(A5)
	endc

		move.l	d7,a0
		lea	tt_DISPLAY-dos_name(a2),a1
		CALLICON2 FindToolType
		move.l	d0,d5			;d5 = string after DITHER=
		beq	.tt_no_dither
		move.l	d5,a0
		lea	ttd_PIP-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_pip
		move.b	#DM_PIP,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_pip	move.l	d5,a0
		lea	ttd_WINDOW-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_window
		move.b	#DM_WINDOW,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_window	move.l	d5,a0
		lea	ttd_TRUECOLOR-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_true
		move.b	#DM_TRUECOLOR,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_true	move.l	d5,a0
		lea	ttd_HICOLOR-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_hicol
		move.b	#DM_HICOLOR,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_hicol	move.l	d5,a0
		lea	ttd_GRAY-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_gray
		move.b	#DM_GRAY,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_gray	

	ifeq	APOLLO_P96ONLY
		move.l	d5,a0
		lea	ttd_ACCUPAK-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_accupak
		move.b	#DM_ACCUPAK,DitherMode-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_accupak	
		move.l	d5,a0
		lea	ttd_DHAM8-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_dham8
		move.b	#DM_DHAM8,DitherMode-VDEC_BASE(A5)
		st	AGA_switch-VDEC_BASE(A5)
		bra	.ttd_done
.ttd_no_dham8	move.l	d5,a0
		lea	ttd_DHAM6-dos_name(a2),a1
		CALLICON2 MatchToolValue
		tst.l	d0
		beq.b	.ttd_no_dham6
		move.b	#DM_DHAM6,DitherMode-VDEC_BASE(A5)
		st	AGA_switch-VDEC_BASE(A5)
		;bra	.ttd_done
.ttd_no_dham6

	endc
.ttd_done
.tt_no_dither
NoDiskObject:

	ifne	APOLLO_CLIP
	; debug msg, whether Apollo Instructions should be used or not
		tst.b	apollo_active-VDEC_BASE(A5)
		bne.s	goapo$

		lea	ApolloOffMSG,a0
		lea	my_easygadget-ApolloOffMSG(a0),a2

		lea     my_easystruct-VDEC_BASE(A5),a1
		move.l  a0,es_TextFormat(a1)
		move.l  a2,es_GadgetFormat(a1)
		lea     my_easytitle-ApolloOffMSG(a0),a0
		move.l  a0,es_Title(a1)
		suba.l	a0,a0			;*Window
		suba.l  a2,a2                   ;*IDCMP_ptr
		CALLINT2 EasyRequestArgs

		bra	ExitMain
goapo$
	endc
		move.l	WBStartMsg-VDEC_BASE(A5),a0
		move.l	sm_NumArgs(a0),d0
		cmp.l	#2,d0				;need min. 2 args
		blt	.nowbargs

		move.l	sm_ArgList(a0),a2		;1st arg = us
		lea	wa_SIZEOF(a2),a2		;2nd arg...
		move.l	wa_Lock(a2),d1
		CALLDOS2	CurrentDir
		tst.l	wa_Name(a2)
		beq.b	.nowbargs
		move.l	wa_Name(a2),filename-VDEC_BASE(A5)

		bra	LockFile
.nowbargs	jsr	OpenFileAsl
		tst.l	d0
		beq	Error
		bra	LockFile


ReadArgs	move.l	#MPEGargs,d1
		move.l	#argers,d2
		moveq	#0,d3
		CALLDOS2	ReadArgs
		move.l	d0,dosarger-VDEC_BASE(A5)
		beq.w	Error

		lea	argers-VDEC_BASE(A5),a4
		move.l	(a4)+,a1			;FILE/M
		tst.l	a1
		beq.b	.AslFile
		move.l	(a1)+,filename-VDEC_BASE(A5)
		bra.s	.FileOK
.AslFile	st	DoAslFlag-VDEC_BASE(A5)
.FileOK
		move.l	(a4)+,a1			;FPS/K/N
		tst.l	a1
		beq.b	Arg_NoFPS
		move.l	(a1)+,FPS_value-VDEC_BASE(A5)
Arg_NoFPS
		move.l	(a4)+,a1			;ZOOM/K/N
		tst.l	a1
		beq.b	Arg_NoZOOM
		move.l	(a1)+,d1
		beq.b	Arg_NoZOOM			;Can't have ZOOM=0 !!! (crash rulez :)
		move.l	d1,ZOOM_value-VDEC_BASE(A5)
Arg_NoZOOM
		move.l	(a4)+,d1
		beq.b	Arg_NoFULL
		not.l	WinFullToggle-VDEC_BASE(A5)

		lea	PIPWinTagBLess,a1
		move.l	#1,PIPWinTagBLess-PIPWinTagBLess(a1)
		clr.l	PIPWinTagDrag-PIPWinTagBLess(a1)
		clr.l	PIPWinTagClose-PIPWinTagBLess(a1)
		clr.l	PIPWinTagDepth-PIPWinTagBLess(a1)
		clr.l	PIPWinTagSize-PIPWinTagBLess(a1)
		clr.l	WindowTitle-PIPWinTagBLess(a1)
		move.l	#1000,ZOOM_value-VDEC_BASE(A5)
Arg_NoFULL

		move.l	(a4)+,p96_switch-VDEC_BASE(A5)		;P96/S
		move.l	(a4)+,cgx_switch-VDEC_BASE(A5)		;CGX/S
		move.l	(a4)+,AGA_switch-VDEC_BASE(A5)		;AGA/S
		move.l	(a4)+,VGA_switch-VDEC_BASE(A5)		;VGA/S
	
		ifeq	APOLLO_P96ONLY			;Ignore CGX or AGA switches in P96ONLY Mode
		 clr.l	cgx_switch-VDEC_BASE(A5)
		 clr.l	AGA_switch-VDEC_BASE(A5)
		endc

	ifne	APOLLO_FULLSCREEN_DEF
		;default: HiColor Fullscreen - maps to YUV on Apollo
		tst.b	apollo_active-VDEC_BASE(A5)
		beq.s	.noapo1
		move.b  #DM_HICOLOR,DitherMode-VDEC_BASE(A5)
.noapo1
	endc

		move.l	(a4)+,d2			;DITHER/K
		beq	NoDITHER
		move.l	#DitherTemplate,d1
		CALLDOS2	FindArg
		tst.l	d0
		bmi	DitherHelp			;if invalid dither (or '?') -> Help
CheckDither	cmp.b	#0,d0
		bgt.b	NotPIP
		move.b	#DM_PIP,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
NotPIP		cmp.b	#1,d0
		bgt.b	NotWINDOW
		move.b	#DM_WINDOW,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
NotWINDOW	cmp.b	#2,d0
		bgt.b	NotTRUECOLOR
		move.b	#DM_TRUECOLOR,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
NotTRUECOLOR	cmp.b	#3,d0
		bgt.b	NotHICOLOR
		move.b	#DM_HICOLOR,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
NotHICOLOR	cmp.b	#5,d0
		bgt.b	NotGRAY
		move.b	#DM_GRAY,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
NotGRAY	
	ifeq	APOLLO_P96ONLY
		cmp.b	#6,d0
		bgt.b	.NotACCUPAK
		move.b	#DM_ACCUPAK,DitherMode-VDEC_BASE(A5)
		bra	DitherOK
.NotACCUPAK	
		cmp.b	#7,d0
		bgt.b	.NotDHAM8
		move.b	#DM_DHAM8,DitherMode-VDEC_BASE(A5)
		st	AGA_switch-VDEC_BASE(A5)
		bra	DitherOK
.NotDHAM8	cmp.b	#8,d0
		bgt.b	.NotDHAM6
		move.b	#DM_DHAM6,DitherMode-VDEC_BASE(A5)
		st	AGA_switch-VDEC_BASE(A5)
		bra	DitherOK
.NotDHAM6
	endc

DitherHelp	OUTTXT	txt_DitherHelp
		move.l	#DitherHelpBuf,d1
		moveq	#32,d2
		moveq	#0,d3
		CALLDOS2	ReadItem
		move.l	#DitherHelpBuf,d2
		move.l	#DitherTemplate,d1
		CALLDOS2	FindArg
		tst.l	d0
		bge	CheckDither
DitherOK	move.l	#1,CLIModeRequest-VDEC_BASE(A5)		;set flag to know that dither is user requested
DitherDone
NoDITHER
		move.l	(a4)+,RTG_switch-VDEC_BASE(A5)		;RTG/S
		move.l	(a4)+,NOSKIP_switch-VDEC_BASE(A5)		;NOSKIP/S
		move.l	(a4)+,verbose_switch-VDEC_BASE(A5)		;VERBOSE/S
		move.l	(a4)+,half_switch-VDEC_BASE(A5)
		;move.l	(a4)+,LOOP_switch-VDEC_BASE(A5)		;LOOP/S

		move.l	(a4)+,PlanarAssistance-VDEC_BASE(A5)

		move.l	(a4)+,d1
		beq.b	.noaudiodisable
		clr.l	GlobalAudioEnable
.noaudiodisable
		move.l	(a4)+,NOVIDEO_switch-VDEC_BASE(A5)
		move.l	(a4)+,NORENDER_switch-VDEC_BASE(A5)
		move.l	(a4)+,d0
		move.b	d0,AHI_switch-VDEC_BASE(A5)
		move.l	(a4)+,d0
		move.b	d0,P14_switch-VDEC_BASE(A5)

		move.l	(a4)+,SAVEAUDIO_name-VDEC_BASE(A5)
		move.l	(a4)+,MONOSURROUND_switch-VDEC_BASE(A5)

		move.l	(a4)+,d1
		beq.b	.nopubscr
		move.l	d1,Pubscr
.nopubscr
		move.l	(a4)+,NOP_switch-VDEC_BASE(A5)
		move.l	(a4)+,NOB_switch-VDEC_BASE(A5)

_qual_
		lea	MP12_Mono_Quality(pc),a3	;MPEGA details
;AUDIOQUALITY/K/N
		move.l	(a4)+,a1
		tst.l	a1
		bne.b	.quality_arg
		moveq	#DEF_AUDIOQUALITY,d1
		bne.b	.not_q0
		bra.s	.is_q0
.quality_arg:
		move.l	(a1),d1
		bne.b	.not_q0
.is_q0:
		clr.w	MP12_Mono_Quality-MP12_Mono_Quality(a3)		;0 = low
		clr.w	MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		clr.w	MP3_Mono_Quality-MP12_Mono_Quality(a3)
		clr.w	MP3_Stereo_Quality-MP12_Mono_Quality(a3)
		bra.b	.quality_done
.not_q0		cmp.b	#2,d1
		beq.b	.not_q2
		move.w	#2,MP12_Mono_Quality-MP12_Mono_Quality(a3)		;2 = high
		move.w	#2,MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Mono_Quality-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Stereo_Quality-MP12_Mono_Quality(a3)
		bra.b	.quality_done
.not_q2		move.w	#1,MP12_Mono_Quality-MP12_Mono_Quality(a3)		;1 = default
		move.w	#1,MP12_Stereo_Quality-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Mono_Quality-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Stereo_Quality-MP12_Mono_Quality(a3)
.quality_done

;AUDIOFREQDIV/K/N
		move.l	(a4)+,a1
		tst.l	a1
		bne.s	.freqdiv_arg

		moveq	#DEF_AUDIOFREQDIV,d1
		cmp.b	#1,d1
		bne.s	.not_d1

		; extra check: if audio frequency division is 1, the sound might be 44 kHz, so check whether we can safely enable it
		; call ECSAudioTest, then check D0
		;  D0 < 0 - OCS chipset
		;     = 0 - ECS/AGA but P96 variable not set
		;     > 0 - proper settings, assume 44 kHz audio is safe
		bsr	ECSAudioTest
		tst.l	d0				;ECS with proper P96 settings found ?
		bgt.s	.is_d1				;yes, accept

		moveq	#2,d1
		bra.s	.not_d1
.freqdiv_arg:
		move.l	(a1),d1
		cmp.b	#1,d1
		bne.b	.not_d1
.is_d1:
		clr.l	audio_freqdiv-MP12_Mono_Quality(a3)			;0 = -d1
		move.w	#1,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#1,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		bra.b	.freqdiv_done
.not_d1		cmp.b	#2,d1
		bne.b	.not_d2
		move.l	#1,audio_freqdiv-MP12_Mono_Quality(a3)		;1 = -d2
		move.w	#2,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#2,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		bra.b	.freqdiv_done
.not_d2		move.l	#2,audio_freqdiv-MP12_Mono_Quality(a3)		;2 = -d4
		move.w	#4,MP12_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP12_Stereo_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP3_Mono_FreqDiv-MP12_Mono_Quality(a3)
		move.w	#4,MP3_Stereo_FreqDiv-MP12_Mono_Quality(a3)
.freqdiv_done

;DEFAULTDIR/K
		move.l	(a4)+,a1
		tst.l	a1
		beq.b	.no_defaultdir
		move.l	a1,ASLDrawerString-MP12_Mono_Quality(a3)
.no_defaultdir

;B=BORDERLESS/S
		move.l	(a4)+,a1
		tst.l	a1
		beq.b	.no_borderless
		move.l	#1,BORDERLESS_switch
.no_borderless

	; debug msg, whether Apollo Instructions should be used or not
		tst.b	apollo_active-VDEC_BASE(A5)
		beq.s	noapo$
		; we are on apollo
		OUTTXT	ApolloOnMSG			;print that Apollo was detected
		bra.s	goapo$
noapo$
		OUTTXT	ApolloOffMSG			;print that Apollo was detected
	ifne	APOLLO_CLIP
		bra	ExitMain
	endc
goapo$
		;TEST: check whether align works
		;lea	main-1,a1
		;OUTHEX	a1
		;ALIGN_PTR	a1
		;OUTHEX	a1

		tst.b	DoAslFlag-VDEC_BASE(A5)
		beq.b	.DontDoAsl
		jsr	OpenFileAsl
		tst.l	d0
		beq	Error
.DontDoAsl

;		tst.l	PlanarAssistance
;		beq.b	DefaultPIPFormat
;		cmp.b	#DM_ACCUPAK,DitherMode
;		bne.b	DefaultPIPFormat
;		move.l	#RGBFB_Y4U1V1,PIPwinformat
;DefaultPIPFormat:

LockFile:	;VDEC_BASE still in A5
		move.l	filename-VDEC_BASE(A5),d1
		move.l	#ACCESS_READ,d2
		CALLDOS2	Lock				;Attempt to lock file
		move.l	d0,filelock-VDEC_BASE(A5)		;Store Lock
		bne.b	.FileLocked
		OUTTXT	LockErrMsg			;If can't lock File, Output error msg
		bra.w	Error				;and close up...
.FileLocked

;_GetFileSize:
		moveq		#DOS_FIB,d1
		moveq		#0,d2
		CALLDOS2	AllocDosObject
		move.l		d0,fileinfo-VDEC_BASE(A5)
		move.l		filelock-VDEC_BASE(A5),d1
		move.l		d0,d2
		CALLDOS2	Examine
		move.l		fileinfo-VDEC_BASE(A5),a0
		move.l		fib_Size(a0),file_size-VDEC_BASE(A5)
;		move.l		fib_DirEntryType(a0),d4
		moveq		#DOS_FIB,d1
		move.l		fileinfo-VDEC_BASE(A5),d2
		CALLDOS2	FreeDosObject

;AllocVidBuffers	
		move.l		#VBUF_SIZE,d0
		moveq		#0,d1
		CALLEXEC 	AllocMem
		move.l		d0,vbuf_1-VDEC_BASE(A5)
		beq		Error
		move.l		#VBUF_SIZE,d0
		moveq		#0,d1
		CALLEXEC 	AllocMem
		move.l		d0,vbuf_2-VDEC_BASE(A5)
		beq		Error

;AllocAudBuffers	
		move.l		#ABUF_SIZE,d0
		moveq		#0,d1
		CALLEXEC 	AllocMem
		move.l		d0,abuf_1-VDEC_BASE(A5)
		beq		Error
		move.l		#ABUF_SIZE,d0
		moveq		#0,d1
		CALLEXEC 	AllocMem
		move.l		d0,abuf_2-VDEC_BASE(A5)
		beq		Error

;OpenGfx		
		lea		gfx_name,a1
		moveq		#39,d0
		CALLEXEC 	OpenLibrary
		move.l		d0,gfxbase-VDEC_BASE(A5)
		beq.w		Error

;OpenAsyncFile
		move.l		filename-VDEC_BASE(A5),d1
		move.l		#MODE_OLDFILE,d2
		CALLDOS2	Open				;Lets Open that File...
		move.l		d0,filehandle-VDEC_BASE(A5)	;And store the File Handle for Read()
		bne.b		.OpenOK
		OUTTXT		OpenErrMsg			;Can't open file...
		bra		Error
.OpenOK
;OpenDebugFile:	move.l	#debugname,d1
;		move.l	#MODE_NEWFILE,d2
;		CALLDOS2	Open
;		move.l	d0,debugfile
;		beq	Error


	;---------------------------------------------------------------------------------------------------
	;Check if input stream contains an MPEG Video sequence or system stream and find initial offset...
	;---------------------------------------------------------------------------------------------------

		moveq	#0,d5				;d5 = global file pointer

		move.l		vbuf_1-VDEC_BASE(A5),d4
		add.l		#VBUF_SIZE-4,d4			;d4 = vbuf max addr.

		move.l		filehandle-VDEC_BASE(A5),d1
		move.l		vbuf_1-VDEC_BASE(A5),d2
		addq.l		#4,d2
		move.l		#VBUF_SIZE-4,d3
		CALLDOS2	Read
		tst.l		d0
		ble		Error

		move.l		vbuf_1-VDEC_BASE(A5),a4
		addq.l		#4,a4
SearchMPEGLoop	cmp.l		#pack_start_code,(a4)
		beq.b		FoundSystem
		cmp.l		#sequence_header_code,(a4)
		beq		FoundSequence
		addq.l		#1,a4
		cmp.l		d4,a4
		blt		SearchMPEGLoop

		add.l		#VBUF_SIZE-4,d5			;increment global file pointer
		move.l		d4,a4
		move.l		vbuf_1-VDEC_BASE(A5),a0
		move.l		(a4),(a0)
		move.l		filehandle-VDEC_BASE(A5),d1
		move.l		vbuf_1-VDEC_BASE(A5),d2
		addq.l		#4,d2
		move.l		#VBUF_SIZE-4,d3
		CALLDOS2	Read
		tst.l		d0
		bgt.b		.bufok

		OUTTXT		NotMpegMsg
		bra		Error

.bufok		move.l		vbuf_1-VDEC_BASE(A5),a4
		bra.b		SearchMPEGLoop

FoundSystem	move.l		#1,SystemStreamFlag
		move.l		#SBUF_SIZE,d0			;allocate system stream buffer!
		moveq		#0,d1
		CALLEXEC	 AllocMem
		move.l		d0,sysbuf-VDEC_BASE(A5)
		beq		Error

		bra.b		FoundMPEG

FoundSequence	clr.l		SystemStreamFlag
		clr.l		GlobalAudioEnable

FoundMPEG	suba.l		vbuf_1-VDEC_BASE(A5),a4
		add.l		a4,d5
		subq.l		#4,d5
		move.l		d5,MPEG_Start_Offset

		CALLEXEC CreateMsgPort
		move.l	d0,AsyncIOControlMsgPort
		CALLEXEC CreateMsgPort
		move.l	d0,AsyncIOControlReplyPort
		CALLEXEC CreateMsgPort
		move.l	d0,AudioControlMsgPort
		CALLEXEC CreateMsgPort
		move.l	d0,AudioControlReplyPort

		CALLEXEC CreateMsgPort			;Create Msg port for audio communication
		move.l	d0,AudioBufferMsgPort
		CALLEXEC CreateMsgPort
		move.l	d0,AudioBufferReplyPort

		lea		AsyncIOTaskTags,a1
		move.l		a1,d1
		CALLDOS2	CreateNewProc			;create AsyncIO subtask...
		move.l		d0,AsyncIOTask
		beq		Error

		tst.l		GlobalAudioEnable
		beq.b		.noaudiotask
		lea	AudioTaskTags,a1
		move.l	a1,d1
		CALLDOS2	CreateNewProc			;create Audio subtask...
		move.l	d0,AudioTask
		beq	Error

		move.l	AudioControlMsgPort(pc),a0
		move.l	AudioTask(pc),MP_SIGTASK(a0)
		move.l	AudioBufferMsgPort(pc),a0
		move.l	AudioTask(pc),MP_SIGTASK(a0)
		move.l	AudioBufferReplyPort(pc),a0
		move.l	AsyncIOTask(pc),MP_SIGTASK(a0)

		lea	AudioControlMsg(pc),a0
		move.b	#NT_MESSAGE,LN_TYPE(a0)
		move.w	#AUDC_SIZE,MN_LENGTH(a0)
		move.l	AudioControlReplyPort(pc),MN_REPLYPORT(a0)

		lea	AudioBufferMsg(pc),a0
		move.b	#NT_MESSAGE,LN_TYPE(a0)
		move.w	#AUDB_SIZE,MN_LENGTH(a0)
		move.l	AudioBufferReplyPort(pc),MN_REPLYPORT(a0)

.noaudiotask

		move.l	AsyncIOControlMsgPort(pc),a0
		move.l	AsyncIOTask(pc),MP_SIGTASK(a0)

		lea	AsyncIOControlMsg(pc),a0
		move.b	#NT_MESSAGE,LN_TYPE(a0)
		move.w	#AIOC_SIZE,MN_LENGTH(a0)
		move.l	AsyncIOControlReplyPort(pc),MN_REPLYPORT(a0)


		bsr	VideoDecoderStart


***************************************************************************
***************************************************************************
****-------------------------------------------------------------------****
****------------------ Close File, Libraries and Exit -----------------****
****-------------------------------------------------------------------****
***************************************************************************
***************************************************************************

Error:		;OUTTXT	msgmainerr

ExitMain:	;OUTTXT	mmsg1
		lea	VDEC_BASE,a5			;internal variables base (not fully implemented right now, sorry -> that's why several reloads of A5 take place)

	ifne	IDCT_COUNT
		OUTTXT  .dct
		OUTDEC	(idct_counters)
		OUTTXT	.next
		OUTDEC	(idct_counters+4)
		OUTTXT	.next
		OUTDEC	(idct_counters+8)
		OUTTXT	.next
		OUTDEC	(idct_counters+12)
		OUTTXT	.next
		OUTDEC	(idct_counters+16)
		OUTTXT	.next
		OUTDEC	(idct_counters+20)
		OUTTXT	.next
		OUTDEC	(idct_counters+24)
		OUTTXT	.next
		OUTDEC	(idct_counters+28)
		OUTTXT	.next

		OUTTXT	.nl
		bra.s	.done
.nl:		dc.b	10,0
.dct:		dc.b	"DCT Counters ",0
.next:		dc.b	" ",0	
		cnop	0,4
.done:
	endc

		tst.l	AsyncIOTask(pc)
		beq.b	.AsyncIOTaskDown
		move.l	AsyncIOControlMsgPort(pc),a0
		lea	AsyncIOControlMsg(pc),a1
		move.b	#AIOCMD_EXIT,AIOC_COMMAND(a1)
		CALLEXEC PutMsg				;Signal AsyncIO task to close down
		move.l	AsyncIOControlReplyPort(pc),a0
		CALLEXEC WaitPort			;Wait for confirmation...
.AsyncIOTaskDown

		tst.l	AudioTask(pc)
		beq.b	.AudioTaskDown

		st	AudioAbortFlag

		move.l	#SIGBREAKF_CTRL_C|SIGBREAKF_CTRL_D|SIGBREAKF_CTRL_F,d0
		move.l	AudioTask(pc),a1
		CALLEXEC Signal

		VBLDELAY	10

		move.l	AudioBufferMsgPort(pc),a0
		lea	AudioBufferMsg(pc),a1
		move.b	#AUDCMD_ABORT,AUDB_COMMAND(a1)
		CALLEXEC PutMsg				;Signal Audio task to close down
		;move.l	AudioBufferReplyPort(pc),a0
		;CALLEXEC WaitPort			;Wait for confirmation...
		move.l	AudioBufferReplyPort(pc),a0
		CALLEXEC GetMsg				;Remove reply from port

		move.l	#SIGBREAKF_CTRL_D,d0
		CALLEXEC Wait				;Wait for Audio Shutdown!!!!

.AudioTaskDown
		move.l	AsyncIOControlMsgPort(pc),d7
		beq.b	.nocontrolport0
		move.l	d7,a0				;loop to remove all messages
		CALLEXEC DeleteMsgPort			;if no more messages, then delete port
.nocontrolport0
		move.l	AsyncIOControlReplyPort(pc),d7
		beq.b	.noreplyport0
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
.noreplyport0
		move.l	AudioControlMsgPort(pc),d7
		beq.b	.nocontrolport1
		move.l	d7,a0				;loop to remove all messages
		CALLEXEC DeleteMsgPort			;if no more messages, then delete port
.nocontrolport1
		move.l	AudioControlReplyPort(pc),d7
		beq.b	.noreplyport1
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
.noreplyport1
		move.l	AudioBufferMsgPort(pc),d7	;delete msg port for audio communication
		beq.b	.noabufmsgport
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
.noabufmsgport
		move.l	AudioBufferReplyPort(pc),d7	;delete msg port for audio communication
		beq.b	.noabufrepport
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
.noabufrepport

		bsr.w	CloseDisplay
		bsr.w	CloseGraphics

.CloseFileReq:	move.l	AslFileReq-VDEC_BASE(A5),d7
		beq.b	.CloseASL
		move.l	d7,a0
		CALLASL	FreeAslRequest
.CloseASL:	move.l	aslbase-VDEC_BASE(A5),d7
		beq.b	.noasl
		move.l	d7,a1
		CALLEXEC CloseLibrary
		clr.l	aslbase-VDEC_BASE(A5)
.noasl

.CloseIntuition:	
		move.l	intbase-VDEC_BASE(A5),d7
		beq	.noint
		move.l	d7,a1
		CALLEXEC CloseLibrary
.noint:

.FreeSysBuf:	move.l	sysbuf,d7
		beq.b	.FreeVBuf1
		move.l	d7,a1
		move.l	#SBUF_SIZE,d0
		CALLEXEC FreeMem
		clr.l	sysbuf
.FreeVBuf1:	move.l	vbuf_1-VDEC_BASE(A5),d7
		beq.b	.FreeVBuf2
		move.l	d7,a1
		move.l	#VBUF_SIZE,d0
		CALLEXEC FreeMem
		clr.l	vbuf_1-VDEC_BASE(A5)
.FreeVBuf2:	move.l	vbuf_2-VDEC_BASE(A5),d7
		beq.b	.FreeABuf1
		move.l	d7,a1
		move.l	#VBUF_SIZE,d0
		CALLEXEC FreeMem
		clr.l	vbuf_2-VDEC_BASE(A5)
.FreeABuf1:	move.l	abuf_1-VDEC_BASE(A5),d7
		beq.b	.FreeABuf2
		move.l	d7,a1
		move.l	#ABUF_SIZE,d0
		CALLEXEC FreeMem
		clr.l	abuf_1-VDEC_BASE(A5)
.FreeABuf2:	move.l	abuf_2-VDEC_BASE(A5),d7
		beq.b	.BuffersFree
		move.l	d7,a1
		move.l	#ABUF_SIZE,d0
		CALLEXEC FreeMem
		clr.l	abuf_2-VDEC_BASE(A5)
.BuffersFree:

.CloseFile:	move.l	filehandle-VDEC_BASE(A5),d1
		beq.b	.UnlockFile
		CALLDOS2	Close
.UnlockFile:	tst.l	filelock-VDEC_BASE(A5)
		beq.b	.FileClosed
		move.l	filelock-VDEC_BASE(A5),d1
		CALLDOS2	UnLock
.FileClosed:

;CloseDebugFile:	move.l	debugfile(pc),d1
;		beq	DebugClosed
;		CALLDOS2	Close
;DebugClosed:

		move.l	RiVADiskObject-VDEC_BASE(A5),d7
		beq.b	.nodiskobject
		move.l	d7,a0
		CALLICON2 FreeDiskObject
.nodiskobject
		move.l	iconbase-VDEC_BASE(A5),d7
		beq.b	.noiconlib
		move.l	d7,a1
		CALLEXEC CloseLibrary
.noiconlib

.FreeArgs:	
		move.l	dosarger-VDEC_BASE(A5),d1
		beq.b	.ArgsFree
		CALLDOS2	FreeArgs
.ArgsFree:

.CloseDOS:	move.l	dosbase-VDEC_BASE(A5),a1
		CALLEXEC CloseLibrary

		move.l  WBStartMsg-VDEC_BASE(A5),d0
		beq.b	.nemwb
		CALLEXEC Forbid
		move.l  WBStartMsg-VDEC_BASE(A5),a1
		CALLEXEC ReplyMsg
.nemwb:

EXIT		clr.l	d0
		rts

; Input:  D0 - input pointer (string, 0 terminated)
;         A1 - output pointer (long)
; Output: *A1 - converted value
rva_StrToLong:
		movem.l		d0-d2,-(sp)
		move.l		d0,d1
		move.l		a1,d2
		CALLDOS2	StrToLong
		movem.l		(sp)+,d0-d2
		rts

***************************************************************************
****-------------------------------------------------------------------****
****---------------------- ECS availability test ----------------------****
****-------------------------------------------------------------------****
***************************************************************************
; call ECSAudioTest, then check D0
;  D0 < 0 - OCS chipset
;     = 0 - ECS/AGA but P96 variable not set
;     > 0 - proper settings, assume 44 kHz audio is safe
;
		include	"ecstest.i"


;----------------------------------------------------------------------------------
				EVEN
MainTask:			dc.l	0
AsyncIOTask:			dc.l	0
AudioTask:			dc.l	0
MPEG_Start_Offset:		dc.l	0
SystemStreamFlag:		dc.l	0
skipthis:			dc.l	0

AsyncIOControlMsgPort:		dc.l	0
AsyncIOControlReplyPort:	dc.l	0

AudioControlMsgPort:		dc.l	0
AudioControlReplyPort:		dc.l	0

AudioBufferMsgPort:		dc.l	0
AudioBufferReplyPort:		dc.l	0

				EVEN
AsyncIOTaskTags:		dc.l	NP_Entry,AsyncIOTaskStart
				dc.l	NP_StackSize,10000
				dc.l	NP_Name,AsyncIOTaskName
				dc.l	NP_Priority,OPT_ASYNCPRI
				dc.l	TAG_END

				EVEN
AudioTaskTags:			dc.l	NP_Entry,AudioTaskStart
				dc.l	NP_StackSize,10000
				dc.l	NP_Name,AudioTaskName
				dc.l	NP_Priority,OPT_AUDIOPRI
				dc.l	TAG_END

				EVEN
AsyncIOTaskName:		dc.b	"RiVA AsyncIO subtask",0
				EVEN
AudioTaskName:			dc.b	"RiVA Audio subtask",0

;debugname			dc.b	"ram:debugfile.txt",0
;audioname			dc.b	"ram:Audio.mp2",0
				EVEN
;debugfile			dc.l	0

;----------------------------------------------------------------------------------

KeyInputCheck:
		movem.l	a0-a5/d0/d5-d7,-(a7)

		move.l	#SIGBREAKF_CTRL_C,d1
		CALLDOS2	CheckSignal				;Check if CTRL+C was pressed!
		move.l	d0,AbortFlag
		move.l	d0,CtrlCAbort
		bne	KeyInputCheckDone

		move.l	MainWindow,a0
		move.l	wd_UserPort(a0),a0
		CALLEXEC GetMsg
		tst.l	d0
		beq	KeyInputCheckDone
		move.l	d0,a1
		move.l	im_Class(a1),d5
		move.w	im_Code(a1),d6
		move.w	im_Qualifier(a1),d7
		move.l	im_IAddress(a1),a4
		jsr	_LVOReplyMsg(a6)

		tst.l	ScreenHandle
		beq.b	CheckWindow

CheckScreen	cmp.l	#IDCMP_MOUSEBUTTONS,d5			;else check in case of screen playback
		bne.b	CheckWindowDone
		st	AbortFlag				;exit at mousepress
		bra	KeyInputCheckDone

CheckWindow	cmp.l	#IDCMP_CLOSEWINDOW,d5			;check in case of window playback
		bne.b	CheckWindowDone				;close!
		st	AbortFlag
		bra	KeyInputCheckDone
CheckWindowDone:

CheckRawKey:	cmp.l	#IDCMP_RAWKEY,d5
		bne	KeyInputCheckDone

		cmp.b	#$c5,d6					;esc
		bne.b	nemesc
		st	AbortFlag
		bra	KeyInputCheckDone
nemesc:
		cmp.b	#$40,d6					;SPACE BAR
		bne.b	notspace

		not.b	PauseFlag				;st	PauseFlag
		bra	KeyInputCheckDone

;		tst.l	AGA_switch				;if not AGA, no HALF option, so don't try to toggle it
;		beq	notspace
;
;		move.b	DitherMode,d1
;		tst.l	half_switch
;		beq.b	.sethalf
;		clr.l	half_switch
;		bra.b	.toggledone
;sethalf	move.l	#1,half_switch
;.toggledone	bsr	OpenDisplay

notspace:
		cmp.b	#$5e,d6
		bne.b	.noplus
		add.l	#10,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.noplus
		cmp.b	#$4a,d6
		bne.b	.nominus
		move.l	ZOOM_value,d1
		cmp.l	#10,d1
		ble.b	.nominus
		sub.l	#10,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nominus
		cmp.b	#$01,d6
		bne.b	.nozoom1
		move.l	#100,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nozoom1
		cmp.b	#$02,d6
		bne.b	.nozoom2
		move.l	#150,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nozoom2
		cmp.b	#$03,d6
		bne.b	.nozoom3
		move.l	#200,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nozoom3
		cmp.b	#$04,d6
		bne.b	.nozoom4
		move.l	#300,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nozoom4
		cmp.b	#$05,d6
		bne.b	.nozoom5
		move.l	#400,ZOOM_value
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.nozoom5
		cmp.b	#$06,d6
		bne.b	.nozoom6
		move.l	#500,ZOOM_value
		bsr	OpenDisplay
.nozoom6

		cmp.b	#$44,d6			;Enter
		bne	.notEnter

		tst.l	AGA_switch
		bne	.notEnter

		move.b	DitherMode,d1

		cmp.b	#DM_GRAY,d1		;if gray screen, don't allow flipping to window mode
		beq	.notEnter		;because missing Cb/Cr data in reference blocks would cause artifacts
		cmp.b	#DM_PIP,d1
		beq.b	.togglepip
	ifeq	APOLLO_P96ONLY
		cmp.b	#DM_ACCUPAK,d1
		beq.b	.togglepip
	endc
		cmp.b	#DM_WINDOW,d1
		beq.b	.togglewindow

		move.b	d1,DitherModeSave
		move.b	#DM_WINDOW,DitherMode
		bsr	OpenDisplay
		bra	KeyInputCheckDone

.togglewindow	move.b	DitherModeSave,d1
		beq.b	.nosavedDM
		move.b	d1,DitherMode
		bra.b	.openscreen
.nosavedDM
	ifne	APOLLO_P96ONLY
		move.b	#DM_HICOLOR,DitherMode
	else
		move.b	#DM_TRUECOLOR,DitherMode
		move.l	PubScreenDepth,d1
		cmp.b	#16,d1
		bgt.b	.nothicolor
		move.b	#DM_HICOLOR,DitherMode
.nothicolor
	endc

.openscreen	bsr	OpenDisplay
		bra	KeyInputCheckDone

.togglepip	not.l	WinFullToggle
		bne	.fullpip
		move.l	WinNormalZoom,ZOOM_value
		clr.l	PIPWinTagBLess
		moveq	#1,d1
		move.l	d1,PIPWinTagDrag
		move.l	d1,PIPWinTagClose
		move.l	d1,PIPWinTagDepth
		move.l	d1,PIPWinTagSize
		
		tst.l	BORDERLESS_switch
		bne.s	.piptitle_ignore
		move.l	#WA_Title,WindowTitle-4
		move.l	#PIPtitle,WindowTitle
		bra	.piptitle_done
.piptitle_ignore
		move.l	#TAG_IGNORE,WindowTitle-4
		move.l	#0,WindowTitle+0
.piptitle_done
		
		bsr	OpenDisplay
		bra	KeyInputCheckDone
.fullpip	move.l	ZOOM_value,WinNormalZoom
		move.l	#1000,ZOOM_value
		move.l	#1,PIPWinTagBLess
		clr.l	PIPWinTagDrag
		clr.l	PIPWinTagClose
		clr.l	PIPWinTagDepth
		clr.l	PIPWinTagSize
		clr.l	WindowTitle
		bsr	OpenDisplay
		bra	KeyInputCheckDone
		
.notEnter
	; FKeys support disabled (again)
	ifne	0
		move.b	d6,d7
		and.b	#$f0,d7
		cmp.b	#$50,d7
		bne.b	.noFkeys
		move.b	d6,d7
		and.b	#$0f,d7
		cmp.b	#$09,d7
		bgt.b	.noFkeys
		extb.l	d7
		move.l	file_size,d0
		divu.l	#10,d0
		mulu.l	d7,d0
		bsr	MPEGSeek
		suba.l	a0,a0
		moveq	#0,d0
		bsr	ReadVideoBuffer			;read buffer
		bra	KeyInputCheckSeek
.noFkeys
	endc
KeyInputCheckDone
		movem.l	(a7)+,a0-a5/d0/d5-d7
		rts


KeyInputCheckSeek
		lea	12(a7),a7		;don't restore a0/d0 (they're zero after seek!) & no rts either
		bra	FindNextIFrame

DitherModeSave	dc.b	0

	ifne	CODE_ALIGN
		CNOP    0,16
	endc
		include	HuffmanTables.i

***************************************
	ifne	CODE_ALIGN
		CNOP    0,16
	else
		CNOP	0,4
	endc
		
AsyncIOTaskStart
		move.l	#PARSE_START,SystemParseMode
		move.l	filehandle,d1
		move.l	MPEG_Start_Offset,d2
		moveq	#OFFSET_BEGINNING,d3
		CALLDOS2 Seek				;seek to start of mpeg stream (skip any garbage :)

AsyncNextBuffer	clr.l	AsyncEOF_Flag
		clr.l	AsyncErrorFlag
		not.l	buftoggle
		beq.b	.buf2
.buf1:		move.l	vbuf_1,vbuf_active
		move.l	abuf_1,abuf_active
		move.l	vbuf_2,vbuf_back
		move.l	abuf_2,abuf_back
		bra.b	.CopyOverlap
.buf2:		move.l	vbuf_2,vbuf_active
		move.l	abuf_2,abuf_active
		move.l	vbuf_1,vbuf_back
		move.l	abuf_1,abuf_back
.CopyOverlap:	move.l	vbuf_active,a1		;else copy overlap from active buffer to background buffer
		add.l	#VBUF_MARGIN,a1			;start address in active buffer in a1
		move.l	vbuf_back,a2		;start address in background buffer in a2
		move.l	#VBUF_OVERLAP/4,d7		;overlap length in d7
.wraploop:	move.l	(a1)+,(a2)+
		subq.l	#1,d7
		bne.b	.wraploop
		bsr	ReadMPEGBuffer
		tst.l	d0
		bne.b	NextBufReadOK
		not.l	buftoggle			;if failed reading buffer
		;st	AsyncErrorFlag
		st	AsyncEOF_Flag
NextBufReadOK

AsyncWaitCmd

		lea	AudioBufferMsg(pc),a1			;send msg to audio task!
		move.l	abuf_back,AUDB_BUFFER_MIN(a1)
		move.l	audbufmax,AUDB_BUFFER_MAX(a1)
		move.l	AudioBufferMsgPort,a0
		CALLEXEC PutMsg


WaitVideo	move.l	AsyncIOControlMsgPort,a0
		CALLEXEC WaitPort
VideoMsg	move.l	AsyncIOControlMsgPort,a0
		CALLEXEC GetMsg				;Get message
		move.l	d0,a0
		move.b	AIOC_COMMAND(a0),d1
		cmp.b	#AIOCMD_EXIT,d1			;if EXIT command
		beq	AsyncExit
		cmp.b	#AIOCMD_READ,d1			;if READ command
		beq	AsyncReplyRead
		cmp.b	#AIOCMD_SEEK,d1
		beq	AsyncSeek
		move.l	d0,a1
		CALLEXEC ReplyMsg
		bra	AsyncWaitCmd			;if unknown command

AsyncReplyRead

		tst.l	AsyncErrorFlag
		bne	AsyncError
		lea	AsyncIOControlMsg(pc),a1
		tst.l	AsyncEOF_Flag(pc)
		beq.b	.noteof
		move.b	#AIOREP_EOF,AIOC_REPLY(a1)
		bra.b	.replydone
.noteof		move.b	#AIOREP_OK,AIOC_REPLY(a1)
.replydone	move.l	vbuf_back,AIOC_VIDBUFMIN(a1)
		move.l	vidbufmax,AIOC_VIDBUFMAX(a1)
		;move.l	filehandle,a0
		;move.l	fh_Pos(a0),AIOC_FILEPOSITION(a1)	; Ez nem a fileban levõ pozició! (enforcer hit)
		CALLEXEC ReplyMsg

		tst.l	GlobalAudioEnable(pc)
		beq.b	.audmsgok



		move.l	AudioBufferReplyPort(pc),a0
		CALLEXEC WaitPort

		move.l	AudioBufferReplyPort(pc),a0
		CALLEXEC GetMsg
		move.l	d0,a1
		move.b	AUDB_REPLY(a1),d1
		cmp.b	#AUDREP_OK,d1
		beq.b	.audmsgok
		clr.l	GlobalAudioEnable		;if audio error, disable audio decoding!
.audmsgok
		bra	AsyncNextBuffer


AsyncExit
		lea	AsyncIOControlMsg(pc),a1
		CALLEXEC ReplyMsg
		rts
**********************************************
AsyncError	lea	AsyncIOControlMsg(pc),a1
		move.b	#AIOREP_ERROR,AIOC_REPLY(a1)
		CALLEXEC ReplyMsg
		bra	AsyncWaitCmd
**********************************************
AsyncSeek	move.l	d0,a0				;message
		move.l	AIOC_DATA(a0),d2
		add.l	MPEG_Start_Offset(pc),d2	;seek is offset from actual start of stream (not start of file)
		move.l	filehandle,d1
		moveq	#OFFSET_BEGINNING,d3
		CALLDOS2 Seek
		lea	AsyncIOControlMsg(pc),a1
		move.b	#AIOREP_OK,AIOC_REPLY(a1)
		CALLEXEC ReplyMsg
		clr.l	last_a0				;for system stream parse
		clr.l	last_d4
		move.l	#PARSE_START,SystemParseMode
		bra	AsyncNextBuffer
**********************************************
		EVEN
AsyncErrorFlag	dc.l	0
AsyncEOF_Flag:	dc.l	0
EOF_Flag	dc.l	0
vidbufmax	dc.l	0
audbufmax	dc.l	0
buftoggle:	dc.l	0

AudioIsDecoding	dc.l	0

			EVEN
AsyncIOControlMsg:	FILL	AIOC_SIZE,0

			EVEN
AudioControlMsg:	FILL	AUDC_SIZE,0

			EVEN
AudioBufferMsg:		FILL	AUDB_SIZE,0

			EVEN
mpegastream		dc.l	0

			EVEN
MPEGA_Control		dc.l	MPEGA_Hook
MPEGA_Layer_1_2		dc.w	0			;Force Mono flag (0 or 1)
MPEGA_Layer_1_2_Mono	
MP12_Mono_FreqDiv	dc.w	4			;Frequency Devision (1, 2 or 4)
MP12_Mono_Quality	dc.w	2			;Quality (2=best, 1=medium, 0=worst)
			dc.l	0			;Freq_max for Automatic mono freq_dif
MPEGA_Layer_1_2_Stereo
MP12_Stereo_FreqDiv	dc.w	4			;Frequency Devision (1, 2 or 4)
MP12_Stereo_Quality	dc.w	2			;Quality (2=best, 1=medium, 0=worst)
			dc.l	0			;Freq_max Automatic mono freq_dif
MPEGA_Layer_3		dc.w	0			;Force Mono flag (0 or 1)
MPEGA_Layer_3_Mono
MP3_Mono_FreqDiv	dc.w	4			;Frequency Devision (1, 2 or 4)
MP3_Mono_Quality	dc.w	2			;Quality (2=best, 1=medium, 0=worst)
			dc.l	0			;Freq_max for Automatic mono freq_dif
MPEGA_Layer_3_Stereo
MP3_Stereo_FreqDiv	dc.w	4			;Frequency Devision (1, 2 or 4)
MP3_Stereo_Quality	dc.w	2			;Quality (2=best, 1=medium, 0=worst)
			dc.l	0			;Freq_max Automatic mono freq_dif
			dc.w	0			;Check MPEG (0=don't check, 1=check)
			dc.l	2048			;Bitstream buffer size (0=default)

			EVEN
MPEGA_Hook		dc.l	0,0			;min node structure
			dc.l	MPEGABitStreamAccess
			dc.l	0,0

;		EVEN
;mpegaopen:	dc.b	"MPEGA_Open()",10,0
;		EVEN
;mpegaread:	dc.b	"MPEGA_Read()",10,0
;		EVEN
;mpegaseek:	dc.b	"MPEGA_Seek()",10,0
;		EVEN
;mpegaclose:	dc.b	"MPEGA_Close()",10,0
;getnewbuf:	dc.b	"getnewbuf",10,0
;waitbuf:	dc.b	"AUDIO: waiting for buf",10,0
;gotbuf:		dc.b	"AUDIO: got buf",10,0
		EVEN

MPEGABitStreamAccess:
;a0 struct Hook  *hook, 
;a2 APTR          handle
;a1 MPEGA_ACCESS *access

		movem.l	d1-a6,-(a7)
		move.l	MPAACC_FUNC(a1),d1
		cmp.l	#MPEGA_BSFUNC_OPEN,d1
		beq.b	MPEGA_Open
		cmp.l	#MPEGA_BSFUNC_READ,d1
		beq.b	MPEGA_Read
		cmp.l	#MPEGA_BSFUNC_SEEK,d1
		beq.b	MPEGA_Seek

MPEGA_Close	;OUTTXT	mpegaclose
		moveq	#0,d0
		movem.l	(a7)+,d1-a6
		rts

MPEGA_Seek	;OUTTXT	mpegaseek
		moveq	#-1,d0
		movem.l	(a7)+,d1-a6
		rts

MPEGA_Open	;OUTTXT	mpegaopen
		;clr.l	MPAACC_OPEN_STREAM_SIZE(a1)	;don't know stream size
		moveq	#1,d0
		movem.l	(a7)+,d1-a6
		rts

MPEGA_Read	;OUTTXT	mpegaread

		tst.l	audiobuffer
		bne.b	.gotaudiobuf

		bsr	GetNewBuf
.gotaudiobuf
		tst.l	AudioAbortFlag
		bne.b	.eof
		move.l	abuf_position,a3		;source
		move.l	MPAACC_READ_BUFFER(a1),a4	;dest
		move.l	abuf_max,d2			;max

		moveq	#0,d0
		move.l	MPAACC_READ_NUM_BYTES(a1),d1

.loop		cmp.l	d2,a3
		blt.b	.bufok
		bsr.b	GetNewBuf
		tst.l	AudioAbortFlag
		bne.b	.eof
		move.l	abuf_position,a3
		move.l	abuf_max,d2
.bufok		move.b	(a3)+,(a4)+
		addq.l	#1,d0
		subq.l	#1,d1
		bne.b	.loop

.done		move.l	a3,abuf_position

		tst.l	audiofile
		beq.b	.nosaveaudio
		movem.l	d0-a6,-(a7)
		move.l	audiofile,d1
		move.l	MPAACC_READ_BUFFER(a1),d2	;d2=buffer
		move.l	d0,d3				;d3=size
		CALLDOS2 Write
		movem.l	(a7)+,d0-a6
.nosaveaudio

		tst.l	AudioAbortFlag
		beq.b	.noabort
.eof		moveq	#0,d0
.noabort	movem.l	(a7)+,d1-a6

		rts

GetNewBuf	movem.l	d0-a6,-(a7)
		move.l	AudioBufferMsgPort(pc),a0
		CALLEXEC WaitPort
		move.l	AudioBufferMsgPort(pc),a0
		CALLEXEC GetMsg
		move.l	d0,a1
		cmp.b	#AUDCMD_ABORT,AUDB_COMMAND(a1)
		bne.b	.gotbuf
		st	AudioAbortFlag
		bra.b	.bufrep
.gotbuf		move.l	AUDB_BUFFER_MIN(a1),audiobuffer
		move.l	AUDB_BUFFER_MIN(a1),abuf_position
		move.l	AUDB_BUFFER_MAX(a1),abuf_max
.bufrep		move.b	#AUDREP_OK,AUDB_REPLY(a1)
		CALLEXEC ReplyMsg
		movem.l	(a7)+,d0-a6
		rts

;--------------------------------------------------------------------------------------------------
ReadMPEGBuffer:
		tst.l	SystemStreamFlag
		bne.b	.ReadSystem

.ReadNotSystem:	moveq	#0,d6				;d6 = sum of bytes read
		moveq	#4,d4
.ReadBufLoop:	move.l	filehandle,d1
		move.l	vbuf_back,d2		;read into background buffer
		add.l	#VBUF_OVERLAP,d2		;read after overlap
		add.l	d6,d2				;read after previous read
		move.l	#VBUF_MARGIN/4,d3		;read BUFSIZE-OVERLAP bytes
		CALLDOS2	Read
		tst.l	d0
		ble.b	.ChkError			;if error, do error handling!

		cmp.l	#VBUF_MARGIN/4,d0
		slt	AsyncEOF_Flag			;if read less than requested ---> EOF!

		add.l	d0,d6
		moveq	#1,d1
		CALLDOS2	Delay				;preempt our task
		subq.l	#1,d4
		bne.b	.ReadBufLoop
		bra.b	.BufRead

.ReadSystem:	bsr	SystemStreamParse		;CRASH HERE
		cmp.l	#VBUF_MARGIN,d6
		beq.b	.BufRead

.ChkError:	tst.l	d6
		ble	.ReadBufEOF			;if nothing in buffer, exit to error
		sgt	AsyncEOF_Flag			;else set error flag
.BufRead:
		move.l	abuf_back,d1		;audio buffer...
		add.l	d7,d1
		move.l	d1,audbufmax

		cmp.l	#VBUF_MARGIN,d6
		beq.b	.BufferFull			;if buffer full, normal vbuf_max
		move.l	vbuf_back,d1		;else no more overlap
		add.l	#VBUF_OVERLAP,d1
		add.l	d6,d1
		move.l	d1,vidbufmax
		bra.b	.BufMaxDone

.BufferFull:	move.l	vbuf_back,d1		;maximum address in background buffer
		add.l	d6,d1
		move.l	d1,vidbufmax

.BufMaxDone:	moveq	#-1,d0
		rts

.ReadBufEOF:	moveq	#0,d0
		rts

;----------------------------------------------------------------------------------------
SystemStreamParse:

		move.l	vbuf_back,d1
		add.l	#VBUF_SIZE,d1
		move.l	d1,vbuf_end

		move.l	last_a0,a0
		move.l	last_d4,d4
		moveq	#0,d0

		move.l	vbuf_back,a1		;video buffer
		add.l	#VBUF_OVERLAP,a1
		move.l	abuf_back,a2		;audio buffer

		move.l	SystemParseMode,d1
		cmp.l	#PARSE_VIDEO,d1
		beq	videodata
		;cmp.l	#PARSE_AUDIO,d1			;audio buffer must never be full with current method!!
		;beq	audiodata

		suba.l	a0,a0
		moveq	#0,d0
		bsr	ReadSystemBuffer
		bra.b	Pack_Parser_main

Pack_Parserpre:
		SKP32

Pack_Parser:	
		NEXT_START_CODE_SYSTEM
		CHK32	d1

Pack_Parser_main:
		CHK32	d1
		cmp.l	#user_data_start_code,d1
		ble	Pack_Parserpre

		cmp.l	#system_header_start_code,d1		
		bhi	Parse_Packet

		cmp.l	#padding_stream_code,d1
		beq	system_header_parsed		;xif (fasza, mindjart padding..)

multiple_pack_start_code:
		SKP32					;skip system stream header ($000001ba)
		SKP32					;skip another 64 bit 
		SKP32					;

Parse_Pack:	CHK32		d1
		cmp.l		#system_header_start_code,d1		
		beq.b		system_header_parse		;required in first pack!
		cmp.l		#system_header_start_code,d1		
		bhi.w		Parse_Packet
		bra.w		Pack_Parsed			;pack parse finished

system_header_parse:
		SKP32					;skip system header start code ($000001bb)
		NGETBITS	16,d4,d2		;get number of bytes in header
		SKP32
		SKP16		;addq.l	#6,a0		;skip rest of system header (48 bits)

system_header_parse_loop:
		CHECK_SYSBUFFER
		CHKBITS		1,d2
		cmp.b		#1,d2
		bne.b		system_header_parsed
		NSKPBITS	24,d2			;addq.l #3,a0	;don't need these either :)
		bra		system_header_parse_loop
system_header_parsed:

Parse_Packet:	CHK32		d2
		cmp.l		#pack_start_code,d2
		bne		nonewpackstart
		bra		multiple_pack_start_code

nonewpackstart:
		NSKPBITS	24,d5			;addq.l	3,a0	;skip packet_start_code_prefix (0x000001)
		GET8		d5			;move.b		(a0)+,d5		;get stream ID
		moveq		#0,d4			;get number of bytes in the rest of packet (important!)
		GET16		d4			;move.w		(a0)+,d4
		cmp.b		#$BF,d5
		beq.w		private_data_stream2
		cmp.b		#$BE,d5
		beq		packet_padding_loop		
		cmp.b		#$c0,d5
		beq		packet_padding_loop
		cmp.b		#$e0,d5
		beq		packet_padding_loop		
		bra		Pack_Parser		;illegal packet

packet_padding_loop:
		CHECK_SYSBUFFER
		CHKBITS		8,d2
		cmp.b		#$ff,d2
		bne.b		no_packet_padding
		NSKPBITS	8,d2			;skip 8bit padding
		subq.l		#1,d4			;decrease packet remain loop
		bra		packet_padding_loop

no_packet_padding:
		CHKBITS		2,d2			;buffer scale and size next
		cmp.b		#%01,d2
		bne.b		no_buffer_scale_and_size
		addq.l		#2,a0
		subq.l		#2,d4			;decrease packet remain loop

no_buffer_scale_and_size:				;time_stamp() start here
		CHKBITS		4,d2			;buffer scale and size next
		cmp.b		#%0010,d2
		bne.b		not_only_PTS_in_time_stamp
		NSKPBITS	4,d2			;skip 4bit constant

;get presentation time stamp
		NSKPBITS	36,d1,d2
		subq.l		#5,d4			;decrease packet remain loop
		bra.w		time_stamp_parsed

not_only_PTS_in_time_stamp:
		cmp.b		#%0011,d2
		bne.w		no_PTS_or_DTS
		addq.l		#5,a0
		subq.l		#5,d4			;decrease packet remain loop
		addq.l		#5,a0

		subq.l		#5,d4			;decrease packet remain loop

		bra.s		time_stamp_parsed
no_PTS_or_DTS:
		NSKPBITS	8,d2			;skip 8 bit constant (%00001111)
		subq.l		#1,d4			;decrease packet remain loop
time_stamp_parsed:

private_data_stream2:

write_packet_data_bytes:
		BYTEALIGN	;make sure, D0 is empty of byte offsets (a0=actual stream pos)

		tst.l	d4
		ble	endwritepacket

		tst.l	skipthis
		bne	.skipthisstuff
		cmp.b	#$e0,d5
		beq	videodata
		cmp.b	#$c0,d5
		beq	audiodata
		cmp.b	#$be,d5
		beq	paddingdata

.skipthisstuff:	clr.l	skipthis

.skip_misc_data:	
		CHECK_SYSBUFFER
		addq.l	#1,a0
		subq.l	#1,d4
		bne.b	.skip_misc_data
		bra	endwritepacket

videodata:	tst.l	d4
		ble	endwritepacket

		BYTEALIGN	;make sure, D0 is empty of byte offsets (a0=actual stream pos)

		CHECK_SYSBUFFER

		move.l	vbuf_end,d1
		sub.l	a1,d1				;align to end
		and.l	#15,d1
		beq.b	.aligndone
.alignloop	cmp.l	vbuf_end,a1
		bge	escape_video
		move.b	(a0)+,(a1)+
		subq.l	#1,d4
		subq.l	#1,d1
		bgt.b	.alignloop
.aligndone:
		move.l	d4,d5
		asr.l	#4,d5
		ble.b	_longloopend

_longloop:	CHECK_SYSBUFFER
		cmp.l	vbuf_end,a1
		bge	escape_video
		move.l	(a0)+,(a1)+
		move.l	(a0)+,(a1)+
		move.l	(a0)+,(a1)+
		move.l	(a0)+,(a1)+
		sub.l	#16,d4
		subq.l	#1,d5
		bgt.b	_longloop

_longloopend:	tst.l	d4
		ble	endwritepacket

_byteloop:	cmp.l	vbuf_end,a1
		bge	escape_video
		move.b	(a0)+,(a1)+
		subq.l	#1,d4
		bgt.b	_byteloop
		bra.b	endwritepacket

.dataok		move.b	(a0)+,(a1)+
		subq.l	#1,d4
		bgt	videodata
		bra.b	endwritepacket


audiodata:
		tst.l	d4
		ble	endwritepacket

		BYTEALIGN	;make sure, D0 is empty of byte offsets (a0=actual stream pos)

		CHECK_SYSBUFFER

		move.b	(a0)+,(a2)+
		subq.l	#1,d4
		bgt.b	audiodata
		bra.b	endwritepacket

paddingdata:	tst.l	d4
		ble	endwritepacket

		CHECK_SYSBUFFER

		addq.l	#1,a0
		subq.l	#1,d4
		bne.b	paddingdata

endwritepacket:

Pack_Parsed:	bra	Pack_Parser

escape_video:	move.l	#PARSE_VIDEO,SystemParseMode
		move.l	a0,last_a0
		move.l	d4,last_d4
		move.l	#VBUF_MARGIN,d6			;number of bytes read into video buffer
		move.l	a2,d7
		sub.l	abuf_back,d7
		rts

sysparse_exit:	move.l	a1,d6
		sub.l	vbuf_back,d6		;size of video read
		move.l	a2,d7
		sub.l	abuf_back,d7		;size of audio read
		rts

sysparse_error:	moveq	#-1,d6
		rts

;-------------------------------------------------------------------------------------------------

;----------------------------------------------------
ReadSystemBuffer
		movem.l	d0-d3/a1-a2,-(a7)

		movem.l	d0-d1/a0-a1,-(a7)
		moveq	#1,d1
		CALLDOS2	Delay
		movem.l	(a7)+,d0-d1/a0-a1

		tst.l	a0
		beq.b	.firstsysbuf

		move.l	sysbuf,a1
		move.l	a1,a2
		add.l	#SBUF_MARGIN,a1
		move.l	#SBUF_OVERLAP/4,d1
.syswraploop	move.l	(a1)+,(a2)+
		subq.l	#1,d1
		bne.b	.syswraploop
		sub.l	#SBUF_MARGIN,a0
		bra.b	.readsysfile

.firstsysbuf:	move.l	sysbuf,a0
		add.l	#SBUF_OVERLAP,a0

.readsysfile:	move.l	a0,sbuf_pointer
		move.l	filehandle,d1
		move.l	sysbuf,d2
		add.l	#SBUF_OVERLAP,d2
		move.l	#SBUF_MARGIN,d3
		CALLDOS2	Read
		tst.l	d0
		ble.b	.sysreaderror

		cmp.l	#SBUF_MARGIN,d0
		beq.b	.bufferfull			;if buffer full, normal sbuf_max

		move.l	sysbuf,d1
		add.l	#SBUF_OVERLAP,d1
		add.l	d0,d1
		bra.b	.maxok

.bufferfull:	move.l	sysbuf,d1
		add.l	d0,d1

.maxok:		move.l	d1,sbuf_max
		move.l	sbuf_pointer,a0

.sysreadok:	movem.l	(a7)+,d0-d3/a1-a2
		rts

.sysreaderror:	movem.l	(a7)+,d0-d3/a1-a2
		addq.l	#4,a7
		bra	sysparse_exit

;-----------------------------------------------------

;MPEGSeek(position)
;            d0

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

MPEGSeek	clr.l	EOF_Flag
		move.l	AsyncIOControlMsgPort,a0
		lea	AsyncIOControlMsg,a1
		move.b	#AIOCMD_SEEK,AIOC_COMMAND(a1)
		move.l	d0,AIOC_DATA(a1)		;seek position
		CALLEXEC PutMsg
		move.l	AsyncIOControlReplyPort,a0
		CALLEXEC WaitPort
		move.l	AsyncIOControlReplyPort,a0
		CALLEXEC GetMsg
		clr.l	bwd_reference_y		;clear forward and backward reference
		clr.l	fwd_reference_y
		rts

;ReadVideoBuffer(vbuf_position)
;                    a0
	ifne	CODE_ALIGN
		CNOP    0,16
	endc
		
ReadVideoBuffer:
		movem.l	d0-d7/a1-a6,-(a7)

		tst.l	EOF_Flag
		bne	.vidend

		move.l	a0,vbuf_position
		move.l	vidbufnew,vidbufold		;previous new buffer will now be old buffer

		move.l	AsyncIOControlMsgPort,a0
		lea	AsyncIOControlMsg,a1
		move.b	#AIOCMD_READ,AIOC_COMMAND(a1)
		CALLEXEC PutMsg
		move.l	AsyncIOControlReplyPort,a0
		CALLEXEC WaitPort
		move.l	AsyncIOControlReplyPort,a0
		CALLEXEC GetMsg
		move.l	d0,a0
		cmp.b	#AIOREP_ERROR,AIOC_REPLY(a0)
		beq	.vidabort
		cmp.b	#AIOREP_EOF,AIOC_REPLY(a0)
		bne.b	.noteof
		st	EOF_Flag
.noteof
		move.l	AIOC_FILEPOSITION(a0),file_position
		move.l	AIOC_VIDBUFMIN(a0),vidbufnew
		move.l	AIOC_VIDBUFMAX(a0),vbuf_max

.calcnewpos:	tst.l	vbuf_position
		bne.b	.wraparound			;if valid buffer pointer, wrap around

		move.l	vidbufnew,a0		;if invalid (NULL) buffer pointer, then initialise buffer pointer
		lea	VBUF_OVERLAP(a0),a0
		move.l	a0,vbuf_position
		bra.b	.exit_OK

.wraparound:	move.l	vbuf_position,a0		;valid address, do wraparound
		sub.l	vidbufold,a0		;offset from base of background (previous) buffer
		sub.l	#VBUF_MARGIN,a0			;wrap offset back
		add.l	vidbufnew,a0		;add offset to base of new buffer
		move.l	a0,vbuf_position

.exit_OK:
		movem.l	(a7)+,d0-d7/a1-a6
		rts

.vidabort	movem.l	(a7)+,d0-d7/a1-a6
		addq.l	#4,a7
		bra	MPEGExit

.vidend		clr.l	EOF_Flag
		movem.l	(a7)+,d0-d7/a1-a6
		addq.l	#4,a7
		bra	MPEGEnd



		EVEN
pcm_buffer	dc.l	pcm_buffer_1
		dc.l	pcm_buffer_2
AudioSamplesPlayed:	dc.l	0		;number of samples played

GlobalAudioEnable	dc.l	1

AudioPort_L:		dc.l	0
AudioPort_R:		dc.l	0
AudioIORequest_L1:	dc.l	0
AudioIORequest_L2:	dc.l	0
AudioIORequest_R1:	dc.l	0
AudioIORequest_R2:	dc.l	0
AudioDevice_L:		dc.l	-1
AudioDevice_R:		dc.l	-1
audiochans_L:		dc.b	2,4		;left chans
audiochans_R:		dc.b	1,8		;right chans

audio_freqdiv	dc.l	2			;freq division (0 = -d1, 1 = -d2, 2 = -d4)

AudioIsPlaying	dc.l	0

	;keep these 6 together
SampleData1:	dc.l	0
SampleData2:	dc.l	0
SampleData1R:	dc.l	0	;unused for AHI
SampleData2R:	dc.l	0	;unused for AHI
SampleData3:	dc.l	0	;HQ_TRIPLEBUF
SampleData3R:	dc.l	0	;HQ_TRIPLEBUF
	ifne	HQ_TRIPLEBUF
NSampleData	EQU	6
	else
NSampleData	EQU	4
	endc


abuftoggle	dc.l	0

AudioAbortFlag	dc.l	0

AudioClock:	dc.l	3546895
AudioPeriod:	dc.w	0
AudioFrequency:	dc.l	0

ahilink		dc.l	0
TMP_PamelaAlloc	dc.b	0

		cnop	0,4

aa_AudioAlloc:	dc.l	IOERR_OPENFAIL
aa_IOAudioRequest:
		dc.l    0,0
		dc.b    NT_REPLYMSG,ADALLOC_MAXPREC
		dc.l    0
		dc.l    aa_AudioReplyPort
		dc.w    0
		dc.l    0,0
		dc.w    ADCMD_ALLOCATE
		dc.b    ADIOF_NOWAIT,0
		dc.w    0
		dc.l    aa_Channels,1
		dc.w    0,0,0
		dc.l    0,0
		dc.b    0,0
		dc.l    0
aa_AudioReplyPort:
		dc.l    0,0
		dc.b    NT_MSGPORT,0
		dc.l    0
		dc.b    0,0
		dc.l    0
aa_AudioRPLH:
		dc.l    -1
		dc.l    0
		dc.l    aa_AudioRPLH
		dc.b    0
		dc.b    0
aa_Channels:	dc.b    1+2+4+8

		even
audint0:	
	        dc.l    0
	        dc.l    0
	        dc.b    2
	        dc.b    127
	        dc.l    INT_Name1               ;Name
	        dc.l    0
	        dc.l    AmpOut_Paula14_INT_Aud  ;Interrupt Routine

OldInt0:	dc.l	0
;used for Pamela, too
paula_free:	dc.b	0	;if != 0, then a new address pair may be written
paula_int:	dc.b	0	;if != 0, then somebody is waiting
paula_started:	dc.b	0	;if != 0, then we`re running
		dc.b	0
paula_sigtask:	dc.l	0	
AmpOut_Pamela16_OLDINT0:	dc.l	0
FilterBufferL:	dc.l	0
FilterBufferR:	dc.l	0
highboost_shift: dc.l	1
AudioSamplesPlayed_D	dc.l	0	;"sent" samples, copied to AudioSamplesPlayed delayed by 1 frame
		EVEN
aa_audiodevname:
AudioDevName	dc.b	"audio.device",0
		EVEN
AHIDevName	dc.b	"ahi.device",0
VResName:	dc.b	"vampire.resource",0
INT_Name1:	dc.b	"RiVA",0
;		EVEN
;writeaud	dc.b	"Writing Audio",10,0
;		EVEN
;sampledone	dc.b	"Sample Done",10,0
***********************************************************************************************
* The Audio Decoder                                                                           *
***********************************************************************************************
		EVEN

;--------------------- return whether this output hardware is available --------------------------------
;input:  A5 - datas
;output: D0 - 1=yes, 0=no
;regs:   all saved (except D0)
AmpOut_Pamela16_Check:
	movem.l	d1-a6,-(sp)
	moveq	#0,d0
	move.l	4.w,a6
	btst	#AFB_68080-8,AttnFlags(A6)	;current Exec with 68080 flag ?
	beq.s	.noapollo

	bsr	AllocAudioPamela
	tst.l	d0
	beq.s	.noapollo
	
;        lea     VResName(pc),a1		;done by AllocAudioPamela
;        jsr     _LVOOpenResource(a6)
;        move.l  d0,vresbase
;	beq.s	.noapollo

	; tricky part: identify Pamela by the new Vampire Interrupt
	jsr	_LVODisable(A6)

	lea	$dff000,a0		;Custom_Base
	move	#$80,d0
	move	#$8080,d4

	move	intenar(a0),d1			;original intena
	move	P16INTENAR-$dff000(a0),d2	;pamela intena
	and.w	d0,d1				;saved paula int0 bit
	and.w	d0,d2				;saved pamela int0 bit (if present)

	move	d0,intena(a0)		 ;clear audio bit 0
	move	d4,P16INTENA-$dff000(a0) ;enable Pamela audio bit 0
	move	intenar(a0),d3		 ;get paula audio bit 0
	and	d0,d3			 ;Bit set now in Paula Intena ?
	beq.s	.nopaulaintena
	; ok. Paula Audio Interrupt is set because the address mirrored
	eor.w	d0,d1			 ;delete bit 80 if it wasn't on before (d1 will be 0x80 in that case)
	move	d1,intena(a0)		 ;either delete int0 bit or do nothing
	moveq	#0,d0
	bra.s	.paulaintena

	;-- continue checking for Pamela --
.nopaulaintena:
	or.w	#$8000,d1		 ;Paula Int0 is confirmed off now
	move	d1,intena(a0)		 ;enable Paula Int0 if it was on before

	;now restore Pamela Int0, if necessary
	move	#0,intena(a0)		  ;clear Bus
	move	P16INTENAR-$dff000(a0),d1 ;read pamela
	and	d4,d1			  ;compare just these two bits $8080
	cmp	d0,d1			  ;if we did find Pamela, this bit will be set
	beq.s	.foundpam		  ;if it was memory, it would be $8080, if garbage, hmm...
	moveq	#0,d0
	bra.s	.paulaintena
.foundpam:
	eor	d0,d2			 ;if Pam bit was set, clear it, if it was off, set it
	move	d2,P16INTENA-$dff000(a0) ;restore PAM Intena

	moveq	#1,d0			;looks like Pam
.paulaintena:
	jsr	_LVOEnable(A6)

	move.l	d0,d7
	bsr	FreeAudioPamela
	move.l	d7,d0

	;moveq	#1,d0			;done above
.noapollo:

	tst.l	d0
	bne.s	.found

;	lea     msg_PamelaHW,a0		;Infotext ausgeben (Requester)
;       bsr     Merror
;       clr.l	vresbase

	moveq	#0,d0

.found:
	movem.l	(sp)+,d1-a6
	rts

AllocAudioPamela:
	movem.l	d1-a6,-(sp)
	lea	VResName(pc),a1
	move.l	4.w,a6
        jsr     _LVOOpenResource(a6)
        tst.l	d0
        beq.s	.fail

        move.l  d0,a6
	lea     INT_Name1(pc),a1
        moveq   #V_PAMELA_45,d0
        jsr     V_AllocExpansionPort(a6)
	tst.l	d0
	bne.s	.fail
	st	TMP_PamelaAlloc
	moveq	#1,d0
	bra.s	.ok
.fail:
	moveq	#0,d0
.ok
	movem.l	(sp)+,d1-a6
	rts

FreeAudioPamela:
	movem.l	d1-a6,-(sp)

	lea	VResName(pc),a1
	move.l	4.w,a6
        jsr     _LVOOpenResource(a6)
        tst.l	d0
        beq.s	.fail

	tst.b	TMP_PamelaAlloc
	beq.s	.fail
	sf	TMP_PamelaAlloc

	move.l  d0,a6
        moveq   #V_PAMELA_45,d0
        jsr     V_FreeExpansionPort(a6)
.fail
	movem.l	(sp)+,d1-a6
	rts

	;TODO: Stop audio
;	bsr	StartPamela
StartPamela:
	movem.l	d1-a6,-(sp)
	move.l	d0,d6			;length in bytes
	move.l	d1,d7			;period
	move.l	a0,a2			;left
	move.l	a1,a3			;right

	move.w	#$3,P16DMACON		;DMA Stop
	move.w	#$100,P16INTENA		;Interrupt disable (second audio)
	move.w	#$100,P16INTREQ		;Interrupt Request reset 

	move.l	4.w,a6
	suba.l	a1,a1
	jsr	_LVOFindTask(a6)
	move.l	d0,paula_sigtask	;re-use Paula sigtask here

	movec	vbr,a0
	move.l	P16VECTOR(a0),AmpOut_Pamela16_OLDINT0
	lea	AmpOut_Pamela16_INT_Aud(pc),a1
	move.l	a1,P16VECTOR(a0)

	move.l	d6,d0			;length in bytes
	move.l	d7,d1			;period
	move.l	a2,a0			;left
	move.l	a3,a1			;right
	bsr	Pamela16_Setpointers

	lea	P16AUD0,a1
	move	#$8030,P16ADKCON-P16AUD0(A1)	;16 bit mode for channels 4,5
	move.w	#$8100,P16INTENA-P16AUD0(a1)
	move.w	#$8003,P16DMACON-P16AUD0(a1)	;DMA Start

	movem.l	(sp)+,d1-a6
	rts

StopPamela:
	movem.l	d1-a6,-(sp)

	move.w	#$3,P16DMACON		;DMA Stop
	move.w	#$100,P16INTENA		;Interrupt disable (second audio)
	move.w	#$100,P16INTREQ		;Interrupt Request Hardwarehack
	move	#$0030,P16ADKCON	;clear 16 bit mode for channels 4,5

	move.l	AmpOut_Pamela16_OLDINT0,d0
	beq.s	.nooldint
	movec	vbr,a0
	move.l	d0,P16VECTOR(a0)
	clr.l	AmpOut_Pamela16_OLDINT0
.nooldint:

	movem.l	(sp)+,d1-a6
	rts

AmpOut_Pamela16_INT_Aud:
;	move	#$8030,P16ADKCON	;16 bit mode for channels 4,5

	move	#$100,$DFF29C		;clear int

	st	paula_free		;new address latched, next address may be written

	tst.b	paula_int(pc)
	beq.s	.noint
	sf	paula_int

	movem.l	d0-d1/a0/a1/a6,-(sp)

	move.l	paula_sigtask(pc),d0
	beq.s	.nosig
	move.l	d0,a1
	move.l	#SIGBREAKF_CTRL_F,d0
	move.l	4.w,a6
	jsr	_LVOSignal(a6)
.nosig
	movem.l	(sp)+,d0-d1/a0/a1/a6
.noint:
	rte


Pamela16_Setpointers:
	movem.l	a0/a1/a2,-(sp)

	lea	P16AUD0,a2
	lsr.l	#1,d0				;length bytes -> length words

	move.l	a0,$dff0a0-$dff0a0(a2)
	move	d0,4+$dff0a0-$dff0a0(a2)
	move	d1,6+$dff0a0-$dff0a0(a2)
	move	#$40,8+$dff0a0-$dff0a0(a2)

	move.l	a1,$dff0b0-$dff0a0(a2)
	move	d0,4+$dff0b0-$dff0a0(a2)
	move	d1,6+$dff0b0-$dff0a0(a2)
	move.w	#$40,8+$dff0b0-$dff0a0(a2)

	movem.l	(sp)+,a0/a1/a2
	rts

;
;in:
; A0 = left channel
; A1 = right channel
; D0 = length in Bytes
; D1 = period
;
Pamela16_SendAudio:
	movem.l	d0/d1/d6/d7/a2/a3/a6,-(sp)
	
	tst.b	paula_started(pc)			;not started yet ?
	bne.s	.running

	sf	paula_int		;
	sf	paula_free		;new address latched, next address may be written
	bsr	StartPamela		;A0/A1/d0/d1
	st	paula_started		;not started yet

	bra	.exit
.running:
	move.l	d0,d6
	move.l	d1,d7
	move.l	a0,a2
	move.l	a1,a3

	move.l	4.w,a6
	move	#$4000,$dff29a

.wloop:
	tst.b	paula_free(pc)
	bne.s	.set

	st	paula_int		;want interrupt
	move.l	#SIGBREAKF_CTRL_F|SIGBREAKF_CTRL_C,d0	;
	move	#$c000,$dff29a
	jsr	_LVOWait(a6)		;

	move	#$4000,$dff29a

	and.l	#SIGBREAKF_CTRL_C,d0
	bne.s	.quit
	
	bra.s	.wloop
.set
	move.l	d6,d0
	move.l	d7,d1
	move.l	a2,a0
	move.l	a3,a1
	bsr	Pamela16_Setpointers
	sf	paula_free
	move	#$c000,$dff29a

.wait2
	tst.b	paula_free(pc)
	bne.s	.exit

	st	paula_int		;want interrupt
	move.l	#SIGBREAKF_CTRL_F|SIGBREAKF_CTRL_C,d0	;
	jsr	_LVOWait(a6)		;
	and.l	#SIGBREAKF_CTRL_C,d0
	bne.s	.quit
	bra.s	.wait2
.quit:
	st	AudioAbortFlag
.exit:
	movem.l	(sp)+,d0/d1/d6/d7/a2/a3/a6
	rts


;-- be a nice citizen and allocate audio channels --
; code originates from startup code by Bifat/TEK, slightly 
; adapted by Bax
; returns: 1=OK, 0=ERROR
AllocAudio:
	movem.l	d1-a6,-(sp)
	lea	aa_AudioReplyPort(pc),a5

	move.l	4.w,a6
	suba.l	a1,a1
	jsr	_LVOFindTask(a6)                ; find own task
	move.l	d0,aa_AudioReplyPort+MP_SIGTASK-aa_AudioReplyPort(a5)
	moveq	#-1,d0                          ; no preference
	jsr	_LVOAllocSignal(a6)             ; get signal and
	move.b	d0,aa_AudioReplyPort+MP_SIGBIT-aa_AudioReplyPort(a5)  ; put it in IORequest

	lea	aa_audiodevname(pc),a0
	lea	aa_IOAudioRequest(pc),a1
	moveq	#0,d0
	moveq	#0,d1
	jsr	_LVOOpenDevice(a6)              ; open audio device
	move.l	d0,aa_AudioAlloc-aa_AudioReplyPort(a5) ; success status

	cmp.l	#IOERR_OPENFAIL,d0              ; error on open?
	beq.s	aa_open_failed
	cmp.l	#ADIOERR_ALLOCFAILED,d0         ; allocation successful?
	bne.s	aa_open_success
aa_open_failed:
	lea	aa_IOAudioRequest(pc),a1
	jsr	_LVOCloseDevice(a6)             ; close audio device
	moveq	#0,d0
	bra.s	aa_open_end
aa_open_success:
	moveq	#1,d0
aa_open_end:
	movem.l	(sp)+,d1-a6
	rts

FreeAudio:
	movem.l d0-a6,-(sp)
	move.l	aa_AudioAlloc(pc),d0
	cmp.l	#IOERR_OPENFAIL,d0
	beq.s	aa_close_ret
	cmp.l	#ADIOERR_ALLOCFAILED,d0
	beq.s	aa_close_ret
	
	move.l	4.w,a6
	moveq   #0,d0
	move.b  aa_AudioReplyPort+MP_SIGBIT(pc),d0
	jsr     _LVOFreeSignal(a6)              ; free sigbit

	lea     aa_IOAudioRequest(pc),a1
	jsr     _LVOCloseDevice(a6)             ; close audio device

	lea	aa_AudioAlloc(pc),a0
	move.l	#IOERR_OPENFAIL,(a0)
aa_close_ret:
	movem.l (sp)+,d0-a6
	rts


StartPaula:
	movem.l	d1-a6,-(sp)
	move.l	d0,d6			;length in bytes
	move.l	d1,d7			;period
	move.l	a0,a2			;left
	move.l	a1,a3			;right
	
	move.w	#15,$dff096		;disable Audio DMA
	move.w	#$780,$dff09a		;disable Audio interrupt
	move.w	#$780,$dff09c		;disable pending Audio interrupts
	move.w	#$780,$dff09c		;disable pending Audio interrupts again (A4000)

	move.l	4.w,a6
	suba.l	a1,a1
	jsr	_LVOFindTask(a6)
	move.l	d0,paula_sigtask

	moveq	#10,d0			;AUD3 interrupt
	lea	audint0(pc),a1
	jsr	-162(a6)		;_LVOSetInterruptVector
	move.l	d0,OldInt0

	move.l	dosbase,d1
	beq.s	.nowait
	move.l	d1,a6
	moveq	#1,d1
	jsr	_LVODelay(a6)
.nowait
	move.l	d6,d0			;length in bytes
	move.l	d7,d1			;period
	move.l	a2,a0			;left
	move.l	a3,a1			;right
	bsr	Paula14_Setpointers

	lea	$dff0a0,a1		;

	move	#$8400,$dff09a-$dff0a0(a1)	;enable first audio interrupt only

	moveq	#15,d0
	bset	#15,d0
	move	d0,$dff096-$dff0a0(a1)		;enable DMA -> audio starts

	st	paula_started			;not started yet

	movem.l	(sp)+,d1-a6
	rts

Paula14_Setpointers:
	movem.l	a0/a1/a2,-(sp)
	lea	$dff0a0,a2		;

	lsr.l	#1,d0			;length in words

	move.l	a0,$dff0a0-$dff0a0(a2)
	move	d0,4+$dff0a0-$dff0a0(a2)
	move	d1,6+$dff0a0-$dff0a0(a2)
	move	#$40,8+$dff0a0-$dff0a0(a2)

	lea	P14_OFF_lowchannel(a0),a0

	move.l	a0,$dff0d0-$dff0a0(a2)
	move	d0,4+$dff0d0-$dff0a0(a2)
	move	d1,6+$dff0d0-$dff0a0(a2)
	move	#$1,8+$dff0d0-$dff0a0(a2)

	move.l	a1,$dff0b0-$dff0a0(a2)
	move	d0,4+$dff0b0-$dff0a0(a2)
	move	d1,6+$dff0b0-$dff0a0(a2)
	move	#$40,8+$dff0b0-$dff0a0(a2)

	lea	P14_OFF_lowchannel(a1),a1

	move.l	a1,$dff0c0-$dff0a0(a2)
	move	d0,4+$dff0c0-$dff0a0(a2)
	move	d1,6+$dff0c0-$dff0a0(a2)
	move	#$1,8+$dff0c0-$dff0a0(a2)

	movem.l	(sp)+,a0/a1/a2
	rts


StopPaula:
	movem.l	d1-a6,-(sp)

	move.w	#15,$dff096		;disable Audio DMA
	move.w	#$780,$dff09a		;disable Audio interrupt
	move.w	#$780,$dff09c		;disable pending Audio interrupts
	move.w	#$780,$dff09c		;disable pending Audio interrupts again (A4000)

	tst.b	paula_started			;not started yet
	beq.s	.noint

	move.l	4.w,a6
	moveq	#10,d0			;AUD3 interrupt
	move.l	OldInt0,a1		;no custom interrupt
	jsr	-162(a6)		;_LVOSetInterruptVector
.noint:
	movem.l	(sp)+,d1-a6
	rts

AmpOut_Paula14_INT_Aud:
	tst.w	$dff01e
	move.w	#$780,$dff09c

	st	paula_free		;new address latched, next address may be written

	tst.b	paula_int(pc)
	beq.s	.noint
	sf	paula_int

	movem.l	d0-d1/a0/a1/a6,-(sp)

	move.l	paula_sigtask(pc),d0
	beq.s	.nosig
	move.l	d0,a1
	move.l	#SIGBREAKF_CTRL_F,d0
	move.l	4.w,a6
	jsr	_LVOSignal(a6)
.nosig
	movem.l	(sp)+,d0-d1/a0/a1/a6
.noint:
	rts

;
;in:
; A0 = left channel
; A1 = right channel
; D0 = length in Bytes
; D1 = period
;
P14_OFF_lowchannel EQU	16*MPEGA_PCM_SIZE

Paula14_SendAudio:
	movem.l	d0/d1/d6/d7/a2/a3/a6,-(sp)
	
	tst.b	paula_started(pc)			;not started yet ?
	bne.s	.running

	sf	paula_int		;
	sf	paula_free		;new address latched, next address may be written
	bsr	StartPaula			;A0/A1/d0/d1

	bra	.exit
.running:
	move.l	d0,d6
	move.l	d1,d7
	move.l	a0,a2
	move.l	a1,a3

	move.l	4.w,a6
	jsr	_LVODisable(a6)		;move	#$4000,$dff09a

.wloop:
	tst.b	paula_free(pc)
	bne.s	.set

	st	paula_int		;want interrupt
	move.l	#SIGBREAKF_CTRL_F|SIGBREAKF_CTRL_C,d0	;
	jsr	_LVOWait(a6)		;

	and.l	#SIGBREAKF_CTRL_C,d0
	bne.s	.quit

	bra.s	.wloop
.set
	move.l	d6,d0
	move.l	d7,d1
	move.l	a2,a0
	move.l	a3,a1
	bsr	Paula14_Setpointers
	sf	paula_free
	jsr	_LVOEnable(A6)		;move	#$c000,$dff09a

	ifne	HQ_TRIPLEBUF
		bra.s	.exit
	else
.wait2
		tst.b	paula_free(pc)
		bne.s	.exit

		st	paula_int		;want interrupt
		move.l	#SIGBREAKF_CTRL_F|SIGBREAKF_CTRL_C,d0	;
		jsr	_LVOWait(a6)		;

		and.l	#SIGBREAKF_CTRL_C,d0
		bne.s	.quit

		bra.s	.wait2
	endc
.quit:
		st	AudioAbortFlag
.exit:
	movem.l	(sp)+,d0/d1/d6/d7/a2/a3/a6
	rts


AudioTaskStart:
		lea	VDEC_BASE,a5

		;Open Audio File
		move.l	SAVEAUDIO_name-VDEC_BASE(a5),d1
		beq.b	.noaudiofile
		move.l	#MODE_NEWFILE,d2
		CALLDOS2	Open
		move.l	d0,audiofile-VDEC_BASE(a5)
		bne	.audiofileopen
		clr.l	SAVEAUDIO_name-VDEC_BASE(A5)
.audiofileopen
.noaudiofile

OpenMPEGA	lea	mpega_name(pc),a1
		moveq	#0,d0
		CALLEXEC OpenLibrary
		move.l	d0,mpegabase-VDEC_BASE(A5)
		beq	AudioError

		tst.b	P14_switch-VDEC_BASE(A5)
		bne	OpenPaula14
;		tst.b	P16_switch-VDEC_BASE(A5)	;currently auto-selects Pamela16
;		bne	OpenPamela16

		tst.b	AHI_switch-VDEC_BASE(A5)
		bne	OpenAHIDev

		bra	OpenAudioDev

OpenPamela16:
		bsr	AllocAudioPamela
		tst.l	d0
		beq.s	.fail

		lea	SampleData1,a4
		moveq	#NSampleData-1,d7
.buffers:
		move.l	#32*MPEGA_PCM_SIZE,d0		;*2 for 16 Bit buffer
		move.l	#MEMF_FAST|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		move.l	d0,(a4)+
		beq.s	.fail
		dbf	d7,.buffers

		move.l	gfxbase,a1
		move.l	gb_DisplayFlags(a1),d1
		btst	#PALn,d1
		beq.b	.PALClock
		move.l	#3579594,AudioClock		;NTSC
.PALClock
		bset	#1,$bfe001			;disable filter

		sf	P14_switch-VDEC_BASE(A5)
		st	P16_switch-VDEC_BASE(A5)
		bra	AudioOpened
	;Pamela16 failed
.fail:
		bsr	FreeAudioPamela

		lea	SampleData1,a4
		moveq	#NSampleData-1,d7
.nobuffers:
		move.l	(a4)+,d0
		beq.s	.next
		move.l	d0,a1
		CALLEXEC FreeVec
.next
		dbf	d7,.nobuffers

		sf	P16_switch-VDEC_BASE(A5)
		bra.s	ForceOpenPaula14		;skip the Pamela check below
OpenPaula14:	;
		; check whether we have Pamela audio
		;
	ifne	HQ_PAMELA16
		bsr	AmpOut_Pamela16_Check
		tst.l	d0
		bne	OpenPamela16			;use Pamela
	endc
		sf	P16_switch-VDEC_BASE(A5)
ForceOpenPaula14:
		sf	paula_started			;not started yet

		bsr	AllocAudio
		tst.l	d0
		beq	OpenPaulaDevError

		lea	SampleData1,a4
		moveq	#NSampleData-1,d7
.buffers:
		move.l	#32*MPEGA_PCM_SIZE,d0		;*2 for 14 Bit buffer
		move.l	#MEMF_CHIP|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		move.l	d0,(a4)+
		beq.s	OpenPaulaDevError
		dbf	d7,.buffers

		move.l	gfxbase,a1
		move.l	gb_DisplayFlags(a1),d1
		btst	#PALn,d1
		beq.b	.PALClock
		move.l	#3579594,AudioClock		;NTSC
.PALClock
		bset	#1,$bfe001			;disable filter
		bra	AudioOpened

OpenPaulaDevError:
		lea	SampleData1,a4
		moveq	#NSampleData-1,d7
.buffers:
		move.l	(a4)+,d0
		beq.s	.next
		move.l	d0,a1
		CALLEXEC FreeVec
.next
		dbf	d7,.buffers

		bsr	FreeAudio

		sf	P14_switch-VDEC_BASE(A5)
		;fall through
OpenAudioDev:
		CALLEXEC CreateMsgPort
		move.l	d0,AudioPort_L
		beq	OpenAudioDevError
		move.l	d0,a0
		moveq.l	#ioa_SIZEOF,d0			;size of AudioIO
		CALLEXEC CreateIORequest
		move.l	d0,AudioIORequest_L1
		beq	OpenAudioDevError
		move.l	d0,a1
		move.l	#audiochans_L,ioa_Data(a1)
		move.l	#2,ioa_Length(a1)
		clr.w	ioa_AllocKey(a1)
		move.b	#127,LN_PRI(a1)
		lea	AudioDevName(pc),a0
		moveq	#0,d0
		moveq	#0,d1
		CALLEXEC OpenDevice
		move.l	d0,AudioDevice_L
		bne	OpenAudioDevError

;		moveq.l	#AHIRequest_SIZEOF,d0		;make a copy of the IORequest structure for double-buffering
		moveq.l #ioa_SIZEOF,d0
		moveq.l	#0,d1
		CALLEXEC AllocVec
		move.l	d0,AudioIORequest_L2
		beq	OpenAudioDevError
		move.l	d0,a1
		move.l	AudioIORequest_L1(pc),a0
		moveq.l #ioa_SIZEOF,d0
;		moveq.l	#AHIRequest_SIZEOF,d0
		CALLEXEC CopyMem

		CALLEXEC CreateMsgPort
		move.l	d0,AudioPort_R
		beq	OpenAudioDevError
		move.l	d0,a0
		moveq.l	#ioa_SIZEOF,d0			;size of AudioIO
		CALLEXEC CreateIORequest
		move.l	d0,AudioIORequest_R1
		beq	OpenAudioDevError
		move.l	d0,a1
		move.l	#audiochans_R,ioa_Data(a1)
		move.l	#2,ioa_Length(a1)
		clr.w	ioa_AllocKey(a1)
		move.b	#127,LN_PRI(a1)
		lea	AudioDevName(pc),a0
		moveq	#0,d0
		moveq	#0,d1		
		CALLEXEC OpenDevice
		move.l	d0,AudioDevice_R
		bne	OpenAudioDevError
		moveq.l #ioa_SIZEOF,d0
		;moveq.l	#AHIRequest_SIZEOF,d0		;make a copy of the IORequest structure for double-buffering
		moveq.l	#0,d1
		CALLEXEC AllocVec
		move.l	d0,AudioIORequest_R2
		beq	OpenAudioDevError
		move.l	d0,a1
		move.l	AudioIORequest_R1,a0
;		moveq.l	#AHIRequest_SIZEOF,d0
		moveq.l #ioa_SIZEOF,d0
		CALLEXEC CopyMem

		move.l	gfxbase,a1
		move.l	gb_DisplayFlags(a1),d1
		btst	#PALn,d1
		beq.b	.PALClock
.NTSCClock	move.l	#3579594,AudioClock		;NTSC
.PALClock

		move.l	#16*MPEGA_PCM_SIZE,d0
		move.l	#MEMF_CHIP|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	OpenAudioDevError
		move.l	d0,SampleData1
		move.l	#16*MPEGA_PCM_SIZE,d0
		move.l	#MEMF_CHIP|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	OpenAudioDevError
		move.l	d0,SampleData2

		move.l	#16*MPEGA_PCM_SIZE,d0
		move.l	#MEMF_CHIP|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	OpenAudioDevError
		move.l	d0,SampleData1R
		move.l	#16*MPEGA_PCM_SIZE,d0
		move.l	#MEMF_CHIP|MEMF_PUBLIC,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	OpenAudioDevError
		move.l	d0,SampleData2R

		moveq	#2,d0
		and.b	$bfe001,d0
		move.b	d0,AudioFilterBit
		bset	#1,$bfe001

		bra	AudioOpened

OpenAudioDevError

		bsr	AudioDevClose

		;Couldn't open audio.device -> fall back to AHI
		st	AHI_switch

OpenAHIDev:
		CALLEXEC CreateMsgPort
		move.l	d0,AudioPort_L
		beq	AudioError
		move.l	d0,a0
		moveq.l	#AHIRequest_SIZEOF,d0			;size of AHI IORequest structure
		CALLEXEC CreateIORequest
		move.l	d0,AudioIORequest_L1
		beq	AudioError
		move.l	d0,a1
		move.w	#4,ahir_Version(a1)
		lea	AHIDevName(pc),a0
		moveq	#0,d0
		moveq	#0,d1
		CALLEXEC OpenDevice
		move.l	d0,AudioDevice_L
		bne	AudioError

		moveq.l	#AHIRequest_SIZEOF,d0			;make a copy of the IORequest structure for double-buffering
		moveq.l	#0,d1
		CALLEXEC AllocVec
		move.l	d0,AudioIORequest_L2
		beq	AudioError
		move.l	d0,a1
		move.l	AudioIORequest_L1,a0
		moveq.l	#AHIRequest_SIZEOF,d0
		CALLEXEC CopyMem

		move.l	#2*16*MPEGA_PCM_SIZE*2,d0
		moveq.l	#0,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	AudioError
		move.l	d0,SampleData1

		move.l	#2*16*MPEGA_PCM_SIZE*2,d0
		moveq.l	#0,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	AudioError
		move.l	d0,SampleData2
	ifne	0
		move.l	#2*16*MPEGA_PCM_SIZE,d0
		moveq.l	#0,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	AudioError
		move.l	d0,SampleData1R

		move.l	#2*16*MPEGA_PCM_SIZE,d0
		moveq.l	#0,d1
		CALLEXEC AllocVec
		tst.l	d0
		beq	AudioError
		move.l	d0,SampleData2R
	else
		clr.l	SampleData1R
		clr.l	SampleData2R
	endc
AudioOpened:
;----------------------------------------------

		tst.l	mpegastream
		bne.b	mpegastreamopen
		suba.l	a0,a0
		lea	MPEGA_Control(pc),a1
		clr.w	4(A1)
		CALLMPEGA open				;open mpega stream if not already open
		move.l	d0,mpegastream
		beq	AudioError
		tst.l	AudioAbortFlag
		bne	AudioExit
mpegastreamopen

		move.l	#SIGBREAKF_CTRL_D,d0
		CALLEXEC Wait

mpegaloop
		move.l	#SIGBREAKF_CTRL_C,d1
		CALLDOS2	CheckSignal
		tst.l	d0
		bne	AudioExit

	ifne	HQ_TRIPLEBUF
		tst.b	P14_switch
		beq	.regular
		move.l	abuftoggle(pc),d5
		addq.b	#1,d5
		cmp.w	#2,d5
		ble.s	.keep
		moveq	#0,d5		
.keep:		move.l	d5,abuftoggle
		lsl	#3,d5
		
		; we do triple buffering here
		lea	SampleData1(pc),a5
		lea	(a5,d5.w),a5
		move.l	4(a5),d5
		move.l	(a5),a5
		bra.s	.wave_ok
.regular:
	endc
		not.l	abuftoggle
		beq.b	.wave2
		move.l	SampleData1,a5
		move.l	SampleData1R,d5
		move.l	AudioIORequest_L1,a2
		move.l	AudioIORequest_R1,a3
		bra.b	.wave_ok

.wave2		move.l	SampleData2,a5
		move.l	SampleData2R,d5
		move.l	AudioIORequest_L2,a2
		move.l	AudioIORequest_R2,a3
.wave_ok
		move.l	a5,pcmptr0
		move.l	d5,pcmptr1			;unused for AHI

		moveq	#0,d6				;samples count
		moveq	#16,d7				;nbuffers

pcmloop		move.l	mpegastream,a0
		lea	pcm_buffer,a1
		CALLMPEGA decode_frame
.keychk
		tst.l	AudioAbortFlag
		bne	AudioExit

		tst.b	PauseFlag
		beq.s	.nopause
		VBLDELAY 10
		bra.s	.keychk
.nopause
		;d0=no. of 16bit samples
		add.l	d0,d6

		move.l	pcm_buffer,a4

		tst.b	P16_switch
		bne	copyPamela16

		tst.b	P14_switch
		bne	copyPaula14

		tst.b	AHI_switch
		bne	copyAHI

		move.l	d5,a0                      ; right PTR


	;this doesn`t have to be fast, actually. We`ve got ample time 
	;between written samples. ChipRAM is ~3 MB/s, which means at least 
	;78 cycles per longword can be wasted with fun stuff.

		moveq	#$7f,d3
.copyAudioDev	
	ifne	AUDIO_ROUND
		move.w	(a4),d1                    ; x x H0 L0
		add.w	d3,d1
		bvc.s	.noo1
		move.w	#$7fff,d1
.noo1
		lsl.l	#8,d1		;x H0 L0 x

		move.w	2(a4),d2
		add.w	d3,d2
		bvc.s	.noo2
		move.w	#$7fff,d2
.noo2
		move.w	d2,d1
		lsl.l	#8,d1		;H0 H1 x x

		move.w	4(a4),d2
		add.w	d3,d2
		bvc.s	.noo3
		move.w	#$7fff,d2
.noo3		
		move.w	d2,d1		;H0 H1 H2 x

		move.w	6(a4),d2
		add.w	d3,d2
		bvc.s	.noo4
		move.w	#$7fff,d2
.noo4	
		lsr.w	#8,d2
		move.b	d2,d1		;H0 H1 H2 H3
		move.l	d1,(a0)+


		move.w	MPEGA_PCM_SIZE*2(a4),d1   ; x x H0 L0
		add.w	d3,d1
		bvc.s	.noo11
		move.w	#$7fff,d1
.noo11:
		lsl.l	#8,d1		;x H0 L0 x

		move.w	MPEGA_PCM_SIZE*2+2(a4),d2
		add.w	d3,d2
		bvc.s	.noo21
		move.w	#$7fff,d2
.noo21:
		move.w	d2,d1
		lsl.l	#8,d1		;H0 H1 x x

		move.w	MPEGA_PCM_SIZE*2+4(a4),d2
		add.w	d3,d2
		bvc.s	.noo31
		move.w	#$7fff,d2
.noo31:
		move.w	d2,d1		;H0 H1 H2 x

		move.w	MPEGA_PCM_SIZE*2+6(a4),d2
		add.w	d3,d2
		bvc.s	.noo41
		move.w	#$7fff,d2
.noo41
		lsr.w	#8,d2
		move.b	d2,d1		;H0 H1 H2 H3
		move.l	d1,(a5)+

;		move.w	MPEGA_PCM_SIZE*2(a4),d1	   ; x x H0 L0
;		move.b	MPEGA_PCM_SIZE*2+2(a4),d1  ; x x H0 H1
;		swap	d1
;		move.w	MPEGA_PCM_SIZE*2+4(a4),d1  ; H0 H1 H2 L2
;		move.b	MPEGA_PCM_SIZE*2+6(a4),d1  ; H0 H1 H2 H3
;			move.l	d1,(a5)+
	else
		IFNE APOLLO_MOVEP
		
		movep.l (0,a4),d1                  ; 
		move.l	d1,(a0)+                   ; 

		movep.l (MPEGA_PCM_SIZE*2,a4),d1   ; 
		move.l	d1,(a5)+                   ; 
		
		ELSE
		
		move.w	(a4),d1                    ; x x H0 L0
		move.b	2(a4),d1                   ; x x H0 H1
		swap	d1
		move.w	4(a4),d1                   ; H0 H1 H2 L2
		move.b	6(a4),d1                   ; H0 H1 H2 H3
		move.l	d1,(a0)+

		move.w	MPEGA_PCM_SIZE*2(a4),d1	   ; x x H0 L0
		move.b	MPEGA_PCM_SIZE*2+2(a4),d1  ; x x H0 H1
		swap	d1
		move.w	MPEGA_PCM_SIZE*2+4(a4),d1  ; H0 H1 H2 L2
		move.b	MPEGA_PCM_SIZE*2+6(a4),d1  ; H0 H1 H2 H3
		move.l	d1,(a5)+
		
		ENDC
	endc
		addq.l	#8,a4

		subq.l	#4,d0
		bgt	.copyAudioDev

		move.l	a0,d5

		subq.l	#1,d7
		bne	pcmloop
		bra	DecodeDone

; needs buffer twice as large as regular audio.device loop
copyPamela16:
		move.l	d5,a0                      ; right PTR
		;a5 = left PTR
		lea	MPEGA_PCM_SIZE*2(a4),a1		;right source
.p16loop
		move.l	(a1)+,(a0)+		;right
		move.l	(a1)+,(a0)+		;right
		move.l	(a4)+,(a5)+		;left
		move.l	(a4)+,(a5)+		;left

		subq.l	#4,d0			   ; d0=no. of 16bit samples
		bgt.s	.p16loop

		move.l	a0,d5

		subq.l	#1,d7
		bne	pcmloop
		
		bra	DecodeDone
; needs buffer twice as large as regular audio.device loop
copyPaula14:
	ifne	HIGHBOOST
		;apply highboost filter
		movem.l	d0/d7,-(sp)

		move.l	d0,d7
		move.l	a4,a0
		lea	FilterBufferL(pc),a1
		
		move.l	d7,-(sp)
		bsr	HighBoostFilter
		move.l	(sp)+,d7
		
		lea	MPEGA_PCM_SIZE*2(a4),a0
		lea	FilterBufferR(pc),a1
		bsr	HighBoostFilter

		movem.l	(sp)+,d0/d7
	endc
		move.l	d5,a0                      ; right PTR
		;a5 = left PTR
.p14loop:
		move	(a4)+,d1		   ; xx xx H0 L0 (left)
		move	(a4)+,d2		   ; xx xx H1 L1 (left)

		move	d1,d3			   ; xx xx H0 L0
		 ror.l	#8,d2			   ; L1 xx xx H1
		move.b	d2,d1			   ; xx xx H0 H1
		 move.b	d3,d2			   ; L1 xx xx L0
		swap	d1			   ; H0 H1 xx xx
		ror.l	#8,d2			   ; L0 L1 xx xx
		
		move	(a4)+,d1		   ; H0 H1 H2 L2
		move	(a4)+,d2		   ; L0 L1 H3 L3

		rol.w	#8,d2			   ; L0 L1 L3 H3
		 move.l	d1,d3			   ; H0 H1 H2 L2
		move.b	d2,d1			   ; H0 H1 H2 H3
		 move.b	d3,d2			   ; L0 L1 L3 L2
		rol.w	#8,d2			   ; L0 L1 L2 L3
		
		move.l	d1,(a5)+		   ; store Left H0 H1 H2 H3
		 lsr.l	#2,d2
		and.l	#$3f3f3f3f,d2		   ; l0 l1 l2 l3
		move.l	d2,16*MPEGA_PCM_SIZE-4(a5)

		lea	MPEGA_PCM_SIZE*2-8(a4),a4
		move	(a4)+,d1		   ; xx xx H0 L0 (right)
		move	(a4)+,d2		   ; xx xx H1 L1 (right)

		move	d1,d3			   ; xx xx H0 L0
		 ror.l	#8,d2			   ; L1 xx xx H1
		move.b	d2,d1			   ; xx xx H0 H1
		 move.b	d3,d2			   ; L1 xx xx L0
		swap	d1			   ; H0 H1 xx xx
		ror.l	#8,d2			   ; L0 L1 xx xx
		
		move	(a4)+,d1		   ; H0 H1 H2 L2
		move	(a4)+,d2		   ; L0 L1 H3 L3

		rol.w	#8,d2			   ; L0 L1 L3 H3
		 move.l	d1,d3			   ; H0 H1 H2 L2
		move.b	d2,d1			   ; H0 H1 H2 H3
		 move.b	d3,d2			   ; L0 L1 L3 L2
		rol.w	#8,d2			   ; L0 L1 L2 L3
		
		move.l	d1,(a0)+		   ; store Right H0 H1 H2 H3
		 lsr.l	#2,d2
		and.l	#$3f3f3f3f,d2		   ; l0 l1 l2 l3
		move.l	d2,16*MPEGA_PCM_SIZE-4(a0) ; store Right L0 L1 L2 L3
		
		lea	-MPEGA_PCM_SIZE*2(a4),a4

		subq.l	#4,d0			   ; d0=no. of 16bit samples
		bgt.s	.p14loop

		move.l	a0,d5

		subq.l	#1,d7
		bne	pcmloop

		bra	DecodeDone
copyAHI	
.copyAHIl
		move.w	(a4)+,(a5)+
		move.w	MPEGA_PCM_SIZE*2-2(a4),(a5)+
		move.w	(a4)+,(a5)+
		move.w	MPEGA_PCM_SIZE*2-2(a4),(a5)+
		;move.l	MPEGA_PCM_SIZE*2(a4),(a0)+
		;move.l	(a4)+,(a5)+
		subq.l	#2,d0
		bne.b	.copyAHIl

		subq.l	#1,d7
		bgt	pcmloop
DecodeDone:
		move.l	picture_rate,d1
		lea	video_rates(pc),a1
		move.l	(a1,d1.w*4),d2		;required fps << 16
		lsr.l	#8,d2
		move.l	vid_rate,d1		;current fps << 16
		divu.l	d2,d1			;current/required << 8
		move.l	mpegastream,a1
		move.l	MPASTRM_FREQUENCY(a1),d2
		mulu.l	d2,d1			;required frequency << 8
		lsr.l	#8,d1
		move.l	audio_freqdiv(pc),d3
		lsr.l	d3,d1
		move.l	d1,AudioFrequency	;store frequency (for AHI)
		
		;highboost calculations, shift pole according to sampling frequency
		moveq	#3,d2
		cmp.l	#28000,d1		;>28 kHz -> use stronger boost
		blt.s	.lo
		moveq	#1,d2
.lo		move.l	d2,highboost_shift

		move.l	AudioClock(pc),d2
		move.l	d1,d3		; AudioClock + divisor/2
		lsr.l	#1,d3		;
		add.l	d3,d2		; 
		divu.l	d1,d2		; -> correctly rounded period
		move.w	d2,AudioPeriod	;store period (for audio.device)

		tst.b	P16_switch
		bne	PlayPamela16

		tst.b	P14_switch
		bne	PlayPaula14

		tst.b	AHI_switch
		bne	PlayAHI


		;move.l	AudioClock(pc),d3	;clock / period = actual frequency (for A/V sync)
		;divu.l	d2,d3			;
		;move.l	d3,AudioFrequency	;


_not_called_PlayAudioDev:
		move.l	pcmptr0,d5

		;Play Left Channel
		move.l	a2,a1				;AudioIORequest_L1 or L2 (depending on doublebuffer state)
		move.w	#CMD_WRITE,IO_COMMAND(a1)
		move.l	d5,ioa_Data(a1)			;pointer to sample
		move.l	d6,ioa_Length(a1)		;sample length
		move.b	#ADIOF_PERVOL,IO_FLAGS(a1)
		move.w	AudioPeriod(pc),ioa_Period(a1)
		move.w	#64,ioa_Volume(a1)
		move.w	#1,ioa_Cycles(a1)
		move.l	IO_DEVICE(a1),a6		; ioaudio+io_device
		jsr	DEV_BEGINIO(a6)			; beginio (-30)


		tst.l	MONOSURROUND_switch
		beq.b	.nosurround
		moveq	#1,d1
		CALLDOS2	Delay
.nosurround
		move.l	pcmptr1,d5

		;Play Right Channel
		move.l	a3,a1				;AudioIORequest_L1 or L2 (depending on doublebuffer state)
		move.w	#CMD_WRITE,IO_COMMAND(a1)
		move.l	d5,ioa_Data(a1)			;pointer to sample
		move.l	d6,ioa_Length(a1)		;sample length
		move.b	#ADIOF_PERVOL,IO_FLAGS(a1)
		move.w	AudioPeriod(pc),ioa_Period(a1)
		move.w	#64,ioa_Volume(a1)
		move.w	#1,ioa_Cycles(a1)
		move.l	IO_DEVICE(a1),a6		; ioaudio+io_device
		jsr	DEV_BEGINIO(a6)			; beginio (-30)

;	ifd	EXTRA_DELAY_FRAME
;		add.l	AudioSamplesPlayed(pc),d6	;store new samples count
;		move.l	d6,AudioSamplesPlayed		;
;	endc

		tst.l	AudioIsPlaying(pc)
		beq	DoneAudioReq

;	ifnd	EXTRA_DELAY_FRAME
		add.l	AudioSamplesPlayed(pc),d6	;store new samples count
		move.l	d6,AudioSamplesPlayed		;
;	endc
	
		;EClock not needed here right now
		;GETECLOCK64 d3,d4		 ; get current time (H: d3 L: d4)
		;sub.l	d2,d4			 ; "corrected start time", i.e. assume that the actual audio frequency is not
		;subx.l	d1,d3			 ; correct, with this construct we calculate a "new" start time 
		
		move.l	AudioPort_L(pc),a0
		CALLEXEC WaitPort		; wait for playback to complete!
		move.l	AudioPort_L(pc),a0
		CALLEXEC GetMsg			; remove reply msg from port

		move.l	AudioPort_R(pc),a0
		CALLEXEC WaitPort		; wait for playback to complete!
		move.l	AudioPort_R(pc),a0
		CALLEXEC GetMsg			; remove reply msg from port

		bsr	CalcAudioSync

	ifne	0
	; old routine, less accurate
			;d6 number of samples in bytes (just written)
			;
		ifne	SYNC_ON_AUDIO
			movem.l	d0-d6,-(sp)

			move.l	e_count_rate,d3	; ticks per second
			lsl.l	#8,d3
			divu.l	AudioFrequency,d3	; ticks*256/AudioFreq
			; ts = AudioSamplesPlayed / AudioFreq   (in seconds)
			; tt = AudioSamplesPlayed * EClock / AudioFreq (in eclock units)
			move.l	AudioSamplesPlayed,d2
			moveq	#0,d1
			mulu.l	d3,d1:d2		;H:d1 L:d2
			moveq	#0,d3
			move.b	d1,d3			;lower 8 bit of D1
			lsr.l	#8,d2			;shift down 8
			lsr.l	#8,d1			;shift down 8
			ror.l	#8,d3			;lower 8 bit of D1 - high bits of D3
			or.l	d3,d2			;move the high bits to D2

		ifne	0
			;problem (sometimes): static offset between audio and video time
			;solution: at start, obtain offset and add that to subsequent a/v sync
			cmp.l	#120000,AudioSamplesPlayed(pc)
			bhi.s	.enoughsamples
			;tst.b	AudioOff(pc)
			;st	AudioOff

			movem.l	actual_time,d4-d5
			sub.l	d2,d5
			subx.l	d1,d4
			movem.l	d4-d5,audio_timeoffset

			bra.s	.noaudiosyncyet	
	.enoughsamples

			movem.l	audio_timeoffset,d4-d5
			add.l	d5,d2
			addx.l	d4,d1
		endc
	;		movem.l	d1-d2,actual_time       ; to sync video on audio

			; this is the "actual" playtime when syncing on audio
			lea.l	audio_time,a0
			movem.l	d1-d2,(a0)               ; to sync video on audio
							 ; side effect: we can still use the EClock on video track, so we can track
							 ; the time between the updates from the audio track (with a slight drift/bias that's 
							 ; not corrected right now)
	.noaudiosyncyet	
			movem.l (sp)+,d0-d6

		endc	; SYNC_ON_AUDIO
	endc
		bra	DoneAudioReq

PlayPamela16:
		move.l	pcmptr0,a0
		move.l	pcmptr1,a1
		move.l	d6,d0
		move.w	AudioPeriod(pc),d1
		
		bsr	Pamela16_SendAudio

		tst.l	AudioIsPlaying(pc)
		beq	DoneAudioReq

		add.l	AudioSamplesPlayed(pc),d6	;store new samples count
		move.l	d6,AudioSamplesPlayed		;
	
		bsr	CalcAudioSync

		bra	DoneAudioReq
PlayPaula14:
		move.l	pcmptr0,a0
		move.l	pcmptr1,a1
		move.l	d6,d0
		move.w	AudioPeriod(pc),d1
		
		bsr	Paula14_SendAudio

		tst.l	AudioIsPlaying(pc)
		beq	DoneAudioReq

	ifne	HQ_TRIPLEBUF
		move.l	AudioSamplesPlayed_D(pc),AudioSamplesPlayed
		add.l	AudioSamplesPlayed_D(pc),d6
		move.l	d6,AudioSamplesPlayed_D
	else
		add.l	AudioSamplesPlayed(pc),d6	;store new samples count
		move.l	d6,AudioSamplesPlayed		;
	endc

		bsr	CalcAudioSync

		bra	DoneAudioReq
PlayAHI:
		move.l	AudioSamplesPlayed(pc),d7
		add.l	d6,d7

		move.l	pcmptr0,d5

		move.l	a2,a1				;AudioIORequest_L1 or L2 (depending on doublebuffer state)
		move.b	#127,LN_PRI(a1)
		move.w	#CMD_WRITE,IO_COMMAND(a1)
		move.l	d5,IO_DATA(a1)			;pointer to sample
		lsl.l	#2,d6				;convert to number of bytes (instead of no. of 16bit samples)
		move.l	d6,IO_LENGTH(a1)		;sample length
		clr.l	IO_OFFSET(a1)			;offset
		move.l	AudioFrequency(pc),ahir_Frequency(a1)
		move.l	#AHIST_S16S,ahir_Type(a1)
		move.l	#$10000,ahir_Volume(a1)
		move.l	#$8000,ahir_Position(a1)
		move.l	ahilink(pc),ahir_Link(a1)
		CALLEXEC SendIO

		move.l	ahilink(pc),d0
		beq.b	.nowaitreply
		move.l	d0,a1
		CALLEXEC WaitIO

		bsr	CalcAudioSync
.nowaitreply	
		move.l	d7,AudioSamplesPlayed		;store new samples count
		move.l	a2,ahilink

DoneAudioReq:

		st	AudioIsPlaying

		bra	mpegaloop

CalcAudioSync:
		;d6 number of samples in bytes (just written)
		;does nothing if sync on audio is deactivated
	ifne	SYNC_ON_AUDIO
		movem.l	d1-d4/a0,-(sp)
		lea.l	audio_time,a0
		move.l	e_count_rate-audio_time(a0),d3	 ; ticks per second (709379, 715909)
		moveq	#12,d4			; 2^32/715909 is roughly 6000, so 4096 as multiplier is ok
		lsl.l	d4,d3
		divu.l	AudioFrequency(pc),d3 ; ticks*256/AudioFreq
		; ts = AudioSamplesPlayed / AudioFreq   (in seconds)
		; tt = AudioSamplesPlayed * EClock / AudioFreq (in eclock units)
		move.l	AudioSamplesPlayed(pc),d2
		blt.s	.nosync
		moveq	#0,d1
		mulu.l	d3,d1:d2		;H:d1 L:d2
		moveq	#-1,d3			;-1
		lsl.l	d4,d3			;prepare 12 bit in D3
		not.l	d3			;swap bits
		and.l	d1,d3			;lower 12 bit of D1
		lsr.l	d4,d2			;shift down 12
		lsr.l	d4,d1			;shift down 12
		ror.l	d4,d3			;lower 12 bit of D1 - high bits of D3
		or.l	d3,d2			;move the high bits to D2

		; this is the "actual" playtime when syncing on audio
		movem.l	d1-d2,(a0)               ; to sync video on audio
						 ; side effect: we can still use the EClock on video track, so we can track
						 ; the time between the updates from the audio track (with a slight drift/bias that's 
						 ; not corrected right now)
.nosync:
		movem.l (sp)+,d1-d4/a0
	endc	; SYNC_ON_AUDIO
		rts

;-----------------------------------------------------------------------------------------------------
;-----------------------------------------------------------------------------------------------------
;-- Apply high-boost filter to input buffer (in-place)                                              --
;-----------------------------------------------------------------------------------------------------
;-----------------------------------------------------------------------------------------------------
; Input:
;       A0    - mixing buffer to be filtered
;       A1    - previous stage of filter, coefficient buffer
;       D7    - number of 16 bit samples in buffer (SamLen*2)
; Output:
;       (A0)+ - filtered data
; Notes: - no normalization is done here, prepare mixing buffer contents for a typical boost of factor 1.5-2
;        - coefficients for 44 kHz sampling rate
;
HighBoostFilter:
	movem.w	(A1),d0-d1	;d0 = 4*x[n-1], d1 = y[n-1]

	;filter term:
	; 
	; 1 - 0.5*z^-1
	; ------------
	; 1 - 0.5*z^-1
	move.l	highboost_shift(pc),d3
	subq	#1,d7
.doboost:
	move	(A0),d2		;x[n]
	 asr.w	#1,d0		;x[n-1]/2=0.5*x[n-1]
	asr.w	d3,d1		;0.5*y[n-1]	;1 or 3 = 0.5 or 0.125
	 neg.w	d0		;-0.5*x[n-1]
	neg.w	d1
	 add.w	d2,d0		;y[n]=x[n]-0.5*x[n-1]
	add.w	d0,d1		;y[n]=x[n]-0.5*x[n-1]-0.5*y[n-1]
	bvs.s	.ovl		;oops: overflow (might happen at frequencies >16 kHz)
.afterovl:
	 move	d2,d0		;remember x[n]
	move.w	d1,(a0)+
	dbf	d7,.doboost

	movem.w	d0-d1,(A1)	;remember last input, output
	rts
; overflow: we had a sign change
.ovl:
	bmi.s	.neg
	; positive after overflow: max. negative
	move	#$8000,d1	;move	#$7fff,d1
	bra.s	.afterovl
.neg:	;was positive: now negativ
	move	#$7fff,d1	;move	#$8000,d1
	bra.s	.afterovl


;AudioOff:	dc.b	0,0

;----------------------------------------------

***********************************************************
AudioError	move.l	AudioBufferReplyPort(pc),a0
		lea	AudioBufferMsg(pc),a1
		move.b	#AUDREP_ERROR,AUDB_REPLY(a1)
		CALLEXEC PutMsg
AudioExit
		tst.b	P16_switch
		bne	ExitPamela16
		tst.b	P14_switch
		bne	ExitPaula14

		tst.l	AudioDevice_L(pc)
		bne.b	.noabortio1
		move.l	AudioIORequest_L1(pc),a1
		move.l	IO_DEVICE(a1),a6
		jsr	DEV_ABORTIO(a6)
		move.l	AudioIORequest_L2(pc),a1
		move.l	IO_DEVICE(a1),a6
		jsr	DEV_ABORTIO(a6)
.noabortio1
		tst.l	AudioDevice_R(pc)
		bne.b	.noabortio2
		move.l	AudioIORequest_R1(pc),a1
		move.l	IO_DEVICE(a1),a6
		jsr	DEV_ABORTIO(a6)
		move.l	AudioIORequest_R2(pc),a1
		move.l	IO_DEVICE(a1),a6
		jsr	DEV_ABORTIO(a6)
.noabortio2
		bra.s	CloseAudioFile
ExitPamela16:
		moveq	#0,d0
		bsr	StopPamela
		bsr	FreeAudioPamela

		bra.s	CloseAudioFile		;memory free`d below
ExitPaula14:
		bsr	StopPaula
		bsr	FreeAudio
		;fall through

CloseAudioFile:	move.l	audiofile,d1
		beq.b	noaudiofile
		CALLDOS2	Close
noaudiofile
		move.l	mpegastream(pc),d7
		beq.b	.nompegastream
		move.l	d7,a0
		CALLMPEGA close
.nompegastream

CloseMPEGA	move.l	mpegabase,d7
		beq.b	nompega
		move.l	d7,a1
		CALLEXEC CloseLibrary
		clr.l	mpegabase
nompega
		bsr	AudioDevClose

		move.l	#SIGBREAKF_CTRL_D,d0
		move.l	MainTask(pc),a1
		CALLEXEC Signal

		;lea	AudioControlMsg(pc),a1		;reply to abort message
		;CALLEXEC ReplyMsg
		clr.l	AudioTask
		rts



		EVEN
AudioDevClose:
		moveq	#-3,d0
		and.b	$bfe001,d0
		or.b	AudioFilterBit,d0
		move.b	d0,$bfe001

		lea	SampleData1(pc),a4
		moveq	#NSampleData-1,d7
.nobuffers:
		move.l	(a4)+,d0
		beq.s	.next
		move.l	d0,a1
		CALLEXEC FreeVec
.next
		dbf	d7,.nobuffers

		tst.l	AudioDevice_L(pc)
		bne.b	.noaudio_l
		move.l	AudioIORequest_L1(pc),a1
		CALLEXEC CloseDevice
		moveq.l	#-1,d0
		move.l	d0,AudioDevice_L
.noaudio_l	move.l	AudioIORequest_L1(pc),d7
		beq.b	.noioreq_l1
		move.l	d7,a0
		CALLEXEC DeleteIORequest		;delete 1st Left IO request
		clr.l	AudioIORequest_L1
.noioreq_l1	move.l	AudioIORequest_L2(pc),d7
		beq.b	.noioreq_l2
		move.l	d7,a1
		CALLEXEC FreeVec			;delete 2nd Left IO request with FreeVec (copy of 1st)
		clr.l	AudioIORequest_L2
.noioreq_l2	move.l	AudioPort_L(pc),d7
		beq.b	.noAudioPort_L
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
		clr.l	AudioPort_L
.noAudioPort_L
		tst.l	AudioDevice_R(pc)
		bne.b	.noaudio_r
		move.l	AudioIORequest_R1(pc),a1
		CALLEXEC CloseDevice
		moveq.l	#-1,d0
		move.l	d0,AudioDevice_R
.noaudio_r	move.l	AudioIORequest_R1(pc),d7
		beq.b	.noioreq_r1
		move.l	d7,a0
		CALLEXEC DeleteIORequest		;delete 1st Right IO request
		clr.l	AudioIORequest_R1
.noioreq_r1	move.l	AudioIORequest_R2(pc),d7
		beq.b	.noioreq_r2
		move.l	d7,a1
		CALLEXEC FreeVec			;delete 2nd Right IO request with FreeVec (copy of 1st)
		clr.l	AudioIORequest_R2
.noioreq_r2	move.l	AudioPort_R(pc),d7
		beq.b	.noAudioPort_R
		move.l	d7,a0
		CALLEXEC DeleteMsgPort
		clr.l	AudioPort_R
.noAudioPort_R
		rts


************************************************************************************************
* The Video Decoder                                                                            *
************************************************************************************************
		EVEN
VideoDecoderStart

CreateTimerPort	CALLEXEC CreateMsgPort			;Create MsgPort for timing...
		move.l	d0,TimerPort
		beq.w	Error

CreateIORequest	move.l	TimerPort,a0
		move.l	#IOTV_SIZE,d0
		CALLEXEC CreateIORequest
		move.l	d0,TimerIO
		beq.w	Error

OpenTimer	move.l	TimerIO,a1
		lea	TimerDev,a0
		move.l	#UNIT_ECLOCK,d0
		moveq	#0,d1
		CALLEXEC OpenDevice
		move.l	d0,TimerClosed
		bne.w	Error
		move.l	TimerIO,a0
		move.l	IO_DEVICE(a0),timerbase

		jsr	CreatePalette

VideoDecodeStart

		suba.l	a0,a0
		moveq	#0,d0
		bsr	ReadVideoBuffer


Video_Sequence	NEXT_START_CODE

		CHK32	d1
		cmp.l	#sequence_header_code,d1
		bne	NotMPEG

sequence_found	move.l	a0,start_of_anim		;This is the starting address of our anim...

		SKP32					;parse to sequence header data
		bsr	Parse_Sequence_Header		;parse sequence header and
		bsr	Display_Sequence_Header		;display header data

		bsr	MPEG_Init			;allocate decoder resources
		tst.l	result
		beq	MPEGExit			;if fail, exit

		move.l	#-1,frame_number		;start from frame 0.
		clr.l	actual_time
		clr.l	4+actual_time			;clear eclock timer

Anim_Start	move.l	start_of_anim,a0		;start from first sequence in file
		moveq	#0,d0

		clr.l	bwd_reference_y		;clear forward and backward reference at beginning of anim!
		clr.l	fwd_reference_y

NextSequence	CHK32	d1
		cmp.l	#sequence_header_code,d1
		beq.b	Sequence_Header			;Sequence Header -> parse header
		;bra	MPEGEnd
		addq.l	#4,a0
		NEXT_START_CODE
		bra.b	NextSequence

;------------------------------------------------------------------------------------------------------------------------;
;------------------------------------------------- Parse sequence header ------------------------------------------------;
;------------------------------------------------------------------------------------------------------------------------;
Sequence_Header	SKP32					;skip sequence start code

		bsr.w	Parse_Sequence_Header

NextGroup	CHK32	d1
		cmp.l	#group_start_code,d1
		beq.b	Group_Of_Pictures
		bra.w	NextSequence

;------------------------------------------------------------------------------------------------------------------------;
;------------------------------------------------- Parse Group Of Pictures ----------------------------------------------;
;------------------------------------------------------------------------------------------------------------------------;
Group_Of_Pictures	SKP32						;skip group header

			move.l	frame_number,d1
			addq.l	#1,d1
			move.l	d1,GOP_base_frame_number

			IFNE	SHOW_PICINFO
			OUTTXT	msg_gop
			ENDC

			NGETDATA 1,drop_frame_flag,d2
			NGETDATA 5,time_code_hours,d2
			NGETDATA 6,time_code_minutes,d2
			NGETDATA 1,gop_marker_bit,d2
			NGETDATA 6,time_code_seconds,d2
			NGETDATA 6,time_code_pictures,d2
			NGETDATA 1,closed_gop,d2
			NGETDATA 1,broken_link,d2

			NEXT_START_CODE

GOP_ext_check	CHK32	d1
		cmp.l	#extension_start_code,d1
		bne.b	no_GOP_extension
		SKP32
GOP_ext_loop	CHECK_BUFFER
		CHK32	d1					;<--- extension data... (ignored)
		clr.b	d1
		cmp.l	#$00000100,d1
		beq.b	no_GOP_extension
		SKP8
		bra.b	GOP_ext_loop
no_GOP_extension

		NEXT_START_CODE

GOP_user_check	CHK32	d1
		cmp.l	#user_data_start_code,d1
		bne.b	no_GOP_user_data
		SKP32
GOP_user_loop	CHECK_BUFFER
		CHK32	d1					;<--- user data... (ignored)
		clr.b	d1
		cmp.l	#$00000100,d1
		beq.b	no_GOP_user_data
		SKP8
		bra.b	GOP_user_loop
no_GOP_user_data

		NEXT_START_CODE

		IFNE	DEBUG
		OUTTXT	TimeCodeMsg
		OUTDEC	time_code_hours(pc)
		OUTTXT	COLON
		OUTDEC	time_code_minutes(pc)
		OUTTXT	COLON
		OUTDEC	time_code_seconds(pc)
		OUTTXT	RETURN
		ENDC

NextPicture	CHK32	d1					;Check if picture follows
		cmp.l	#picture_start_code,d1
		beq.b	Picture
		bra.w	NextGroup

;------------------------------------------------------------------------------------------------------------------------;
;--------------------------------------------------- Parse Picture ------------------------------------------------------;
;------------------------------------------------------------------------------------------------------------------------;
; A5 - base ptr
; D1 - framebuffer base Y
; D2 - framebuffer base Cb
; D3 - framebuffer base Cr
; TODO: verify that A1 is clean to use
Picbase_init:
		; y is stored 4x for faster decisions in iDCT reconstruction stage
		lea	y_bitmap_base(pc),a1
		move.l	d1,y_bitmap_base-y_bitmap_base(a1)
		move.l	d1,y_bitmap_base+4-y_bitmap_base(a1)
		move.l	d1,y_bitmap_base+8-y_bitmap_base(a1)
		move.l	d1,y_bitmap_base+12-y_bitmap_base(a1)
		move.l	d2,cb_bitmap_base-y_bitmap_base(a1)
		move.l	d3,cr_bitmap_base-y_bitmap_base(a1)
		rts

Picture:
			CHECK_BUFFER
			SKP32						;skip picture start code

			lea	VDEC_BASE,a5

			add.l	#1,frame_number-VDEC_BASE(a5)
			move.l	#-1,last_Macroblock-VDEC_BASE(a5)

			IFNE	SHOW_PICINFO
			OUTDEC	frame_number
			OUTTXT	SPACE
			ENDC

_ParsePicture_marker_only:

			move.l	a0,picture_start_address-VDEC_BASE(a5)		;for measuring picture size
			NGETDATA 10,temporal_reference-VDEC_BASE(a5),d2
			NGETDATA 3,picture_coding_type-VDEC_BASE(a5),d2
			NGETDATA 16,vbv_delay-VDEC_BASE(a5),d2

			move.l	GOP_base_frame_number-VDEC_BASE(a5),d1
			bne.b	.usegopnumber
			move.l	picture_coding_type-VDEC_BASE(a5),d2
			cmp.b	#I_FRAME,d2
			bne.b	.usegopnumber
			move.l	frame_number-VDEC_BASE(a5),actual_frame_number-VDEC_BASE(a5)
			bra.b	.PictureCalcTime

.usegopnumber		add.l	temporal_reference-VDEC_BASE(a5),d1
			move.l	d1,actual_frame_number-VDEC_BASE(a5)

.PictureCalcTime:
			GETECLOCK64 d1,d2				;calculate current time
			tst.l	frame_number-VDEC_BASE(a5)
			bne.b	.notfirstframe
			moveq	#0,d3					;1st frame -> Actual time = 0
			moveq	#0,d4
			bra	.gotactualtime
.notfirstframe		move.l	d1,d3
			move.l	d2,d4
			movem.l	last_eclock-VDEC_BASE(a5),d5-d6
			sub.l	d6,d4					;difference in EClock units
			subx.l	d5,d3					;

	ifne	SYNC_ON_AUDIO
			;elapsed system time
			movem.l	actual_time-VDEC_BASE(a5),d5-d6		;total elapsed time, accumulated from EClock
			add.l	d4,d6					;
			addx.l	d3,d5					;
			movem.l	d5-d6,actual_time-VDEC_BASE(a5)		;d3:d4 = Actual Time!

			;elapsed audio time used for syncing
			movem.l	audio_time-VDEC_BASE(a5),d5-d6		;total elapsed time, accumulated from EClock
			add.l	d6,d4					;
			addx.l	d5,d3					;
			movem.l	d3-d4,audio_time-VDEC_BASE(a5)		;d3:d4 = Actual Time!

		DOUTDEC	d5
		DOUTTXT	SPACE
		DOUTDEC	d6
		DOUTTXT	SPACE
		DOUTDEC	d3
		DOUTTXT	SPACE
		DOUTDEC	d4
		DOUTTXT	RETURN
	else
			movem.l	actual_time-VDEC_BASE(a5),d5-d6		;total elapsed time, accumulated from EClock
			add.l	d6,d4					;
			addx.l	d5,d3					;
			movem.l	d3-d4,actual_time-VDEC_BASE(a5)		;d3:d4 = Actual Time!
	endc
	
.gotactualtime		movem.l	d1-d2,last_eclock-VDEC_BASE(a5)
			move.l	actual_frame_number-VDEC_BASE(a5),d1		;calculate required frametime
			move.l	frame_time-VDEC_BASE(a5),d2
			mulu.l	d1,d1:d2
			movem.l	d1-d2,required_time-VDEC_BASE(a5)

			sub.l	d4,d2					;difference between required and actual time
			subx.l	d3,d1					;don't need upper upper word... (I think ;)

			DOUTDEC	d1
			DOUTTXT	SPACE
			DOUTDEC	d2
			DOUTTXT	RETURN

			tst.l	NOSKIP_switch-VDEC_BASE(a5)
			bne.b	CheckLead				;if NOSKIP, skip the skip ;)

			;Allow up to 100% lag to prevent unnecessary frameskipping
CheckLag		cmp.l	max_lag-VDEC_BASE(a5),d2
			bge.b	CheckLead

			;OUTTXT	msg_SKIP
			move.l	#8,afterskip_mode-VDEC_BASE(a5)
			move.l	picture_coding_type-VDEC_BASE(a5),d1
			cmp.b	#B_FRAME,d1
			blt	FindNextIFrame				;if I or P frame skip, then jump to next I frame!
			bra	Skip_Picture				;else, just skip to next pic!

CheckLead		

	ifne	APOLLO_NSAGABUFS
			tst.b	VBTimerInit
			beq	Timing_NOVBTimer
	
			move.l	actual_frame_number-VDEC_BASE(a5),d1		;which of these is correct ? actual_frame_number or frame_number
	ifd	EXTRA_DELAY_FRAME
		addq.l	#EXTRA_DELAY_FRAME,d1						    ;delay frame time by 1
	endc
			move.l	frame_time-VDEC_BASE(a5),d2
			mulu.l	d1,d1:d2

			sub.l	d4,d2	;frame_time - elapsed time
			subx.l	d3,d1	;= lead time (should be >0)
			bge.s	.timeok
			moveq	#0,d1
			move.l	frame_time-VDEC_BASE(a5),d2
			lsr.l	#1,d2	;frame_time/2
.timeok:
			movem.l	last_eclock-VDEC_BASE(a5),d5-d6	;ec
			add.l	d6,d2
			addx.l	d5,d1
			movem.l	d1-d2,required_time-VDEC_BASE(a5)	;store time for displaying

			DOUTTXT	SPACE
			DOUTTXT	SPACE
			DOUTDEC	d1
			DOUTTXT	SPACE
			DOUTDEC	d2
			DOUTTXT	SPACE
			DOUTDEC	d5
			DOUTTXT	SPACE
			DOUTDEC	d6
			DOUTTXT	RETURN

			bra	Timing_NODELAY
Timing_NOVBTimer:
	endc	;APOLLO_NSAGABUFS
			move.l	frame_number-VDEC_BASE(a5),d1		;which of these is correct ?
			move.l	frame_time-VDEC_BASE(a5),d2
			mulu.l	d1,d1:d2

			sub.l	d4,d2
			blt.b	_TimingDone
			subx.l	d3,d1
			blt.b	_TimingDone

			tst.l	afterskip_mode-VDEC_BASE(a5)
			beq.b	.noafterskip
			divu.l	afterskip_mode-VDEC_BASE(a5),d2
.noafterskip
			;OUTTXT	msg_WAIT
			;OUTDEC	d2
			movem.l	a0/d0,-(a7)				;delay for required time...
			move.l	TimerIO,a1
			move.l	TimerPort,MN_REPLYPORT(a1)
			move.w	#TR_ADDREQUEST,IO_COMMAND(a1)
			move.l	#0,EV_HI+IO_SIZE(a1)
			move.l	d2,EV_LO+IO_SIZE(a1)
			CALLEXEC DoIO
			movem.l	(a7)+,a0/d0

_TimingDone:
			tst.l	afterskip_mode-VDEC_BASE(a5)
			beq.b	.nodecr
			sub.l	#1,afterskip_mode-VDEC_BASE(a5)
.nodecr
			tst.l	NOVIDEO_switch-VDEC_BASE(a5)
			beq	.nodecodeskip

			tst.l	GlobalAudioEnable
			beq.b	.nosignalaudio
			movem.l	d0/a0,-(a7)
			move.l	#SIGBREAKF_CTRL_D,d0
			move.l	AudioTask(pc),a1
			CALLEXEC Signal
			movem.l	(a7)+,d0/a0
.nosignalaudio		bra	Skip_Picture
.nodecodeskip
			;OUTTXT	RETURN
Timing_NODELAY:
			move.l	picture_coding_type-VDEC_BASE(a5),d4
			cmp.b	#P_FRAME,d4
			bgt.b	.not_i_or_p

			;move.l	picture_coding_type,d4
			cmp.b	#I_FRAME,d4
			beq	_Init_Pic_I
			bra	_Init_Pic_P

.not_i_or_p		;move.l	picture_coding_type(pc),d4
			cmp.b	#B_FRAME,d4
			bgt	Skip_Picture				;if not I, P or B, skip picture!

.Init_Pic_B:		tst.l	NOB_switch-VDEC_BASE(a5)
			bne	Skip_Picture

			tst.l	bwd_reference_y-VDEC_BASE(a5)
			beq	Skip_Picture
			tst.l	fwd_reference_y-VDEC_BASE(a5)
			beq	Skip_Picture

			move.l	FrameBuffer3-VDEC_BASE(a5),d1    
			move.l	FrameBuffer3_Cb-VDEC_BASE(a5),d2 
			move.l	FrameBuffer3_Cr-VDEC_BASE(a5),d3 
			bsr	Picbase_init

			bra	_ParsePicMotion

_Init_Pic_P		tst.l	NOP_switch-VDEC_BASE(a5)
			bne	FindNextIFrame

			IFNE	SHOW_PICINFO
			move.l	back_ref_frame_number-VDEC_BASE(a5),fwd_ref_frame_number-VDEC_BASE(a5)
			move.l	frame_number-VDEC_BASE(a5),back_ref_frame_number-VDEC_BASE(a5)
			ENDC

			move.l	bwd_reference_y-VDEC_BASE(a5),fwd_reference_y-VDEC_BASE(a5)	;previous forward reference will be backward reference!
			move.l	bwd_reference_cb-VDEC_BASE(a5),fwd_reference_cb-VDEC_BASE(a5)
			move.l	bwd_reference_cr-VDEC_BASE(a5),fwd_reference_cr-VDEC_BASE(a5)

			tst.l	framebuf_toggle-VDEC_BASE(a5)
			beq.b	.pbuf2

			move.l	FrameBuffer1-VDEC_BASE(a5),d1    
			move.l	FrameBuffer1_Cb-VDEC_BASE(a5),d2 
			move.l	FrameBuffer1_Cr-VDEC_BASE(a5),d3 
			bsr	Picbase_init
			bra.b	.pbufdone
.pbuf2
			move.l	FrameBuffer2-VDEC_BASE(a5),d1    
			move.l	FrameBuffer2_Cb-VDEC_BASE(a5),d2 
			move.l	FrameBuffer2_Cr-VDEC_BASE(a5),d3 
			bsr	Picbase_init
.pbufdone
			not.l	framebuf_toggle-VDEC_BASE(a5)

			move.l	d1,bwd_reference_y-VDEC_BASE(a5) ;if last frame was reference -> fwd ref for this frame!
			move.l	d2,bwd_reference_cb-VDEC_BASE(a5)
			move.l	d3,bwd_reference_cr-VDEC_BASE(a5)


_ParsePicMotion		
			NGETBITS 1,d1,d2
			move.b	d1,full_pel_forward_vector-VDEC_BASE(a5)
			NGETBITS 3,d1,d2
			subq.l	#1,d1
			move.b	d1,forward_r_size-VDEC_BASE(a5)

			cmp.b	#P_FRAME,d4
			beq	Init_Pic_Done			;if P frame, skip backward vector data

			NGETBITS 1,d1,d2
			move.b	d1,full_pel_backward_vector-VDEC_BASE(a5)
			NGETBITS 3,d1,d2
			subq.l	#1,d1
			move.b	d1,backward_r_size-VDEC_BASE(a5)

			bra	Init_Pic_Done

_Init_Pic_I:
			move.l	bwd_reference_y-VDEC_BASE(a5),fwd_reference_y-VDEC_BASE(a5) ;previous forward reference will be backward reference!
			move.l	bwd_reference_cb-VDEC_BASE(a5),fwd_reference_cb-VDEC_BASE(a5)
			move.l	bwd_reference_cr-VDEC_BASE(a5),fwd_reference_cr-VDEC_BASE(a5)

			IFNE	SHOW_PICINFO
			move.l	back_ref_frame_number-VDEC_BASE(a5),fwd_ref_frame_number-VDEC_BASE(a5)
			move.l	frame_number-VDEC_BASE(a5),back_ref_frame_number-VDEC_BASE(a5)
			ENDC

			tst.l	framebuf_toggle-VDEC_BASE(a5)
			beq.b	.ibuf2

			move.l	FrameBuffer1-VDEC_BASE(a5),d1    
			move.l	FrameBuffer1_Cb-VDEC_BASE(a5),d2 
			move.l	FrameBuffer1_Cr-VDEC_BASE(a5),d3 
			bra.b	.ibufdone
.ibuf2:
			move.l	FrameBuffer2-VDEC_BASE(a5),d1    
			move.l	FrameBuffer2_Cb-VDEC_BASE(a5),d2 
			move.l	FrameBuffer2_Cr-VDEC_BASE(a5),d3 
.ibufdone			
			bsr	Picbase_init

			not.l	framebuf_toggle-VDEC_BASE(a5)

			move.l	d1,bwd_reference_y-VDEC_BASE(a5)	;if last frame was reference -> fwd ref for this frame!
			move.l	d2,bwd_reference_cb-VDEC_BASE(a5)
			move.l	d3,bwd_reference_cr-VDEC_BASE(a5)

Init_Pic_Done:
			;==================== SHOW_PICINFO START ======================
			IFNE	SHOW_PICINFO
			move.l	picture_coding_type-VDEC_BASE(a5),d1
			cmp.b	#B_FRAME,d1
			
			bge.b	.bpic
			cmp.b	#P_FRAME,d1
			beq.b	.ppic
.ipic			OUTTXT	msg_i_pic
			bra.b	.framenumber
.ppic			OUTTXT	msg_p_pic
			bra.b	.framenumber
.bpic			OUTTXT	msg_b_pic
.framenumber		OUTTXT	msg_frame
			OUTDEC	actual_frame_number
			OUTTXT	SPACE
			OUTTXT	BRACKET_OPEN
			OUTDEC	temporal_reference(pc)
			OUTTXT	BRACKET_CLOSE
			cmp.b	#I_FRAME,d1
			beq.b	.done
			OUTTXT	COMMA
			OUTTXT	msg_fwd_ref
			OUTDEC	fwd_ref_frame_number
			cmp.b	#P_FRAME,d1
			beq.b	.done
			OUTTXT	COMMA
			OUTTXT	msg_back_ref
			OUTDEC	back_ref_frame_number
.done	
chk_buf			OUTTXT	COMMA
			OUTTXT	msg_buf
			move.l	y_bitmap_base(pc),d1
			cmp.l	FrameBuffer1(pc),d1
			beq.b	.buf1
			cmp.l	FrameBuffer2(pc),d1
			beq.b	.buf2
			cmp.l	FrameBuffer3(pc),d1
			beq.b	.buf3
			OUTTXT	msg_unknownbuf
			bra.b	.bufdone
.buf1			OUTTXT	msg_buf1
			bra.b	.bufdone
.buf2			OUTTXT	msg_buf2
			bra.b	.bufdone
.buf3			OUTTXT	msg_buf3
.bufdone
chk_fwd_buf		OUTTXT	COMMA
			OUTTXT	msg_for
			move.l	fwd_reference_y,d1
			cmp.l	FrameBuffer1(pc),d1
			beq.b	.buf1
			cmp.l	FrameBuffer2(pc),d1
			beq.b	.buf2
			cmp.l	FrameBuffer3(pc),d1
			beq.b	.buf3
			OUTTXT	msg_unknownbuf
			bra.b	.bufdone
.buf1			OUTTXT	msg_buf1
			bra.b	.bufdone
.buf2			OUTTXT	msg_buf2
			bra.b	.bufdone
.buf3			OUTTXT	msg_buf3
.bufdone
chk_back_buf		OUTTXT	COMMA
			OUTTXT	msg_back
			move.l	bwd_reference_y,d1
			cmp.l	FrameBuffer1(pc),d1
			beq.b	.buf1
			cmp.l	FrameBuffer2(pc),d1
			beq.b	.buf2
			cmp.l	FrameBuffer3(pc),d1
			beq.b	.buf3
			OUTTXT	msg_unknownbuf
			bra.b	.bufdone
.buf1			OUTTXT	msg_buf1
			bra.b	.bufdone
.buf2			OUTTXT	msg_buf2
			bra.b	.bufdone
.buf3			OUTTXT	msg_buf3
.bufdone		OUTTXT	RETURN
			ENDC
			;================== SHOW PICINFO END ==================


;			move.l	picture_coding_type(pc),d1	;temporary code: render B buffer & skip B data!
;			cmp.b	#B_FRAME,d1
;			bne.b	oops
;			bsr	RenderPictureQueue
;			bra	Skip_Picture
;oops

chk_pic_extra		NGETBITS 1,d1,d2
			tst	d1
			beq.b	.no_extra_pic_info
			SKP8					;extra picture info ignored
			bra.b	chk_pic_extra
.no_extra_pic_info

			NEXT_START_CODE

pic_ext_check		cmp.l	#extension_start_code,d1
			bne.b	no_pic_extension
			SKP32
.pic_ext_loop		CHECK_BUFFER
			CHK32	d1				;<--- extension data... (ignored)
			clr.b	d1
			cmp.l	#$00000100,d1
			beq.b	no_pic_extension
			SKP8
			bra.b	.pic_ext_loop
no_pic_extension

			NEXT_START_CODE

pic_user_check		CHK32	d1
			cmp.l	#user_data_start_code,d1
			bne.b	no_pic_user_data
			SKP32
pic_user_loop		CHECK_BUFFER
			CHK32	d1
			clr.b	d1
			cmp.l	#$00000100,d1
			beq.b	no_pic_user_data
			SKP8					;<--- user data... (ignored)
			bra.b	pic_user_loop
no_pic_user_data

			NEXT_START_CODE

NextSlice		CHK32	d1				;Check if slice follows
			cmp.w	#$0101,d1
			blt.b	no_slice
			cmp.w	#$01AF,d1
			bgt.b	no_slice
			bra.w	Slice
no_slice
			move.l	#1,PictureReconFlag			;indicate successful picture reconstruction!
ParsePictureDone
			tst.l	GlobalAudioEnable(pc)
			beq.b	.nosignalaudio
			movem.l	d0/a0,-(a7)
			move.l	#SIGBREAKF_CTRL_D,d0
			move.l	AudioTask,a1
			CALLEXEC Signal
			movem.l	(a7)+,d0/a0
.nosignalaudio
			lea	VDEC_BASE,a5

			tst.l	NORENDER_switch-VDEC_BASE(a5)
			bne	.pic_rendered

;Render Picture
			tst.l	PictureReconFlag-VDEC_BASE(a5)		;if frame not reconstructed, don't render!
			beq.b	.pic_rendered

			;move.l	picture_coding_type(pc),d1
			;cmp.b	#B_FRAME,d1
			;beq	dorender

			move.l	frame_number-VDEC_BASE(a5),d1
			cmp.l	actual_frame_number-VDEC_BASE(a5),d1
			bgt	.dorender				;if frame's to be rendered straight away, then do it!

			move.l	frame_number-VDEC_BASE(a5),d1
			cmp.l	queued_frame_number-VDEC_BASE(a5),d1
			blt	.no_render_queue
			bsr	RenderPictureQueue
.no_render_queue
			move.l	y_bitmap_base(pc),queued_reference_y-VDEC_BASE(a5)	;if not time to display yet, put into queue
			move.l	cb_bitmap_base(pc),queued_reference_cb-VDEC_BASE(a5)
			move.l	cr_bitmap_base(pc),queued_reference_cr-VDEC_BASE(a5)
			move.l	actual_frame_number-VDEC_BASE(a5),queued_frame_number-VDEC_BASE(a5)
			movem.l	required_time-VDEC_BASE(a5),d1-d2
			movem.l	d1-d2,queued_timestamp-VDEC_BASE(a5)

			IFNE	SHOW_RENDERINFO
			OUTTXT	msg_frame
			OUTDEC	actual_frame_number
			OUTTXT	msg_intoqueue
			ENDC

			bra.b	.pic_rendered				;and don't render of course!
.dorender
			IFNE	SHOW_RENDERINFO
			OUTTXT	msg_render
			OUTDEC	actual_frame_number
			OUTTXT	RETURN
			ENDC

			bsr.l	mpr_RenderFrame				;a5: VDEC_BASE
			add.l	#1,pictures_played
.pic_rendered

.keychk
			bsr	KeyInputCheck
			tst.b	PauseFlag
			beq.s	.nopause
			VBLDELAY 10
			bra.s	.keychk
.nopause
			tst.l	AbortFlag
			bne	MPEGExit
Picture_End
			bra.w	NextPicture

;------------------------------------------------------------------------------------------------------------------------;
;----------------------------------------------- Parse Slice in I-frames ------------------------------------------------;
;------------------------------------------------------------------------------------------------------------------------;
Slice:		SKP32						;Skip slice start code

		lea	VDEC_BASE,a5			;use a base ptr to avoid reloc

		clr.l	dct_dc_y_past-VDEC_BASE(a5)		;Clear all deltas
		clr.l	dct_dc_cb_past-VDEC_BASE(a5)
		clr.l	dct_dc_cr_past-VDEC_BASE(a5)

		clr.l	mv_fwd_xy_long-VDEC_BASE(a5)
		clr.l	mv_bwd_xy_long-VDEC_BASE(a5)

		and.l	#$000000ff,d1
		move.l	d1,slice_vertical_position-VDEC_BASE(a5)
		subq.l	#1,d1
		move.l	MB_x_total,d2
		mulu.w	d2,d1
		subq.l	#1,d1
		move.l	d1,previous_MB_Address-VDEC_BASE(a5)	;calculate preious_MB_Address at start of slice

		NGETDATA 5,quantizer_scale,d2

		IFNE	SHOW_MBINFO
		OUTTXT	msg_slice
		OUTDEC	slice_vertical_position(pc)
		OUTTXT	msg_quant
		OUTDEC	quantizer_scale
		OUTTXT	RETURN
		ENDC

.chk_slice_extra	
		NGETBITS 1,d1,d2
		tst.l	d1
		beq.b	.no_extra_slice_info
		SKP8						;extra slice info ignored
		bra.b	.chk_slice_extra
.no_extra_slice_info

NextMacroblock:
		CHKBITS	23,d1					;check if a macroblock follows
		bne.b	_Macroblock				;if so, do macroblock
		NEXT_START_CODE

		bra	NextSlice

;------------------------------------------------------------------------------------------------------------------------;
;-------------- Parse Macroblock - Branch to required Macroblock Parser, depending on Picture_Coding_Type ---------------;
;------------------------------------------------------------------------------------------------------------------------;
_Macroblock:		
			lea	VDEC_BASE,a5			;use a base ptr to avoid reloc
			moveq	#21,d7					;32-11 for fast mask/align of 11 bits
			move.l	lookup_MB_address-VDEC_BASE(a5),a2	;put some distance between (a2,d1*2) and loading of A2

			CHECK_BUFFER
			CHKBITS 11,d1
_chk_MB_stuffing: ;not local label, due to CHECK_BUFFER macro
			cmp.l	#$0000000f,d1
			bne.b	_no_MB_stuffing
			NSKPBITS 11,d2
			CHECK_BUFFER
			CHKBITS	11,d1
			bra.b	_chk_MB_stuffing
_no_MB_stuffing
			moveq	#0,d6				;d6 = MB_Escape!!!
.macro_esc_chk:
			cmp.l	#$00000008,d1
			bne.b	_no_macro_escape
			add.l	#33,d6
			NSKPBITS 11,d2

			CHECK_BUFFER
			CHKBITS	11,d1
			bra.b	.macro_esc_chk
_no_macro_escape: ; not local label, due to CHECK_BUFFER macro
			;no need here: d1 is known from above
			;move.l	(a0),d1		;load 32 Bit
			;lsl.l	d0,d1		;clean top bits (D0=0..7)
			;lsr.l	d7,d1		;

			move.w	(a2,d1*2),d1			;get both VLC data and VLC length from table
			NNEXTVLC d1,d2				;add vlc length to input pointer
			lsr.w	#8,d1				;8-bit data from vlc table
			move.l	previous_MB_Address-VDEC_BASE(a5),d2
			add.l	d6,d1				;Add extra from MB escape!
			move.l	d1,MB_address_increment-VDEC_BASE(a5)
			add.l	d2,d1
			move.l	d1,MB_Address-VDEC_BASE(a5)

			IFNE	SHOW_MBINFO
			OUTTXT	msg_macroblock
			OUTDEC	MB_Address
			OUTTXT	SPACE
			OUTTXT	BRACKET_OPEN
			OUTTXT	PLUS
			OUTDEC	MB_address_increment(pc)
			OUTTXT	BRACKET_CLOSE
			ENDC

			move.l	picture_coding_type-VDEC_BASE(a5),d4
			cmp.b	#I_FRAME,d4
			beq	I_MacroBlock			;If I-frame, then branch to I-MacroBlock parser!
			cmp.b	#P_FRAME,d4
			beq	P_MacroBlock			;If P-frame, then branch to P-MacroBlock parser!
			cmp.b	#B_FRAME,d4
			beq	B_MacroBlock			;If B-frame, then branch to P-MacroBlock parser!
			bra	MPEGExit			;else, exit (because unsupported picture type!)

Macroblock_Done:
			lea	VDEC_BASE,a5
			move.l	MB_Address-VDEC_BASE(a5),previous_MB_Address-VDEC_BASE(a5)
			move.l	MB_Address-VDEC_BASE(a5),last_Macroblock-VDEC_BASE(a5)
			bra	NextMacroblock

;------------------------------------;
;--- Parse Macroblock in I frames ---;
;------------------------------------;
; A5:    base register
; A0/D0: bit stream
I_MacroBlock:

.i_mb_type_chk:
		CHKBITS	1,d4
		bne.s	.i_mb_quant_skip
		CHKBITS 7,d1
		and.b	#$1f,d1
		NSKPBITS 6,d2
		move.l	d1,quantizer_scale-VDEC_BASE(a5)	;the quantizer scale to use from now on...
.i_mb_quant_skip:
		NSKPBITS 1,d2
		lea	intra_quant_matrix_zz-VDEC_BASE(a5),a3	;Always use intra quant matrix in I-frames!!!

		IFNE	SHOW_MBINFO
		OUTTXT	RETURN
		ENDC

		;Read Macroblock bitmap offset from tables, according to MB_Address
		;-------------------------------------------------------------------
		;I_CalcMBAddress:
		move.l	MB_Address-VDEC_BASE(a5),d1
		lea	(MB_y_OffsetTable-VDEC_BASE).l(a5),a1
		lea	(MB_c_OffsetTable-VDEC_BASE).l(a5),a2
		move.l	y_MB_max_addr(pc),d6
		lea	MB_y_BitmapOffset(pc),a4

		move.l	(a1,d1.w*4),d2					;for y_bitmap
		move.l	(a2,d1.w*4),MB_c_BitmapOffset-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)	;for c_bitmaps
		move.l	(a2,d1.w*4),MB_c_BitmapOffset+4-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)	;for c_bitmaps

		cmp.l	d6,d2
		bgt.w	Skip_CurrentPic

		move.l	d2,MB_y_BitmapOffset-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
		move.l	d2,MB_y_BitmapOffset+4-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
		move.l	d2,MB_y_BitmapOffset+8-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
		move.l	d2,MB_y_BitmapOffset+12-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)

I_dct_reconstruct: ; called also from P/B frames 
		clr.b	DCT_P_CLEAR			;dct array used

		moveq	#0,d6
I_block_loop:	move.l	d6,block_count

	;------------------------------------------------------------------------
	ifne	APOLLO_IDCT
	;------------------------------------------------------------------------

	; the APOLLO_IDCT setting tiggers a different processing order
	; instead of vertical - horizontal, the new routines require
	; horizontal - vertical order, since the current MMX routine works only
	; on vertical data
	;

	; the DCTX settings must be equal for in-place operation
	; DCTYO must be 16 for the AMMX iDCT routine(s)

	;
	;!!!!!!!!!!!!!!!!  ATTENTION: this choice affects zz_lines, too !!!!!!!!!!!!!!!!!!!
	;
DCTXI		EQU	2		;x loop input coefficient stride
DCTXIL		EQU	16		;x loop offset between columns
DCTXO		EQU	2		;offset in A1 for outputs (2=transposed, 16=natural)
DCTXL		EQU	16		;when DCTXO==2, this is 16 (or vice versa)

DCTYO		EQU	16		;reversed order for the Y loop
DCTYL		EQU	2		;when changing X constants, swap these two as well
ZZ_TRANSPOSED	EQU	1

	;------------------------------------------------------------------------
	else ; APOLLO_IDCT
	;------------------------------------------------------------------------

	; the DCTX settings must be equal for in-place operation
	; DCTYO must be 16 for the AMMX iDCT routine(s)
DCTXI		EQU	16		;x loop input coefficient stride
DCTXIL		EQU	2		;x loop offset between columns
DCTXO		EQU	16		;offset in A1 for outputs (2=transposed, 16=natural)
DCTXL		EQU	2		;when DCTXO==2, this is 16 (or vice versa)

DCTYO		EQU	2		;reversed order for the Y loop
DCTYL		EQU	16		;when changing X constants, swap these two as well
ZZ_TRANSPOSED	EQU	0

	;------------------------------------------------------------------------
	endc ; APOLLO_IDCT
	;------------------------------------------------------------------------


**************************************************************************************************************************
* Decode Blocks: This code parses coefficients, in file and reconstructs some or all the blocks in a macroblock,         *
*                according to coded_block_pattern. It performs dequantization of all coefficients, according to          *
*                the quantization matrix pointed to by register - a3 - which must be in zigzag-ed order, and must        *
*                also have FFT to DCT transform constants multiplied with it, as the DCT is implemented as a fast        *
*                FFT algorithm.                                                                                          *
*                                                                                                                        *
*                DO NOT FORGET!!! REGISTER -a3- MUST CONTAIN THE QUANTIZATION MATRIX PRIOR TO CALLING THIS FUNCTION!!!   *
*                             !!! FROM HERE ON, A5 is no longer the BASE REGISTER !!!!!                                  *
**************************************************************************************************************************
	ifne	APOLLO_DCTCLEAR
			lea	dct_zz,a5			;address of dct coeff matrix
	else
			lea	64*2+dct_zz,a5		;address of dct coeff matrix
			rept	32
				clr.l	-(a5)				;Clear entire coeff matrix
			endr
	endc
			move.l	lookup_DCT_coeff,a4
			lea	de_zz_order(pc),a6

	ifne	DCTLINEPOP
			moveq	#-1,d2
			move.l	d2,dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
			move.l	d2,4+dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
	endc
			moveq	#0,d7				;Starting position for ac coefficients.
			cmp.b	#4,d6
			blt.b	I_parse_y_coefficients
			cmp.b	#5,d6
			blt	I_parse_cb_coefficients
			bra	I_parse_cr_coefficients

;*******************************************************************************************************
; D7: counter (=0)
I_parse_y_coefficients:
			move.l	lookup_DCT_size_lum,a2

			DECODE_INTRA_DC	0,dct_dc_y_past
			move.l	d1,dct_dc_y_past
			
			;------------------------------------------------------
			;DeQuantize dc coefficient
			;------------------------------------------------------
			;Here, many quantization steps are performed in one ASL:
			;1. coeff * 2 / 16   =   coeff / 8   => coeff << 3		;normal dc dequantization
			;2. coeff * 0.125 * 256 = coeff * 32 => coeff << 5		;dft->dct conversion
			;3. coeff * 16                       => coeff << 4		;4 bits are used for fraction in idct
			;4. coeff / 256                      => coeff >> 8		;remove fractional part from coeff
			;------------------------------------------------------
			asl.l	#IDCT_FRAC,d1			;Combining all quantization steps
		ifne	DCTLINEPOP
			seq	d3				;if( dc == 0 ) d3 = -1 else d3=0
			move.w	d1,(a5)
			move.b	d3,dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
		else
			move.w	d1,(a5)
		endc
I_next_start:	
			moveq	#0,d6
I_next_coeff:
			DECODE_AC_COEFF	1,I_reconstruction_done

			add.b	d2,d7
			cmp.b	#62,d7
			bgt.w	block_err
;--------------------------------------------
;DeQuantize dct_coeff_next
;--------------------------------------------
			muls.w	2(a3,d7*2),d1		;Dequantize coefficient
			 move.w	2(a6,d7*2),d4
			 asr.l	#4,d1
			muls.w	quantizer_scale+2,d1
		ifne    DCTLINEPOP
			move.w	d4,d2			;zz pos
		 ifne    ZZ_TRANSPOSED
			 lsr.w	#3,d2			;(zz_pos>>3) = line index
		 else
			 and.b	#7,d2			;zz_pos&0xf  = column index
		 endc
		endc

;--------------------------------------------
;There are 3 steps, all done in a single ASR:
;1. coeff / 256 => coeff >> 8				;remove fraction part from previous muls
;2. coeff * 16  => coeff << 4				;add 4 bits of fraction for idct
;3. coeff / 8   => coeff >> 3				;normal quantization step (2 * coeff / 16)
;--------------------------------------------
			 asr.l	#7-IDCT_FRAC,d1		;All the above combined
			move.w	d1,(a5,d4*2)
		ifne	DCTLINEPOP
		 ifne	ZZ_TRANSPOSED
			and.b	#7,d4			;hor index (monotonically increasing)
		 else
			lsr.w	#3,d4			;row index
		 endc
			move.b	d4,dct_linepopulation-dct_zz(a5,d2.w)
		endc
			addq.l	#1,d7
			addq.l	#1,d6			;count number of coefficients extracted in d6
			bra.w	I_next_coeff

***********************
I_parse_cb_coefficients:
***********************
			move.l	lookup_DCT_size_chrom,a2

			DECODE_INTRA_DC 1,dct_dc_cb_past	;chroma mode, obtain "last" one from dct_dc_cb_past
			move.l	d1,dct_dc_cb_past

;------------------------------------------------------
;DeQuantize dc coefficient
;------------------------------------------------------
;Here, many quantization steps are performed in one ASL:
;1. coeff * 2 / 16   =   coeff / 8   => coeff << 3		;normal dc dequantization
;2. coeff * 0.125 * 256 = coeff * 32 => coeff << 5		;dft->dct conversion
;3. coeff * 16                       => coeff << 4		;4 bits are used for fraction in idct
;4. coeff / 256                      => coeff >> 8		;remove fractional part from coeff
;------------------------------------------------------
			asl.l	#IDCT_FRAC,d1			;Combining all quantization steps
		ifne	DCTLINEPOP
			seq	d3				;if( dc == 0 ) d3 = -1 else d3=0
			move.w	d1,(a5)
			move.b	d3,dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
		else
			move.w	d1,(a5)
		endc
			; the rest of the coefficients are identical in decoding between luma and chroma
			bra	I_next_start

***********************
I_parse_cr_coefficients:
***********************
			move.l	lookup_DCT_size_chrom,a2

			DECODE_INTRA_DC 1,dct_dc_cr_past	;chroma mode, obtain "last" one from dct_dc_cb_past

			move.l	d1,dct_dc_cr_past

			;------------------------------------------------------
			;DeQuantize dc coefficient
			;------------------------------------------------------
			;Here, many quantization steps are performed in one ASL:
			;1. coeff * 2 / 16   =   coeff / 8   => coeff << 3		;normal dc dequantization
			;2. coeff * 0.125 * 256 = coeff * 32 => coeff << 5		;dft->dct conversion
			;3. coeff * 16                       => coeff << 4		;4 bits are used for fraction in idct
			;4. coeff / 256                      => coeff >> 8		;remove fractional part from coeff
			;------------------------------------------------------
			asl.l	#IDCT_FRAC,d1			;Combining all quantization steps
		ifne	DCTLINEPOP
			seq	d3				;if( dc == 0 ) d3 = -1 else d3=0
			move.w	d1,(a5)
			move.b	d3,dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
		else
			move.w	d1,(a5)
		endc
			; the rest of the coefficients are identical in decoding between luma and chroma
			bra	I_next_start

I_reconstruction_done:

		IFNE	SHOW_COEFFS
****************************************
*         Display Coefficients         *
****************************************
		OUTTXT	RETURN
		OUTTXT	msg_macroblock
		OUTDEC	MB_Address
		OUTTXT	msg_block
		OUTDEC	block_count
		OUTTXT	RETURN
		lea	zz_order(pc),a4	;??
		lea	dct_zz,a5
		moveq	#8-1,d1
dct_loop_y	moveq	#8-1,d2
dct_loop_x	move.w	(a5)+,d3
		ext.l	d3
		OUTDECS16 d3
		dbf	d2,dct_loop_x
		OUTTXT	RETURN
		dbf	d1,dct_loop_y
		ENDC


idct_intra_start	
		ifeq	APOLLO_P96ONLY			; gray mode support disabled in P96ONLY build - we want color, don't we ?
			tst.b	GrayMode
			beq.b	.idct_all_blocks
			move.l	block_count,d1	;If gray playback, skip Cb/Cr blocks
			btst	#2,d1
			bne.w	skip_block
.idct_all_blocks:
		endc

			tst.l	d7
			beq.w	dc_only_idct		;If only dc coefficient in block

	ifne	APOLLO_IDCTX
			lea	zz_lines(pc),a6
	
			movem.l	d0/a0,-(a7)
			lea	dct_zz,a1

			moveq	#0,d0		;F
			move.b	(a6,d7.w),d0	;F B
			move.l	d0,a6		;  B  - number of populated lines

			IDCTX_Apollo
	else
			;--------------------------------------------------------
			; 68k iDCT
			;--------------------------------------------------------
			;Note: DCT line population could be used in the 68k DCT
			;      further: to simplify the "OR" op cascades
			movem.l	d0/a0,-(a7)
			IDCTX_INTRA_68k
	endc

	;------------------------------------------------------------------------
	ifne	APOLLO_IDCT
	;------------------------------------------------------------------------

	 ;-----------------------------------------------------
	 ifne	APOLLO_IDCTX
	 ;-----------------------------------------------------

		PORDDB	1,1,10	;move.l	d1,a1

		move.l	block_count,d1
		lea	idct_modulo_add,a5

		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)
		move.l	(a5,d1.w*4),a6		;a6: line stride

		PORBBD	10,10,1	;move.l	a1,d1
		lea	dct_zz,a1

		IDCTY_Apollo

		;
		;CLIP 8x8 block and store to framebuffer
		;
		paddw.w		#128,d0,d0
		paddw.w		#128,d4,d4
		packuswb	d0,d4,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,d1,d1
		paddw.w		#128,d5,d5
		packuswb	d1,d5,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,d2,d2
		paddw.w		#128,d6,d6
		packuswb	d2,d6,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,d3,d3
		paddw.w		#128,d7,d7
		packuswb	d3,d7,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,e0,e0
		paddw.w		#128,e4,e4
		packuswb	e0,e4,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,e1,e1
		paddw.w		#128,e5,e5
		packuswb	e1,e5,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,e2,e2
		paddw.w		#128,e6,e6
		packuswb	e2,e6,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		paddw.w		#128,e3,e3
		paddw.w		#128,e7,e7
		packuswb	e3,e7,(a0)	 ; pack and store dest1
;		adda.l		a6,a0		 ; next line

	 ;-----------------------------------------------------
	 else	; APOLLO_IDCTX
	 ;-----------------------------------------------------

		move.l	block_count,d1
		moveq	#2-1,d0

		lea	idct_modulo_add,a4
		move.l	convert_to_bitmap,a5

		lea	dct_zz,a1				;in-place iDCT

		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)
		lea	(a4,d1.l*4),a4

		; old Apollo iDCT see MacrosIDCTApolloOld.m
		APOLLO_IDCTY_OLD

	 ;-----------------------------------------------------
	 endc	; APOLLO_IDCTX
	 ;-----------------------------------------------------

		movem.l	(a7)+,d0/a0

	;------------------------------------------------------------------------
	else	; APOLLO_IDCT
	;------------------------------------------------------------------------

		move.l	block_count,d1

		lea	idct_modulo_add,a4
		move.l	convert_to_bitmap,a5
		lea	dct_zz,a1				;in-place iDCT

		moveq	#8,d0
		move.l	d0,a6
		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)

		; actual call for generic 68k iDCT
		IDCTY_INTRA_68k

	;------------------------------------------------------------------------
	endc	; APOLLO_IDCT
	;------------------------------------------------------------------------
		bra.w	skip_block

block_err	bra	Skip_CurrentPic

;		OUTTXT	BlockErrMsg
;		OUTTXT	msg_BufPos
;		move.l	a0,d1
;		sub.l	vidbufnew,d1
;		OUTDEC	d1
;		OUTTXT	msg_Data
;		OUTHEX	(a0)
;		OUTTXT	SPACE
;		OUTHEX	4(a0)
;		OUTTXT	MBAddressMsg
;		OUTDEC	MB_Address
;		OUTTXT	BlockNumberMsg
;		OUTDEC	block_count
;		OUTTXT	msg_runlevel
;		OUTDEC8	d2
;		OUTTXT	PER
;		OUTDECS_W d1
;		OUTTXT	RETURN
;		lea	zz_order(pc),a4
;		lea	dct_zz,a5
;		moveq	#8-1,d1
;blockerr_loop_y	moveq	#8-1,d2
;blockerr_loop_x	move.w	(a5)+,d3
;		OUTDECS16 d3
;		dbf	d2,blockerr_loop_x
;		OUTTXT	RETURN
;		dbf	d1,blockerr_loop_y
;		CHK32	d1
;		OUTBIN	d1
;		OUTTXT	RETURN
;		bra	NextSlice

dc_only_idct:
		move.w	dct_zz,d2
		move.l	block_count,d1	

	ifne	APOLLO_DCTZERO
		asr.w	#IDCT_FRAC,d2
	else
		move.l	convert_to_bitmap,a6
		asr.w	#IDCT_FRAC+1,d2
	endc
	ifne	APOLLO_DCTCLEAR
		clr.w	dct_zz
	endc
		move.l	MB_y_BitmapOffset.w(pc,d1.l*4),a1	;4xluma,Cb,Cr

	ifne	APOLLO_DCTZERO
		add.w	#128,d2
		;d2.w - unclipped DC
		VPERM	#$67676767,D2,D2,e8 	;DC.w DC.w DC.w DC.w
	endc

		add.l	y_bitmap_base.w(pc,d1.l*4),a1		;4xluma,Cb,Cr

		cmp.b	#4,d1
		blt.b	lum_dc_idct

	ifne	APOLLO_DCTZERO

		PACKUSWB	e8,e8,(a1)
dc_c_idct1A:	PACKUSWB	e8,e8,160*1(a1)
dc_c_idct2A:	PACKUSWB	e8,e8,160*2(a1)
dc_c_idct3A:	PACKUSWB	e8,e8,160*3(a1)
dc_c_idct4A:	PACKUSWB	e8,e8,160*4(a1)
dc_c_idct5A:	PACKUSWB	e8,e8,160*5(a1)
dc_c_idct6A:	PACKUSWB	e8,e8,160*6(a1)
dc_c_idct7A:	PACKUSWB	e8,e8,160*7(a1)
		bra.b	skip_block

lum_dc_idct:
		add.l	y_block_offset_table.w(pc,d1.l*4),a1

		PACKUSWB	e8,e8,(a1)
dc_y_idct1A:	PACKUSWB	e8,e8,320*1(a1)
dc_y_idct2A:	PACKUSWB	e8,e8,320*2(a1)
dc_y_idct3A:	PACKUSWB	e8,e8,320*3(a1)
dc_y_idct4A:	PACKUSWB	e8,e8,320*4(a1)
dc_y_idct5A:	PACKUSWB	e8,e8,320*5(a1)
dc_y_idct6A:	PACKUSWB	e8,e8,320*6(a1)
dc_y_idct7A:	PACKUSWB	e8,e8,320*7(a1)

	else
		move.b	(a6,d2.w*2),d2			;remove fractional part

		move.b	d2,d1
		lsl.w	#8,d1
		move.b	d2,d1
		move.w	d1,d2
		swap	d1
		move.w	d2,d1
		move.l	d1,(a1)
dc_c_idct1:	move.l	d1,160*0+4(a1)	;beware: self modifying code
dc_c_idct2:	move.l	d1,160*1+0(a1)
dc_c_idct3:	move.l	d1,160*1+4(a1)
dc_c_idct4:	move.l	d1,160*2+0(a1)
dc_c_idct5:	move.l	d1,160*2+4(a1)
dc_c_idct6:	move.l	d1,160*3+0(a1)
dc_c_idct7:	move.l	d1,160*3+4(a1)
dc_c_idct8:	move.l	d1,160*4+0(a1)
dc_c_idct9:	move.l	d1,160*4+4(a1)
dc_c_idct10:	move.l	d1,160*5+0(a1)
dc_c_idct11:	move.l	d1,160*5+4(a1)
dc_c_idct12:	move.l	d1,160*6+0(a1)
dc_c_idct13:	move.l	d1,160*6+4(a1)
dc_c_idct14:	move.l	d1,160*7+0(a1)
dc_c_idct15:	move.l	d1,160*7+4(a1)
		bra.b	skip_block

lum_dc_idct:
		add.l	y_block_offset_table.w(pc,d1.l*4),a1
		move.b	(a6,d2.w*2),d2				;remove fractional part
		move.b	d2,d1
		lsl.w	#8,d1
		move.b	d2,d1
		move.w	d1,d2
		swap	d1
		move.w	d2,d1
		move.l	d1,(a1)
dc_y_idct1:	move.l	d1,320*0+4(a1)	;self modifying code
dc_y_idct2:	move.l	d1,320*1+0(a1)
dc_y_idct3:	move.l	d1,320*1+4(a1)
dc_y_idct4:	move.l	d1,320*2+0(a1)
dc_y_idct5:	move.l	d1,320*2+4(a1)
dc_y_idct6:	move.l	d1,320*3+0(a1)
dc_y_idct7:	move.l	d1,320*3+4(a1)
dc_y_idct8:	move.l	d1,320*4+0(a1)
dc_y_idct9:	move.l	d1,320*4+4(a1)
dc_y_idct10:	move.l	d1,320*5+0(a1)
dc_y_idct11:	move.l	d1,320*5+4(a1)
dc_y_idct12:	move.l	d1,320*6+0(a1)
dc_y_idct13:	move.l	d1,320*6+4(a1)
dc_y_idct14:	move.l	d1,320*7+0(a1)
dc_y_idct15:	move.l	d1,320*7+4(a1)

	endc

skip_y_dc_idct

skip_block:
		moveq	#1,d6
		add.l	block_count,d6
		cmp.b	#6,d6
		bge	Macroblock_Done				;current MB finished -> check for next MB
		bra	I_block_loop


;------------------------------------;
;--- Parse Macroblock in P frames ---;
;------------------------------------;
P_MacroBlock:		move.l	MB_Address,d1		;calculate base address of current macroblock
			lea	(MB_y_OffsetTable-VDEC_BASE).l(a5),a1 ; change to absolute if assembler barks
			lea	(MB_c_OffsetTable-VDEC_BASE).l(a5),a2
			;lea	VDEC_BASE,a5			;use a base ptr to avoid reloc

			lea	MB_y_BitmapOffset(pc),a4
			move.l	(a1,d1.l*4),d2			;for y_bitmap
			move.l	(a2,d1.l*4),d3

			cmp.l	y_MB_max_addr(pc),d2
			bgt.w	Skip_CurrentPic
			subq.l	#1,d1

		ifne	APOLLO_MOT
			move.l	d2,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			 vperm	#$45674567,d2,d2,d2
			 move.l	d3,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
			vperm	#$45674567,d3,d3,d3
			store	d2,MB_y_BitmapOffset-MB_y_BitmapOffset(a4);
			 store	d2,MB_y_BitmapOffset+8-MB_y_BitmapOffset(a4);
			store	d3,MB_c_BitmapOffset-MB_y_BitmapOffset(a4)
		else
			move.l	d2,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+4-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+8-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+12-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)

			move.l	d3,MB_c_BitmapOffset-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)	;for c_bitmaps
			move.l	d3,MB_c_BitmapOffset+4-MB_y_BitmapOffset(a4);-VDEC_BASE(a5)	;for c_bitmaps
			move.l	d3,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
		endc
			sub.l	last_Macroblock-VDEC_BASE(a5),d1	;check if skipped any blocks!
			beq	p_normal_mb_increment

p_mb_skip:
			clr.l	mv_fwd_xy_long-VDEC_BASE(a5)
			clr.l	dct_dc_y_past-VDEC_BASE(a5)		;Clear all deltas
			clr.l	dct_dc_cb_past-VDEC_BASE(a5)
			clr.l	dct_dc_cr_past-VDEC_BASE(a5)

			;Straight copy from reference bitmap for skipped areas!!!

p_mb_skip_process_col:
			moveq	#1,d2
			add.l	last_Macroblock-VDEC_BASE(a5),d2		;start from 1st skipped block (d2 = temp_mb_address)
		
			moveq	#-16,d4
			add.l	y_bitmap_width(pc),d4
			moveq	#-8,d5
			add.l	c_bitmap_width(pc),d5
p_mb_skipped_loop:
			; parameters for luma
			move.l	(MB_y_OffsetTable.l,d2.l*4),d6

			cmp.l	y_MB_max_addr,d6
			bgt.w	Skip_CurrentPic			;-> done, no more Macroblocks left in current picture

			move.l	d6,a1
			add.l	fwd_reference_y,a1	;reference in a1
			move.l	d6,a2
			add.l	y_bitmap_base(pc),a2		;current bitmap in a2
			
			; 16x16 copy to aligned framebuffer
			moveq	#8-1,d1
			lea.l	16(a1,d4.l),a3
			lea.l	16(a2,d4.l),a5
.mb_copy_loop_y:
			;move16	(a1)+,(a2)+
			
			move.l	(a1)+,(a2)+
			move.l	(a1)+,(a2)+
			move.l	(a1)+,(a2)+
			move.l	(a1)+,(a2)+
			
			lea	16(a1,d4.l*2),a1
			lea	16(a2,d4.l*2),a2

			;move16	(a3)+,(a5)+

			move.l	(a3)+,(a5)+
			move.l	(a3)+,(a5)+
			move.l	(a3)+,(a5)+
			move.l	(a3)+,(a5)+

			lea	16(a3,d4.l*2),a3
			lea	16(a5,d4.l*2),a5
			dbf	d1,.mb_copy_loop_y

		IFEQ	APOLLO_CLIP
			tst.b	GrayMode
			bne.b	p_mb_copy_loop_next
		ENDC
			; parameters for chroma
			move.l  (MB_c_OffsetTable.l,d2.l*4),d6

			move.l	fwd_reference_cb,a3	;reference in a3
			add.l	d6,a3
			move.l	fwd_reference_cr,a5	;reference in a5
			add.l	d6,a5
			move.l	cb_bitmap_base(pc),a4		;current bitmap in a4
			add.l	d6,a4
			move.l	cr_bitmap_base(pc),a6		;current bitmap in a6
			add.l	d6,a6
			
			; chroma copy
			moveq	#8-1,d1				;this inner loop copies an entire macroblock
.mb_copy_loop_c	
			move.l	(a3)+,(a4)+
			move.l	(a3)+,(a4)+
			add.l	d5,a3
			add.l	d5,a4
			move.l	(a5)+,(a6)+
			move.l	(a5)+,(a6)+
			add.l	d5,a5
			add.l	d5,a6
			dbf	d1,.mb_copy_loop_c

p_mb_copy_loop_next	addq.l	#1,d2
			cmp.l	MB_Address,d2
			blt	p_mb_skipped_loop		;loop till current MB_Address, clearing all skipped macroblocks

			lea	VDEC_BASE,a5			;use a base ptr to avoid reloc
p_normal_mb_increment:
p_mb_type_chk:
			move.l	lookup_MB_type_P-VDEC_BASE(A5),a1

		ifne	NOA0DIRECT
			CHKBITS 6,d7
		else
			move.w	(a0),d7				;CHKBITS 6,d7
			moveq	#10,d6
			lsl.l	d0,d7
			lsr.w	d6,d7
		endc
			move.w	(a1,d7.w*2),d7
			NNEXTVLC d7,d1
			lsr.w	#8,d7
			move.l	d7,MB_Type-VDEC_BASE(a5)

			btst	#MB_QUANT,d7
			beq	p_mb_quant_skip

p_mb_quant		NGETBITS 5,d1,d2			;if mb_quant, get quant. scale!
			move.l	d1,quantizer_scale-VDEC_BASE(a5)
p_mb_quant_skip
			btst	#MB_INTRA,d7
			beq.b	.p_mb_nonintra

			IFNE	SHOW_MBINFO
			OUTTXT	msg_intra
			ENDC
			move.b	#1,macroblock_intra-VDEC_BASE(a5)
			move.b	#63,coded_block_pattern-VDEC_BASE(a5)	;if intra macroblock, all blocks are stored!!!
			clr.l	mv_fwd_xy_long-VDEC_BASE(a5)
			lea	intra_quant_matrix_zz,a3		;if intra macrblock, use intra quant matrix!!!
			bra	P_dct_reconstruct

.p_mb_nonintra:
			clr.b	macroblock_intra-VDEC_BASE(a5)
			clr.l	dct_dc_y_past-VDEC_BASE(a5)				;Clear all deltas
			clr.l	dct_dc_cb_past-VDEC_BASE(a5)
			clr.l	dct_dc_cr_past-VDEC_BASE(a5)

			btst	#MB_MOTION_FWD,d7
			beq	p_no_mb_fwd

		;MV DEC for P-Frames horizontal
			move.l	lookup_motion_vector-VDEC_BASE(a5),a1		;for DECODE_MV
			CHKBITS 11,d1						;\1 in DECODE_MV
			moveq	#0,d4
			move.b	forward_r_size-VDEC_BASE(a5),d4			;d4 = forward_r_size for DECODE_MV
			
			DECODE_MV d1,d3		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels

_marker_p_mv_decode_vert:
		;MV DEC for P-Frames vertical 
			CHKBITS 11,d7				;get next code

			DECODE_MV d7,d2		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels

		; MVD to MV for both hor and ver components
			add.w	mv_fwd_x-VDEC_BASE(a5),d3	;add.l   recon_right_for_prev(pc),d3
			moveq	#27,d5		;bound motion vector:( vec <<(27-f_code) ) >> (27-f_code) );
			sub.l	d4,d5
			add.w	mv_fwd_y-VDEC_BASE(a5),d2	;add.l	recon_down_for_prev(pc),d2
			ext.l	d3
			ext.l	d2
			lsl.l	d5,d3
			lsl.l	d5,d2
			asr.l	d5,d3
			asr.l	d5,d2
			move.w	d3,mv_fwd_x-VDEC_BASE(a5)	;move.w	d3,recon_right_for_prev-VDEC_BASE(a5)
			move.l	d2,d4		;d4 used in MC routines as vertical displacement
			move.w	d2,mv_fwd_y-VDEC_BASE(a5)	;move.w	d2,recon_down_for_prev-VDEC_BASE(a5)

			IFNE	SHOW_MBINFO
			OUTTXT	msg_fwd
			OUTDECS	d3
			OUTTXT	COMMA
			OUTDECS	d4
			OUTTXT	msg_fwd_end
			ENDC

			bra.s	P_calc_ref_offset

p_no_mb_fwd:
			IFNE	SHOW_MBINFO
			OUTTXT	msg_no_motion
			ENDC
			clr.l	mv_fwd_xy_long-VDEC_BASE(a5)
			clr.l	MB_c_fwd_dydx-VDEC_BASE(a5)			; store chroma fractional displacement

			lea	y_block_offset_table(pc),a4
			move.l	block_buffer_tmp_y1,a2
			bsr	MOT_P_getCBP_InterMC

			moveq	#0,d5
			moveq	#0,d6

			move.l	d0,-(sp)
			bra	MC_P_Start
;			bra	ref_full_all

;------------------------------------------------------------------------
;
; P-Frame Motion compensation
;-----------------------------------------------------------------------
P_calc_ref_offset:
			bsr	MOT_P_getCBP_InterMC	;get CBP and pointer to MC structure

			move.l	d0,-(sp)

			; convert full pel to halfpel vectors if necessary
			tst.b	full_pel_forward_vector
			beq.b	.half_fwd_vector
			add.l	d3,d3
			add.l	d4,d4
.half_fwd_vector

	;---------------- chroma vector adjustment -----------------------------
			ADJUST_CVECTOR  ; in/out: d3/d4 out: d1/d2 trash: all other data regs

	;------------------ luma position calculation in reference frame --------------
			move.l	y_bitmap_width(pc),d7
			asr.l	#1,d4					;d4 = down_for (for luminance)
			scs	d6					;d6 = down_half (TRUE/FALSE)!
			muls.w	d4,d7					;vertical offset (no. of bytes)
			asr.l	#1,d3					;d3 = right_for (for luminance)
			scs	d5					;d5 = right_half (TRUE/FALSE)!
			add.l	d3,d7
			add.l	d7,MB_y_fwd_BitmapOffset

	;---- chroma frame position calculation and halfpel remainder calculation ------
			; Apollo magic: 4 instructions in one clock 
			moveq	#1,d7	; just keep lowest bit
			and.l	d2,d7	; dy
			moveq	#1,d0	; 
			and.l	d1,d0	; dx
			 add.l	d7,d7
			 asr.l	#1,d2
			add.l	d0,d7

			move.l	c_bitmap_width(pc),d3
			asr.l	#1,d1	; chroma fullpel
			muls.w	d2,d3
			move.l	d7,MB_c_fwd_dydx			; store chroma fractional displacement
			add.l	d1,d3
			add.l	d3,MB_c_fwd_BitmapOffset

MC_P_Start:	; jump in from p_no_mb_fwd
			move.l	fwd_reference_y,a1
			move.l	fwd_reference_cb,a2
			move.l	MB_c_fwd_BitmapOffset,d3
			moveq	#0,d1
			add.l	MB_y_fwd_BitmapOffset,a1
			add.l	d3,a2
			move.l	fwd_reference_cr,a3
			add.l	d3,a3

			move.l	a2,MC_SrcPTR_Cb(a6)			; source pointers chroma
			move.l	a3,MC_SrcPTR_Cr(a6)

			;dotouch	(a1,d1.l)	;
			
			bsr	MC_COPY
			move.l	(sp)+,d0
			bra	MOT_P_ref_block_done

;-------------------------------------------------------------------------------------------------------------------
; Motion Compensation in copy mode (copy from reference frame to output frame or temp buffer)
; Input: D5 - != 0 -> interpolation in X required
;        D6 - != 0 -> interpolation in Y required
;        A6 - MC_DATA struct
;	 A1 - reference ptr Y ( fwd_reference_y + MB_y_fwd_BitmapOffset or the respective backward combo )
;
; Registers used/trashed:
;        ALL Dn, An
;-------------------------------------------------------------------------------------------------------------------
MC_COPY:
			;rts	;do nothing (debug only)

			; Luma halfpel position decision
			tst.b	d5
			beq	ref_no_right_half
			tst.b	d6
			beq	ref_right_half

;----------------------------------------------------------
; P FRAME - HALF ALL (Both X and Y coordinates half values)
;----------------------------------------------------------
ref_half_all:
			move.l	y_bitmap_width(pc),a5	;B
			move.l	a5,a3			;B

			moveq	#3,d4			
			; in:     -
			; out: D0 - mask for lower two bits ($03030303)
			;      D6 - mask for upper six bits ($FCFCFCFC)
			MOT_8x1_HALFHORVER_INIT

			move.l	MC_DestPTR_Y(a6),a2
			add.l	a1,a3
.lum_loop
			moveq	#8-1,d5
.y_yloop
			; in:  D0 - mask for lower two bits ($03030303 - 68k only)
			;      D6 - mask for upper six bits ($FCFCFCFC - 68k only)
			;      A1 - input pixel pointer first line
			;      A2 - output pointer 
			;      A3 - input pixel pointer second line
			;      A5 - input stride
			; out:
			;      A1 - next position to load from (+4) 
			;      A2 - next position to write to  (+4)
			;      A3 - next position to load from (+4)
			; trash:
			;      D1,D2,D3,D7
			MOT_8x1_HALFHORVER 0
			;dotouch	(a3,a5.l) ; moved into macro
			
			add.l	a5,a1			;go to next line in src bitmap (row 1)
			 add.w	MC_Y_LINESTRIDE0(a6),a2
			 add.l	a5,a3			;go to next line in src bitmap (row 2)

			dbf	d5,.y_yloop
	
			move.w	MC_Y_BLOCKOFF0(a6),d5
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6
			add.w	d5,a3
			 ;dotouch (a1,d5.w)
			 add.w	d5,a1
			dbf	d4,.lum_loop

			subq.l	#8,a6			;restore MC ptr
			bra	forw_mot_c

;----------------------------------------------------------
; P FRAME - RIGHT HALF (X coordinate half value)
;----------------------------------------------------------
ref_right_half:
			moveq	#3,d4

			MOT_8x1_HALFHOR_INIT

			move.l	y_bitmap_width(pc),a5
			
			move.l	MC_DestPTR_Y(a6),a2
.lum_loop
	ifne	APOLLO_MOT
			moveq	#8-1,d5
			moveq	#8,d1
			add.w	MC_Y_LINESTRIDE0(a6),d1
.y_yloop
			LOAD	(A1),E8
			PAVGB	1(A1),E8,E9
			;dotouch	(a1,a5.l)
			add.l	a5,a1
			STORE	E9,(a2)
			add.w	d1,a2
			dbf	d5,.y_yloop
	else
			moveq	#8-1,d5
.y_yloop
			MOT_8x1_HALFHOR	0
			add.l	a5,a1
			add.w	MC_Y_LINESTRIDE0(a6),a2
			dbf	d5,.y_yloop
	endc

			add.w	MC_Y_BLOCKOFF0(a6),a1
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6

			dbf	d4,.lum_loop
			subq.l	#8,a6

			bra	forw_mot_c

ref_no_right_half
			tst.b	d6
			beq		ref_full_all
;----------------------------------------------------------
; P FRAME - DOWN HALF (Y coordinates half value)
;----------------------------------------------------------
ref_down_half:
			move.l	y_bitmap_width(pc),d7
			moveq	#3,d4

			 MOT_8x1_HALFHOR_INIT
			
			move.l	MC_DestPTR_Y(a6),a2
			move.l	d7,a5
			lea	(a1,d7.l),a3
.lum_loop
	ifne	APOLLO_MOT
			moveq	#8-1,d5
			moveq	#8,d1
			add.w	MC_Y_LINESTRIDE0(a6),d1
.y_yloop
			LOAD	(A1),E8
			add.l	a5,a1
			PAVGB	(A3),E8,E9
			add.l	a5,a3
			;dotouch	(a1,a5.l)
			STORE	E9,(a2)
			add.w	d1,a2
			dbf	d5,.y_yloop
	else
			moveq	#8-1,d5
.y_yloop
			MOT_8x1_HALFVER	0

			 ;dotouch (a3,a5.l)
			 add.l	a5,a1
			add.w	MC_Y_LINESTRIDE0(a6),a2
			add.l	a5,a3

			dbf	d5,.y_yloop
	endc
			move.w	MC_Y_BLOCKOFF0(a6),d5
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6
			add.w	d5,a1
			add.w	d5,a3

			dbf	d4,.lum_loop
			subq.l	#8,a6

			bra	forw_mot_c

;----------------------------------------------------------
; P FRAME - FULL ALL (Both X and Y coordinates full values)
;----------------------------------------------------------
ref_full_all:		
			move.l	y_bitmap_width(pc),d0
			moveq	#3,d4
			 move.l	MC_DestPTR_Y(a6),a2
			 move.l	a1,a3
			move.w	MC_Y_LINESTRIDE0(a6),d1
			add.l	d0,a3
.lum_loop		
			 move.w	MC_Y_BLOCKOFF0(a6),d6
			 moveq	#8-1,d5
.yloop
			move.l	(a1)+,(a2)+	;F Apollo fuses this construct
			move.l	(a1)+,(a2)+	;F
			move.l	a3,a1
			 ;dotouch (a3,d0.l)
			 add.w	d1,a2
			lea	(a3,d0.l),a3

			dbf	d5,.yloop
	
			 add.w	d6,a1
			 add.w	MC_Y_DESTOFF0(a6),a2
			add.w	d6,a3
			addq.l	#2,a6

			dbf	d4,.lum_loop
			subq.l	#8,a6

			bra	forw_mot_c

;-----------------------------------------------------------
; Chroma Forward Motion for P Frames redirector 
;-----------------------------------------------------------
forw_mot_c:
			move.l	MB_c_fwd_dydx,d4	; chroma fractional displacement
			beq.w	forw_mot_c_00		; full pel chroma MC
			cmp.w	#1,d4			; dy=0 dx=1
			beq	forw_mot_c_01		;
			cmp.w	#2,d4			; dy=1 dx=0
			beq	forw_mot_c_10		;
;			bra	forw_mot_c_11		; enable when moving forw_mot_c_11 away

;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors both directions 
;-----------------------------------------------------------
forw_mot_c_11:		;
			move.l	MC_SrcPTR_Cb(a6),a1
			moveq	#8-1,d5
			 move.l	c_bitmap_width(pc),a5
			 move.l	a1,a3
			MOT_8x1_HALFHORVER_INIT
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
			add.l	a5,a3				;a3: row 2
			 ;dotouch (a1,a5.l) ;
.cb_yloop
			MOT_8x1_HALFHORVER 0
			;dotouch	(a3,a5.l)	;moved into macro
			
			add.l	a5,a1			;go to next line in src bitmap (row 1)
			 add.w	MC_C_LINESTRIDE(a6),a2
			 add.l	a5,a3			;go to next line in src bitmap (row 2)
			dbf	d5,.cb_yloop

			move.l	a5,a3
			move.l	MC_SrcPTR_Cr(a6),a1
			 moveq	#8-1,d5
			 move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
			add.l	a1,a3
.cr_yloop
			MOT_8x1_HALFHORVER 0
			;dotouch	(a3,a5.l)	;moved into macro
			
			add.l	a5,a1			;go to next line in src bitmap (row 1)
			 add.w	MC_C_LINESTRIDE(a6),a2
			 add.l	a5,a3			;go to next line in src bitmap (row 2)
			dbf	d5,.cr_yloop

			rts

;-----------------------------------------------------------
; Chroma Forward Motion for Full Pel Vectors
;-----------------------------------------------------------
forw_mot_c_00:		; full pel chroma MC
			move.l	MC_SrcPTR_Cb(a6),a1
			moveq	#8-1,d5
			 move.l	c_bitmap_width(pc),d0
			 move.l	a1,a3
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
			add.l	d0,a3
			 ;dotouch (a1,d0.l)
			move.w	MC_C_LINESTRIDE(a6),d1
.cb_yloop
			move.l	(a1)+,(a2)+
			move.l	(a1)+,(a2)+
			 ;dotouch (a3,d0.l)
			 move.l	a3,a1
			lea	(a3,d0.l),a3
			add.w	d1,a2
			dbf	d5,.cb_yloop

			 move.l	MC_SrcPTR_Cr(a6),a1	;B
			 move.l	a1,a3			;B
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
			 add.l	d0,a3
.cr_yloop
			move.l	(a1)+,(a2)+
			move.l	(a1)+,(a2)+
			 ;dotouch (a3,d0.l)
			 move.l	a3,a1
			lea	(a3,d0.l),a3
			add.w	d1,a2
			dbf	d5,.cr_yloop

			rts

;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors horizontal
;-----------------------------------------------------------
forw_mot_c_01:		;
			MOT_8x1_HALFHOR_INIT

			move.l	MC_SrcPTR_Cb(a6),a1
			 move.l	c_bitmap_width(pc),a3
			 moveq	#8-1,d5
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
			 move.w	MC_C_LINESTRIDE(a6),a5
.cb_yloop
			MOT_8x1_HALFHOR 0
			;dotouch (a1,a3.l)
			add.l	a3,a1
			add.w	a5,a2
			dbf	d5,.cb_yloop

			move.l	MC_SrcPTR_Cr(a6),a1
			moveq	#8-1,d5
			 move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_HALFHOR 0
			;dotouch (a1,a3.l)
			add.l	a3,a1
			add.w	a5,a2
			dbf	d5,.cr_yloop
			rts
;
;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors vertical
;-----------------------------------------------------------
forw_mot_c_10:		;
			move.l	MC_SrcPTR_Cb(a6),a1
			MOT_8x1_HALFHOR_INIT
			move.l	c_bitmap_width(pc),d7
			move.l	d7,a5
			moveq	#8-1,d5
			lea	(a1,d7.l),a3
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
.cb_yloop
			MOT_8x1_HALFVER 0
			;dotouch	(a3,a5.l)
			add.l	a5,a1
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3
			dbf	d5,.cb_yloop

			move.l	a5,a3
			move.l	MC_SrcPTR_Cr(a6),a1
			add.l	a1,a3
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_HALFVER 0
			;dotouch	(a3,a5.l)
			add.l	a5,a1
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3
			dbf	d5,.cr_yloop

			rts


;-----------------------------------------------------------
; Chroma Backward Motion for B Frames redirector 
;-----------------------------------------------------------
backw_mot_c_ADD:
			move.l	MB_c_bwd_dydx,d4	; chroma fractional displacement
			beq.w	backw_mot_c_00ADD	; full pel chroma MC
			cmp.w	#1,d4			; dy=0 dx=1
			beq	backw_mot_c_01ADD	;
			cmp.w	#2,d4			; dy=1 dx=0
			beq	backw_mot_c_10ADD	;
;			bra	backw_mot_c_11ADD	; enable when moving backw_mot_c_11ADD away

;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors both directions 
;-----------------------------------------------------------
backw_mot_c_11ADD:	;

			move.l	MC_SrcPTR_Cb(a6),a1

			moveq	#8-1,d5
			move.l	a1,a3

			move.l	c_bitmap_width(pc),a5
			MOT_8x1_HALFHORVER_INIT
			add.l	a5,a3				;a3: row 2
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
.cb_yloop
			MOT_8x1_HALFHORVER_ADD 0

			add.l	a5,a1			;go to next line in src bitmap (row 1)
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3			;go to next line in src bitmap (row 2)
			dbf	d5,.cb_yloop

			move.l	a5,a3
			move.l	MC_SrcPTR_Cr(a6),a1
			moveq	#8-1,d5
			add.l	a1,a3
			;0 1 4 5 . . . .
			;2 3 6 7 . . . .
			 move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_HALFHORVER_ADD 0

			add.l	a5,a1			;go to next line in src bitmap (row 1)
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3			;go to next line in src bitmap (row 2)
			dbf	d5,.cr_yloop

			rts

;-----------------------------------------------------------
; Chroma Forward Motion for Full Pel Vectors
;-----------------------------------------------------------
backw_mot_c_00ADD:	; full pel chroma MC
			MOT_8x1_HALFHOR_INIT

			move.l	MC_SrcPTR_Cb(a6),a1
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
.cb_yloop
			MOT_8x1_HALFHOR_ADD 0
			add.l	c_bitmap_width(pc),a1
			add.w	MC_C_LINESTRIDE(a6),a2
			dbf	d5,.cb_yloop

			move.l	MC_SrcPTR_Cr(a6),a1
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_HALFHOR_ADD 0
			add.l	c_bitmap_width(pc),a1
			add.w	MC_C_LINESTRIDE(a6),a2
			dbf	d5,.cr_yloop
			rts

;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors horizontal
;-----------------------------------------------------------
backw_mot_c_01ADD:	;
			MOT_8x1_HALFHOR_INIT

			move.l	MC_SrcPTR_Cb(a6),a1
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
.cb_yloop
			MOT_8x1_FULL_ADD 0
			add.l	c_bitmap_width(pc),a1
			add.w	MC_C_LINESTRIDE(a6),a2
			dbf	d5,.cb_yloop

			move.l	MC_SrcPTR_Cr(a6),a1
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_FULL_ADD 0
			add.l	c_bitmap_width(pc),a1
			add.w	MC_C_LINESTRIDE(a6),a2
			dbf	d5,.cr_yloop

			rts

;-----------------------------------------------------------
; Chroma Forward Motion for Half Pel Vectors vertical
;-----------------------------------------------------------
backw_mot_c_10ADD:		;
			move.l	MC_SrcPTR_Cb(a6),a1
			MOT_8x1_HALFHOR_INIT
			move.l	c_bitmap_width(pc),d7
			move.l	d7,a5
			moveq	#8-1,d5
			lea	(a1,d7.l),a3
			move.l	MC_DestPTR_Cb(a6),a2		;destination pointer
.cb_yloop
			MOT_8x1_HALFVER_ADD 0
			add.l	a5,a1
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3
			dbf	d5,.cb_yloop

			move.l	a5,a3
			move.l	MC_SrcPTR_Cr(a6),a1
			add.l	a1,a3
			moveq	#8-1,d5
			move.l	MC_DestPTR_Cr(a6),a2		;destination pointer
.cr_yloop
			MOT_8x1_HALFVER_ADD 0
			add.l	a5,a1
			add.w	MC_C_LINESTRIDE(a6),a2
			add.l	a5,a3
			dbf	d5,.cr_yloop

			rts


;----------------------------------------------------------------------------------------------------
; Read P frame CBP, select between direct MC or buffered MC
;----------------------------------------------------------------------------------------------------
MOT_P_getCBP_InterMC:
.p_chk_pattern:	
			clr.b	coded_block_pattern
			moveq	#1<<MB_PATTERN,d7
			and.l	MB_Type,d7
			beq	.p_direct_mc

			CHKBITS	9,d1
			move.l	lookup_block_pattern,a1
			move.w	(a1,d1.l*2),d1
			NNEXTVLC d1,d2
			lsr.w	#8,d1
			move.b	d1,coded_block_pattern
			beq.s	.p_direct_mc

			; at least one block has coefficients,
			; perform buffered motion compensation
			lea	mc_offsets_buffered,a6	;nothing more to do, parameters are constant across one frame
			rts
.p_direct_mc:
			; no coefficients, predict directly to framebuffer
			lea	mc_offsets_direct,a6	;direct MC implementation

			move.l	y_bitmap_base(pc),a1
			add.l	MB_y_BitmapOffset(pc),a1	;get current MB base
			move.l	a1,MC_DestPTR_Y(a6)

			move.l	MB_c_BitmapOffset(pc),d1	;

			move.l	cb_bitmap_base(pc),a1
			move.l	cr_bitmap_base(pc),a2
			add.l	d1,a1
			move.l	a1,MC_DestPTR_Cb(a6)
			add.l	d1,a2
			move.l	a2,MC_DestPTR_Cr(a6)
			rts


;----------------------------------------------------------------------------------------------------
MOT_P_ref_block_done:
			lea	nonintra_quant_matrix_zz,a3		;if non-intra mb, use non-intra quant matrix!!!
			
	;		bsr	MOT_P_getCBP_InterMC
			tst.b	coded_block_pattern
			beq	Macroblock_Done

P_dct_reconstruct:	;called for P, B and Intra in P/B, branches away for intra MBs
			IFNE	SHOW_MBINFO
			OUTTXT	RETURN
			ENDC

			tst.b	macroblock_intra		;if intra macroblock -> render intra!
			bne	I_dct_reconstruct

		moveq	#0,d6
P_block_loop:
			moveq	#0,d2
			move.b	DCT_P_CLEAR,d2

			move.l	d6,block_count

**************************************************************************************************************************
* Decode Blocks: This code parses coefficients in file and reconstructs some or all the blocks in a macroblock,          *
*                according to coded_block_pattern. It performs dequantization of all coefficients, according to          *
*                the quantization matrix pointed to by register - a3 - which must be in zigzag-ed order, and must        *
*                also have FFT to DCT transform constants multiplied with it, as the DCT is implemented as a fast        *
*                FFT algorithm.                                                                                          *
*                                                                                                                        *
*                DO NOT FORGET!!! REGISTER -a3- MUST CONTAIN THE QUANTIZATION MATRIX PRIOR TO CALLING THIS FUNCTION!!!   *
**************************************************************************************************************************

			move.l	quantizer_scale,d5
			move.l	lookup_DCT_coeff,a4
			lea	de_zz_order(pc),a6

			move.b	coded_block_pattern,d1
	ifne	APOLLO_DCTCLEAR
		ifeq	DCTCLEAR_BYPASS
			lea	dct_zz,a5			;address of dct coeff matrix
		endc
	else
		ifne DCTCLEAR_BYPASS
			dbf	d2,.dct_clear			;tst.w d2 ; bge.w .dct_clear
		endc
			lea	64*2+dct_zz,a5		;address of dct coeff matrix
			rept	32
				clr.l	-(a5)				;Clear entire coeff matrix
			endr
	endc
			move.b	#1,DCT_P_CLEAR
.dct_clear
	ifne	DCTCLEAR_BYPASS
			lea	dct_zz,a5		;address of dct coeff matrix
	endc
			moveq	#5,d2
			sub.l	d6,d2
			btst	d2,d1
	ifne	IDCT_BYPASS
			beq	idct_bypass
	else
			beq	idct_forward_start		;skip if block is not coded (according to coded_block_pattern)
	endc
			clr.b	DCT_P_CLEAR			;DCT is not clear

			bra	P_parse_coefficients
	ifne	IDCT_BYPASS
;added by Buggs - 16 Oct 2016 - BYPASS ALL DCT CONSIDERATIONS IF CBP BIT IS 0
idct_bypass:
		move.l	d6,d2
		move.l	block_buffer_tmp_y1,a4

		lea	idct_modulo_add,a5
		move.l	y_bitmap_base.w(pc,d6.l*4),a1		;destination ptr 
		lsl.l	#6,d2					;block_count * 64 = offset for y block
		add.l	MB_y_BitmapOffset.w(pc,d6.l*4),a1	;MB offset
		add.l	d2,a4					;reference base in a4
		add.l	y_block_offset_table.w(pc,d6.l*4),a1	;add block offset to bitmap offset (a0)

		moveq	#8-1,d1
		moveq	#-8,d2
		add.l	(a5,d6.w*4),d2
.idct_bypass_loop:
		move.l	(a4)+,(a1)+
		move.l	(a4)+,(a1)+
		add.l	d2,a1
		dbf	d1,.idct_bypass_loop

		bra	fwd_skip_block
	endc

;**************************************************************************************************************
; Common subroutine for Y,Cb,Cr in B/P frames
;
P_parse_coefficients:
			moveq	#0,d7			;Starting position for ac coefficients.

	ifne	DCTLINEPOP
			moveq	#-1,d2
			move.l	d2,dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
			move.l	d2,4+dct_linepopulation-dct_zz(a5)	;track populated lines in DCT array
	endc

_P_coeff_first: ; just a marker
			DECODE_AC_COEFF	0,P_coeff_first_out
P_coeff_first_out:
			move.b	d2,d7
			;addq.l	#1,d7
			cmp.b	#63,d2
			bgt.w	block_err
;--------------------------------------------
;DeQuantize dct_coeff_next
;--------------------------------------------
			muls.w	(a3,d7*2),d1		;Dequantize coefficient
			 move.w	(a6,d7*2),d4		;zz pos
			 asr.l	#4,d1
			muls.w	d5,d1
			move.w	d4,d2			;zz pos
		ifne    DCTLINEPOP
		 ifne    ZZ_TRANSPOSED
			 lsr.w	#3,d2			;(zz_pos>>3) = line index
		 else
			 and.b	#7,d2			;zz_pos&0xf  = column index
		 endc
		endc
;--------------------------------------------
;There are 3 steps, all done in a single ASR:
;1. coeff / 256 => coeff >> 8			;remove fraction part from previous muls
;2. coeff * 16  => coeff << 4			;add 4 bits of fraction for idct
;3. coeff / 8   => coeff >> 3			;normal quantization step (2 * coeff / 16)
;--------------------------------------------
			 asr.l	#7-IDCT_FRAC,d1		;All the above combined
			move.w	d1,(a5,d4*2)
		ifne	DCTLINEPOP
		 ifne	ZZ_TRANSPOSED
			and.b	#7,d4			;hor index (monotonically increasing)
		 else
			lsr.w	#3,d4			;row index
		 endc
			move.b	d4,dct_linepopulation-dct_zz(a5,d2.w)
		endc
_P_next_start: ; just a marker
			moveq	#0,d6

P_next_coeff:
			DECODE_AC_COEFF	1,P_reconstruction_done

			add.b	d2,d7	;+1 postponed
			cmp.b	#62,d7	;63 normally, now 62
			bgt.w	block_err
;--------------------------------------------
;DeQuantize dct_coeff_next
;--------------------------------------------
			move.w	2(a6,d7*2),d4
			 muls.w	2(a3,d7*2),d1		;Dequantize coefficient
			 move.w	d4,d3
			asr.l	#4,d1
			move.w	d4,d2
			 muls.w	d5,d1
		ifne    DCTLINEPOP
		 ifne    ZZ_TRANSPOSED
			 lsr.w	#3,d2			;(zz_pos>>3) = line index
		 else
			 and.b	#7,d2			;zz_pos&0xf  = column index
		 endc
		endc

;--------------------------------------------
;There are 3 steps, all done in a single ASR:
;1. coeff / 256 => coeff >> 8				;remove fraction part from previous muls
;2. coeff * 16  => coeff << 4				;add 4 bits of fraction for idct
;3. coeff / 8   => coeff >> 3				;normal quantization step (2 * coeff / 16)
;--------------------------------------------
			asr.l	#7-IDCT_FRAC,d1		;All the above combinedi
		 ifne	ZZ_TRANSPOSED
			and.b	#7,d3			;hor index (monotonically increasing)
		 else
			lsr.w	#3,d3			;row index
		 endc
			move.w	d1,(a5,d4*2)
			addq.l	#1,d7
		ifne	DCTLINEPOP
			move.b	d3,dct_linepopulation-dct_zz(a5,d2.w)
		endc

			addq.l	#1,d6			;count number of coefficients extracted in d6
			bra.w	P_next_coeff

P_reconstruction_done
			move.b	macroblock_intra,d1
			bne	idct_intra_start

idct_forward_start	
		ifeq	APOLLO_P96ONLY			;gray mode support disabled in Apollo Build (i.e. transform all blocks)
			tst.b	GrayMode
			beq.b	.idct_all_blocks

			move.l	block_count,d1	;If gray playback, skip Cb/Cr blocks
			btst	#2,d1
			bne.w	fwd_skip_block
.idct_all_blocks
		endc

fwd_idct_X:
	ifne	APOLLO_IDCTX
			lea	zz_lines(pc),a6
	
			movem.l	d0/a0,-(a7)
			lea	dct_zz,a1

			moveq	#0,d0		;F
			move.b	(a6,d7.w),d0	;F B
			move.l	d0,a6		;  B  - number of populated lines

			IDCTX_Apollo
	else
			;--------------------------------------------------------
			; 68k iDCT
			;--------------------------------------------------------
			movem.l	d0/a0,-(a7)
			IDCTX_INTRA_68k	; actually, that one applies for P, too
	endc

	; note this loop requires that the iDCT runs in a different order
	;  first across columns
	;  second across rows
	ifne	APOLLO_IDCT

	;------------------------------------------------------------------------
	;  AMMX iDCT
	;------------------------------------------------------------------------
	 ;------------------------------------------------------------------------
	 ifne	APOLLO_IDCTX
	 ;------------------------------------------------------------------------
		PORDDB	1,1,10	;move.l	d1,a1	;TODO: use A1 again, once the core is fixed

		move.l	block_count,d1
		lea	idct_modulo_add,a5
		move.l	block_buffer_tmp_y1,a4

		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)
		move.l	(a5,d1.w*4),a6		;a6: line stride
		lsl.l	#6,d1					;block_count * 64 = offset for y block
		add.l	d1,a4					;reference base in a4

		PORBBD	10,10,1		;move.l	a1,d1
		lea	dct_zz,a1

		; IDCT vertical stage for 8x8 blocks
		IDCTY_Apollo

		;add + clip stage for INTER blocks 
		PSUBWBBB	15,15,15 ; B15=0

		LOADd16AB	0,4,8		 ;p10 p11 p12 p13 p14 p15 p16 p17
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	0,9,9		;d0
		PADDWDBB	4,8,8		;d4
		packuswb	E17,E16,(a0)	;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line
			
		LOADd16AB	8,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	1,9,9		;d1
		PADDWDBB	5,8,8		;d5
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line
			
		LOADd16AB	16,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	2,9,9		;d2
		PADDWDBB	6,8,8		;d6
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line
			
		LOADd16AB	24,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	3,9,9		;d3
		PADDWDBB	7,8,8		;d7
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line
			
		;2nd half, from En (former An) registers
		LOADd16AB	32,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	8,9,9		;E0
		PADDWDBB	12,8,8		;E4
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		LOADd16AB	40,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	9,9,9		;E0
		PADDWDBB	13,8,8		;E4
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		LOADd16AB	48,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	10,9,9		;E0
		PADDWDBB	14,8,8		;E4
		packuswb	E17,E16,(a0)     ;b1,b0,(a0)	 ; pack and store dest1
		adda.l		a6,a0		 ; next line

		LOADd16AB	56,4,8
		VPERMiBBB	$48494a4b,15,8,9 ;0 p10 0 p11 0 p12 0 p13
		VPERMiBBB	$4c4d4e4f,15,8,8 ;0 p14 0 p15 0 p16 0 p17
		PADDWDBB	11,9,9		;E0
		PADDWDBB	15,8,8		;E4
		packuswb	E17,E16,(a0)      ;b1,b0,(a0)	 ; pack and store dest1
;		adda.l		a6,a0		 ; next line

	 ;------------------------------------------------------------------------
	 else	; APOLLO_IDCTX
	 ;------------------------------------------------------------------------

		; iDCT second direction, Apollo version
		move.l	block_count,d1
		lea	dct_zz,a1
		move.l	block_count,d7
		lea	idct_modulo_add,a5

		move.l	d1,d2
		move.l	block_buffer_tmp_y1,a4

		lsl.l	#6,d2					;block_count * 64 = offset for y block
		add.l	d2,a4					;reference base in a4

		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)

		APOLLO_IDCTY_P_OLD
		
	 ;------------------------------------------------------------------------
	 endc	; APOLLO_IDCTX
	 ;------------------------------------------------------------------------
		
		movem.l	(a7)+,d0/a0

	;--------------------------------------------------------------
	else	; APOLLO_IDCT
	;--------------------------------------------------------------
	; iDCT second direction, 68k version

		;lea	block_temp(pc),a1
fwd_idct_Y:
		move.l	block_count,d1
		lea	dct_zz,a1

		move.l	d1,d2
		move.l	block_buffer_tmp_y1,a4

		lsl.l	#6,d2					;block_count * 64 = offset for y block
		add.l	d2,a4					;reference base in a4

		move.l	y_bitmap_base.w(pc,d1.l*4),a0		;destination ptr 
		add.l	MB_y_BitmapOffset.w(pc,d1.l*4),a0		;MB offset
		add.l	y_block_offset_table.w(pc,d1.l*4),a0	;add block offset to bitmap offset (a0)

		; actual call
		IDCTY_INTER_68k
		movem.l	(a7)+,d0/a0

	;--------------------------------------------------------------
	endc	; APOLLO_IDCT
	;--------------------------------------------------------------


fwd_skip_block		move.l	block_count,d6
			addq.l	#1,d6
			cmp.b	#5,d6
			bgt	Macroblock_Done				;current MB finished -> check for next MB
			bra	P_block_loop

;------------------------------------;
;--- Parse Macroblock in B frames ---;
;------------------------------------;
; A5: base register
B_MacroBlock:
			moveq	#-1,d2
			sub.l	last_Macroblock-VDEC_BASE(a5),d2
			move.l	MB_Address-VDEC_BASE(a5),d1
			lea	(MB_y_OffsetTable-VDEC_BASE).l(a5),a1 ; change to absolute if assembler barks

			add.l	d1,d2
			beq	.b_normal_mb_increment		;if MB_Address == last_Macroblock + 1, continue normally else skip loop

.b_mb_skip_marker
; Skipped macroblocks in B pictures:
;
; 1. Use same MB_Type as previous block (ie don't extract new MB_Type, it's not coded!)
; 2. Use same motion vectors (forward, backward, or both!) as previous block (no vectors are coded!)
; 3. No differential DCT data is coded.
;
; Basically all we need to do is to loop through all skipped blocks and compute the 16x16 luminance and
; the two 8x8 chrominance data from the backward verctor, or forward vector, or both vectors, depending
; on MB_Type.
			moveq	#1,d2
			add.l	last_Macroblock-VDEC_BASE(a5),d2		;start from 1st skipped block (d2 = temp_mb_address)

			clr.l	dct_dc_y_past-VDEC_BASE(a5)			;Clear all deltas
			clr.l	dct_dc_cb_past-VDEC_BASE(a5)
			clr.l	dct_dc_cr_past-VDEC_BASE(a5)
.b_mb_skipped_loop
			move.l	(a1,d2.l*4),d6			;for y_bitmap
			cmp.l	y_MB_max_addr(pc),d6
			bgt	Skip_CurrentPic
		ifne	APOLLO_MOT
			vperm	#$45674567,d6,d6,d6
			lea	MB_y_BitmapOffset(pc),a4
			 move.l	d6,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			 lea	(MB_c_OffsetTable-VDEC_BASE).l(a5),a1
			move.l	d6,MB_y_bwd_BitmapOffset-VDEC_BASE(a5)
			 store	d6,MB_y_BitmapOffset-MB_y_BitmapOffset(a4) ;MB_y_BitmapOffset,MB_y_BitmapOffset+4
			store	d6,MB_y_BitmapOffset+8-MB_y_BitmapOffset(a4) ;MB_y_BitmapOffset+8,MB_y_BitmapOffset+12
			 move.l	(a1,d2.l*4),d6
			vperm	#$45674567,d6,d6,d1
			move.l	d6,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
			 move.l	d6,MB_c_bwd_BitmapOffset-VDEC_BASE(a5)
			store	d1,MB_c_BitmapOffset-MB_y_BitmapOffset(a4) ;MB_c_BitmapOffset,MB_c_BitmapOffset+4
		else
			lea	(MB_c_OffsetTable-VDEC_BASE).l(a5),a1

			move.l	d6,MB_y_BitmapOffset;-VDEC_BASE(a5)
			move.l	d6,MB_y_BitmapOffset+4;-VDEC_BASE(a5) ; not needed here, quite possibly -> no coeffs present in SKIP mode
			move.l	d6,MB_y_BitmapOffset+8;-VDEC_BASE(a5)
			move.l	d6,MB_y_BitmapOffset+12;-VDEC_BASE(a5)

			move.l	d6,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			move.l	d6,MB_y_bwd_BitmapOffset-VDEC_BASE(a5)
			move.l	(a1,d2.l*4),d6
			move.l	d6,MB_c_BitmapOffset;-VDEC_BASE(a5)		;for c_bitmaps
			move.l	d6,MB_c_BitmapOffset+4;-VDEC_BASE(a5)		;for c_bitmaps
			move.l	d6,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
			move.l	d6,MB_c_bwd_BitmapOffset-VDEC_BASE(a5)
		endc
			move.l	d2,-(a7)
			lea	mc_offsets_direct,a6	;direct MC implementation

			move.l	y_bitmap_base(pc),a1
			add.l	MB_y_BitmapOffset(pc),a1	;get current MB base
			move.l	a1,MC_DestPTR_Y(a6)

			move.l	MB_c_BitmapOffset(pc),d1	;
			move.l	cb_bitmap_base(pc),a1
			move.l	cr_bitmap_base(pc),a2
			add.l	d1,a1
			add.l	d1,a2
			move.l	a1,MC_DestPTR_Cb(a6)
			move.l	a2,MC_DestPTR_Cr(a6)
			bsr	b_render_blocks

			lea	VDEC_BASE,a5		;restore A5 (less hassle than movem)
			move.l	(a7)+,d2

			move.l	MB_Address,d1
			lea	(MB_y_OffsetTable-VDEC_BASE).l(a5),a1 ; change to absolute if assembler barks

			addq.l	#1,d2
			cmp.l	d1,d2
			blt	.b_mb_skipped_loop		;loop till current MB_Address, clearing all skipped macroblocks
			; B skip done

.b_normal_mb_increment:
			move.l	(a1,d1.l*4),d2			;for y_bitmap
			cmp.l	y_MB_max_addr(pc),d2
			bgt.w	Skip_CurrentPic

		ifne	APOLLO_MOT
			vperm	#$45674567,d2,d2,d2
			lea	MB_y_BitmapOffset(pc),a4
			 move.l	d2,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			 lea	(MB_c_OffsetTable-VDEC_BASE).l(a5),a1
			move.l	d2,MB_y_bwd_BitmapOffset-VDEC_BASE(a5)
			 store	d2,MB_y_BitmapOffset-MB_y_BitmapOffset(a4) ;MB_y_BitmapOffset,MB_y_BitmapOffset+4
			store	d2,MB_y_BitmapOffset+8-MB_y_BitmapOffset(a4) ;MB_y_BitmapOffset+8,MB_y_BitmapOffset+12
			 move.l	(a1,d1.l*4),d2
			vperm	#$45674567,d2,d2,d1
			move.l	d2,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
			 move.l	d2,MB_c_bwd_BitmapOffset-VDEC_BASE(a5)
			store	d1,MB_c_BitmapOffset-MB_y_BitmapOffset(a4) ;MB_c_BitmapOffset,MB_c_BitmapOffset+4
		else
			lea	MB_c_OffsetTable,a1
			move.l	d2,MB_y_BitmapOffset;-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+4;-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+8;-VDEC_BASE(a5)
			move.l	d2,MB_y_BitmapOffset+12;-VDEC_BASE(a5)
			move.l	d2,MB_y_fwd_BitmapOffset-VDEC_BASE(a5)
			move.l	d2,MB_y_bwd_BitmapOffset-VDEC_BASE(a5)
			move.l	(a1,d1.l*4),d2
			move.l	d2,MB_c_BitmapOffset;-VDEC_BASE(a5)		;for c_bitmaps
			move.l	d2,MB_c_BitmapOffset+4;-VDEC_BASE(a5)		;for c_bitmaps
			move.l	d2,MB_c_fwd_BitmapOffset-VDEC_BASE(a5)
			move.l	d2,MB_c_bwd_BitmapOffset-VDEC_BASE(a5)
		endc

; Normally encoded (not skipped) macroblocks in B pictures:
;
; These are decoded according to ISO 11172-2, page 36-37.

.b_mb_type_chk_marker_unused:
			move.l	lookup_MB_type_B-VDEC_BASE(a5),a1
			CHKBITS	6,d7
			;.oO(Bubble)
			move.w	(a1,d7.w*2),d7
			NNEXTVLC d7,d1
			lsr.w	#8,d7
			move.l	d7,MB_Type-VDEC_BASE(a5)

			btst	#MB_QUANT,d7
			beq.b	.b_mb_quant_skip
			NGETBITS 5,d1,d2			;if mb_quant, get quant. scale!
			move.l	d1,quantizer_scale-VDEC_BASE(a5)
.b_mb_quant_skip
			btst	#MB_INTRA,d7
			beq.b	.b_mb_nonintra

			IFNE	SHOW_MBINFO
			OUTTXT	msg_intra
			ENDC

			move.b	#1,macroblock_intra-VDEC_BASE(a5)
			move.b	#63,coded_block_pattern-VDEC_BASE(a5)	;if intra macroblock, all blocks are stored!!!
			clr.l	mv_fwd_xy_long-VDEC_BASE(a5)
			clr.l	mv_bwd_xy_long-VDEC_BASE(a5)
			lea	intra_quant_matrix_zz,a3		;if intra macrblock, use intra quant matrix!!!
			bra	B_dct_reconstruct
			; intra MB done....
			
.b_mb_nonintra:
			clr.b	macroblock_intra-VDEC_BASE(a5)
			clr.l	dct_dc_y_past-VDEC_BASE(a5)		;Clear all deltas
			clr.l	dct_dc_cb_past-VDEC_BASE(a5)
			clr.l	dct_dc_cr_past-VDEC_BASE(a5)

			move.l	lookup_motion_vector-VDEC_BASE(A5),a1		;for DECODE_MV (fw and bw)

			btst	#MB_MOTION_FWD,d7
			beq	b_no_mb_fwd

			;----------------------;
			; Parse Forward Motion ;
			;----------------------;
			CHKBITS 11,d3					;\1 in DECODE_MV
			moveq	#0,d4
			move.b	forward_r_size-VDEC_BASE(a5),d4			;d4 = forward_r_size for DECODE_MV
		
			DECODE_MV d3,d1		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels
			;D1: MVD forw hor
_marker_bf_mv_decode_vert:

			CHKBITS 11,d7

			DECODE_MV d7,d2		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels
			;D2: MVD forw ver

			add.w	mv_fwd_x-VDEC_BASE(a5),d1	;add.l	recon_right_for_prev(pc),d1
			moveq	#27,d5		;bound motion vector:( vec <<(27-f_code) ) >> (27-f_code) );
			sub.l	d4,d5
			add.w	mv_fwd_y-VDEC_BASE(a5),d2	;add.l	recon_down_for_prev(pc),d2
			lsl.l	d5,d1
			lsl.l	d5,d2
			asr.l	d5,d1
			asr.l	d5,d2
			move.w	d1,mv_fwd_x-VDEC_BASE(a5)	;recon_right_for_prev-VDEC_BASE(a5)
			move.w	d2,mv_fwd_y-VDEC_BASE(a5)	;recon_down_for_prev-VDEC_BASE(a5)

			IFNE	SHOW_MBINFO
			OUTTXT	msg_fwd
			OUTDECS	d3
			OUTTXT	COMMA
			OUTDECS	d4
			OUTTXT	msg_fwd_end
			ENDC

b_no_mb_fwd
			move.l	MB_Type,d7
			btst	#MB_MOTION_BWD,d7
			beq	b_no_mb_bwd

			;-----------------------;
			; Parse Backward Motion ;
			;-----------------------;

			CHKBITS 11,d3					;\1 in DECODE_MV
			moveq	#0,d4
			move.b	backward_r_size-VDEC_BASE(a5),d4			;d4 = backward_r_size
		
			DECODE_MV d3,d1		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels
			;D1: MVD forw hor
_marker_bw_mv_decode_vert:

			CHKBITS 11,d7

			DECODE_MV d7,d2		;Trash: d5,d6,d7 (d7 is OK as input), MacrosMVDec.m, uses local labels
			;D2: MVD forw ver

			; MVD to MV, bound MVs in process
			add.w	mv_bwd_x-VDEC_BASE(A5),d1
			moveq	#27,d5		;bound motion vector:( vec <<(27-f_code) ) >> (27-f_code) );
			sub.l	d4,d5
			add.w	mv_bwd_y-VDEC_BASE(A5),d2
			lsl.l	d5,d1
			lsl.l	d5,d2
			asr.l	d5,d1
			asr.l	d5,d2
			move.w	d1,mv_bwd_x-VDEC_BASE(a5)
			move.w	d2,mv_bwd_y-VDEC_BASE(a5)


			IFNE	SHOW_MBINFO
			OUTTXT	msg_bwd
			OUTDECS	d3
			OUTTXT	COMMA
			OUTDECS	d4
			OUTTXT	msg_fwd_end
			ENDC
b_no_mb_bwd:

b_chk_pattern:
			CHKBITS	9,d1
			move.l	lookup_block_pattern-VDEC_BASE(A5),a1

			clr.b	coded_block_pattern-VDEC_BASE(a5)
			move.l	MB_Type,d7
			btst	#MB_PATTERN,d7
			beq	.b_no_mb_pattern

			move.w	(a1,d1.l*2),d1
			NNEXTVLC d1,d2
			lsr.w	#8,d1
			move.b	d1,coded_block_pattern-VDEC_BASE(a5)
			bne.s	.render_indirect
.b_no_mb_pattern
			lea	mc_offsets_direct,a6	;direct MC implementation

			move.l	y_bitmap_base(pc),a1
			add.l	MB_y_BitmapOffset(pc),a1	;get current MB base
			move.l	a1,MC_DestPTR_Y(a6)

			move.l	MB_c_BitmapOffset(pc),d1	;

			move.l	cb_bitmap_base(pc),a1
			add.l	d1,a1
			move.l	a1,MC_DestPTR_Cb(a6)

			move.l	cr_bitmap_base(pc),a1
			add.l	d1,a1
			move.l	a1,MC_DestPTR_Cr(a6)
			bsr	b_render_blocks

			bra	Macroblock_Done
.render_indirect:
			lea	mc_offsets_buffered,a6
			bsr	b_render_blocks
			lea	nonintra_quant_matrix_zz,a3		;if non-intra mb, use non-intra quant matrix!!!

B_dct_reconstruct	bra	P_dct_reconstruct




;----------------------------------------------------------------------
; BFrame Motion Compensation
;----------------------------------------------------------------------
; Input: A6 - mc_offsets_buffered or mc_offsets_direct
b_render_blocks:
			move.l	MB_Type,d7
			btst	#MB_MOTION_FWD,d7
			beq	no_render_fwd

			move.l	d0,-(sp)

			move.w	mv_fwd_x,d3
			moveq	#0,d7
			move.w	mv_fwd_y,d4
			ext.l	d3
			move.b	full_pel_backward_vector,d7
			ext.l	d4
			lsl.l	d7,d3
			lsl.l	d7,d4

	;---------------- chroma vector adjustment -----------------------------
			ADJUST_CVECTOR  ; in: d3/d4 out: d1/d2 trash: all other data regs

	;------------------ luma position calculation in reference frame --------------
			move.l	y_bitmap_width(pc),d7
			asr.l	#1,d4					;d4 = down_for (for luminance)
			scs	d6					;d6 = down_half (TRUE/FALSE)!
			muls.w	d4,d7					;vertical offset (no. of bytes)
			asr.l	#1,d3					;d3 = right_for (for luminance)
			scs	d5					;d5 = right_half (TRUE/FALSE)!
			add.l	d3,d7
			add.l	d7,MB_y_fwd_BitmapOffset

	;---- chroma frame position calculation and halfpel remainder calculation ------
			moveq	#1,d7	; just keep lowest bit
			and.l	d2,d7	; dy
			 moveq	#1,d0	; 
			 and.l	d1,d0	; dx
			 add.l	d7,d7
			 asr.l	#1,d2
			add.l	d0,d7

			move.l	c_bitmap_width(pc),d3
			asr.l	#1,d1	; chroma fullpel
			muls.w	d2,d3
			move.l	d7,MB_c_fwd_dydx			; store chroma fractional displacement
			add.l	d1,d3
			add.l	d3,MB_c_fwd_BitmapOffset

			; use the copy mode for B-frames as well
			move.l	fwd_reference_y,a1
			move.l	fwd_reference_cb,a3
			moveq	#0,d1
			move.l	MB_c_fwd_BitmapOffset,d3
			move.l	fwd_reference_cr,a2

			add.l	MB_y_fwd_BitmapOffset,a1
			add.l	d3,a3
			add.l	d3,a2

			move.l	a3,MC_SrcPTR_Cb(a6)			; source pointers chroma
			move.l	a2,MC_SrcPTR_Cr(a6)

			;dotouch	(a1,d1.w)

			bsr	MC_COPY
			move.l	(sp)+,d0
	; forward B-Frame MC done
fwd_block_done
no_render_fwd
	; backward B-Frame MC
			move.l	MB_Type,d7
			btst	#MB_MOTION_BWD,d7
			beq	no_render_bwd

			move.l	d0,-(sp)				;store D0, possibly also A0 ?

			move.w	mv_bwd_x,d3
			moveq	#0,d7
			move.w	mv_bwd_y,d4
			ext.l	d3
			move.b	full_pel_backward_vector,d7
			ext.l	d4
			lsl.l	d7,d3
			lsl.l	d7,d4
	;---------------- chroma vector adjustment -----------------------------
			ADJUST_CVECTOR  ; in: d3/d4 out: d1/d2 trash: all other data regs

	;------------------ luma position calculation in reference frame --------------
			move.l	y_bitmap_width(pc),d7
			asr.l	#1,d4					;d4 = down_for (for luminance)
			scs	d6					;d6 = down_half (TRUE/FALSE)!
			muls.w	d4,d7					;vertical offset (no. of bytes)
			asr.l	#1,d3					;d3 = right_for (for luminance)
			scs	d5					;d5 = right_half (TRUE/FALSE)!
			add.l	d3,d7
			add.l	d7,MB_y_bwd_BitmapOffset

	;---- chroma frame position calculation and halfpel remainder calculation ------
			moveq	#1,d7	; just keep lowest bit
			and.l	d2,d7	; dy
			 moveq	#1,d0	; 
			 and.l	d1,d0	; dx
			 add.l	d7,d7
			asr.l	#1,d2
			add.l	d0,d7

			move.l	c_bitmap_width(pc),d3
			asr.l	#1,d1	; chroma fullpel
			muls.w	d2,d3
			move.l	d7,MB_c_bwd_dydx			; store chroma fractional displacement
			add.l	d1,d3
			move.l	bwd_reference_y,a1
			add.l	d3,MB_c_bwd_BitmapOffset

			move.l	MB_Type,d7		;restore MB Type

			move.l	bwd_reference_cb,a2
			move.l	MB_c_bwd_BitmapOffset,d3
			moveq	#0,d1
			move.l	bwd_reference_cr,a3
			add.l	d3,a2
			add.l	MB_y_bwd_BitmapOffset,a1
			add.l	d3,a3

			move.l	a2,MC_SrcPTR_Cb(a6)			; source pointers chroma
			move.l	a3,MC_SrcPTR_Cr(a6)

			btst	#MB_MOTION_FWD,d7
			bne	bwd_add_mode

			;dotouch	(a1,d1)

			bsr	MC_COPY
			bra	bwd_block_done

	; backward add mode is handled on it's own
bwd_add_mode:

			tst.b	d5
			beq	bwd_no_right_half
			tst.b	d6
			beq	bwd_right_half_ADD

; -------------------------  backward prediction for both components halfpel ------------------------------
bwd_half_all_ADD:
			move.l	y_bitmap_width(pc),a5	;B
			move.l	a5,a3			;B

			moveq	#3,d4			
			MOT_8x1_HALFHORVER_INIT	; D0 - mask for lower two bits ($03030303), D6 - mask for upper six bits ($FCFCFCFC)

			move.l	MC_DestPTR_Y(a6),a2

			add.l	a1,a3
.lum_loop
			moveq	#8-1,d5
.y_yloop
			; in:  D0 - mask for lower two bits ($03030303)
			;      D6 - mask for upper six bits ($FCFCFCFC)
			;      A1 - input pixel pointer first line
			;      A2 - output pointer 
			;      A3 - input pixel pointer second line
			; out:
			;      A1 - next position to load from (+4) 
			;      A2 - next position to write to  (+4)
			;      A3 - next position to load from (+4)
			; trash:
			;      D1,D2,D3,D7
			MOT_8x1_HALFHORVER_ADD 0

			add.l	a5,a1			;go to next line in src bitmap (row 1)
			add.w	MC_Y_LINESTRIDE0(a6),a2
			add.l	a5,a3			;go to next line in src bitmap (row 2)

			dbf	d5,.y_yloop
	
			move.w	MC_Y_BLOCKOFF0(a6),d5
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6
			add.w	d5,a1
			add.w	d5,a3
			dbf	d4,.lum_loop
			subq.l	#8,a6			;restore MC ptr

			bsr	backw_mot_c_ADD
			bra	bwd_block_done

; -------------------------  backward prediction for horizontal direction halfpel -------------------
bwd_right_half_ADD:
			moveq	#3,d4

			MOT_8x1_HALFHOR_INIT

			move.l	y_bitmap_width(pc),a5
			
			move.l	MC_DestPTR_Y(a6),a2
.lum_loop
			moveq	#8-1,d5
.y_yloop
			MOT_8x1_HALFHOR_ADD	0

			;dotouch	(a1,a5.l)
			add.l	a5,a1
			add.w	MC_Y_LINESTRIDE0(a6),a2

			dbf	d5,.y_yloop

			add.w	MC_Y_BLOCKOFF0(a6),a1
			add.w	MC_Y_DESTOFF0(a6),a2
			addq.l	#2,a6

			dbf	d4,.lum_loop

			subq.l	#8,a6

			bsr	backw_mot_c_ADD
			bra	bwd_block_done

; ---------------  backward prediction for vertical direction halfpel or full pel -------------------
bwd_no_right_half:	tst.b	d6
			beq	bwd_full_all_ADD

; ---------------------------  backward prediction for vertical direction halfpel -------------------
bwd_down_half_ADD:
			moveq	#3,d4

			 MOT_8x1_HALFHOR_INIT

			move.l	y_bitmap_width(pc),d7	;B
			
			move.l	MC_DestPTR_Y(a6),a2
			lea	(a1,d7.l),a3
			move.l	d7,a5
.lum_loop
			moveq	#8-1,d5
.y_yloop
			MOT_8x1_HALFVER_ADD	0

			;dotouch	(a3,a5.l)
			add.l	a5,a1
			add.w	MC_Y_LINESTRIDE0(a6),a2
			add.l	a5,a3

			dbf	d5,.y_yloop

			move.w	MC_Y_BLOCKOFF0(a6),d5
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6
			add.w	d5,a1
			add.w	d5,a3

			dbf	d4,.lum_loop
			subq.l	#8,a6

			bsr	backw_mot_c_ADD
			bra	bwd_block_done
; ---------------------------  backward prediction for fullpel -------------------
bwd_full_all_ADD:
			moveq	#3,d4
			move.l	y_bitmap_width(pc),a5
			 MOT_8x1_HALFHOR_INIT
			move.l	MC_DestPTR_Y(a6),a2
.lum_loop
			moveq	#8-1,d5
.y_yloop
			MOT_8x1_FULL_ADD	0

			;dotouch	(a1,a5.l)
			add.l	a5,a1
			add.w	MC_Y_LINESTRIDE0(a6),a2

			dbf	d5,.y_yloop

			move.w	MC_Y_BLOCKOFF0(a6),d5
			 add.w	MC_Y_DESTOFF0(a6),a2
			 addq.l	#2,a6
			add.w	d5,a1

			dbf	d4,.lum_loop
			subq.l	#8,a6

			bsr	backw_mot_c_ADD
bwd_block_done
			move.l	(sp)+,d0
no_render_bwd
			rts


; ###############
;
; #### #   # ###
; #    ##  # #  #
; ###  # # # #  #
; #    #  ## #  #
; #### #   # ###
;
; ###############


		EVEN
MPEGEnd		tst.l	LOOP_switch
		beq.b	MPEGExit

		moveq	#0,d0
		bsr	MPEGSeek
		suba.l	a0,a0
		moveq	#0,d0
		bsr	ReadVideoBuffer			;read buffer

.keychk
		bsr	KeyInputCheck
		tst.b	PauseFlag
		beq.s	.nopause
		VBLDELAY 10
		bra.s	.keychk
.nopause
		tst.l	AbortFlag
		bne	MPEGExit
		bra	FindNextIFrame			;get first I frame

MPEGExit:
		st	AudioAbortFlag
		
DispCtrlCMsg	tst.l	CtrlCAbort
		beq.b	Cleanup
		OUTTXT	UserBreakMsg


Cleanup		move.l	4+actual_time,d1		;calculate total playback time...
		move.l	d1,e_clock_time

		tst.l	verbose_switch
		beq.w	VideoCloseDown

		OUTTXT	AudioOutModeMsg
		lea	VDEC_BASE,a5
		
		lea	AudioModePamela16(pc),a0
		tst.b	P16_switch-VDEC_BASE(A5)	;currently auto-selects Pamela16
		bne.s	.got
		lea	AudioModePaula14(pc),a0
		tst.b	P14_switch-VDEC_BASE(A5)
		bne.s	.got
		lea	AudioModeAHI(pc),a0
		tst.b	AHI_switch-VDEC_BASE(A5)
		bne.s	.got
		lea	AudioModeADev(pc),a0
.got		move.l	a0,d2
		bsr	OutputText	;OUTTXT	a0

		move.l	frame_number,d1
		addq.l	#1,d1
		move.l	d1,pictures_total
		sub.l	pictures_skipped,d1
		move.l	d1,pictures_played

		OUTTXT	PicsPlayedMsg
		OUTDEC	pictures_played
		OUTTXT	PicsSkippedMsg
		OUTDEC	pictures_skipped
		OUTTXT	PicsTotalMsg
		OUTDEC	pictures_total
		OUTTXT	RETURN

		bsr.w	CalcTotalSeconds
		bsr.w	CalcAverageFPS

		OUTTXT	TotalTimeMsg
		OUTNUM64 time_seconds_h,time_seconds_l
		OUTTXT	SecondsMsg
		OUTTXT	AvgFrameRateMsg
		OUTNUM64 average_fps_h,average_fps_l
		OUTTXT	FPSMsg
		OUTTXT	DispFrameRateMsg
		OUTNUM64 displayed_fps_h,displayed_fps_l
		OUTTXT	FPSMsg
		OUTTXT	RETURN
		OUTTXT	RETURN

VideoCloseDown:	
		bsr	FreeBuffers

CloseTimer:	tst.l	TimerClosed
		bne.b	DeleteIORequest
		move.l	TimerIO,a1
		CALLEXEC CloseDevice

DeleteIORequest	tst.l	TimerIO
		beq.b	DeleteMsgPort
		move.l	TimerIO,a0
		CALLEXEC DeleteIORequest

DeleteMsgPort:	tst.l	TimerPort
		beq.b	VideoReturn
		move.l	TimerPort,a0
		CALLEXEC DeleteMsgPort

VideoReturn	rts

****************************************************************
*--------------------------------------------------------------*
*------------------------- Subroutines ------------------------*
*--------------------------------------------------------------*
****************************************************************

		include	SubroutinesGen.i

		EVEN

;Close screen (if any were opened):
;----------------------------------
;A5 - VDEC_BASE
CloseDisplay:
		lea	VDEC_BASE,a5			;just make sure...

		bsr	CloseWindows			;close any windows that might be open

		IFNE APOLLO_YUYV
		IFNE	APOLLO_NSAGABUFS
		bsr	ExitYUYUTimer			;stop timer.device for buffer swap interrupt
		ENDC
	
		tst.l    YUYVBufPtr-VDEC_BASE(A5)			; Check if YUYV Buffer is allocated,
		beq.b    .noYUYVBuffer					; Else skip this part.

		move.l   YUYVBufPtr-VDEC_BASE(A5),a1                    ; ClearScreen before restoring RGB565 
		move.l	 YUYVBufSize-VDEC_BASE(A5),d1                   ; pixelformat. This offers a cleaner exit 
		sub.l	 #FRAMEBUFFER_ALIGN,d1                          ; when in full screen.
yuyvbufcls$	move.l	 #$00800080,(a1)+                               ; Clear 2 pixels (YUYV BLACK)
		subq.l	 #4,d1                                          ; Next pixels
		bgt.s    yuyvbufcls$                                    ; Continue

		move.w	#$2,$dff1f4					; return to RGB565

		move.l   YUYVBufPtr-VDEC_BASE(A5),a1 			; Memory Address
		move.l   YUYVBufSize-VDEC_BASE(A5),d0			; Memory Size
		CALLEXEC FreeMem					; Free Memory
		clr.l    YUYVBufPtr-VDEC_BASE(A5) 			; Reset
		clr.l    YUYVBufSize-VDEC_BASE(A5) 			; Reset
	ifeq	APOLLO_NSAGABUFS
		clr.l    YUYVBufPtr1-VDEC_BASE(A5) 			; Reset
		clr.l    YUYVBufPtr2-VDEC_BASE(A5) 			; Reset
		clr.l    YUYVBufPtr3-VDEC_BASE(A5) 			; Reset
		OUTTXT   SAGAFreeYUYV					; DEBUG OUTPUT
	endc
		
		ENDC

.noYUYVBuffer:
		tst.l	P96ScreenHandle-VDEC_BASE(A5)
		beq.b	.noP96screen
		move.l	P96ScreenHandle-VDEC_BASE(A5),a0
		CALLP96	CloseScreen
		clr.l	P96ScreenHandle-VDEC_BASE(A5)
		bra	NoScreen
.noP96screen

	ifeq	APOLLO_P96ONLY
		bsr	CloseCGXScreen
	endc

		tst.l	ScreenHandle
		beq	NoScreen
		move.l	ScreenHandle,a0
		CALLINT2	CloseScreen

NoScreen	clr.l	ScreenHandle
		rts

;Close RiVA windows...
;--------------------
;A5 - VDEC_BASE
CloseWindows:	
		tst.l	MainWindow-VDEC_BASE(A5)
		beq.w	CloseWindowsDone			;if no window open, skip

		tst.l	p96PIPWinHandle-VDEC_BASE(A5)
		beq.b	Nop96PIP

		tst.l	PlanarAssistance-VDEC_BASE(A5)
		beq.b 	No_Planar_Stuff2

		movea.l	PIVRegisterBase(pc),a0
		move.b	#CRTC_MiscellaneousVideoControl,(CRTCI,a0)
		move.b	(CRTCD,a0),d0
		and.b	#~(1<<4),d0		; YUV12PA Support Enable = Off
		move.b	d0,(CRTCD,a0)

No_Planar_Stuff2
		move.l	p96PIPWinHandle-VDEC_BASE(A5),a0
		move.l	p96base-VDEC_BASE(A5),a6
		jsr	_LVOp96PIP_Close(a6)		
		clr.l	MainWindow-VDEC_BASE(A5)
		clr.l	p96PIPWinHandle-VDEC_BASE(A5)			;always clear handles/pointers when closing objects!
		bra.w	CloseWindowsDone
Nop96PIP
		ifeq    APOLLO_P96ONLY
		 bsr	CloseCGXPIP
		endc

		move.l	MainWindow-VDEC_BASE(A5),a0
		CALLINT2	CloseWindow
		clr.l	MainWindow-VDEC_BASE(A5)			;clear winhandle to prevent closing twice!

		move.l	p96WinPlayBitMap-VDEC_BASE(A5),a0
		tst.l	a0
		beq.b	.nop96winplaybitmap
		CALLP96	FreeBitMap
		clr.l	p96WinPlayBitMap-VDEC_BASE(A5)
.nop96winplaybitmap

		ifeq    APOLLO_P96ONLY
		 move.l	cgxWinPlayBitMap(pc),a0
		 tst.l	a0
		 beq.b	.nocgxwinplaybitmap
		 CALLGFX	FreeBitMap
		 clr.l	cgxWinPlayBitMap
.nocgxwinplaybitmap
		endc

CloseWindowsDone:
		rts

;Close all graphics-related libraries:
;-------------------------------------
;A5 - VDEC_BASE
CloseGraphics:

CloseGfxLib:	move.l	gfxbase,d7
		beq.b	CloseP96Lib
		move.l	d7,a1
		CALLEXEC CloseLibrary

CloseP96Lib:	move.l	p96base,d7
		beq.b	.nop96
		move.l	d7,a1
		CALLEXEC CloseLibrary
.nop96
	ifeq	APOLLO_P96ONLY
		move.l	cgxbase,d7
		beq.b	.NoCgxLib
		move.l	d7,a1
		CALLEXEC CloseLibrary
.NoCgxLib:
	endc

ClosedGfxLibs:	rts

;Free all internal buffers:
;--------------------------
FreeBuffers:
;FreeIBuffer:	
		movem.l	d6/a2,-(sp)
		lea	FrameBufferPointers,a2

		move.l	FrameBufferAllocSize,d6
		beq.s	.BuffersNotAllocated
		move.l	(a2),d7
		beq.b	.IBufferFree
		clr.l	(a2)
		move.l	d7,a1
		move.l	d6,d0
		CALLEXEC FreeMem
		clr.l	FrameBuffer1
.IBufferFree:

;FreePBuffer:	
		move.l	4(a2),d7
		beq.b	.PBufferFree
		clr.l	4(a2)
		move.l	d7,a1
		move.l	d6,d0
		CALLEXEC FreeMem
.PBufferFree:

;FreeBBuffer
		move.l	8(a2),d7
		beq.b	.BBufferFree
		clr.l	8(a2)
		move.l	d7,a1
		move.l	d6,d0
		CALLEXEC FreeMem
.BBufferFree:
.BuffersNotAllocated:
		movem.l	(sp)+,d6/a2
		clr.l	FrameBufferAllocSize


FreeTables:	move.l	Addr_LookupTables,d7
		beq.b	TablesFree
		move.l	d7,a1
		move.l	#SIZE_LookupTables,d0
		CALLEXEC FreeMem
		clr.l	Addr_LookupTables
TablesFree

FreeYUVtoHiCol	move.l	YUVtoHiColorTable,d7
		beq.b	YUVtoHiColFree
		move.l	d7,a1
		move.l	#SIZE_YUVtoHiColorTable,d0
		CALLEXEC FreeMem
		clr.l	YUVtoHiColorTable
YUVtoHiColFree

FreeYUVtoBGGR	move.l	YUVtoBGGRTable,d7
		beq	YUVtoBGGRFree
		move.l	d7,a1
		move.l	#SIZE_YUVtoBGGRTable,d0
		CALLEXEC FreeMem
		clr.l	YUVtoBGGRTable
YUVtoBGGRFree
TablesAreFree:	rts

NoFile		OUTTXT	NoFileMsg
		bra	Error

NotMPEG		OUTTXT	NotMpegMsg
		bra	MPEGExit

NoGOP		OUTTXT	NoGOPMsg
		bra	MPEGExit

NoSlice		OUTTXT	NoSliceMsg
		bra	MPEGExit

MemAllocError	OUTTXT	MemAllocErrMsg
		rts

Skip_CurrentPic:
		BYTEALIGN
		;clr.l	PictureReconFlag

.seek_pic:	CHECK_BUFFER
		move.l	(a0),d1
		clr.b	d1
		cmp.l	#$00000100,d1
		bne.b	.seek_next		;if not start code, keep searching...

		move.l	(a0),d2			;look for picture, sequence, group of end code...
		cmp.b	#$00,d2
		beq.b	.seek_done
		cmp.b	#$b3,d2
		beq.b	.seek_done
		cmp.b	#$b8,d2
		beq.b	.seek_done
		;cmp.b	#$b7,d2
		;beq.b	.seek_done

.seek_next:	addq.l	#1,a0
		bra.b	.seek_pic

.seek_done:	bra	ParsePictureDone

Skip_Picture
		add.l	#1,pictures_skipped
		BYTEALIGN			;if error in pic, JMP here...
		clr.l	PictureReconFlag	;NO RECONSTRUCION!!!

.seek_pic:	CHECK_BUFFER
		move.l	(a0),d1
		clr.b	d1
		cmp.l	#$00000100,d1
		bne.b	.seek_next		;if not start code, keep searching...

		move.l	(a0),d2			;if start code, try to identify code...
		cmp.b	#$00,d2
		beq	NextPicture
		cmp.b	#$b3,d2
		beq	NextSequence
		cmp.b	#$b8,d2
		beq	NextGroup
		;cmp.b	#$b7,d2
		;beq	MPEGEnd

.seek_next:	addq.l	#1,a0
		bra.b	.seek_pic


FindNextIFrame	clr.l	bwd_reference_y
		clr.l	fwd_reference_y
		BYTEALIGN
		add.l	#1,pictures_skipped

.seek_pic:	CHECK_BUFFER
		move.l	(a0),d1
		clr.b	d1
		cmp.l	#$00000100,d1
		bne.b	.seek_next		;if not start code, keep searching...

		move.l	(a0),d2			;if start code, try to identify it...
		cmp.b	#$00,d2
		beq.b	.check_i_frame
		cmp.b	#$b3,d2
		beq	NextSequence
		cmp.b	#$b8,d2
		beq	NextGroup
		;cmp.b	#$b7,d2
		;beq	MPEGEnd

.seek_next:	addq.l	#1,a0
		bra.b	.seek_pic

.check_i_frame:	move.l	4(a0),d1		;get data after start code
		moveq	#19,d2
		lsr.l	d2,d1
		and.w	#7,d1			;picture coding type (3 bits) in d1.
		cmp.b	#I_FRAME,d1
		beq	NextPicture		;if I frame, found I pic!
		add.l	#1,frame_number
		SKP32
		bra	FindNextIFrame		;else try next frame...


;Render Picture in Picture Queue
;A5: lea VDEC_BASE(pc),a5
;A5  is preserved (in mpr_RenderFrame and here)
;-------------------------------
RenderPictureQueue	tst.l	queued_reference_y-VDEC_BASE(a5)
			beq.b	.queue_empty				;if queue empty, don't render from queue
	ifne	1
			move.l	y_bitmap_base(pc),-(a7)
			move.l	cb_bitmap_base(pc),-(a7)
			move.l	cr_bitmap_base(pc),-(a7)
	else
			move.l	y_bitmap_base-VDEC_BASE(a5),-(a7)
			move.l	cb_bitmap_base-VDEC_BASE(a5),-(a7)
			move.l	cr_bitmap_base-VDEC_BASE(a5),-(a7)
	endc
			;move.l	actual_frame_number,-(a7)
			movem.l	required_time,d1-d2
			movem.l	d1-d2,-(a7)
			
			movem.l	queued_timestamp-VDEC_BASE(a5),d1-d2
			movem.l	d1-d2,required_time-VDEC_BASE(a5)
	ifne	1
			move.l	queued_reference_y-VDEC_BASE(a5),y_bitmap_base
			move.l	queued_reference_cb-VDEC_BASE(a5),cb_bitmap_base
			move.l	queued_reference_cr-VDEC_BASE(a5),cr_bitmap_base
	else
			move.l	queued_reference_y-VDEC_BASE(a5),y_bitmap_base-VDEC_BASE(a5)
			move.l	queued_reference_cb-VDEC_BASE(a5),cb_bitmap_base-VDEC_BASE(a5)
			move.l	queued_reference_cr-VDEC_BASE(a5),cr_bitmap_base-VDEC_BASE(a5)
			;move.l	queued_frame_number,actual_frame_number
	endc
			IFNE	SHOW_RENDERINFO
			OUTTXT	msg_renderqueue
			OUTDEC	queued_frame_number
			OUTTXT	RETURN
			ENDC

			bsr	mpr_RenderFrame

			movem.l	(a7)+,d1-d2
			movem.l	d1-d2,required_time-VDEC_BASE(A5)
			;move.l	(a7)+,actual_frame_number-VDEC_BASE(a5)
	ifne	1
			move.l	(a7)+,cr_bitmap_base
			move.l	(a7)+,cb_bitmap_base
			move.l	(a7)+,y_bitmap_base
	else
			move.l	(a7)+,cr_bitmap_base-VDEC_BASE(a5)
			move.l	(a7)+,cb_bitmap_base-VDEC_BASE(a5)
			move.l	(a7)+,y_bitmap_base-VDEC_BASE(a5)
	endc
			clr.l	queued_reference_y-VDEC_BASE(a5)			;clear queue after rendereing it
.queue_empty
			rts


;Initialise MPEG decoder resources
;---------------------------------
MPEG_Init	bsr	AllocTables
		tst.l	result
		beq.b	MPEG_Init_Error

		bsr	GenerateAllTables

		bsr	DFTtoDCTQuantAdjust_intra
		bsr	DFTtoDCTQuantAdjust_nonintra
		bsr	QuantToZZ_intra
		bsr	QuantToZZ_nonintra

		bsr	CalcScreenStuff
		tst.l	result
		beq.b	MPEG_Init_Error

		bsr	HardcodeIDCTOffsets
		bsr	EClockInit

		bsr	AllocBitmaps
		tst.l	result
		beq.b	MPEG_Init_Error

		bsr	GenerateYUVConversionTable

		bsr	OpenDisplay
		tst.l	result
		beq.b	MPEG_Init_Error

MPEG_Init_OK	move.l	#1,result
		bra.b	MPEG_Init_Done

MPEG_Init_Error	clr.l	result

MPEG_Init_Done	rts

;-----------------------------------------------;
;----------- Open window for screen ------------;
;-----------------------------------------------;
OpenWindow:	movem.l	a0/d0,-(a7)

		tst.l	ScreenHandle(pc)		;only open if have screenhandle!
		beq.b	OpenWindowDone

		move.l	ScreenWidth(pc),WindowWidth
		move.l	ScreenHeight(pc),WindowHeight

		tst.l	MainWindow			;ha már van window...
		bne.b	OpenWindowDone
		suba.l	a0,a0
		lea	WindowTagList(pc),a1
		CALLINT2	OpenWindowTagList
		move.l	d0,MainWindow

		;make pointer blank
		move.l	d0,a0			;Window
		lea	EmptyPointer,a1		;Data to pointer
		moveq	#0,d0			;Height of pointer
		moveq	#0,d1			;Width of pointer
		moveq	#0,d2			;Offset X
		moveq	#0,d3			;Offset Y
		CALLINT2	SetPointer

OpenWindowDone	movem.l	(a7)+,a0/d0
		rts

;-------------------------------------------------;
;---------- Center Screen horizontally -----------;
;-------------------------------------------------;
CenterScreen:	movem.l	a0/d0,-(a7)

		tst.l	ScreenHandle(pc)			;skip if don't have screenhandle
		beq.b	DoNotCenter
		tst.l	YUYVTLOffY.l(pc)
		bne.s	DoNotCenter

		move.l	ScreenHandle(pc),a0
		lea 	sc_ViewPort(a0),a0
		CALLGFX	GetVPModeID

		move.l	d0,d2				;ViewPort ID in d2
		suba.l	a0,a0
		lea	DisplayInfoBuf,a1
		move.l	#80,d0
		move.l	#DTAG_DIMS,d1
		CALLGFX	GetDisplayInfoData

		moveq	#$00,d0
		lea	DisplayInfoBuf,a1
		move.w	30(a1),d0
		addq.l	#1,d0				;Screen width

.notpip		sub.l	width(pc),d0
		tst.b	doublewidth
		beq.b	.normalwidth
		sub.l	width(pc),d0
.normalwidth	lsr.l	#1,d0
		move.l	d0,d1				;xpos

		move.l	ScreenHandle(pc),a0
		move.l	#SPOS_ABSOLUTE,d0
		moveq	#0,d2				;ypos=0
		CALLINT2	ScreenPosition

DoNotCenter	movem.l	(a7)+,a0/d0
		rts

;----------------------------------------------;
;---------- Open screen for viewing -----------;
;----------------------------------------------;
OpenDisplay:	movem.l	d0-d7/a0-a6,-(a7)
		lea	VDEC_BASE,a5

		bsr	CloseDisplay			;Close current Display... (if any)

		clr.l	firsttime			;for planar y playback
		
.OD_Check_p96	tst.l	p96_switch-VDEC_BASE(a5)			;Check if any display can be opened with Picasso96 first!
		beq.b	.OD_Check_cgx
		bsr.w	Init_p96
		tst.l	d0
		beq.w	Error_p96
		bra.w	OpenDisplayOK

.OD_Check_cgx:
	ifeq	APOLLO_P96ONLY	
		tst.l	cgx_switch-VDEC_BASE(a5)			;If not, check with that lame cybercrapx ;)
		beq.b	OD_Check_AGA
		bsr.w	Init_cgx
		tst.l	d0
		beq.w	Error_cgx
		bra.w	OpenDisplayOK

OD_Check_AGA	move.l	AGA_switch-VDEC_BASE(a5),d1		;Otherwhise use the good old AGA ;-)
		or.l	VGA_switch-VDEC_BASE(a5),d1
		beq.b	OD_Check_xxx
		bsr.w	Init_AGA
		tst.l	d0
		beq.w	Error_AGA
		bra.w	OpenDisplayOK

OD_Check_xxx
	endc
	;;;future display options...


OpenDisplayBest						;if no flags, attempt to open best available display...

Try_p96			bsr.w	Init_p96
			tst.l	d0
			beq.b	.Try_cgx
			bra.w	OpenDisplayOK
.Try_cgx:
	ifeq	APOLLO_P96ONLY	
			bsr.w	Init_cgx
			tst.l	d0
			beq.b	Try_AGA
			bra.w	OpenDisplayOK

Try_AGA			bsr.w	Init_AGA
			tst.l	d0
			beq.b	Try_xxx
			bra.w	OpenDisplayOK

Try_xxx
	endc
	;;;future display types...

			bra	OpenDisplayError

Error_p96		bra.w	OpenDisplayBest

Error_cgx		bra.w	OpenDisplayBest

Error_AGA		bra.w	OpenDisplayBest

OpenDisplayOK:		bsr	CenterScreen
			bsr	GenerateBitmapConversionTable		;<- Must know if color or gray mode!
			bsr	OpenWindow
			move.l	#1,result
			bra.b	OpenDisplayDone

OpenDisplayError	OUTTXT	msg_ScreenOpenError
			clr.l	result

OpenDisplayDone		movem.l	(a7)+,d0-d7/a0-a6
			rts


;-----------------------------------------------;
;------ Picasso96 Initialization routines ------;
;-----------------------------------------------;
DisplayModeReject:	tst.l	CLIModeRequest		;and only if dither request from command line!
			beq.b	.norejectmessage
			OUTTXT	msg_UserModeReqReject
			clr.l	CLIModeRequest
.norejectmessage	rts

;--- Start Here! ---;
Init_p96:		moveq	#0,d0
			lea	picasso96_name(pc),a1
			CALLEXEC OpenLibrary			;Attempt to open Picasso96API.library
			move.l	d0,p96base
			beq	Init_p96_Error

			move.l	width(pc),d1			;minimum allowed display size = 320x240
			cmp.l	#320,d1
			bge.b	.p96BIDWidthOK
			move.l	#320,d1
.p96BIDWidthOK		move.l	d1,p96BIDWidth
			move.l	height(pc),d1
			cmp.l	#240,d1
			bge.b	.p96BIDHeightOK
			move.l	#240,d1
.p96BIDHeightOK		move.l	d1,p96BIDHeight

			move.l	height(pc),YUYVRows		;for now: width/height equal to video
			move.l	width(pc),YUYVCols

;First check the requested dithermode...

			move.b	DitherMode,d1
			cmp.b	#DM_PIP,d1
			beq	Chk_p96_PIP
			cmp.b	#DM_WINDOW,d1
			beq	Chk_p96_window

			cmp.b	#DM_TRUECOLOR,d1
			beq	Chk_p96_bgr24
			cmp.b	#DM_HICOLOR,d1
			beq	Chk_p96_rgbhicolor
	ifeq	APOLLO_P96ONLY
			cmp.b	#DM_ACCUPAK,d1
			beq	Chk_p96_accupak
	endc
			cmp.b	#DM_GRAY,d1
			beq	Chk_p96_gray
			bra	Chk_p96_NoDither

Chk_p96_PIP		bsr	Init_p96_PIP
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Try_p96_window

Chk_p96_window		bsr	Init_p96_window
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Try_p96_PIP


Chk_p96_bgr24		bsr	Init_p96_bgr24			;NOTE: bgr24 and argb32 are checked together...
			tst.l	d0
			bne	Init_p96_OK
Chk_p96_argb32		bsr	Init_p96_argb32
			tst.l	d0
			bne	Init_p96_OK
Chk_p96_bgra32		bsr	Init_p96_bgra32
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Try_p96_rgbhicolor

Chk_p96_rgbhicolor	bsr	Init_p96_rgbhicolor
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Try_p96_bgr24
	ifeq	APOLLO_P96ONLY
Chk_p96_accupak		bsr	Init_p96_accupak
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Try_p96_PIP
	endc

Chk_p96_gray		bsr	Init_p96_gray
			tst.l	d0
			bne	Init_p96_OK
			bsr	DisplayModeReject
			bra	Init_p96_Error

;fall through to here if no dithermode specified

Chk_p96_NoDither

Try_p96_PIP		bsr	Init_p96_PIP			;colour dithers start here...
			tst.l	d0
			bne.b	Init_p96_OK

Try_p96_window		bsr	Init_p96_window
			tst.l	d0
			bne.b	Init_p96_OK

Try_p96_bgr24		bsr	Init_p96_bgr24
			tst.l	d0
			bne.b	Init_p96_OK

Try_p96_argb32		bsr	Init_p96_argb32
			tst.l	d0
			bne.b	Init_p96_OK

Try_p96_bgra32		bsr	Init_p96_bgra32
			tst.l	d0
			bne.b	Init_p96_OK

Try_p96_rgbhicolor	bsr	Init_p96_rgbhicolor
			tst.l	d0
			bne.b	Init_p96_OK


Try_p96_gray		bsr	Init_p96_gray
			tst.l	d0
			bne.b	Init_p96_OK

Init_p96_Error		CALLEXEC CacheClearU
			moveq	#0,d0				;fail -> return 0
			bra.b	Init_p96_Done

Init_p96_OK		CALLEXEC CacheClearU
			moveq	#1,d0				;success -> return 1

Init_p96_Done		tst.l	CLIModeRequest
			beq.b	.notclirequest
			clr.l	CLIModeRequest			;if cli request, next dither won't be!
.notclirequest		rts


			;---------------------;
			; P96 - PIP Init      ;
			;---------------------;
Init_p96_PIP:		move.l	#RGBFB_Y4U2V2,PIPwinformat
			bsr	Open_p96_PIP					;Attempt to Open p96 PIP window
			tst.l	d0
			beq.b	Init_p96_PIP_Error				;can't open -> error
			lea	mpr_jsr_offsets(pc),a1				;open ok -> cleanup & exit
			move.l	#mpr_YUV422,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMapPIP,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapPIP,mpr_UnLockBitMap(a1)
			move.b	#DM_PIP,DitherMode
			clr.b	GrayMode
Init_p96_PIP_OK:	moveq	#1,d0
			rts
Init_p96_PIP_Error:	moveq	#0,d0
			rts

	ifeq	APOLLO_P96ONLY
			;---------------------;
			; P96 - Accupak Init  ;
			;---------------------;
Init_p96_accupak:	move.l	#RGBFB_Y4U1V1,PIPwinformat
			bsr	Open_p96_PIP
			tst.l	d0
			beq.b	Init_p96_accupak_Error
			lea	mpr_jsr_offsets(pc),a1				;open ok -> cleanup & exit
			move.l	#mpr_accupak,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMapPIP,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapPIP,mpr_UnLockBitMap(a1)
			move.b	#DM_PIP,DitherMode
			clr.b	GrayMode
Init_p96_accupak_OK	moveq	#1,d0
			rts
Init_p96_accupak_Error	moveq	#0,d0
			rts
	endc
	
			;---------------------;
			; P96 - Window Init   ;
			;---------------------;
Init_p96_window:	bsr	Open_p96_Window
			tst.l	d0
			beq.b	Init_p96_window_Error
			lea	mpr_jsr_offsets(pc),a1				;open ok -> cleanup & exit

			move.l	mpr_LockBitMap(a1),d0		;don`t lock window bitmap if banging on screen
			cmp.l	#mpr_p96LockBitMapDirect,d0	;evil, ofc.
			beq.s	.nosetlock

			move.l	#mpr_p96LockBitMapWin,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapWin,mpr_UnLockBitMap(a1)
.nosetlock
			move.b	#DM_WINDOW,DitherMode
			clr.b	GrayMode
Init_p96_window_OK	moveq	#1,d0
			rts
Init_p96_window_Error	moveq	#0,d0
			rts

			

			;---------------------;
			; P96 - BGR24 Init    ;
			;---------------------;
Init_p96_bgr24:	
			move.l	#RGBFF_B8G8R8,d0
			bsr	P96_BestScreenMode
			beq.b	Init_p96_bgr24_Error				;invalid id -> error

			bsr	Calc_CenterCrop			;TODO: into BestScreenMode

			bsr	Open_p96_Screen					;Open Screen
			tst.l	d0
			beq.b	Init_p96_bgr24_Error
			bsr	DitherInit_bgr24				;dither init...
			lea	mpr_jsr_offsets(pc),a1				;everyting ok -> cleanup & exit
			move.l	#mpr_bgr24,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
Init_p96_bgr24_OK:	moveq	#1,d0
			rts
Init_p96_bgr24_Error:	moveq	#0,d0
			rts

			;---------------------;
			; P96 - ARGB32 Init   ;
			;---------------------;
Init_p96_argb32:
			move.l	#RGBFF_A8R8G8B8,d0
			bsr	P96_BestScreenMode
			beq.b	Init_p96_argb32_Error

			bsr	Calc_CenterCrop			;TODO: into BestScreenMode

			bsr	Open_p96_Screen					;Open Screen
			tst.l	d0
			beq.b	Init_p96_argb32_Error
			bsr	DitherInit_argb32				;dither init...
			lea	mpr_jsr_offsets(pc),a1				;all ok -> cleanup & exit
			move.l	#mpr_argb32,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
init_p96_argb32_OK	moveq	#1,d0
			rts
Init_p96_argb32_Error	moveq	#0,d0
			rts

			;---------------------;
			; P96 - BGRA32 Init   ;
			;---------------------;
Init_p96_bgra32:
			move.l	#RGBFF_B8G8R8A8,d0
			bsr	P96_BestScreenMode
			beq.b	Init_p96_bgra32_Error

			bsr	Calc_CenterCrop			;TODO: into BestScreenMode

			bsr	Open_p96_Screen					;Open Screen
			tst.l	d0
			beq.b	Init_p96_bgra32_Error
			bsr	DitherInit_bgra32				;dither init...
			lea	mpr_jsr_offsets(pc),a1				;all ok -> cleanup & exit
			move.l	#mpr_bgra32,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMap,mpr_UnLockBitMap(a1)
			move.b	#DM_TRUECOLOR,DitherMode
			clr.b	GrayMode
init_p96_bgra32_OK	moveq	#1,d0
			rts
Init_p96_bgra32_Error	moveq	#0,d0
			rts

			;------------------------;
			; P96 - RGBHICOLOR Init  ;
			;------------------------;
Init_p96_rgbhicolor:	
		ifne	0
		;ifne	APOLLO_P96KLUDGE
				move.l	p96BIDWidth(pc),d0
				mulu.l	#9,d0		;
				lsr.l	#4,d0		;calc optimal 16:9 height
				cmp.l	p96BIDHeight(pc),d0
				blt.s	.kludge_skip
				move.l	d0,p96BIDHeight
.kludge_skip:
			ifne 0
				;old kludge
				; open 320 or 640 screens, depending on video size
				move.l	p96BIDWidth(pc),d0
				cmp.w	#352,d0
				ble.s	.kludge_320
				move.w	#640,d0
				move.l	d0,p96BIDWidth
				move.w	#360,d0
				move.l	d0,p96BIDHeight
			endc
.kludge_320:
		endc
			move.l	#RGBFF_R5G6B5PC,d0
			move.b	#$00,hicolorformat				;rgb16pC
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main

			move.l	#RGBFF_R5G6B5,d0
			move.b	#$01,hicolorformat				;rgb16pC
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main

			move.l	#RGBFF_R5G5B5PC,d0
			move.b	#$02,hicolorformat				;rgb15PC
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main

			move.l	#RGBFF_R5G5B5,d0
			move.b	#$03,hicolorformat				;rgb15
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main

			move.l	#RGBFF_B5G6R5PC,d0
			move.b	#$04,hicolorformat				;bgr16pC
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main

			move.l	#RGBFF_B5G5R5PC,d0
			move.b	#$05,hicolorformat				;bgr15pC
			bsr	P96_BestScreenMode
			cmp.l	#INVALID_ID,d0
			bne.w	Init_p96_rgbhicolor_main
			
			bra.w	Init_p96_rgbhicolor_Error
	ifne	1
	;debug: print the actual screen size
bla0txt			dc.b	"w ",0
bla1txt			dc.b	" h ",0
bla2txt			dc.b	10,0,0
	endc

; Input:  D0 = P96 Screenmode ID
; Output: D0 = INVALID_ID (Z bit set) or actual screenmode
; Trash:  D1
P96BSCM_Sw	EQU	-2
P96BSCM_Sh	EQU	-4
P96BSCM_Sm	EQU	-8
P96BSCM_Lw	EQU	-10
P96BSCM_Lh	EQU	-12
P96BSCM_Lm	EQU	-16
P96BSCM_SZ	EQU	-16

DEBUG_BSCM	EQU	0
	ifne	DEBUG_BSCM
bscmtxt:	dc.b	' in BestScreenMode',10,0
bscmtxt2:	dc.b	' in OldBestScreenMode',10,0
bscmtxt3:	dc.b	' have list',10,0
bscmtxt4	dc.b	' list non-empty',10,0
bscmtxtL1	dc.b	'l1',10,0
bscmtxtS1	dc.b	's1',10,0
bscmtxtLc	dc.b	' lcopy',10,0
bscmtxtSc	dc.b	' scopy',10,0
bscmspc		dc.b	' ',0
bscmtxtFin	dc.b	' after loop',10,0
bscmtxtFinCmp	dc.b	' after crop inset compare',10,0
bscmtxtOV	dc.b	" overriding present entry",10,0

	endc
		even
;RGBFF_NONE              EQU     (1<<RGBFB_NONE)		1
;RGBFF_CLUT              EQU     (1<<RGBFB_CLUT)		2
;RGBFF_R8G8B8            EQU     (1<<RGBFB_R8G8B8)		4
;RGBFF_B8G8R8            EQU     (1<<RGBFB_B8G8R8)		8
;RGBFF_R5G6B5PC          EQU     (1<<RGBFB_R5G6B5PC)		16
;RGBFF_R5G5B5PC          EQU     (1<<RGBFB_R5G5B5PC)		32
;RGBFF_A8R8G8B8          EQU     (1<<RGBFB_A8R8G8B8)		64
;RGBFF_A8B8G8R8          EQU     (1<<RGBFB_A8B8G8R8)		128
;RGBFF_R8G8B8A8          EQU     (1<<RGBFB_R8G8B8A8)		256
;RGBFF_B8G8R8A8          EQU     (1<<RGBFB_B8G8R8A8)		512
;RGBFF_R5G6B5            EQU     (1<<RGBFB_R5G6B5)
;RGBFF_R5G5B5            EQU     (1<<RGBFB_R5G5B5)
;RGBFF_B5G6R5PC          EQU     (1<<RGBFB_B5G6R5PC)
;RGBFF_B5G5R5PC          EQU     (1<<RGBFB_B5G5R5PC)
;RGBFF_YUV422CGX         EQU     (1<<RGBFB_YUV422CGX)
;RGBFF_YUV411            EQU     (1<<RGBFB_YUV411)
;RGBFF_YUV411PC          EQU     (1<<RGBFB_YUV411PC)
;RGBFF_YUV422            EQU     (1<<RGBFB_YUV422)
;RGBFF_YUV422PC          EQU     (1<<RGBFB_YUV422PC)
;RGBFF_YUV422PA          EQU     (1<<RGBFB_YUV422PA)

; absolute distance between \1 and \2 into \3
BSM_DIST 	macro
		move	\1,\3
		sub.w	\2,\3
		bge.s	.h\0\@
		neg.w	\3
.h\0\@
		endm


P96_BestScreenMode:
		; manual parsing of the mode list to find (truly) best mode
		movem.l	d1-a6,-(sp)
		move.l	d0,d2						;remember format for old routine
	ifd	CUSTOM_BESTSCREENMODE
		;desired width/height (mode in D0)
		move.l	p96BIDWidth(pc),d3				;wanted width
		move.l	p96BIDHeight(pc),d4				;wanted height

		ifne	DEBUG_BSCM
		OUTDEC	d0
		OUTTXT	bscmspc
		OUTDEC	d3
		OUTTXT	bscmspc
		OUTDEC	d4
		OUTTXT	bscmtxt
		endc

		lea	p96ModelistTags(pc),a0
		move.l	d0,p96ModeListFormat-p96ModelistTags(a0)	;Format
		CALLP96	AllocModeListTagList
		tst.l	d0
		beq	.old_bestmodeid					;no list, goto old routine
		move.l	d0,a2						;remember list

		;OUTTXT	bscmtxt3

		move.l	(a2),d0
		beq	.old_bestmodeid					;bad list ?

		;OUTTXT	bscmtxt4

		link	a5,#P96BSCM_SZ
		moveq	#-1,d5
		move.l	#INVALID_ID,d1
		move.w	d5,P96BSCM_Sw(a5)				;smaller than desired mode
		move.w	d5,P96BSCM_Sh(a5)
		move.l	d1,P96BSCM_Sm(a5)
		move.w	#$7fff,d5
		move.w	d5,P96BSCM_Lw(a5)				;larger than desired mode
		move.w	d5,P96BSCM_Lh(a5)
		move.l	d1,P96BSCM_Lm(a5)
.loop
		move.l	d0,a1
		tst.l	(a1)				;no successor ? tail of list
		beq	.done

		moveq	#0,d0
		moveq	#0,d1
		move	p96m_Width(a1),d0		;proposed width
		move	p96m_Height(a1),d1		;proposed height

			ifne	DEBUG_BSCM
			move.l	p96m_DisplayID(a1),d7
			OUTDEC	d7
			OUTTXT	bscmspc
			OUTTXT	bla0txt
			OUTDEC	d0
			OUTTXT	bla1txt			
			OUTDEC	d1
			OUTTXT	bla2txt			
			endc
			
		cmp.w	d0,d3
		bge	.small
		
			ifne	DEBUG_BSCM
			OUTTXT	bscmtxtL1
			endc

		;the current mode is wider than what we'd like to get
		cmp.w	P96BSCM_Lw(a5),d0		;proposed width < current best "larger" ?
		bgt	.next
		bne.s	.takeL

		; same width: check if proposed height is sexier
		;check height: if the height we have is better suited (4:3 vs. 16:9)...
		cmp.w	d1,d4				;
		bgt.s	.h1L				;bah. proposed height smaller than what we want
		beq.s	.takeL				;proposed height is a match
		; Proposed height fits our needs. But check whether proposed height is actually better than what we have now
		
		cmp.w	P96BSCM_Lh(a5),d1		;proposed height > current ?
		bgt.s	.h2L				;yes, check if old was fitting already
		; proposed height <= current (and fits, take it)
		bra.s	.takeL
.h2L:		;proposed height > current
		cmp.w	P96BSCM_Lh(a5),d4		;was current sufficient ?
		ble	.next				;current >= wanted, bail out
		
		ifne	DEBUG_BSCM
		 move.w	P96BSCM_Lh(a5),d7
		 ext.l	d7
		 OUTDEC	d7
		 OUTTXT	bscmtxtOV
		endc
		
		bra.s	.takeL				;this one is all good (for now)
.h1L:		;proposed height smaller than what we want but perhaps larger than current (still same width)
		cmp.w	P96BSCM_Lh(a5),d1
		bgt.s	.takeL
		bra	.next
.takeL:
			ifne	DEBUG_BSCM
			 OUTDEC	d0
			 OUTTXT	bscmtxtLc
			 OUTDEC  d1
			 OUTTXT	bscmtxtLc
			endc

		move	d0,P96BSCM_Lw(a5)		;new width is better
		move	d1,P96BSCM_Lh(a5)		;new height is better
		move.l	p96m_DisplayID(a1),P96BSCM_Lm(a5)
		bra	.next
.small:		;the current mode is less wide than what we'd like to have (or same)
			ifne	DEBUG_BSCM
			OUTTXT	bscmtxtS1
			endc

		cmp.w	P96BSCM_Sw(a5),d0		;proposed width > current best "smaller" ?
		blt.s	.next
		bne.s	.takeS
		; same width
		cmp.w	d1,d4				;proposed height fits ?
		bge.s	.sm_toosmall
		; proposed height fits, but is it closer to optimum ?

		BSM_DIST P96BSCM_Sw(a5),d4,d6
		BSM_DIST d1,d4,d7

		cmp.w	 d6,d7		;proposed distance < what we have ?
		bgt.s	.takeS		;yes, take this mode
		bra.s	.next
				;no, new height even smaller
.sm_toosmall:	; proposed height doesn't fit but we'll take it if it's bigger than the old
		cmp.w	P96BSCM_Sh(a5),d1		;new height bigger than old ?
		ble.s	.next				;no, new height even smaller
.takeS:
			ifne	DEBUG_BSCM
			 OUTDEC	d0
			 OUTTXT	bscmtxtSc
			 OUTDEC  d1
			 OUTTXT	bscmtxtSc
			endc

		move	d0,P96BSCM_Sw(a5)	;new width is better
		move	d1,P96BSCM_Sh(a5)	;new height is better
		move.l	p96m_DisplayID(a1),P96BSCM_Sm(a5)
.next:
		move.l	(a1),d0						
		bra	.loop				;
.done:
		;select best mode from either the largest smaller than desired or the smallest larger than desired
		;rule: if the cropping is less than the inset, then use the smaller mode
		move.w	P96BSCM_Lw(a5),d0
		move.w	P96BSCM_Lh(a5),d1
		sub	d3,d0
		sub	d4,d1
		add	d0,d1			;inset

		move.w	P96BSCM_Sw(a5),d5
		move.w	P96BSCM_Sh(a5),d6
		sub	d3,d5
		add.w	d5,d5			;hcrop * 2
		
		sub	d4,d6
		cmp.w	#-48,d6			;more than 3 macroblocks cropped?
		blt.s	.use_larger		;
		
		add	d5,d6			;crop
		neg	d6

		ext.l	d6
		ext.l	d1

			ifne	DEBUG_BSCM
			 OUTDEC	d1
			 OUTTXT	bscmspc
		  	 OUTDEC	d6
			 OUTTXT 	bscmtxtFinCmp
			endc

		cmp.w	d1,d6
		bgt.s	.use_larger
		move.w	P96BSCM_Sw(a5),d0
		move.w	P96BSCM_Sh(a5),d1
		move.l	P96BSCM_Sm(a5),d3
		bra.s	.id_found
.use_larger:
		move.w	P96BSCM_Lw(a5),d0
		move.w	P96BSCM_Lh(a5),d1
		move.l	P96BSCM_Lm(a5),d3
.id_found:
		ext.l	d0
		ext.l	d1
		move.l	d0,YUYVScrWidth
		move.l	d1,YUYVScrHeight

		unlk	a5

		move.l	a2,a0
		CALLP96	FreeModeList

			ifne	DEBUG_BSCM
			 move.l	YUYVScrWidth,d0
			 OUTDEC	D0
			 OUTTXT  bscmspc  
			 move.l	YUYVScrHeight,d0
			 OUTDEC	D0
			 OUTTXT  bscmspc  
			 OUTDEC	d3
			 OUTTXT	bscmtxtFin
			endc
			
		move.l	d3,d0
		cmp.l	#INVALID_ID,d3
		beq	.old_bestmodeid

		move.l	d0,p96ScreenModeID

		bra	.rts
.old_bestmodeid:
	endc	;CUSTOM_BESTSCREENMODE

			;OUTTXT  bscmtxt2

			lea	p96BIDTags(pc),a0				;Get BestID
			move.l	d2,p96BIDFormat-p96BIDTags(a0)
			CALLP96	BestModeIDTagList
			move.l	d0,p96ScreenModeID
			cmp.l	#INVALID_ID,d0
			beq.s	.rts

			move.l	d0,d2
			move.l	#P96IDA_WIDTH,d1
			CALLP96 GetModeIDAttr
			move.l  d0,YUYVScrWidth

			move.l	d2,d0
			move.l	#P96IDA_HEIGHT,d1
			CALLP96 GetModeIDAttr
			move.l  d0,YUYVScrHeight

			move.l	d2,d0
.rts
			cmp.l	#INVALID_ID,d0

		movem.l	(sp)+,d1-a6
		rts


; list = p96AllocModeListTagList(Tags)
; d0                             a0
; p96FreeModeList(ModeList) a0
; STRUCTURE P96Mode,LN_SIZE
; ...
;        UWORD   p96m_Width
;        UWORD   p96m_Height
;        UWORD   p96m_Depth
;        ULONG   p96m_DisplayID

; Dummy right now
Calc_CenterCropWin:
			clr.l	YUYVTLOffY	; no top/left offsets for the video by default
			clr.l	YUYVTLOffC
			clr.l	YUYVScrYOffset          ; no screen offset yet 
			;move.l	height(pc),YUYVRows	; done in init_p96
			;move.l	width(pc),YUYVCols

			rts
; input:  -
; output: YUYVRows,YUYVCols
;         YUYVTLOffY,YUYVTLOffC
;
; TODO:   base relative variables
;
Calc_CenterCrop:	;center/crop video on screen

			movem.l  d0-d7/a0-a6,-(sp)          ; Store registers

			;
			; offsets into the picture and number of rows/columns to draw
			; These are NOT the screen offsets but into the movie, if necessary
			;
			clr.l	YUYVTLOffY	; no top/left offsets for the video by default
			clr.l	YUYVTLOffC
			clr.l	YUYVScrYOffset          ; no screen offset yet 
			;move.l	height(pc),YUYVRows	; done in init_p96
			;move.l	width(pc),YUYVCols

			;-----------------------------------------------------------------
			; Calculate X Offset when movie is wider than screen
			;-----------------------------------------------------------------
			move.l	YUYVScrWidth,d0        ; Screen Width
			move.l	d0,d1
			sub.l	width,d0               ; - Movie Width
			beq.s	.xsok
			bgt.s	.xwide

			and.b	#$f8,d1			     ; multiple of 8 (SCRW)
			move.l	d1,YUYVCols		     ; don`t write too many columns

			neg.l	d0			     ; make diff positive
			lsr.l	#1,d0			     ; half
			and.b	#$fe,d0			     ; align for chroma
			move.l	d0,YUYVTLOffY

			lsr.l	#1,d0			     ; luma to chroma
			move.l	d0,YUYVTLOffC
			bra.s	.xsok
.xwide:
			;--- center horizontally when movie is less wide than screen ------
			;lsr.l	#1,d0			     ; ( screen_width - movie_width )/2
			and.b	#$fc,d0			     ; constrain to multiple of 4 (YUYV is 4 bytes per position)
			move.l	d0,YUYVScrYOffset
.xsok:
			;-----------------------------------------------------------------
			; Calculate Y Offset for Vertical Centering
			;-----------------------------------------------------------------

			move.l   YUYVScrHeight,d0       ; Screen Height
			sub.l	 height,d0              ; - Movie Height
			bge	.ysok

			moveq	#-8,d1			;
			and.l	YUYVScrHeight,d1	;
			move.l	d1,YUYVRows		; don`t draw more rows than screen space

			move.l	height,d0		;
			sub.l	d1,d0			;
			asr.l	#1,d0			; (movie_h-screen_h)/2
			ble	.noyoff			; shouldn`t happen but you need to be careful

			and.b	#$fe,d0			; make sure the offset is aligned to chroma

			move.l	d0,d1			;
			lsr.l	#1,d1			; for chroma
			
			mulu	(width+2),d0
			add.l	YUYVTLOffY,d0
			move.l	d0,YUYVTLOffY


			mulu	(width+2),d1		;
			lsr.l	#1,d1			; chroma_w is half of luma
			add.l	YUYVTLOffC,d1
			move.l	d1,YUYVTLOffC

.noyoff:
			moveq	#0,d0			    ; safety net: no negative screen offset
.ysok:
			mulu.l   YUYVScrWidth,d0            ; * Screen Width
			add.l	 YUYVScrYOffset,d0	    ; add X offset, if present
			move.l   d0,YUYVScrYOffset          ; Store Y Offset

			movem.l	(sp)+,d0-d7/a0-a6
			rts


Init_p96_rgbhicolor_main:
			move.l	d0,p96ScreenModeID
	ifne	1
			OUTTXT	bla0txt
	endc
			move.l	p96ScreenModeID,d0
			move.l	#P96IDA_WIDTH,d1
			CALLP96 GetModeIDAttr
			move.l  d0,YUYVScrWidth
	ifne	1
			OUTDEC	d0
			OUTTXT	bla1txt			
	endc
			move.l	p96ScreenModeID,d0
			move.l	#P96IDA_HEIGHT,d1
			CALLP96 GetModeIDAttr
			move.l  d0,YUYVScrHeight
	ifne	1
			OUTDEC	d0
			OUTTXT	bla2txt			
	endc
			move.l	p96ScreenModeID,d0

			bsr	Calc_CenterCrop		;center/crop video on screen

			IFNE APOLLO_YUYV

			movem.l  d0-d7/a0-a6,-(sp)          ; Store registers

			;-----------------------------------------------------------------
			; Allocate Memory for the Triple Buffering
			;-----------------------------------------------------------------
			move.l   YUYVScrHeight,d5           ; Calc Frame Size
			mulu.l   YUYVScrWidth,d5            ; Width * Height
			asl.l    #1,d5                      ; Bytes per pixel

		ifne	APOLLO_NSAGABUFS
			;d5: framebuffer size (output screen)

			move.l   d5,d0                      ; Calc Buffer Size
			mulu.l   #APOLLO_NSAGABUFS,d0       ; Number of buffers
			add.l	 #FRAMEBUFFER_ALIGN*2,d0    ; 32-bytes Alignment
			move.l   d0,YUYVBufSize             ; Store result
			
			move.l   #MEMF_PUBLIC,d1            ; Memory Type
			CALLEXEC AllocMem                   ; Allocate Memory
			move.l   d0,YUYVBufPtr              ; Save result
			beq.w	 Init_p96_rgbhicolor_Error  ; Exit on Error
			add.l    #(FRAMEBUFFER_ALIGN-1),d0  ; 32-bytes Alignment
			and.l    #~(FRAMEBUFFER_ALIGN-1),d0 ; Aligned FrameBuffer #1
			move.l	 d0,a3

			move.l   a3,a1                      ; buffer ptr
			move.l	 YUYVBufSize,d1
			sub.l	 #FRAMEBUFFER_ALIGN,d1
yuyvclear$		move.l	 #$00800080,(a1)+           ; Clear 2 pixels (YUYV BLACK)
			subq.l	 #4,d1
			bgt.s    yuyvclear$                 ; Continue


			lea	VidBuf_TMRSig,a2
			bsr	InitYUYUTimer		;prepare timer.device for buffer swap interrupt

			; init counters for timer interrupt and main task
			; rules:
			;  - timer updates VidBuf_TMR, main updates VidBuf_RVA
			;  - timer: if( VidBuf_RVA > VidBuf_TMR ) output_image(),VidBuf_TMR++;
			;  - main:  if( (VidBuf_RVA-VidBuf_TMR) < APOLLO_NSAGABUFS )
			;            curfrm = VidBuf_RVA & (APOLLO_NSAGABUFS-1)
			;            CopyFrame( curfrm )
			;            VidBuf_RVA++;
			;            if( !tmr ) startTimer
			;
			clr.l	VidBuf_TMR-VidBuf_TMRSig(a2) ; 
			clr.l	VidBuf_RVA-VidBuf_TMRSig(a2) ; 

			; init nodes
			lea	VidBuf_Store-VidBuf_TMRSig(a2),a2 ;VidBuf_Store
			moveq	#APOLLO_NSAGABUFS-1,d7
allocsagabufs$:
			move.l	a3,VIDBUF_Y(a2)		;store current frame buffer pointer
			;clr.l	VIDBUF_TimeStampH(a2)	;will be written when the struct goes active
			;clr.l	VIDBUF_TimeStampL(a2)
			move.l  YUYVScrHeight,VIDBUF_Height(a2)
			move.l  YUYVScrWidth,VIDBUF_Width(a2)
			clr.w	VIDBUF_FMT(a2)		    ; def: YUYV

			lea	VIDBUF_SIZE(a2),a2
			add.l	d5,a3					; Aligned FrameBuffer #2
			dbf	d7,allocsagabufs$

		;backwards compatibility: keep YUYVBufPtrN active for now (TBR)
		;	lea	VidBuf_Store,a2 ;VidBuf_Store
		;	move.l	VIDBUF_Y(a2),YUYVBufPtr1
		;	lea	VIDBUF_SIZE(a2),a2
		;	move.l	VIDBUF_Y(a2),YUYVBufPtr2
		;	lea	VIDBUF_SIZE(a2),a2
		;	move.l	VIDBUF_Y(a2),YUYVBufPtr3
		;	lea	VIDBUF_SIZE(a2),a2

		else
			move.l   d5,d0                      ; Calc Buffer Size
			mulu.l   #3,d0                      ; Number of buffers
			add.l    #FRAMEBUFFER_ALIGN,d0      ; 32-bytes Alignment
			move.l   d0,YUYVBufSize             ; Store result
			
			move.l   #MEMF_PUBLIC,d1            ; Memory Type
			CALLEXEC AllocMem                   ; Allocate Memory
			tst.l	 d0                         ; Check result
			beq.w	 Init_p96_rgbhicolor_Error  ; Exit on Error
			move.l   d0,YUYVBufPtr              ; Save result
			add.l    #(FRAMEBUFFER_ALIGN-1),d0  ; 32-bytes Alignment
			and.l    #~(FRAMEBUFFER_ALIGN-1),d0 ; Aligned FrameBuffer #1
			move.l   d0,YUYVBufPtr1             ; Store it
			add.l    d5,d0                      ; Aligned FrameBuffer #2
			move.l   d0,YUYVBufPtr2             ; Store it
			add.l    d5,d0                      ; Aligned FrameBuffer #2
			move.l   d0,YUYVBufPtr3             ; Store it
			add.l    d5,d0                      ; Aligned FrameBuffer #2

			;-----------------------------------------------------------------
			; Fill Triple Buffer with YUYV BLACK color ($0080)
			;-----------------------------------------------------------------
			
			move.l   d0,a1                      ; End of Aligned buffer
			move.l   YUYVBufPtr1,a0             ; Start of Aligned buffer
yuyvclear$		move.l	 #$00800080,(a0)+           ; Clear 2 pixels (YUYV BLACK)
			cmp.l	 a0,a1                      ; Check End of Aligned buffer
			bgt.s    yuyvclear$                 ; Continue

			OUTTXT   SAGAAllocYUYV              ; DEBUG OUTPUT
		endc		
			
			movem.l  (sp)+,d0-d7/a0-a6          ; Restore registers
			
			ENDC

			bsr	Open_p96_Screen					;Open Screen
			tst.l	d0
			beq.s	Init_p96_rgbhicolor_Error
			bsr	DitherInit_rgbhicolor				;dither init...
			tst.l	d0
			beq.s	Init_p96_rgbhicolor_Error
			lea	mpr_jsr_offsets(pc),a1				;all ok -> cleanup & exit

			;tst.b	apollo_active(pc)				;enable YUV rendering only when we run on Apollo
			;beq.s	noapollo$					;disabled: always use YUV for Apollo builds
		ifne	APOLLO_YUYV
			move.l	#mpr_YUV422,mpr_RenderBitMap(a1)
			move.l	#Init_p96_rgbhicolor_OK,mpr_LockBitMap(a1)
			move.l	#Init_p96_rgbhicolor_OK,mpr_UnLockBitMap(a1)
		else
			move.l	#mpr_rgbhicolor,mpr_RenderBitMap(a1)
			move.l	#mpr_p96LockBitMap,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMap,mpr_UnLockBitMap(a1)
		endc
			move.b	#DM_HICOLOR,DitherMode
			clr.b	GrayMode
Init_p96_rgbhicolor_OK	moveq	#1,d0
			rts
Init_p96_rgbhicolor_Error	
			moveq	#0,d0
			rts

		ifne	APOLLO_NSAGABUFS

InitYUYUTimer:		;prepare timer.device for buffer swap interrupt
			movem.l	d1-a6,-(sp)
			lea	VidBuf_TMRSig,a2

			sf	VBTimerInit-VidBuf_TMRSig(a2)

			;signal to wait for from timer output if output queue full
			moveq	#-1,d0
			CALLEXEC AllocSignal
			move.l	d0,VidBuf_TMRSig-VidBuf_TMRSig(a2)
			not.l	d0				; if( d0 == -1 ) d0 = 0
			beq	.error				; error: have to go (shouldn't happen)

			CALLEXEC CreateMsgPort			;Create MsgPort for timing...
			move.l	d0,VBTimerPort-VidBuf_TMRSig(a2)
			beq	.error				;0=error

			move.l	VBTimerPort-VidBuf_TMRSig(a2),a0
			move.l	#IOTV_SIZE,d0
			CALLEXEC CreateIORequest
			move.l	d0,VBTimerIO-VidBuf_TMRSig(a2)
			beq	.error				;0=error

			lea.l	VBTimerInt-VidBuf_TMRSig(a2),a0
			lea	VBTimerRoutine(pc),a1
			move.l	a1,IS_CODE(a0)
			move.b	#NT_INTERRUPT,LN_TYPE(a0)
			move.b	#0,LN_PRI(a0)			;-32,-16,0,16,32

			move.l	VBTimerPort-VidBuf_TMRSig(a2),a1
			;move.b	#NT_MSGPORT,LN_TYPE(a1)
			move.b	#PA_SOFTINT,MP_FLAGS(a1)
			move.l	a0,MP_SIGTASK(a1)		;Softint eintragen

			;move.l	VBTimerIO-VidBuf_TMRSig(a2),a0	;don't do this manually, see above
			;move.l	VBTimerPort-VidBuf_TMRSig(a2),a1
			;move.l	a1,MN_REPLYPORT(a0)
			;move.w	#IOTV_SIZE,MN_LENGTH(a0)

			lea	VBtimer_name(pc),a0
			moveq	#UNIT_WAITECLOCK,d0
			move.l	VBTimerIO-VidBuf_TMRSig(a2),a1
			moveq	#0,d1
			CALLEXEC OpenDevice
			tst.l	d0
			beq.s	.ok
			moveq	#0,d0
			bra.s	.error
.ok			;OK, timer device is ready
			st	VBTimerInit-VidBuf_TMRSig(a2)

			moveq	#1,d0				;OK
.error
			movem.l	(sp)+,d1-a6
			;ret 0 <= err, >0 = ok
			rts

ExitYUYUTimer:		;stop timer.device for buffer swap interrupt
			movem.l	d0-a6,-(sp)
			lea	VidBuf_TMRSig,a2

			sf	VBTimerInit-VidBuf_TMRSig(a2)
			
			move.l	VBTimerIO-VidBuf_TMRSig(a2),d5
			beq.s	.noIO
			move.l	d5,a1
			move.l	IO_DEVICE(a1),a6
			jsr	DEV_ABORTIO(A6)	;clear pending requests

			move.l	d5,a1
			CALLEXEC CloseDevice
			move.l	d5,a0
			CALLEXEC DeleteIORequest
			clr.l	VBTimerIO-VidBuf_TMRSig(a2)
.noIO:
			move.l	VBTimerPort-VidBuf_TMRSig(a2),d0
			beq.s	.noPort
			move.l	d0,a0
			CALLEXEC DeleteMsgPort
			clr.l	VBTimerPort-VidBuf_TMRSig(a2)
.noPort:

			move.l	VidBuf_TMRSig-VidBuf_TMRSig(a2),d0
			cmp.l	#-1,d0
			beq	.nosig
			CALLEXEC FreeSignal
			move.l	#-1,VidBuf_TMRSig-VidBuf_TMRSig(a2)
.nosig:

			movem.l	(sp)+,d0-a6
			rts

; input: D1 - destination timestamp high
;        D2 - destination timestamp low
StartYUYVTimer:
	ifne	1
		movem.l	d0-a6,-(Sp)

		move.l	VBTimerIO,d0
		beq.s	.notimer
		move.l	d0,a1
		move.w	#TR_ADDREQUEST,IO_COMMAND(a1)
		clr.b	IO_FLAGS(a1)
		move.l	d1,IOTV_TIME+EV_HI(a1)
		move.l	d2,IOTV_TIME+EV_LO(a1)

		move.l	IO_DEVICE(a1),a6
		jsr	DEV_BEGINIO(A6)
.notimer
		movem.l	(sp)+,d0-a6
	endc
	ifne	0
		;just some random code I used in other projects, might come in handy
		lea	TimerRequest(a5),a1
		move.l	io_Device(a1),a6
		lea	Timer_Lasttime(a5),a0
		jsr	_LVOReadEClock(A6)		;Return: in D0 Taktfrequenz

		move.l	a_count_rate,d1	;time for 1 sec.
		divu.l	#1000,d1			;time for 1 millisec (can be useful sometimes)
		move.l	d1,e_clock_millisec

		move.l	frame_time(pc),d1
		neg.l	d1
		move.l	d1,max_lag		;max. allowed lag = -100%

		move.l	frame_time(pc),d1
		lsr.l	#2,d1
		move.l	d1,max_lead
	endc
			rts

VBTimerRoutine:
	movem.l	d1-a6,-(sp)
	lea	VidBuf_TMRSig,a2
	
	tst.b	VBTimerInit-VidBuf_TMRSig(a2)
	beq	.error

	move.l	VBTimerPort-VidBuf_TMRSig(a2),a0
	CALLEXEC	GetMsg
	tst.l	d0
	beq	.error				;Msg nicht da ? -> Fehler

	move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d1
	cmp.l	VidBuf_RVA-VidBuf_TMRSig(a2),d1
	bge.s	.stoptimer
	move.l	d1,d2
	and.l   #APOLLO_NSAGABUFS-1,d1
	mulu	#VIDBUF_SIZE,d1			; buffer index * struct size
	lea     VidBuf_Store-VidBuf_TMRSig(a2,d1.l),a0
	move.l	VIDBUF_Y(a0),a0			; dest ptr
	;
	;
	move.l 	a0,$DFF1EC               	; Update SAGA video register
	;
	;
	addq.l	#1,d2
	move.l	d2,VidBuf_TMR-VidBuf_TMRSig(a2)

	cmp.l   VidBuf_RVA-VidBuf_TMRSig(a2),d2
	bge.s	.stoptimer			; stoptimer will issue one "last" signal

	; currently. we issue a signal every time (whether somebody waits or not)
	move.l	VidBuf_TMRSig-VidBuf_TMRSig(a2),d0
	move.l	MainTask(pc),a1
        CALLEXEC Signal
	;

	;Input: D2 = current timer buffer index (played frames)
	bsr	GetVidBufTimer			;
	st	VBTimerRunning-VidBuf_TMRSig(a2)
	bsr	StartYUYVTimer

	movem.l	(sp)+,d1-a6
	moveq	#0,d0
	rts
.stoptimer:
        move.l  VidBuf_TMRSig-VidBuf_TMRSig(a2),d0
        move.l  MainTask(pc),a1
        CALLEXEC Signal

.error:		;error is right now the same as "finish"
	sf	VBTimerRunning-VidBuf_TMRSig(a2)
	moveq	#0,d0
	movem.l	(sp)+,d1-a6
	rts

;
; Input:  D2    = current timer buffer index (played frames)
;         A2    = pointer to VidBuf_TMRSig
; Output: D1:D2 = Timer position to wait for (D1 high, D2 low)
;
GetVidBufTimer:
	move.l	a0,-(sp)
	and.l   #APOLLO_NSAGABUFS-1,d2
	mulu	#VIDBUF_SIZE,d2			; buffer index * struct size
	lea     VidBuf_Store-VidBuf_TMRSig(a2,d2.l),a0
	movem.l	VIDBUF_TimeStampH(a0),d1-d2	;get dest timestamp
	; D1 - destination timestamp high
	; D2 - destination timestamp low
	move.l	(sp)+,a0
	rts

;
; We waited a while for audio to start up, use audio time base
; to correct the video time base before timer goes live
;
; This function hacks in the a/v sync into the current queue
;
; A2 = pointer to VidBuf_TMRSig
CorrectVidBufTimer:
	movem.l	d0-d7/a0-a1,-(sp)

	movem.l	audio_time-VidBuf_TMRSig(a2),d5-d6	; audio time position (lower word is interesting

	GETECLOCK64 d3,d4				; current position in ext clock

	move.l	frame_time-VidBuf_TMRSig(a2),d1		; video start position low
	;lsr.l	#1,d1					; half frame delay
	moveq	#0,d0					; video start position high (not needed)

	move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d7
.correctloop
	cmp.l	VidBuf_RVA-VidBuf_TMRSig(a2),d7
	bge.s	.done

	move.l	d7,d2
	addq.l	#1,d7

	and.l   #APOLLO_NSAGABUFS-1,d2
	mulu	#VIDBUF_SIZE,d2			; buffer index * struct size
	lea     VidBuf_Store-VidBuf_TMRSig(a2,d2.l),a0

	cmp.l	d6,d1				; audio time beyond current frame ?
	blt.s	.skip

	move.l	d1,a1
	move.l	d0,d2

	add.l	d4,d1
	addx.l	d3,d0
	movem.l	d0-d1,VIDBUF_TimeStampH(a0)	; recalculated timestamp

	move.l	d2,d0
	move.l	a1,d1
	bra.s	.noskip
.skip
	move.l	d7,VidBuf_TMR-VidBuf_TMRSig(a2)
.noskip
	moveq	#0,d2
	add.l	frame_time-VidBuf_TMRSig(a2),d1
	addx.l	d2,d0
	bra.s	.correctloop
.done
	movem.l	(sp)+,d0-d7/a0-a1
	rts

	endc	; APOLLO_NSAGABUFS

	
			;---------------------;
			; P96 - GRAY Init     ;
			;---------------------;
Init_p96_gray		move.l	#RGBFF_CLUT,p96BIDFormat
			lea	p96BIDTags(pc),a0
			CALLP96	BestModeIDTagList				;Get BestID
			cmp.l	#INVALID_ID,d0
			beq.b	Init_p96_gray_Error
			move.l	d0,p96ScreenModeID
			bsr	Open_p96_8bit					;Open Screen
			tst.l	d0
			beq.b	Init_p96_gray_Error
			lea	mpr_jsr_offsets(pc),a1				;all ok -> cleanup & exit
			move.l	#mpr_gray,mpr_RenderBitMap(a1)
			move.l	#mpr_DummyRTS,mpr_LockBitMap(a1)
			move.l	#mpr_DummyRTS,mpr_UnLockBitMap(a1)
			move.b	#DM_GRAY,DitherMode
			st	GrayMode
Init_p96_gray_OK	moveq	#1,d0
			rts
Init_p96_gray_Error	moveq	#0,d0
			rts

*=======================================================*
*======== Picasso96 PIP Initialisation Routines ========*
*=======================================================*
Open_p96_PIP		move.l	width(pc),d1
			move.l	d1,PIPSourceWidth
			mulu.l	ZOOM_value,d1
			divu.l	#100,d1
			move.l	d1,PIPWinWidth

			move.l	height(pc),d1
			move.l	d1,PIPSourceHeight
			mulu.l	ZOOM_value,d1
			divu.l	#100,d1
			move.l	d1,PIPWinHeight

			move.l	intbase,a6
			move.l	Pubscr(pc),a0
			jsr	_LVOLockPubScreen(a6)
			move.l	d0,PubScreen
			beq.b	lockbug

			bsr.w	centerwinbeforeopen

			move.l	intbase,a6
			sub.l	a0,a0
			move.l	PubScreen,a1
			jsr	_LVOUnlockPubScreen(a6)

lockbug:		bsr.w	setwindowfilename

			move.l	p96base,a6
			lea	OpenPIPtags(pc),a0
			jsr	_LVOp96PIP_OpenTagList(a6)
			move.l	d0,p96PIPWinHandle
			beq	Open_p96_PIP_Error
			move.l	d0,MainWindow

			move.l	p96base,a6		
			lea	Getp96PIPtags(pc),a1
			move.l	p96PIPWinHandle,a0
			jsr	_LVOp96PIP_GetTagList(a6)

			bsr.w	mpr_p96LockBitMapPIP
			lea	p96RenderInfo,a0		;Get the renderinfo structure
			move.l	gri_Memory(a0),a1
			move.l	a1,GfxMemBase
			moveq	#0,d1
			move.w	gri_BytesPerRow(a0),d1
			move.l	d1,BitmapModulo
			bsr.w	mpr_p96UnLockBitMapPIP

Open_p96_PIP_OK		CALLEXEC CacheClearU

			tst.l	PlanarAssistance
			beq.b 	No_Planar_Stuff

			move.l	#P96BMA_BOARDIOBASE,d0
			move.l	p96PIPBitmap,a0	
			move.l	p96base,a6
			jsr	_LVOp96GetBitMapAttr(a6)
			move.l	d0,PIVRegisterBase

			movea.l	PIVRegisterBase,a0
			move.b	#CRTC_MiscellaneousVideoControl,(CRTCI,a0)
			move.b	(CRTCD,a0),d0
			or.b	#(1<<4),d0		; YUV12PA Support Enable = On
			move.b	d0,(CRTCD,a0)

No_Planar_Stuff:	moveq	#1,d0
			bra.b	Open_p96_PIP_Done

Open_p96_PIP_Error	CALLEXEC CacheClearU
			moveq	#0,d0

Open_p96_PIP_Done	rts








centerwinbeforeopen:

			move.l	PubScreen,a1
			moveq	#$00,d3
			move.w	sc_Width(a1),d3
			move.w	d3,PIPZoomData+4

			moveq	#$00,d3
			move.b	sc_BarHeight(a1),d3
			addq.l	#1,d3				;Pavel oruljon jol :)
			move.w	d3,PIPZoomData+2

			moveq	#$00,d0
			move.w	sc_Height(a1),d0
			sub.l	d3,d0
			move.w	d0,PIPZoomData+6

			move.l	sc_WBorTop(a1),WinBorTop
			tst.l	wincenterw			;x poz
			bne.b	maspos1

			moveq	#$00,d0
			move.w	sc_Width(a1),d0
			moveq	#$00,d3
			move.b	sc_BarHeight(a1),d3
		
			moveq	#$00,d1
			move.b	WinBorLeft,d1
			moveq	#$00,d2
			move.b	WinBorRight,d2
			add.l	d2,d1
			add.l	d3,d1

			add.l	PIPWinWidth(pc),d1

			sub.l	d1,d0
			bmi.b	maspos1				;bugg! (negativ)

			asr.l	#1,d0
			move.l	d0,WindowLeft

maspos1:		tst.l	wincenterh			;y poz
			bne.b	maspos

			move.l	PubScreen,a1
			moveq	#$00,d0
			move.w	sc_Height(a1),d0

			move.l	sc_Font(a1),a2		;Bar méretének kiszámítása
			moveq	#$0,d3
			move.w	ta_YSize(a2),d3

			moveq	#$00,d1
			move.b	WinBorTop,d1
			moveq	#$00,d2
			move.b	WinBorBottom,d2
			add.l	d2,d1
			add.l	d3,d1
			addq.l	#1,d1

			add.l	PIPWinHeight(pc),d1

			sub.l	d1,d0
			bmi.b	maspos				;bugg! (negativ)

			asr.l	#1,d0
			move.l	d0,WindowTop

maspos:			rts


setwindowfilename:

			move.l	filename,d1
			move.l	dosbase,a6
			jsr	_LVOFilePart(a6)
			move.l	d0,a0

			lea	Windowfilename,a1
nametowin:		move.b	(a0)+,d0
			tst.b	d0
			beq.b	vegename

			move.b	d0,(a1)+
			bra.b	nametowin

vegename:		move.l	#"   [",(a1)+

			move.l	width(pc),d1
			divu.w	#100,d1
			or.b	#$30,d1
			move.b	d1,(a1)+
			clr.w	d1
			swap	d1		;maradek

			divu.w	#10,d1
			or.l	#$00300030,d1
			move.b	d1,(a1)+
			swap	d1
			move.b	d1,(a1)+
			move.b	#"x",(a1)+

			move.l	height(pc),d1
			divu.w	#100,d1
			or.b	#$30,d1
			move.b	d1,(a1)+
			clr.w	d1
			swap	d1		;maradek

			divu.w	#10,d1
			or.l	#$00300030,d1
			move.b	d1,(a1)+
			swap	d1
			move.b	d1,(a1)+
			move.w	#$5d00,(a1)+		;]es vege!

			rts


*=======================================================*
*====== Picasso96 Window Initialisation Routines =======*
*=======================================================*
Open_p96_Window
			move.l	Pubscr(pc),a0
			CALLINT2	LockPubScreen
			move.l	d0,PubScreen
			beq	Open_p96_Window_Error

			sub.l	a0,a0
			move.l	PubScreen,a1
			CALLINT2	UnlockPubScreen

			move.l	PubScreen,a1
			lea	sc_BitMap(a1),a0
			move.l	a0,a5
			move.l	#P96BMA_DEPTH,d0
			CALLP96	GetBitMapAttr
			move.l	d0,PubScreenDepth
			move.l	d0,d5
			cmp.l	#15,d5
			blt	Open_p96_Window_Error			;screen bitmap less than 16bit -> error

			move.l	a5,a0
			move.l	#P96BMA_RGBFORMAT,d0
			CALLP96	GetBitMapAttr				;check colour format of screen bitmap
			move.l	d0,PubScreenColorFmt

			cmp.l	#24,d5
			blt	.hicolor

			;truecolor screen
			move.l	#mpr_rgb24,d1				;RGB24
			cmp.b	#RGBFB_R8G8B8,d0
			beq.s	.isRGB24
			cmp.b	#RGBFB_B8G8R8,d0
			bne.b	.notBGR24
			move.l	mpr_bgr24,d1
.isRGB24:
			lea	mpr_jsr_offsets(pc),a1
			move.l	d1,mpr_RenderBitMap(a1)			;BGR24 or RGB24

			bsr	Open_p96_Window_Bitmap
			tst.l	d0
			beq	Open_p96_Window_Error			;can't allocate bitmap -> error

			bsr	DitherInit_bgr24
	ifne	WRENDER_DIRECT
			lea	mpr_jsr_offsets(pc),a1
			;move.l	#mpr_bgr24,mpr_RenderBitMap(a1)		;BGR24
			move.l	#mpr_p96LockBitMapDirect,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapDirect,mpr_UnLockBitMap(a1)
			move.b	#3,PubScrDepth
	endc
			bra	.color_format_ok

.notBGR24		cmp.b	#RGBFB_A8R8G8B8,d0
			bne.b	.notARGB32
			bsr	Open_p96_Window_Bitmap
			tst.l	d0
			beq	Open_p96_Window_Error			;can't allocate bitmap -> error
			bsr	DitherInit_argb32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_argb32,mpr_RenderBitMap(a1)	;ARGB32
	ifne	WRENDER_DIRECT
			move.l	#mpr_p96LockBitMapDirect,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapDirect,mpr_UnLockBitMap(a1)
			move.b	#4,PubScrDepth
	endc
			bra	.color_format_ok

.notARGB32		cmp.b	#RGBFB_B8G8R8A8,d0
			bne.b	.notBGRA32
			bsr	Open_p96_Window_Bitmap
			tst.l	d0
			beq	Open_p96_Window_Error			;can't allocate bitmap -> error
			bsr	DitherInit_bgra32
			lea	mpr_jsr_offsets(pc),a1
			move.l	#mpr_bgra32,mpr_RenderBitMap(a1)	;ARGB32
			bra	.color_format_ok

.notBGRA32		bra	Open_p96_Window_Error			;unsupported truecolor format -> error


.hicolor		;hicolor screen
			cmp.b	#RGBFB_R5G6B5PC,d0
			bne.b	.notRGB16PC
			move.b	#$00,hicolorformat
			bra.b	.hicolor_ok
.notRGB16PC		cmp.b	#RGBFB_R5G6B5,d0
			bne.b	.notRGB16
			move.b	#$01,hicolorformat
			bra.b	.hicolor_ok
.notRGB16		cmp.b	#RGBFB_R5G5B5PC,d0
			bne.b	.notRGB15PC
			move.b	#$02,hicolorformat
			bra.b	.hicolor_ok
.notRGB15PC		cmp.b	#RGBFB_R5G5B5,d0
			bne.b	.notRGB15
			move.b	#$03,hicolorformat
			bra.b	.hicolor_ok
.notRGB15		cmp.b	#RGBFB_B5G6R5PC,d0
			bne.b	.notBGR16PC
			move.b	#$04,hicolorformat
			bra.b	.hicolor_ok
.notBGR16PC		cmp.b	#RGBFB_B5G5R5PC,d0
			bne.b	.notBGR15PC
			move.b	#$05,hicolorformat
			bra.b	.hicolor_ok
.notBGR15PC		bra	Open_p96_Window_Error			;unsupported hicolor format -> error

.hicolor_ok
			bsr	Open_p96_Window_Bitmap
			tst.l	d0
			beq	Open_p96_Window_Error			;can't allocate bitmap -> error

			bsr	DitherInit_rgbhicolor
			tst.l	d0
			beq	Open_p96_Window_Error			;can't initialise hicolor render -> error
			lea	mpr_jsr_offsets(pc),a1

			move.l	#mpr_rgbhicolor,mpr_RenderBitMap(a1)
	ifne	WRENDER_DIRECT
			move.l	#mpr_p96LockBitMapDirect,mpr_LockBitMap(a1)
			move.l	#mpr_p96UnLockBitMapDirect,mpr_UnLockBitMap(a1)
			move.b	#2,PubScrDepth
	endc
.color_format_ok
			move.l	width(pc),d1
			move.l	d1,WinWidth
			move.l	d1,PIPWinWidth			;needed for centerwinbeforeopen
			move.l	height(pc),d1
			move.l	d1,WinHeight
			move.l	d1,PIPWinHeight			;needed for centerwinbeforeopen

			bsr	centerwinbeforeopen

			bsr	setwindowfilename

			tst.l	 BORDERLESS_switch
			beq.s    .borders_on
			move.l	 #TAG_IGNORE,WindowTitle-4	; no piptitle
			move.l	 #0,WindowTitle+0		; when borderless
			move.l	 #1,WinBorderless
			move.l	 #0,WinDragBar
			move.l	 #0,WinCloseGadget
			move.l	 #0,WinDepthGadget
.borders_on
			suba.l	a0,a0
			lea	WindowTags(pc),a1
			CALLINT2	OpenWindowTagList
			move.l	d0,MainWindow
			beq.b	Open_p96_Window_Error

			tst.l	 BORDERLESS_switch
			beq.s    .borders_on_2
			
		;	move.l   MainWindow,a0
		;	move.l   #MySizeGadget,a1
		;	move.w   width(pc),gg_LeftEdge(a1)
		;	move.w   height(pc),gg_TopEdge(a1)
		;	sub.w    #10,gg_LeftEdge(a1)
		;	sub.w    #10,gg_TopEdge(a1)
		;	move.w   #10,gg_Width(a1)
		;	move.w   #10,gg_Height(a1)
		;	moveq.l  #~0,d0
		;	CALLINT2 AddGadget
		
			move.l   MainWindow,a0
			lea      MyDragGadget,a1
			move.w   #0,gg_LeftEdge(a1)
			move.w   #0,gg_TopEdge(a1)
			move.w   width+2,gg_Width(a1)
			move.w   height+2,gg_Height(a1)
			move.w   #~0,d0
			CALLINT2 AddGadget
.borders_on_2
			move.l	MainWindow,d0

			;window open, return winhandle

			rts


Open_p96_Window_Error
			move.l	p96WinPlayBitMap,a0
			tst.l	a0
			beq.b	.NoWinBitmap
			CALLP96	FreeBitMap
			clr.l	p96WinPlayBitMap
.NoWinBitmap
			moveq	#0,d0
			rts

Open_p96_Window_Bitmap:
			move.l	width(pc),d0
			move.l	height(pc),d1
			move.l	PubScreenDepth,d2
			moveq	#0,d3
			move.l	PubScreenColorFmt,d7
			suba.l	a0,a0
			CALLP96	AllocBitMap
			move.l	d0,p96WinPlayBitMap
			beq.b	.BitmapOpenDone
			bsr	mpr_p96LockBitMapWin		;get info on bitmap (base, modulo, etc.)
			move.l	p96WinPlayBitMap,a0
			move.l	p96_bitmaplock,d0
			CALLP96	UnlockBitMap

			bsr	Calc_CenterCropWin

			move.l	p96WinPlayBitMap,d0
.BitmapOpenDone
			rts

*=======================================================*
*====== Picasso96 Screen Initialisation Routines =======*
*=======================================================*
Open_p96_Screen:	lea	p96ScreenTags(pc),a0
			bra.b	Open_p96_start

Open_p96_8bit:		lea	p96ScreenTags8(pc),a0

Open_p96_start:
			move.l	YUYVCols.l(pc),p96ScreenWidth
			move.l	YUYVCols.l(pc),ScreenWidth
			move.l	YUYVRows.l(pc),p96ScreenHeight
			move.l	YUYVRows.l(pc),ScreenHeight

			CALLP96	OpenScreenTagList

			move.l	d0,ScreenHandle
			beq.b	Open_p96_End

			move.l	ScreenHandle(pc),a1
			lea	sc_RastPort(a1),a1
			move.l	a1,ScreenRastport

			bsr	mpr_p96LockBitMap		;get info on bitmap (base, modulo, etc.)
			bsr	mpr_p96UnLockBitMap

Open_p96_End		move.l	ScreenHandle,d0			;this returns NULL if can't open.
			move.l	d0,P96ScreenHandle
			rts

DitherInit_rgbhicolor:
	IFEQ APOLLO_YUYV
			move.l	y_bitmap_width(pc),d1
			subq.l	#4,d1
			move.w	d1,2+mpr_rgbhicolor_ofst0
			move.l	BitmapModulo,d1
			subq.l	#4,d1
			move.w	d1,2+mpr_rgbhicolor_ofst1
			move.w	d1,2+mpr_rgbhicolor_ofst2
	ENDC
			bsr	CreateYUVtoHiColorTable
			tst.l	d0
			beq.b	Init_rgbhicolor_error

Init_rgbhicolor_ok	moveq	#1,d0
			rts
Init_rgbhicolor_error	bsr	CloseDisplay			;Screen is already open, but can't init. render -> Close!
			moveq	#0,d0
			rts

DitherInit_bgr24
			move.l	BitmapModulo,d1
			move.w	d1,2+modulo1
			addq.l	#4,d1
			move.w	d1,2+modulo2
			addq.l	#4,d1
			move.w	d1,2+modulo3
			move.l	y_bitmap_width,d1
			move.w	d1,2+width0
			addq.l	#1,d1
			move.w	d1,2+width1
			addq.l	#1,d1
			move.w	d1,2+width2
			addq.l	#1,d1
			move.w	d1,2+width3
			rts

DitherInit_argb32:
	IFEQ	APOLLO_YUYV
			move.l	BitmapModulo,d1
			move.w	d1,2+argb32modulo0
			addq.l	#4,d1
			move.w	d1,2+argb32modulo1
			addq.l	#4,d1
			move.w	d1,2+argb32modulo2
			addq.l	#4,d1
			move.w	d1,2+argb32modulo3
			move.l	y_bitmap_width,d1
			move.w	d1,2+argb32width0
			addq.l	#1,d1
			move.w	d1,2+argb32width1
			addq.l	#1,d1
			move.w	d1,2+argb32width2
			addq.l	#1,d1
			move.w	d1,2+argb32width3
	ENDC
			rts

DitherInit_bgra32
			move.l	BitmapModulo,d1
			move.w	d1,2+bgra32modulo0
			addq.l	#4,d1
			move.w	d1,2+bgra32modulo1
			addq.l	#4,d1
			move.w	d1,2+bgra32modulo2
			addq.l	#4,d1
			move.w	d1,2+bgra32modulo3
			move.l	y_bitmap_width,d1
			move.w	d1,2+bgra32width0
			addq.l	#1,d1
			move.w	d1,2+bgra32width1
			addq.l	#1,d1
			move.w	d1,2+bgra32width2
			addq.l	#1,d1
			move.w	d1,2+bgra32width3
			rts


	ifeq		APOLLO_P96ONLY
		include	"RendererCGXInit.i"
	endc
	ifeq		APOLLO_P96ONLY
		include	"RendererAGAInit.i"
	endc


;Parse sequence header
;---------------------
Parse_Sequence_Header
		NGETDATA	12,width,d2
		NGETDATA	12,height,d2
		NGETDATA	4,pel_aspect_ratio,d2
		NGETDATA	4,picture_rate,d2

		move.l	FPS_value,d2
		bne.b	UserSpecifiedFPS
		lea	video_rates(pc),a2
		move.l	(a2,d1.w*4),d2
		bne.b	ValidFPS
		move.l	#1638400,d2
ValidFPS:		move.l	d2,vid_rate
		bra.b	FPS_Done

UserSpecifiedFPS	lsl.l	#8,d2				;convert to 16:16 Int
		lsl.l	#8,d2
		move.l	d2,vid_rate
FPS_Done

		NGETDATA	18,bit_rate,d2
		NSKPBITS	1,d2				;marker bit ignored...
		NGETDATA	10,vbv_buffer_size,d2
		NGETDATA	1,constrained_param_flag,d2

chk_intra_mtx	NGETBITS	1,d1,d2			;Check for custom intra quant. matrix
		tst.l	d1
		beq.b	chk_nonintr_mtx
		bsr.w	LoadCustomIntraMatrix		;Load matrix array if exists...

chk_nonintr_mtx	NGETBITS	1,d1,d2			;Check for custom non-intra quant. matrix
		tst.l	d1
		beq.b	seq_ext_check
		bsr.w	LoadCustomNonIntraMatrix	;Load array if it exists

		NEXT_START_CODE

seq_ext_check	CHK32	d1
		cmp.l	#extension_start_code,d1
		bne.b	no_sequence_extension
		SKP32
seq_ext_loop	SKP8					;<--- extension data... (ignored)
		CHK32	d1
		clr.b	d1
		cmp.l	#$00000100,d1
		bne.b	seq_ext_loop
no_sequence_extension

		NEXT_START_CODE

seq_user_check	CHK32	d1
		cmp.l	#user_data_start_code,d1
		bne.b	no_sequence_user_data
		SKP32
seq_user_loop	SKP8					;<--- user data... (ignored)
		CHK32	d1
		clr.b	d1
		cmp.l	#$00000100,d1
		bne.b	seq_user_loop
no_sequence_user_data

		NEXT_START_CODE

		IFNE	DEBUG
		movem.l	a0/d0,-(a7)
		bsr	DisplayQuantMatrices
		movem.l	(a7)+,a0/d0
		ENDC

sequence_header_done

		rts

;Display sequence header data
;----------------------------
Display_Sequence_Header
		tst.l	verbose_switch
		beq.w	skip_header_info

		OUTTXT	VideoInfoMsg
		OUTDEC	width(pc)
		OUTTXT	TIMES
		OUTDEC	height(pc)
		OUTTXT	COMMA

		move.l	picture_rate,d1
		lea	video_rates(pc),a2
		move.l	(a2,d1.w*4),d1
		bne.b	vidrate_valid
		OUTTXT	msg_Unknown
vidrate_valid	OUTNUMN	d1,3
vidrate_displayed
		OUTTXT	FPSMsg

		OUTTXT	AudioInfoMsg
		tst.l	AudioTask(pc)
		beq	.no_audio

		OUTTXT	AudioModeMsg
		move.l	mpegastream(pc),a2
		moveq	#0,d1
		move.w	MPASTRM_NORM(a2),d1
		OUTDEC	d1
		OUTTXT	MINUS
		OUTTXT	AudioLayerMsg
		move.w	MPASTRM_LAYER(a2),d1
		OUTDEC	d1
		move.w	MPASTRM_MODE(a2),d1
		beq.b	.stereo
		cmp.b	#1,d1
		beq.b	.jstereo
		cmp.b	#2,d2
		beq.b	.dual
		OUTTXT	AudioMonoMsg
		bra.b	.mode_done
.dual		OUTTXT	AudioDualMsg
		bra.b	.mode_done
.jstereo	OUTTXT	AudioJStereoMsg
		bra.b	.mode_done
.stereo		OUTTXT	AudioStereoMsg
.mode_done
		move.w	MPASTRM_BITRATE(a2),d1
		OUTDEC	d1
		OUTTXT	AudioKbps
		move.l	MPASTRM_FREQUENCY(a2),d1
		OUTDEC	d1
		OUTTXT	AudioHz
		rts

.no_audio	OUTTXT	NoAudioMsg

skip_header_info
		rts



		EVEN
OpenFileAsl	moveq	#ASL_FileRequest,d0		;If no filename specified, use ASL requester
		suba.l	a0,a0				;tags
		CALLASL	AllocAslRequest
		move.l	d0,AslFileReq
		beq.b	OpenFileAslError		;if cannot open requester, give error message & exit
		move.l	d0,a0
		lea	AslFileReqTags(pc),a1		;tags
		CALLASL	AslRequest
		tst.l	d0
		beq.b	OpenFileAslError		;if no file requested, exit quietly...

		move.l	AslFileReq,a0
		;move.l	fr_NumArgs(a0),AslFileCount	;only needed for multiple files (not yet...)
		;move.l	fr_ArgList(a0),FileList		;this will give list of all selected files
		move.l	fr_Drawer(a0),a1
		lea	filenamestring,a2
		move.l	a2,filename
.copypath	move.b	(a1)+,d1
		move.b	d1,(a2)+
		bne.b	.copypath
		move.l	filename,d1
		move.l	fr_File(a0),d2
		move.l	#4096,d3
		CALLDOS2	AddPart

		moveq	#1,d0
		rts
OpenFileAslError
		moveq	#0,d0
		rts


***************************************************************************
*  variables that are read in register-heavy loops, kept in code section  *
***************************************************************************
;BASE:
width:				dc.l	0
height:				dc.l	0
y_bitmap_width:			dc.l	0
y_bitmap_height:		dc.l	0
y_bitmap_size:			dc.l	0
y_MB_max_addr:			dc.l	0
c_bitmap_width:			dc.l	0
c_bitmap_height:		dc.l	0
c_bitmap_size:			dc.l	0

; keep MB_y_BitmapOffset and MB_c_BitmapOffset together!
MB_y_BitmapOffset:		dc.l	0,0,0,0 ;four dwords with identical content to simplify recon loops after DCT
MB_c_BitmapOffset:		dc.l	0,0	;two dwords with identical content to simplify recon loops after DCT
; keep y_bitmap_base and cX_bitmap_base together!
y_bitmap_base:			dc.l	0,0,0,0 ;4x the same pointer
cb_bitmap_base:			dc.l	0
cr_bitmap_base:			dc.l	0
; includes two dummy offsets for chroma (indices 4,5)
y_block_offset_table:		dc.l	0,8,160*8,160*8+8,0,0

no_of_loops:			dc.l	0


dos_name:		dc.b	"dos.library",0
intuition_name:		dc.b	"intuition.library",0
gfx_name:		dc.b	"graphics.library",0
picasso96_name:		dc.b	"Picasso96API.library",0
cybergraphics_name:	dc.b	"cybergraphics.library",0
cgxvideolib_name:	dc.b	"cgxvideo.library",0
asl_name:		dc.b	"asl.library",0
icon_name:		dc.b	"icon.library",0
mpega_name:		dc.b	"mpega.library",0
tt_FPS			dc.b	"FPS",0
tt_ZOOM			dc.b	"ZOOM",0
tt_FULLPIP		dc.b	"FULLPIP",0
tt_NOSKIP		dc.b	"NOSKIP",0
tt_P96			dc.b	"PICASSO96",0
tt_CGX			dc.b	"CYBERGFX",0
tt_AGA			dc.b	"AGA",0
tt_HALF			dc.b	"HALFHEIGHT",0
tt_LOOP			dc.b	"LOOP",0
tt_DISPLAY		dc.b	"DISPLAY",0
tt_PA			dc.b	"PIVPLANARASSIST",0
tt_AHI			dc.b	"AHI",0
tt_NOAUDIO		dc.b	"NOAUDIO",0
tt_NOVIDEO		dc.b	"NOVIDEO",0
tt_NORENDER		dc.b	"NORENDER",0
tt_MONOSURROUND		dc.b	"MONOSURROUND",0
tt_AUDIOQUALITY		dc.b	"AUDIOQUALITY",0
tt_AUDIOFREQDIV		dc.b	"AUDIOFREQDIV",0
tt_PUBSCREEN		dc.b	"PUBSCREEN",0
tt_DEFAULTDIR		dc.b	"DEFAULTDIR",0
tt_BORDERLESS		dc.b	"BORDERLESS",0
;
ttd_PIP			dc.b	"PIP",0
ttd_WINDOW		dc.b	"WINDOW",0
ttd_TRUECOLOR		dc.b	"TRUECOLOR",0
ttd_HICOLOR		dc.b	"HICOLOR",0
ttd_GRAY		dc.b	"GRAY",0
	ifeq	APOLLO_P96ONLY
ttd_ACCUPAK		dc.b	"ACCUPAK",0
ttd_DHAM8		dc.b	"DHAM8",0
ttd_DHAM6		dc.b	"DHAM6",0
	endc
tt_HQAUDIO		dc.b	"HQAUDIO",0

TimerDev:		dc.b	"timer.device",0,0

MPEGargs:		dc.b	"FILE/M,"
			dc.b	"FPS/K/N,"
			dc.b	"ZOOM/K/N,"
			dc.b	"FULL=FULLPIP/S,"
			dc.b	"P96=PICASSO96/S,"
			dc.b	"CGX=CGFX=CYBERGFX/S,"
			dc.b	"AGA/S,"
			dc.b	"VGA=MULTISCAN/S,"
			dc.b	"DISPLAY=DITHER/K,"
			dc.b	"RTG=AKIKO/S,"
			dc.b	"NOSKIP/S,"
			dc.b	"VERBOSE/S,"
			dc.b	"HALF=HALFHEIGHT/S,"
			dc.b	"PIVPLANARASSIST=PA/S,"
			dc.b	"NOSOUND=NOAUDIO/S,"
			dc.b	"NOVIDEO/S,"
			dc.b	"NORENDER/S,"
			dc.b	"AHI/S,"
			dc.b	"HQAUDIO/S,"
			dc.b	"SAVEAUDIO/K,"
			dc.b	"SURROUND=MONOSURROUND/S,"
			dc.b	"PUBSCREEN/K,"
			dc.b	"NOP/S,"
			dc.b	"NOB/S,"
			dc.b	"AUDIOQUALITY/K/N,"
			dc.b	"AUDIOFREQDIV/K/N,"
			dc.b	"DEFAULTDIR/K,"
			dc.b	"B=BORDERLESS/S"
			dc.b	0

DitherTemplate:		dc.b	"PIP,WINDOW,TRUECOLOR,HICOLOR,GRAY,GREY"
	ifeq	APOLLO_P96ONLY
			dc.b	",ACCUPAK,DHAM8,DHAM6"
	endc
			dc.b	0

txt_DitherHelp:		dc.b	10,$9b,$31,$6d,"Selectable Display Types:",$9b,$30,$6d,10
			dc.b	"PIP.............Picture In Picture Window",10
			dc.b	"WINDOW..........Normal Window (only on HiColor/TrueColor screens)",10
			dc.b	"TRUECOLOR.......24bit or 32bit TrueColor Screen",10
			dc.b	"HICOLOR.........16bit HiColor Screen",10
			dc.b	"GRAY/GREY.......Standard 8bit grayscale Screen",10
	ifeq	APOLLO_P96ONLY
			dc.b	"ACCUPAK.........Special FAST Color Mode for PicassoIV only!",10
			dc.b	"DHAM8...........Double-width HAM8 Colour AGA mode (default on AGA)",10
			dc.b	"DHAM6...........Double-width HAM6 Colour AGA mode (very fast!)",10
	endc
			dc.b	"Please enter display type: ",0

		EVEN
AslFileReqTags:		;dc.l	ASLFR_DoMultiSelect
			;dc.l	1
			dc.l	ASLFR_RejectIcons
			dc.l	1
			dc.l	ASLFR_InitialDrawer
ASLDrawerString:	dc.l	EmptyDrawer
			dc.l	TAG_END



			EVEN
BIDTags:
			dc.l	BIDTAG_MonitorID
BIDMonitorID:		dc.l	PAL_MONITOR_ID
			dc.l	BIDTAG_NominalWidth
BIDWidth:		dc.l	320
			dc.l	BIDTAG_NominalHeight
BIDHeight:		dc.l	240
			dc.l	BIDTAG_Depth
BIDDepth:		dc.l	8
			dc.l	TAG_END

			EVEN
p96BIDTags:
			dc.l	P96BIDTAG_NominalWidth
p96BIDWidth:		dc.l	320
			dc.l	P96BIDTAG_NominalHeight
p96BIDHeight:		dc.l	240
			dc.l	P96BIDTAG_FormatsAllowed
p96BIDFormat:		dc.l	RGBFF_CLUT
			dc.l	TAG_END

; manual parsing of the mode list to find (truly) best mode
p96ModelistTags:	dc.l	P96MA_FormatsAllowed
p96ModeListFormat:	dc.l	0
			dc.l	TAG_END

use_bestmode		dc.l	1

p96ScreenTags8:		dc.l	P96SA_Colors32,Palette
			dc.l	TAG_MORE,p96ScreenTags

ScreenWidth		dc.l	0
ScreenHeight		dc.l	0

p96ScreenTags:		dc.l	P96SA_Quiet,1
			dc.l	P96SA_Title,ScreenTitle
			dc.l	P96SA_Behind,0
			dc.l	P96SA_AutoScroll,1
			dc.l	P96SA_NoSprite,1
			dc.l	P96SA_Left
p96ScreenPosX:		dc.l	0
			dc.l	P96SA_Top
p96ScreenPosY:		dc.l	0
			dc.l	P96SA_Width
p96ScreenWidth:		dc.l	320
			dc.l	P96SA_Height
p96ScreenHeight:	dc.l	240
			dc.l	P96SA_Depth
p96Depth:		dc.l	8
			dc.l	P96SA_DisplayID
p96ScreenModeID:	dc.l	0
			dc.l	TAG_END

WindowTags
			dc.l	WA_InnerWidth
WinWidth:		dc.l	160
			dc.l	WA_InnerHeight
WinHeight:		dc.l	120
			dc.l	WA_Borderless
WinBorderless:		dc.l	0
			dc.l	WA_DragBar
WinDragBar:		dc.l	1
			dc.l	WA_CloseGadget
WinCloseGadget:		dc.l	1
			dc.l	WA_DepthGadget
WinDepthGadget:		dc.l	1
			dc.l	WA_IDCMP,IDCMP_CLOSEWINDOW+IDCMP_RAWKEY
			dc.l	TAG_MORE,CommonWinTags

OpenPIPtags:		dc.l	P96PIP_SourceFormat
PIPwinformat:		dc.l	RGBFB_Y4U2V2		;ajuvifortutu ;)

			dc.l	WA_Width
PIPWinWidth:		dc.l	160
			dc.l	WA_Height
PIPWinHeight:		dc.l	120

			dc.l	P96PIP_SourceWidth
PIPSourceWidth:		dc.l	160
			dc.l	P96PIP_SourceHeight
PIPSourceHeight:	dc.l	120

;			dc.l	WA_NotifyDepth,1

			dc.l	P96PIP_ErrorCode
			dc.l	PIPerror

			dc.l	P96PIP_Type,P96PIPT_MemoryWindow

;			dc.l	P96PIP_Relativity,PIPRel_Height
;			dc.l	P96PIP_Height
;guiheight:		dc.l	-30
;			dc.l	WA_IDCMP,IDCMP_CLOSEWINDOW+IDCMP_RAWKEY+IDCMP_GADGETUP+IDCMP_GADGETDOWN+IDCMP_MOUSEMOVE+IDCMP_CHANGEWINDOW+IDCMP_REFRESHWINDOW
			dc.l	WA_IDCMP,IDCMP_CLOSEWINDOW+IDCMP_RAWKEY

			dc.l	TAG_MORE,CommonPIPTags

			EVEN



CommonPIPTags:

		dc.l	WA_DragBar
PIPWinTagDrag	dc.l	1
		dc.l	WA_CloseGadget
PIPWinTagClose	dc.l	1
		dc.l	WA_DepthGadget
PIPWinTagDepth	dc.l	1
		dc.l	WA_SizeGadget
PIPWinTagSize	dc.l	1
		dc.l	WA_Borderless
PIPWinTagBLess	dc.l	0

		dc.l	TAG_MORE,CommonWinTags



CommonWinTags:
		dc.l	WA_Left
WindowLeft:	dc.l	20
		dc.l	WA_Top
WindowTop:	dc.l	20

		dc.l	WA_Title
WindowTitle	dc.l	PIPtitle
		dc.l	WA_ScreenTitle,ScreenTitle

		dc.l	WA_Activate,1

		dc.l	WA_RMBTrap,1

		dc.l	WA_PubScreenName
Pubscr:		dc.l	PubScrname

		dc.l	TAG_END

		EVEN
WinBorTop:	ds.b	1	;a center windowhoz!
WinBorLeft:	ds.b	1
WinBorRight:	ds.b	1
WinBorBottom:	ds.b	1
hicolorformat:	ds.b	1	;0=rgb16pC,1=rgb16,2=rgb15pC,3=rgb15,4=bgr16pC,5=bgr15pC
PubScrDepth:	ds.b	1	;depth in Byte per Pixel

PubScrname:		dc.b	"Workbench",0
			EVEN

Getp96PIPtags:		dc.l	P96PIP_SourceRPort,p96PIPrport
			dc.l	P96PIP_SourceBitMap,p96PIPBitmap
			dc.l	TAG_END
 

PIVRegisterBase:	dc.l	0
MyYUVInfo:		dcb.b	yi_SIZEOF,0
			cnop	0,4



WindowTagList:		dc.l	WA_Left,0
			dc.l	WA_Top,0
			dc.l	WA_Width
WindowWidth:		dc.l	320
			dc.l	WA_Height
WindowHeight:		dc.l	240
			dc.l	WA_CustomScreen
ScreenHandle:		dc.l	0
			dc.l	WA_Borderless,1
			dc.l	WA_Activate,1
			dc.l	WA_RMBTrap,1
			dc.l	WA_IDCMP,IDCMP_RAWKEY|IDCMP_MOUSEBUTTONS
			dc.l	TAG_END

ScreenTitle:		dc.b	"RiVA Screen",0


RETURN:			dc.b	$a,0
SPACE:			dc.b	" ",0
COLON:			dc.b	":",0
POINT:			dc.b	".",0
PER:			dc.b	"/",0
PLUS:			dc.b	"+",0
MINUS:			dc.b	"-",0
TIMES:			dc.b	"x",0
COMMA:			dc.b	", ",0
BRACKET_OPEN:		dc.b	"(",0
BRACKET_CLOSE:		dc.b	")",0

;Typefaces:	Normal:	$9b,$30,$6d
;		Bold:	$9b,$31,$6d

NoFileMsg:		dc.b	10," ERROR: No file to play.",10,10,0
LockErrMsg:		dc.b	10," ERROR: I'm unable to lock the file.",10,10,0
OpenErrMsg:		dc.b	10," ERROR: I'm unable to open the file for reading.",10,10,0
SeekErrMsg:		dc.b	10," ERROR: A Seek error has occured.",10,10,0
NotMpegMsg:		dc.b	10," ERROR: This does not appear to be an MPEG File.",10,10,0
MemAllocErrMsg:		dc.b	10," ERROR: Unable to allocate enough memory.",10,10,0
NoGOPMsg:		dc.b	10," ERROR: I couldn't find any groups of pictures.",10,10,0
NoPicErrMsg:		dc.b	10," ERROR: I could not find any pictures in this MPEG file.",10,10,0
NoSliceMsg:		dc.b	10," ERROR: I couldn't find any slices in picture.",10,10,0
	IFNE	APOLLO_CLIP
ApolloOnMSG:		dc.b	" Apollo Core Detected: Enjoy the fastest m68k MPEG-1 player.",10,0
ApolloOffMSG:		dc.b	" Apollo Core Gold2 or better not detected. This is an AMMX Apollo build!",10
			dc.b	" Please download the m68k version instead for this machine.",10,0
my_easygadget:		dc.b	"   OK   ",0
my_easytitle:		dc.b	"RiVA Message",0
	ELSE
ApolloOnMSG:		dc.b	10," Apollo Core Detected: Please download the (faster) Apollo version of RiVA.",10,0
ApolloOffMSG:		dc.b	" Generic m68k build of RiVA running.",10,0
	ENDC
	IFEQ	APOLLO_NSAGABUFS
SAGAAllocYUYV           dc.b    10," SAGA: Allocate YUYV triple-buffer.",10,0
SAGAFreeYUYV            dc.b    10," SAGA: Unallocate YUYV triple-buffer.",10,0
	endc
msg_ScreenOpenError:	dc.b	10," ERROR: Unable to open screen.",10,10,0
msg_ScreenSizeError:	dc.b	10," ERROR: Invalid Screen Size.",10,10,0
msg_UserModeReqReject:	dc.b	10," I am unable to open the requested display."
			dc.b	10," I'll try to open a suitable display for you...",10,10,0

			IFNE	SHOW_RENDERINFO
msg_intoqueue:		dc.b	" -> QUEUE",10,0
msg_render:		dc.b	"render frame ",0
msg_renderqueue:	dc.b	"render (queue) frame ",0
			ENDC

;msg_WAIT:		dc.b	" WAIT ",0
;msg_SKIP:		dc.b	" SKIP",10,0

			IFNE	(SHOW_PICINFO|SHOW_RENDERINFO)
msg_frame:		dc.b	"frame ",0
			ENDC

			IFNE	SHOW_PICINFO
msg_i_pic:		dc.b	"(I)",0
msg_p_pic:		dc.b	"(P)",0
msg_b_pic:		dc.b	"(B)",0
msg_buf			dc.b	"buf=",0
msg_buf1		dc.b	"(Buffer1)",0
msg_buf2		dc.b	"(Buffer2)",0
msg_buf3		dc.b	"(Buffer3)",0
msg_unknownbuf		dc.b	"(???)",0
msg_for			dc.b	"for=",0
msg_back		dc.b	"back=",0
msg_fwd_ref:		dc.b	"fwd.ref ",0
msg_back_ref:		dc.b	"back.ref ",0
msg_gop:		dc.b	" - group start",10,0
			ENDC

			IFNE	SHOW_MBINFO
msg_intra:		dc.b	" intra",0
msg_fwd:		dc.b	" fwd: <",0
msg_bwd:		dc.b	" bwd: <",0
msg_fwd_end		dc.b	">",0
msg_slice:		dc.b	"slice ",0
msg_quant:		dc.b	" quant=",0
msg_no_motion:		dc.b	" no motion ",0
			ENDC

			IFNE	SHOW_COEFFS
msg_block:		dc.b	" block ",0
			ENDC

			IFNE	(SHOW_MBINFO|SHOW_COEFFS)
msg_macroblock:		dc.b	" macroblock ",0
			ENDC


;msg_HalfPel:		dc.b	"vectors are half-pixel",10,0
;msg_FullPel:		dc.b	"vectors are full-pixel",10,0
;msg_bfcode:		dc.b	"backward_f_code = ",0
;msg_fwdf:		dc.b	"forward_f = ",0
;msg_bfwdf:		dc.b	"backward_f = ",0
;msg_mvfr:		dc.b	"motion_vertical_forward_r = ",0
;msg_chfr:		dc.b	"complement_horizontal_forward_r = ",0
;msg_cvfr:		dc.b	"complement_vertical_forward_r = ",0
;msg_right_little:	dc.b	"right_little = ",0
;msg_right_big:		dc.b	"right_big = ",0
;msg_down_little:	dc.b	"down little = ",0
;msg_down_big:		dc.b	"down_big = ",0

VideoInfoMsg:		dc.b	$a," Video: ",0
AudioInfoMsg:		dc.b	$a," Audio: ",0
AudioModeMsg:		dc.b	"MPEG",0
AudioLayerMsg:		dc.b	"Layer",0
NoAudioMsg:		dc.b	"<NONE>",$a,0
AudioMonoMsg:		dc.b	" Mono ",0
AudioDualMsg:		dc.b	" Dual ",0
AudioJStereoMsg:	dc.b	" J-Stereo ",0
AudioStereoMsg:		dc.b	" Stereo ",0
AudioKbps:		dc.b	"kbps ",0
AudioHz:		dc.b	"Hz",$a,0

			IFNE	DEBUG
IntQuantMtxMsg:		dc.b	$a,$a,"Current Intra Quantization Matrix:",$a,0
NonIntQuantMtxMsg:	dc.b	$a,$a,"Current Non-Intra Quantization Matrix:",$a,0
TimeCodeMsg:		dc.b	$a,"Time Code ",0
			ENDC

;stuff for block_err
;BlockErrMsg:		dc.b	10,10," ERROR: Premature end of block.",10,0
;msg_BufPos:		dc.b	10,"Position in buffer: ",0
;msg_Data:		dc.b	"      Data: ",0
;MBAddressMsg:		dc.b	$a,"Current Macroblock Address: ",0
;BlockNumberMsg:	dc.b	$a,"Block number: ",0
;msg_runlevel:		dc.b	10,"RUN/LEVEL = ",0

AudioOutModeMsg:	dc.b	$a," Audio mode: ",0
AudioModeADev:		dc.b	"Audio Device 8 Bit",0
AudioModeAHI:		dc.b	"AHI 16 Bit",0
AudioModePaula14:	dc.b	"Paula 14 Bit",0
AudioModePamela16:	dc.b	"Pamela 16 Bit",0

PicsPlayedMsg:		dc.b	$a," Number of frames played:  ",0
PicsSkippedMsg:		dc.b	$a," Number of frames skipped: ",0
PicsTotalMsg:		dc.b	$a," Total number of frames:   ",0
TotalTimeMsg:		dc.b	$a," Total playback time: ",0
SecondsMsg:		dc.b	" seconds.",0
AvgFrameRateMsg:	dc.b	$a," Average framerate:   ",0
DispFrameRateMsg:	dc.b	$a," Displayed framerate: ",0
FPSMsg:			dc.b	" fps",0
UserBreakMsg:		dc.b	$a,"**BREAK** User Aborted.",$a,0
msg_Unknown:		dc.b	"Unknown",0

	ifne	CODE_ALIGN
		CNOP    0,16
	else
		CNOP	0,4
	endc

video_rates:		dc.l	0,1571292,1572864,1638400,1964114,1966080,3276800,3928228,3932160,983040,0,0,0,0,0,0

;All cosine constants are multiplied by 256. Cu=0.5/sqr(2) at x=0, and 0.5 at x>0

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

zz_order:		dc.b	0,1,5,6,14,15,27,28	;note the dc.b (saves some bytes in binary)
			dc.b	2,4,7,13,16,26,29,42
			dc.b	3,8,12,17,25,30,41,43
			dc.b	9,11,18,24,31,40,44,53
			dc.b	10,19,23,32,39,45,52,54
			dc.b	20,22,33,38,46,51,55,60
			dc.b	21,34,37,47,50,56,59,61
			dc.b	35,36,48,49,57,58,62,63
de_zz_order:		dc.w	0,1,8,16,9,2,3,10
			dc.w	17,24,32,25,18,11,4,5
			dc.w	12,19,26,33,40,48,41,34
			dc.w	27,20,13,6,7,14,21,28
			dc.w	35,42,49,56,57,50,43,36
			dc.w	29,22,15,23,30,37,44,51
			dc.w	58,59,52,45,38,31,39,46
			dc.w	53,60,61,54,47,55,62,63

	ifne	ZZ_TRANSPOSED
zz_lines:
	; this is for dct over columns in x stage
			dc.b	1,1
			dc.b	2
			dc.b	3,3,3,3,3,3
			dc.b	4
			dc.b	5,5,5,5,5,5,5,5,5,5
			dc.b	6
			dc.b	7,7,7,7,7,7,7,7,7,7,7,7,7,7
			dc.b	8,8,8,8,8,8,8,8,8,8,8,8,8,8
			dc.b	8,8,8,8,8,8,8,8,8,8,8,8,8,8
			dc.b	8
	else ; ZZ_TRANSPOSED
zz_lines:
	; this is for dct over rows in x stage
			dc.b	1
			dc.b	2,2,2,2
			dc.b	3
			dc.b	4,4,4,4,4,4,4,4
			dc.b	5
			dc.b	6,6,6,6,6,6,6,6,6,6,6,6
			dc.b	7
			dc.b	8,8,8,8,8,8,8,8
			dc.b	8,8,8,8,8,8,8,8
			dc.b	8,8,8,8,8,8,8,8
			dc.b	8,8,8,8,8,8,8,8
			dc.b	8,8,8,8
	endc ; ZZ_TRANSPOSED



CreateYUVtoHiColorTable
			tst.l	YUVtoHiColorTable		;If table already exists, don't need to create
			bne	CreateYUVtoHiColorTable_ok	;it again.

			move.l	#SIZE_YUVtoHiColorTable,d0
			moveq	#0,d1
			CALLEXEC AllocMem
			move.l	d0,YUVtoHiColorTable
			beq	CreateYUVtoHiColorTable_error

			move.l	YUVtoHiColorTable,a1
			lea	CrTable,a2
			lea	CbTable,a3
			lea	512+Clamp256,a4

			moveq	#0,d1
hicolor_tablegen_loop
			move.l	d1,d2				;d2=[xxxxxxxx][xxxxxxuu][uuuuvvvv][vvyyyyyy]
			lsl.b	#2,d2				;d2=[xxxxxxxx][xxxxxxuu][uuuuvvvv][yyyyyy--]
			and.w	#$00fc,d2			;d2=[xxxxxxxx][xxxxxxuu][--------][yyyyyy--] = Y (6-bit)

			move.l	d1,d3
			lsr.w	#4,d3				;d3=[xxxxxxxx][xxxxxxuu][----uuuu][vvvvvvyy]
			and.w	#$00fc,d3			;d3=[xxxxxxxx][xxxxxxuu][--------][vvvvvv--] = Cr (6-bit)

			move.l	d1,d4				;d4=[xxxxxxxx][xxxxxxuu][uuuuvvvv][vvyyyyyy]
			lsr.l	#8,d4				;d4=[--------][xxxxxxxx][xxxxxxuu][uuuuvvvv]
			lsr.l	#2,d4				;d4=[--------][--xxxxxx][xxxxxxxx][uuuuuuvv]
			and.w	#$00fc,d4			;d4=[--------][--xxxxxx][--------][uuuuuu--] = Cb (6-bit)

			move.l	(a2,d3.w*4),d3			;d3=[Cr-G,Cr-R]
			move.l	(a3,d4.w*4),d4			;d4=[Cb-G,Cb-B]
			move.l	d4,d5
			add.w	d2,d3				;Red
			add.w	d2,d4				;Blue
			move.b	(a4,d3.w),d7			;d7=[--------][--------][--------][rrrrrrrr]

			lsl.l	#5,d7				;d7=[--------][--------][---rrrrr][rrr-----]
			swap	d3
			swap	d5
			add.w	d5,d3
			sub.w	d3,d2				;Green
			move.b	(a4,d2.w),d7			;d7=[--------][--------][---rrrrr][gggggggg]

			tst.b	hicolorformat			;rgb16pC
			bne.b	nohicolorbit16pC
								;d7=[--------][--------][---rrrrr][gggggggg]
			lsl.l	#6,d7				;d7=[--------][-----rrr][rrgggggg][gg------]
			move.b	(a4,d4.w),d7			;d7=[--------][-----rrr][rrgggggg][bbbbbbbb]			
			lsr.l	#3,d7				;d7=[--------][--------][rrrrrggg][gggbbbbb]
			rol.w	#8,d7				;d7=[--------][--------][gggbbbbb][rrrrrggg]
			bra	hicolorsetjump

nohicolorbit16pC:	cmp.b	#1,hicolorformat		;rgb16
			bne.b	nohicolorbit16
								;d7=[--------][--------][---rrrrr][gggggggg]
			lsl.l	#6,d7				;d7=[--------][-----rrr][rrgggggg][gg------]
			move.b	(a4,d4.w),d7			;d7=[--------][-----rrr][rrgggggg][bbbbbbbb]			
			lsr.l	#3,d7				;d7=[--------][--------][rrrrrggg][gggbbbbb]
			bra.b	hicolorsetjump

nohicolorbit16:		cmp.b	#2,hicolorformat		;rgb15pc
			bne.b	nohicolorbit15pC

								;d7=[--------][--------][---rrrrr][gggggggg]
			lsl.l	#5,d7				;d7=[--------][------rr][rrrggggg][ggg-----]
			move.b	(a4,d4.w),d7			;d7=[--------][-----rrr][rrgggggg][bbbbbbbb]			
			lsr.l	#3,d7				;d7=[--------][--------][-rrrrrgg][gggbbbbb]
			and.w	#%0111111111111111,d7
			rol.w	#8,d7				;d7=[--------][--------][gggbbbbb][-rrrrrgg]
			bra.b	hicolorsetjump

nohicolorbit15pC:	cmp.b	#3,hicolorformat		;rgb15
			bne.b	nohicolorbit15
								;d7=[--------][--------][---rrrrr][gggggggg]
			lsl.l	#5,d7				;d7=[--------][------rr][rrrggggg][ggg-----]
			move.b	(a4,d4.w),d7			;d7=[--------][-----rrr][rrgggggg][bbbbbbbb]			
			lsr.l	#3,d7				;d7=[--------][--------][-rrrrrgg][gggbbbbb]
			and.w	#%0111111111111111,d7
			bra.b	hicolorsetjump

nohicolorbit15:		cmp.b	#4,hicolorformat		;bgr16pc
			bne.b	nohicolorbitbgr16pc

								;d7=[--------][--------][---rrrrr][gggggggg]
			lsr.b	#2,d7				;d7=[--------][--------][---rrrrr][--gggggg]
			ror.w	#8,d7				;d7=[--------][--------][--gggggg][---rrrrr]
			lsl.b	#3,d7				;d7=[--------][--------][--gggggg][rrrrr---]
			swap	d7				;d7=[--gggggg][rrrrr---][--------][--------]
			lsl.l	#2,d7				;d7=[ggggggrr][rrr-----][--------][--------]

			move.b	(a4,d4.w),d7			;d7=[ggggggrr][rrr-----][--------][bbbbbbbb]
			lsr.b	#3,d7				;d7=[ggggggrr][rrr-----][--------][---bbbbb]
			rol.l	#8,d7				;d7=[rrr-----][--------][---bbbbb][ggggggrr]
			rol.l	#3,d7				;d7=[--------][--------][bbbbbggg][gggrrrrr]
			rol.w	#8,d7				;d7=[--------][--------][gggrrrrr][bbbbbggg]
			bra.b	hicolorsetjump


nohicolorbitbgr16pc:	cmp.b	#5,hicolorformat		;bgr15pC
			;bne	nohicolorbitbgr15pc
								;d7=[--------][--------][---rrrrr][gggggggg]
			lsr.b	#3,d7				;d7=[--------][--------][---rrrrr][---ggggg]
			ror.w	#8,d7				;d7=[--------][--------][---ggggg][---rrrrr]
			lsl.b	#3,d7				;d7=[--------][--------][---ggggg][rrrrr---]
			swap	d7				;d7=[---ggggg][rrrrr---][--------][--------]
			lsl.l	#3,d7				;d7=[gggggrrr][rr------][--------][--------]

			move.b	(a4,d4.w),d7			;d7=[gggggrrr][rr------][--------][bbbbbbbb]
			lsr.b	#3,d7				;d7=[gggggrrr][rr------][--------][--xbbbbb]
			rol.l	#8,d7				;d7=[rr------][--------][--xbbbbb][gggggrrr]
			rol.l	#2,d7				;d7=[--------][--------][xbbbbbgg][gggrrrrr]
			and.w	#%0111111111111111,d7
			rol.w	#8,d7				;d7=[--------][--------][gggrrrrr][xbbbbbgg]
			;bra	hicolorsetjump

hicolorsetjump:		move.w	d7,(a1,d1.l*2)
			
			addq.l	#1,d1
			cmp.l	#262144,d1
			bne.w	hicolor_tablegen_loop

CreateYUVtoHiColorTable_ok
			moveq	#1,d0
			rts
CreateYUVtoHiColorTable_error
			moveq	#0,d0
			rts


CreateYUVtoBGGRTable:
			tst.l	YUVtoBGGRTable		;If table already exists, don't need to create
			bne	CreateYUVtoBGGRTable_ok		;it again.
			move.l	#SIZE_YUVtoBGGRTable,d0
			moveq	#0,d1
			CALLEXEC AllocMem
			move.l	d0,YUVtoBGGRTable
			beq	CreateYUVtoBGGRTable_error
			move.l	d0,YUV_BG_Table			;YUV to BG table
			add.l	#SIZE_YUVtoBGGRTable/2,d0
			move.l	d0,YUV_GR_Table			;YUV to GR table

			move.l	YUV_BG_Table,a1
			move.l	YUV_GR_Table,a2
			lea	CrTable,a3
			lea	CbTable,a4
			lea	512+Clamp256,a5

			moveq	#0,d1
bggr_tablegen_loop
			move.l	d1,d2				;d2=[xxxxxxxx][xxxxxxuu][uuuuvvvv][vvyyyyyy]
			lsl.b	#2,d2				;d2=[xxxxxxxx][xxxxxxuu][uuuuvvvv][yyyyyy--]
			and.w	#$00fc,d2			;d2=[xxxxxxxx][xxxxxxuu][--------][yyyyyy--] = Y (6-bit)

			move.l	d1,d3
			lsr.w	#4,d3				;d3=[xxxxxxxx][xxxxxxuu][----uuuu][vvvvvvyy]
			and.w	#$00fc,d3			;d3=[xxxxxxxx][xxxxxxuu][--------][vvvvvv--] = Cr (6-bit)

			move.l	d1,d4				;d4=[xxxxxxxx][xxxxxxuu][uuuuvvvv][vvyyyyyy]
			lsr.l	#8,d4				;d4=[--------][xxxxxxxx][xxxxxxuu][uuuuvvvv]
			lsr.l	#2,d4				;d4=[--------][--xxxxxx][xxxxxxxx][uuuuuuvv]
			and.w	#$00fc,d4			;d4=[--------][--xxxxxx][--------][uuuuuu--] = Cb (6-bit)

			move.l	(a3,d3.w*4),d3			;d3=[Cr-G,Cr-R]
			move.l	(a4,d4.w*4),d4			;d4=[Cb-G,Cb-B]
			move.l	d4,d5
			add.w	d2,d3				;Red
			add.w	d2,d4				;Blue
			move.b	(a5,d3.w),d7			;d7=[--------][--------][--------][rrrrrrrr]
			lsl.l	#8,d7				;d7=[--------][--------][rrrrrrrr][--------]
			swap	d3
			swap	d5
			add.w	d5,d3
			sub.w	d3,d2				;Green
			move.b	(a5,d2.w),d7			;d7=[--------][--------][rrrrrrrr][gggggggg]
			ror.w	#8,d7				;d7=[--------][--------][gggggggg][rrrrrrrr]

			move.w	d7,(a2,d1.l*2)			;GR

			move.b	(a5,d4.w),d7			;d7=[--------][--------][gggggggg][bbbbbbbb]			
			ror.w	#8,d7				;d7=[--------][--------][bbbbbbbb][gggggggg]

			move.w	d7,(a1,d1.l*2)			;BG

			addq.l	#1,d1
			cmp.l	#262144,d1
			bne.w	bggr_tablegen_loop

CreateYUVtoBGGRTable_ok	moveq	#1,d0
			rts
CreateYUVtoBGGRTable_error
			moveq	#0,d0
			rts


;-------------------------------------------\\\
;-------------------------------------------///
	ifne	CODE_ALIGN
		CNOP    0,16
	endc

;--------------------------------- Render picture onto bitmap ----------------------------
;-----------------------------------------------------------------------------------------
mpr_LockBitMap		EQU	2
mpr_RenderBitMap	EQU	2+mpr_jsr_render-mpr_jsr_offsets
mpr_UnLockBitMap	EQU	2+mpr_jsr_unlock-mpr_jsr_offsets
;mpr_RenderBitMap	EQU	8
;mpr_UnLockBitMap	EQU	14


; A0/D0 - bitstream, A5=VDEC_BASE
mpr_RenderFrame:
		movem.l	d0/a0/a5,-(a7)
mpr_jsr_offsets	jsr	mpr_p96LockBitMap

mpr_jsr_render
		jsr	mpr_bgr24

mpr_jsr_unlock
		jsr	mpr_p96UnLockBitMap
		movem.l	(a7)+,d0/a0/a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96LockBitMapWin:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5
		move.l	p96WinPlayBitMap-VDEC_BASE(a5),a0
		lea	p96RenderInfo-VDEC_BASE(a5),a1
		moveq	#12,d0
		CALLP96	LockBitMap			;lock p96 bitmap
		move.l	d0,p96_bitmaplock-VDEC_BASE(a5)
		lea	p96RenderInfo-VDEC_BASE(A5),a0
		move.l	gri_Memory(a0),a1		;get bitmap base address
		move.l	a1,GfxMemBase-VDEC_BASE(A5)
		moveq	#0,d1
		move.w	gri_BytesPerRow(a0),d1
		move.l	d1,BitmapModulo-VDEC_BASE(a5)		;get modulo
		move.l	(sp)+,a5
;		bsr	mpr_p96UnLockBitMapWin2
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96UnLockBitMapWin:
;		rts
;mpr_p96UnLockBitMapWin2:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5
		move.l	p96WinPlayBitMap-VDEC_BASE(a5),a0
		move.l	p96_bitmaplock-VDEC_BASE(a5),d0
		CALLP96	UnlockBitMap
		move.l	p96WinPlayBitMap-VDEC_BASE(a5),a0	;src bitmap
		moveq	#0,d0			;src x
		moveq	#0,d1			;src y
		move.l	MainWindow-VDEC_BASE(a5),a2
		move.l	wd_RPort(a2),a1		;dest rastport
		moveq	#0,d2			;dest x
		move.b	wd_BorderLeft(a2),d2
		moveq	#0,d3			;dest y
		move.b	wd_BorderTop(a2),d3
		move.l	width(pc),d4		;size x
		move.l	height(pc),d5		;size y
		move.b	#$C0,d6			;minterm
		CALLGFX	BltBitMapRastPort	;blit image into window
		move.l	(sp)+,a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96LockBitMap:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5

		move.l	ScreenRastport-VDEC_BASE(a5),a0	;lock p96 bitmap
		move.l	rp_BitMap(a0),a0
		lea	p96RenderInfo-VDEC_BASE(A5),a1
		moveq	#12,d0
		CALLP96	LockBitMap			;lock p96 bitmap
		move.l	d0,p96_bitmaplock-VDEC_BASE(A5)
		lea	p96RenderInfo-VDEC_BASE(A5),a0
		move.l	gri_Memory(a0),a1		;get bitmap base address
		move.l	a1,GfxMemBase-VDEC_BASE(a5)
		moveq	#0,d1
		move.w	gri_BytesPerRow(a0),d1
		move.l	d1,BitmapModulo-VDEC_BASE(a5)	;get modulo

		move.l	(sp)+,a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96UnLockBitMap:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5
		move.l	ScreenRastport-VDEC_BASE(a5),a0		;unlock p96 bitmap
		move.l	rp_BitMap(a0),a0
		move.l	p96_bitmaplock-VDEC_BASE(a5),d0
		CALLP96	UnlockBitMap
		move.l	(sp)+,a5
mpr_DummyRTS:
		rts


mpr_p96LockBitMapDirect:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5

		move.l	PubScreen-VDEC_BASE(a5),a1
;		lea	sc_BitMap(a1),a0
		lea	sc_RastPort(a1),a0
		move.l	rp_BitMap(a0),a0

		lea	p96RenderInfo-VDEC_BASE(A5),a1
		moveq	#12,d0
		CALLP96	LockBitMap			;lock p96 bitmap
		move.l	d0,p96_bitmaplock-VDEC_BASE(A5)

		lea	p96RenderInfo-VDEC_BASE(A5),a0
		move.l	gri_Memory(a0),a1		;get bitmap base address
		moveq	#0,d7
		move.w	gri_BytesPerRow(a0),d7
	IFEQ APOLLO_YUYV
		moveq	#-4,d1
		add.l	d7,d1
		move.w	d1,2+mpr_rgbhicolor_ofst1
		move.w	d1,2+mpr_rgbhicolor_ofst2
	ENDC
		move.l	PubScreen-VDEC_BASE(a5),a0
		move.l	sc_Font(a0),a0
		move.w	ta_YSize(a0),d1

		move.l	MainWindow,a0
		
		tst.l	BORDERLESS_switch
		beq.s	.calcsize_borders
.calcsize_borderless
		moveq	#0,d0
		add.w	wd_TopEdge(a0),d0
		mulu.l	d7,d0
		add.l	d0,a1				
		moveq	#0,d0
		bra	.calcsize_done
.calcsize_borders
		moveq	#1,d0
		add.b	WinBorTop,d0
		add.w	wd_TopEdge(a0),d0
		add.w	d1,d0
		mulu.l	d7,d0
		add.l	d0,a1
		moveq	#0,d0
		move.b	WinBorLeft,d0
.calcsize_done
		add.w	wd_LeftEdge(a0),d0
		moveq	#0,d1
		move.b	PubScrDepth(pc),d1
		mulu	d1,d0
		add.l	d0,a1

		move.l	a1,GfxMemBase-VDEC_BASE(a5)
		move.l	d7,BitmapModulo-VDEC_BASE(a5)	;get modulo

		move.l	(sp)+,a5
		rts

mpr_p96UnLockBitMapDirect:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5

		move.l	PubScreen-VDEC_BASE(a5),a1
		;lea	sc_BitMap(a1),a0
		lea	sc_RastPort(a1),a0
		move.l	rp_BitMap(a0),a0

		move.l	GfxMemBase-VDEC_BASE(a5),d0
		CALLP96	UnlockBitMap

		move.l	(sp)+,a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96LockBitMapPIP:
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5

		move.l	p96PIPBitmap-VDEC_BASE(a5),a0
		lea	p96RenderInfo-VDEC_BASE(a5),a1
		moveq	#12,d0
		CALLP96	LockBitMap
		move.l	d0,p96_bitmaplock-VDEC_BASE(a5)
		lea	p96RenderInfo-VDEC_BASE(a5),a1
		move.l	gri_Memory(a1),a2			;get bitmap base address
		move.l	a2,GfxMemBase-VDEC_BASE(a5)
		move.l	(sp)+,a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

mpr_p96UnLockBitMapPIP
		move.l	a5,-(sp)
		lea	VDEC_BASE,a5

		move.l	p96PIPBitmap-VDEC_BASE(a5),a0
		move.l	p96_bitmaplock-VDEC_BASE(a5),d0
		CALLP96	UnlockBitMap

		move.l	(sp)+,a5
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

;Multiply quantization matrix with DFT->DCT conversion constants:
;----------------------------------------------------------------
DFTtoDCTQuantAdjust_intra
			lea	cos_constants(pc),a3
			lea	intra_quant_matrix(pc),a4
			moveq	#0,d1
.DFTtoDCT_loop		move.l	d1,d2
			move.l	d1,d3
			and.w	#7,d2			;d2 is x location in matrix
			lsr.w	#3,d3			;d3 is y location in matrix
			move.w	(a3,d2.w*2),d2		;Cos(x*pi/16)
			move.w	(a3,d3.w*2),d3		;Cos(y*pi/16)
			muls.w	d3,d2
			asr.l	#8,d2
			bcc.b	.DFT_conv_round		;Do some rounding...
			addq.l	#1,d2
.DFT_conv_round		move.w	(a4,d1.w*2),d3
			muls.w	d3,d2
			move.w	d2,(a4,d1.w*2)
			addq.l	#1,d1
			cmp.b	#64,d1
			bne.b	.DFTtoDCT_loop

.DFTtoDCT_done		rts

DFTtoDCTQuantAdjust_nonintra
			lea	cos_constants(pc),a3
			lea	nonintra_quant_matrix,a4
			moveq	#0,d1
.DFTtoDCT_loop		move.l	d1,d2
			move.l	d1,d3
			and.w	#7,d2			;d2 is x location in matrix
			lsr.w	#3,d3			;d3 is y location in matrix
			move.w	(a3,d2.w*2),d2		;Cos(x*pi/16)
			move.w	(a3,d3.w*2),d3		;Cos(y*pi/16)
			muls.w	d3,d2
			asr.l	#8,d2
			bcc.b	.DFT_conv_round		;Do some rounding...
			addq.l	#1,d2
.DFT_conv_round		move.w	(a4,d1.w*2),d3
			muls.w	d3,d2
			move.w	d2,(a4,d1.w*2)
			addq.l	#1,d1
			cmp.b	#64,d1
			bne.b	.DFTtoDCT_loop
.DFTtoDCT_done		rts


;Convert quantization matrix to zigzagged order, for on-the-fly dequantization:
;------------------------------------------------------------------------------
QuantToZZ_intra		lea	intra_quant_matrix,a3
			lea	intra_quant_matrix_zz,a4
			lea	zz_order,a5
			moveq	#0,d1
.convert_to_zz:
			moveq	#0,d2
			move.b	(a5,d1.w),d2			;Current offset to zz order matrix
			move.w	(a3,d1.w*2),d3
			addq.l	#1,d1
			move.w	d3,(a4,d2.w*2)
			cmp.b	#64,d1
			bne.b	.convert_to_zz
			rts

QuantToZZ_nonintra	lea	nonintra_quant_matrix,a3
			lea	nonintra_quant_matrix_zz,a4
			lea	zz_order(pc),a5
			moveq	#0,d1
.convert_to_zz:
			moveq	#0,d2
			move.b	(a5,d1.w),d2			;Current offset to zz order matrix
			move.w	(a3,d1.w*2),d3
			addq.l	#1,d1
			move.w	d3,(a4,d2.w*2)
			cmp.b	#64,d1
			bne.b	.convert_to_zz
			rts


GenerateAllTables	MakeLookupTable	MB_address,2048
			MakeLookupTable	MB_type_P,64
			MakeLookupTable	MB_type_B,64
			MakeLookupTable	block_pattern,512
			MakeLookupTable	motion_vector,2048
			MakeLookupTable	DCT_size_lum,128
			MakeLookupTable	DCT_size_chrom,256
			lea	huff_DCT_coeff,a3

			move.l	lookup_DCT_coeff,a4
			move.l	#32768,a5
			bsr.w	GenerateDCT_VLCTable
			rts

;Calculate screen size values, etc.
;----------------------------------
CalcScreenStuff

		movem.l	a0/d0,-(a7)

		tst.l	width(pc)
		beq	ScreenSizeError
		tst.l	height(pc)
		beq	ScreenSizeError

		move.l	width(pc),d1
		and.b	#$f0,d1
		cmp.l	width(pc),d1
		beq.b	y_bitmap_width_ok
		add.l	#16,d1
y_bitmap_width_ok:
		move.l	d1,y_bitmap_width
		lsr.l	#1,d1
		move.l	d1,c_bitmap_width

;Fill in idct_modulo_add table
		lea	idct_modulo_add,a1
		move.l	y_bitmap_width(pc),d1
		move.l	d1,(a1)+
		move.l	d1,(a1)+
		move.l	d1,(a1)+
		move.l	d1,(a1)+
		move.l	c_bitmap_width(pc),d1
		move.l	d1,(a1)+
		move.l	d1,(a1)+

		move.l	height(pc),d1
		and.b	#$f0,d1
		cmp.l	height(pc),d1
		beq.b	bitmap_hgt_ok
		add.l	#16,d1
bitmap_hgt_ok	move.l	d1,y_bitmap_height
		lsr.l	#1,d1
		move.l	d1,c_bitmap_height

		move.l	y_bitmap_width(pc),d1
		move.l	y_bitmap_height(pc),d2
		mulu.w	d2,d1
		move.l	d1,d3
		move.l	d3,y_bitmap_size
		lsr.l	#2,d1
		move.l	d1,c_bitmap_size
		lsl.l	#1,d1
		add.l	d1,d3
		move.l	d3,total_bitmap_size

		move.l	y_bitmap_width(pc),d1
		lsr.l	#4,d1			;Calculate no. of macroblocks per row
		move.l	d1,MB_x_total		;Number of macroblocks per row
		move.l	y_bitmap_height(pc),d2
		lsr.l	#4,d2			;macroblock height
		bcc.b	MB_y_ok			;If half macroblock
		addq.l	#1,d2
MB_y_ok		mulu.w	d1,d2			;MB_x * MB_y
		move.l	d2,MB_total		;Total (maximum) number of macroblocks per picture
		lsl.l	#8,d2
		move.l	d2,max_pic_size

;Calculate macroblock offset table:
;----------------------------------
		move.l	y_bitmap_size(pc),d1
		move.l	y_bitmap_width(pc),d2
		mulu.w	#15,d2
		sub.l	d2,d1
		sub.l	#16,d1
		move.l	d1,y_MB_max_addr

		lea	MB_y_OffsetTable,a1
		lea	MB_c_OffsetTable,a2
		move.l	MB_total,d1			;d1 is macroblock address (count)
mb_table_loop
		move.l	MB_x_total,d7
		move.l	d1,d2
		move.l	d1,d4
		divu.l	d7,d2				;d2 = mb_row = mb_width / mb_address
		move.l	d2,d3
		mulu.l	d7,d3
		sub.l	d3,d4				;d1 = mb_column = mb_width % mb_address
		move.l	y_bitmap_width(pc),d7
		mulu.l	d7,d2
		lsl.l	#4,d2				;d2 = row start address in y_bitmap
		lsl.l	#4,d4				;d1 = column start address in y_bitmap
		move.l	d4,d3
		add.l	d2,d4				;d4 = total macroblock offset in y_bitmap
		move.l	d4,(a1,d1.w*4)			;offsets for y_bitmap
		lsr.l	#2,d2
		lsr.l	#1,d3
		add.l	d2,d3				;d3 = total macroblock offset in c_bitmaps
		move.l	d3,(a2,d1.w*4)			;offsets for c_bitmaps

		subq.l	#1,d1
		bne.b	mb_table_loop

		move.l	#1,result
		bra.b	ScreenCalcDone

ScreenSizeError	OUTTXT	msg_ScreenSizeError
		clr.l	result

ScreenCalcDone	movem.l	(a7)+,a0/d0
		rts


AllocTables
		movem.l	a0/d0,-(a7)

		move.l	#SIZE_LookupTables,d0
		moveq	#0,d1
		CALLEXEC AllocMem
		move.l	d0,Addr_LookupTables		;single allocation of all lookup tables
		beq.w	TableAllocError

		move.l	d0,lookup_MB_address
		add.l	#SIZE_MB_address,d0		;location of next table
		move.l	d0,lookup_MB_type_P
		add.l	#SIZE_MB_type_P,d0
		move.l	d0,lookup_MB_type_B
		add.l	#SIZE_MB_type_B,d0
		move.l	d0,lookup_block_pattern
		add.l	#SIZE_block_pattern,d0
		move.l	d0,lookup_motion_vector
		add.l	#SIZE_motion_vector,d0
		move.l	d0,lookup_DCT_size_lum
		add.l	#SIZE_DCT_size_lum,d0
		move.l	d0,lookup_DCT_size_chrom
		add.l	#SIZE_DCT_size_chrom,d0
		move.l	d0,d1
		add.l	#SIZE_DCT_coeff/2,d1
		move.l	d1,lookup_DCT_coeff		;base of dct_coeff is in the middle of table (signed table)
		add.l	#SIZE_DCT_coeff,d0
		move.l	d0,d1
		add.l	#SIZE_convert_to_bitmap/2,d1
		move.l	d1,convert_to_bitmap		;base of convert_to_bitmap is in the middle of table
		add.l	#SIZE_convert_to_bitmap,d0
		move.l	d0,d1
		add.l	#SIZE_predict_clamp/2,d1
		move.l	d1,predict_clamp
		;add.l	#SIZE_predict_clamp,d0

TableAllocOK	move.l	#1,result
		bra.b	TableAllocReturn

TableAllocError	bsr	MemAllocError
		clr.l	result

TableAllocReturn	movem.l	(a7)+,a0/d0
		rts

; clear row after last "valid" row in chroma components as safety buffer when 
; chroma vector adjustment yields invalid inputs for motion compensation
; consequence: occasionally less colorful in the last row - but no speed loss (!)
;d0 - ptr
;d1 - chroma size
;d2 - line width
Clear_Chroma:
		movem.l	d2/a0,-(sp)

		move.l	d0,a0
		adda.l	d1,a0
		lsr.l	#2,d2
		subq.l	#1,d2
.clr
		move.l	#$80808080,(a0)+
		dbf	d2,.clr

		movem.l	(sp)+,d2/a0
		rts

;Allocate Bitmap Buffers
;-----------------------
AllocBitmaps:		movem.l	a0/a2/d0,-(a7)
;FrameBufferPointers:		ds.l	3	; allocated pointers, the aligned ones are not suitable for FreeMem
;FrameBufferAllocSize:		dc.l	0

;Allocate I-Buffer
			moveq	#FRAMEBUFFER_ALIGN*3,d0
			add.l	c_bitmap_width(pc),d0		;allocate two chroma lines more (one Cb, one Cr) to be filled with chroma zero (128)
			lsl.l	#1,d0

			add.l	total_bitmap_size,d0
			move.l	d0,FrameBufferAllocSize		;reserve some space in FrameBuffer pointers
			lea	FrameBufferPointers,a2
			clr.l	(a2)				;alloc size is set, make sure we've got
			clr.l	4(a2)				;empty pointers in case of alloc failure
			clr.l	8(a2)

			move.l	#MEMF_CLEAR,d1
			CALLEXEC AllocMem
			tst.l	d0
			beq	BitMapAllocError
			move.l	d0,(a2)

			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer1

			add.l	y_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer1_Cb

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma

			add.l	c_bitmap_width(pc),d0

			add.l	c_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer1_Cr

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma

;Allocate P-Buffer
			move.l	FrameBufferAllocSize,d0	;move.l	total_bitmap_size,d0
			move.l	#MEMF_CLEAR,d1
			CALLEXEC AllocMem
			tst.l	d0
			beq	BitMapAllocError
			move.l	d0,4(a2)

			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer2

			add.l	y_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer2_Cb

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma

			add.l	c_bitmap_width(pc),d0
			add.l	c_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer2_Cr

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma
;Allocate B-Buffer
			move.l	FrameBufferAllocSize,d0	;move.l	total_bitmap_size,d0
			move.l	#MEMF_CLEAR,d1
			CALLEXEC AllocMem
			tst.l	d0
			beq	BitMapAllocError
			move.l	d0,8(a2)

			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer3

			add.l	y_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer3_Cb

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma

			add.l	c_bitmap_width(pc),d0
			add.l	c_bitmap_size(pc),d0
			ALIGN_D0				;same as ALIGN_PTR d0
			move.l	d0,FrameBuffer3_Cr

			move.l	c_bitmap_size(pc),d1
			move.l	c_bitmap_width(pc),d2
			bsr	Clear_Chroma

BitMapAllocOK		move.l	#1,result
			bra.b	BitMapAllocDone

BitMapAllocError	clr.l	result
			bsr	MemAllocError

BitMapAllocDone		movem.l	(a7)+,a0/a2/d0
			rts

;Hardcode the bitmap width offsets into the idct routine...
;----------------------------------------------------------
HardcodeIDCTOffsets:

		move.l	y_bitmap_width(pc),d1
		lsl.l	#3,d1
		move.l	d1,add_8_rows

		move.l	y_bitmap_width(pc),d1		;Relative offset calculation table for y blocks
		lsl.l	#3,d1
		move.l	d1,y_block_offset_table+8
		addq.l	#8,d1
		move.l	d1,y_block_offset_table+12

	;--------------------------------------------------------------------
	; buffered mode MC: perform operation to temporary buffer
		lea	mc_offsets_buffered,a1

	; the blockoffs will be added after processing of an 8x8 block
	; after one 8x8 block, the source location is at the top left edge of 
	; the next 8x8 block below, so the first offset will move 8 lines up and 
	; 8 pixel right to the top edge of the top/right block same goes for the 
	; other blocks

		moveq	#8,d2
		move.l	y_bitmap_width(pc),d1		;
		lsl.l	#3,d1				;8 lines
		sub.l	d1,d2				;8 lines up, 8 right
		move.w	d2,MC_Y_BLOCKOFF0(a1)		;after Block 0 at Block 2, move to Block 1
		move.w	d2,MC_Y_BLOCKOFF2(a1)		;after Block 2 below block 2, move to Block 3
		moveq	#-8,d2
		move.w	d2,MC_Y_BLOCKOFF1(a1)		;after Block 1 at Block 3, move left to Block 2
		clr.w	MC_Y_BLOCKOFF3(a1)		;nothing to be done after block 3

		clr.w	MC_Y_LINESTRIDE0(a1)		;no additional destination location jump in buffered mode
		clr.w	MC_Y_LINESTRIDE1(a1)		;these are per line, after 8 pixels
		clr.w	MC_Y_LINESTRIDE2(a1)
		clr.w	MC_Y_LINESTRIDE3(a1)
		clr.w	MC_C_LINESTRIDE(a1)

		clr.w	MC_Y_DESTOFF0(a1)		;no additional destination location jump in buffered mode
		clr.w	MC_Y_DESTOFF1(a1)		;these are per block, after each 8x8 block
		clr.w	MC_Y_DESTOFF2(a1)
		clr.w	MC_Y_DESTOFF3(a1)

		move.l	block_buffer_tmp_y1,a2
		move.l	a2,MC_DestPTR_Y(a1)
		lea	256(a2),a2
		move.l	a2,MC_DestPTR_Cb(a1)
		lea	64(a2),a2
		move.l	a2,MC_DestPTR_Cr(a1)

	;--------------------------------------------------------------------

	; direct mode MC: perform operation to framebuffer
		lea	mc_offsets_direct,a1

		;input:  same as in buffered mode
		;output: same as input (here) 
		moveq	#8,d2
		move.l	y_bitmap_width(pc),d1		;
		lsl.l	#3,d1				;8 lines
		sub.l	d1,d2				;8 lines up, 8 right
		move.w	d2,MC_Y_BLOCKOFF0(a1)		;after Block 0 at Block 2, move to Block 1
		move.w	d2,MC_Y_DESTOFF0(a1)		;first block to second block
		move.w	d2,MC_Y_BLOCKOFF2(a1)		;after Block 2 below block 2, move to Block 3
		move.w	d2,MC_Y_DESTOFF2(a1)		;first block to second block
		moveq	#-8,d2
		move.w	d2,MC_Y_BLOCKOFF1(a1)		;after Block 1 at Block 3, move left to Block 2
		move.w	d2,MC_Y_DESTOFF1(a1)		;first block to second block
		clr.w	MC_Y_BLOCKOFF3(a1)		;nothing to be done after block 3
		clr.w	MC_Y_DESTOFF3(a1)

		moveq	#-8,d1
		add.l	y_bitmap_width(pc),d1		;go to next line in destination after 8x1
		move.w	d1,MC_Y_LINESTRIDE0(a1)		;
		move.w	d1,MC_Y_LINESTRIDE1(a1)		;
		move.w	d1,MC_Y_LINESTRIDE2(a1)		;
		move.w	d1,MC_Y_LINESTRIDE3(a1)		;

		moveq	#-8,d1
		add.l	c_bitmap_width(pc),d1
		move.w	d1,MC_C_LINESTRIDE(a1)
	;--------------------------------------------------------------------


	ifne	APOLLO_DCTZERO
		lea	dc_y_idct1A(pc),a1
		move.l	y_bitmap_width(pc),d1
		move.l	d1,d2

		move.w	d1,4(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct2A-dc_y_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct3A-dc_y_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct4A-dc_y_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct5A-dc_y_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct6A-dc_y_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_y_idct7A-dc_y_idct1A(a1)

		lsr.l	#1,d2
		move.l	d2,d1

		lea	dc_c_idct1A(pc),a1
		move.w	d1,4(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct2A-dc_c_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct3A-dc_c_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct4A-dc_c_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct5A-dc_c_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct6A-dc_c_idct1A(a1)
		add.l	d2,d1	
		move.w	d1,4+dc_c_idct7A-dc_c_idct1A(a1)

	else
		moveq	#4,d3
		move.l	y_bitmap_width(pc),d1
		move.l	d1,d2
		move.w	d3,2+dc_y_idct1
		move.w	d1,2+dc_y_idct2
		move.w	d1,2+dc_y_idct3
		add.w	d3,2+dc_y_idct3
		add.w	d2,d1
		move.w	d1,2+dc_y_idct4
		move.w	d1,2+dc_y_idct5
		add.w	d3,2+dc_y_idct5
		add.w	d2,d1
		move.w	d1,2+dc_y_idct6
		move.w	d1,2+dc_y_idct7
		add.w	d3,2+dc_y_idct7
		add.w	d2,d1
		move.w	d1,2+dc_y_idct8
		move.w	d1,2+dc_y_idct9
		add.w	d3,2+dc_y_idct9
		add.w	d2,d1
		move.w	d1,2+dc_y_idct10
		move.w	d1,2+dc_y_idct11
		add.w	d3,2+dc_y_idct11
		add.w	d2,d1
		move.w	d1,2+dc_y_idct12
		move.w	d1,2+dc_y_idct13
		add.w	d3,2+dc_y_idct13
		add.w	d2,d1
		move.w	d1,2+dc_y_idct14
		move.w	d1,2+dc_y_idct15
		add.w	d3,2+dc_y_idct15

		moveq	#4,d3
		move.l	c_bitmap_width(pc),d1
		move.l	d1,d2
		move.w	d3,2+dc_c_idct1
		move.w	d1,2+dc_c_idct2
		move.w	d1,2+dc_c_idct3
		add.w	d3,2+dc_c_idct3
		add.w	d2,d1
		move.w	d1,2+dc_c_idct4
		move.w	d1,2+dc_c_idct5
		add.w	d3,2+dc_c_idct5
		add.w	d2,d1
		move.w	d1,2+dc_c_idct6
		move.w	d1,2+dc_c_idct7
		add.w	d3,2+dc_c_idct7
		add.w	d2,d1
		move.w	d1,2+dc_c_idct8
		move.w	d1,2+dc_c_idct9
		add.w	d3,2+dc_c_idct9
		add.w	d2,d1
		move.w	d1,2+dc_c_idct10
		move.w	d1,2+dc_c_idct11
		add.w	d3,2+dc_c_idct11
		add.w	d2,d1
		move.w	d1,2+dc_c_idct12
		move.w	d1,2+dc_c_idct13
		add.w	d3,2+dc_c_idct13
		add.w	d2,d1
		move.w	d1,2+dc_c_idct14
		move.w	d1,2+dc_c_idct15
		add.w	d3,2+dc_c_idct15
	endc
		CALLEXEC CacheClearU

		rts

;Initialise E-Clock timing...
;---------------------------------------------------------------------------------------------
EClockInit
		moveq	#0,d3			;total count (for average calc)
		clr.l	e_clock_correction	;If called multiple times, make sure correct is 0.
		move.l	#256-1,d2
timer_loop	TIMERSTART
		TIMERSTOP
		move.l	e_clock_time,d1
		add.l	d1,d3
		dbf	d2,timer_loop
		asr.l	#8,d3
		bcc.b	e_correct_done
		addq.l	#1,d3
e_correct_done	move.l	d3,e_clock_correction

		move.l	e_count_rate,d1
		move.l	d1,d2
		moveq.l	#16,d3
		lsr.l	d3,d1
		lsl.l	d3,d2
		move.l	vid_rate,d3
		beq.b	.skipdiv
		divu.l	d3,d1:d2
.skipdiv	move.l	d2,frame_time

		move.l	e_count_rate,d1	;time for 1 sec.
		divu.l	#1000,d1			;time for 1 millisec (can be useful sometimes)
		move.l	d1,e_clock_millisec

		move.l	frame_time,d1
		neg.l	d1
		move.l	d1,max_lag		;max. allowed lag = -100%

		move.l	frame_time,d1
		lsr.l	#2,d1
		move.l	d1,max_lead

		rts

;Calculate total playback time in seconds from e-time measured:
;--------------------------------------------------------------
CalcTotalSeconds
		movem.l	d1-d7,-(a7)

	ifne	0
	; debug: print actual time, audio time and nframes
		movem.l	actual_time,d2-d3
;		OUTNUM64 d2,d3
		OUTHEX	d2
		OUTTXT	SPACE
		OUTHEX	d3
		OUTTXT	SPACE

		movem.l	audio_time,d2-d3
;		OUTNUM64 d2,d3
		OUTTXT	SPACE
		OUTHEX	d2
		OUTTXT	SPACE
		OUTHEX	d3
		OUTTXT	SPACE
		OUTTXT	SPACE
		move.l	AudioSamplesPlayed,d2
		OUTHEX	d2
	endc
		move.l	actual_time,d2
		move.l	4+actual_time,d3
		move.l	e_count_rate,d4
		beq.b	.h_nodivzero
		divu.l	d4,d2:d3
.h_nodivzero	move.l	d3,time_seconds_h

		move.l	actual_time,d2
		lsl.l	#8,d2
		lsl.l	#8,d2
		move.w	4+actual_time,d2
		move.l	4+actual_time,d3
		lsl.l	#8,d3
		lsl.l	#8,d3
		move.l	e_count_rate,d4
		beq.b	.l_nodivzero
		divu.l	d4,d2:d3
.l_nodivzero	swap	d3
		clr.w	d3
		move.l	d3,time_seconds_l

		movem.l	(a7)+,d1-d7
		rts

;Calculate average fps from time_seconds and pictures_total:
;-----------------------------------------------------------
CalcAverageFPS

		movem.l	d1-d7,-(a7)

		move.l	time_seconds_h,d2
		move.l	time_seconds_l,d3
		move.l	pictures_total,d4
		beq.b	.h_nodivzero1
		divu.l	d4,d2:d3
.h_nodivzero1	move.l	#1,d2
		moveq	#0,d4
		tst.l	d3
		beq.b	.h_nodivzero2
		divu.l	d3,d2:d4
.h_nodivzero2	move.l	d4,average_fps_h

		move.l	time_seconds_h,d2
		move.l	time_seconds_l,d3
		move.l	pictures_total,d4
		beq.b	.l_nodivzero1
		divu.l	d4,d2:d3
.l_nodivzero1	move.l	#65536,d2
		moveq	#0,d4
		tst.l	d3
		beq.b	.l_nodivzero2
		divu.l	d3,d2:d4
.l_nodivzero2	swap	d4
		clr.w	d4
		move.l	d4,average_fps_l

CalcDisplayedFPS
		move.l	time_seconds_h,d2
		move.l	time_seconds_l,d3
		move.l	pictures_played,d4
		beq.b	.h_nodivzero1
		divu.l	d4,d2:d3
.h_nodivzero1	move.l	#1,d2
		moveq	#0,d4
		tst.l	d3
		beq.b	.h_nodivzero2
		divu.l	d3,d2:d4
.h_nodivzero2	move.l	d4,displayed_fps_h

		move.l	time_seconds_h,d2
		move.l	time_seconds_l,d3
		move.l	pictures_played,d4
		beq.b	.l_nodivzero1
		divu.l	d4,d2:d3
.l_nodivzero1	move.l	#65536,d2
		moveq	#0,d4
		tst.l	d3
		beq.b	.l_nodivzero2
		divu.l	d3,d2:d4
.l_nodivzero2	swap	d4
		clr.w	d4
		move.l	d4,displayed_fps_l


		movem.l	(a7)+,d1-d7
		rts

		IFNE	DEBUG

;Display quantization matrices
;-----------------------------
DisplayQuantMatrices
		OUTTXT	IntQuantMtxMsg				;Display intra matrix
		lea	intra_quant_matrix(pc),a2
		moveq.l	#0,d0
disp_int_mtx	moveq.l	#0,d1
		move.w	(a2,d0.w*2),d1
		OUTDECS	d1
		OUTTXT	SPACE
		addq.l	#1,d0
		move.l	d0,d2
		lsr.l	#3,d2
		lsl.l	#3,d2
		cmp.l	d0,d2
		bne.b	noCR_int_mtx
		OUTTXT	RETURN
noCR_int_mtx	cmp.b	#64,d0
		bne.b	disp_int_mtx
		OUTTXT	NonIntQuantMtxMsg			;Display non-intra matrix
		lea	nonintra_quant_matrix(pc),a2
		moveq.l	#0,d0
disp_nonint_mtx	moveq.l	#0,d1
		move.w	(a2,d0.w*2),d1
		OUTDECS	d1
		OUTTXT	SPACE
		addq.l	#1,d0
		move.l	d0,d2
		lsr.l	#3,d2
		lsl.l	#3,d2
		cmp.l	d0,d2
		bne.b	noCR_nonint_mtx
		OUTTXT	RETURN
noCR_nonint_mtx	cmp.b	#64,d0
		bne.b	disp_nonint_mtx
		rts

		ENDC


GenerateBitmapConversionTable
		move.l	convert_to_bitmap,a2
		move.l	predict_clamp,a3
		move.w	#-2048,d1
bitmapconv_loop	move.w	d1,d2			;take offset
		tst.b	GrayMode
		beq.b	bmct_notgray
		sub.w	#16,d2			;if grayscale, lum-16
bmct_notgray	cmp.w	#127,d2
		bgt.b	.too_high
		cmp.w	#-128,d2
		blt.b	.too_low
		add.b	#128,d2
		bra.b	pixel_ok
.too_high	move.b	#255,d2
		bra.b	pixel_ok
.too_low	moveq	#0,d2
pixel_ok	move.b	d2,(a2,d1.w)
		move.w	d1,d2
		bmi.b	.too_low
		cmp.w	#255,d2
		bgt.b	.too_high
		bra.b	pixel2_ok
.too_high	move.b	#255,d2
		bra.b	pixel2_ok
.too_low	moveq	#0,d2
pixel2_ok	move.b	d2,(a3,d1.w)
		addq.w	#1,d1
		cmp.w	#2048,d1
		blt.b	bitmapconv_loop
		rts

LoadCustomIntraMatrix
		lea	intra_quant_matrix(pc),a2
		moveq	#0,d2
int_mtx_load	GET8	d1
		extb.l	d1
		;move.w	d1,(a2,d2*2)
		addq.l	#1,d2
		cmp.b	#64,d2
		bne.b	int_mtx_load
		;bsr	DFTtoDCTQuantAdjust_intra
		;bsr	QuantToZZ_intra
		rts

LoadCustomNonIntraMatrix
		lea	nonintra_quant_matrix(pc),a2
		moveq.l	#0,d2
nonint_mtx_load	GET8	d1
		extb.l	d1
		;move.w	d1,(a2,d2*2)
		addq.l	#1,d2
		cmp.b	#64,d2
		bne.b	nonint_mtx_load
		;bsr	DFTtoDCTQuantAdjust_nonintra
		;bsr	QuantToZZ_nonintra
		rts

;Create Palette Structure before opening the screen...
;-----------------------------------------------------
CreatePalette	lea	Palette,a0
		move	#256,d7				;full AGA palette
		move	d7,(a0)+			;number of colors
		clr.w	(a0)+				;first index in the palette

		subq	#1,d7
		moveq	#0,d6
.colsloop:
		move.l	d6,(a0)+
		move.l	d6,(a0)+
		move.l	d6,(a0)+
		add.l	#$01010101,d6
		dbf	d7,.colsloop
		
		clr.l	(a0)				;Close Structure
		rts


GenerateYUVConversionTable
		lea	CbTable,a1
		lea	CrTable-CbTable(a1),a2
		moveq	#0,d1
YUVLoop
		move.w	d1,d2
		sub.w	#128,d2
		move.w	d2,d3
		move.w	d2,d4
		move.w	d2,d5
		muls.w	#FIX_0_3359,d2
		asr.l	#8,d2
		muls.w	#FIX_1_7337,d3
		asr.l	#8,d3
		muls.w	#FIX_0_6985,d4
		asr.l	#8,d4
		muls.w	#FIX_1_3711,d5
		asr.l	#8,d5
		move.w	d2,(a1,d1.w*4)
		move.w	d3,2(a1,d1.w*4)
		move.w	d4,(a2,d1.w*4)
		move.w	d5,2(a2,d1.w*4)

		addq.b	#1,d1
		bne.b	YUVLoop

		lea	512+Clamp256-CbTable(a1),a1
		move.w	#-256,d1
ClampLoop
		move.w	d1,d2
		sub.w	#16,d2
;		muls.w	#FIX_1_164,d2
;		asr.l	#8,d2
		bmi.b	Minimize
		cmp.w	#255,d2
		ble.b	OK
		move.b	#255,d2
		bra.b	OK
Minimize	moveq	#0,d2
OK		move.b	d2,(a1,d1.w)

		addq.w	#1,d1
		cmp.w	#512,d1
		bne.b	ClampLoop

		rts


*-------------------------------------------------------------------*
*------------------- VLC Lookup Table Generator --------------------*
*-------------------------------------------------------------------*
*------- Inputs: a3 - Huffman Table Address (Source)         -------*
*-------         a4 - VLC Lookup Table Address (Destination) -------*
*-------         a5 - No. of VLC Lookup Table Entries        -------*
*-------------------------------------------------------------------*

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

GenerateVLCTable:
		movem.l	d0-d7/a3-a6,-(a7)
		move.w	(a3)+,d7		;number of entries in huffman table
		move.w	(a3)+,d6		;maximum vlc length
		move.l	a3,a6			;a6 = start of actual table contents
		moveq	#0,d0			;d0 = lookup table offset variable
.lookup_loop	move.l	a6,a3			;reset huffman table pointer to first VLC.
		move.w	d7,d1			;d1 = vlc check loop counter (from no. of entries in huffman table)
.vlc_check_loop	move.w	(a3)+,d2		;vlc code
		move.b	(a3)+,d3		;length of vlc code
		move.b	d6,d4			;get max vlc length
		sub.b	d3,d4			;number of shifts in d4
		lsl.w	d4,d2			;make vlc code start from MSB.
		move.l	d0,d5			;A copy is made of lookup table offset variable d0.
		lsr.w	d4,d5			;Every bit that's not used by the current vlc is
		lsl.w	d4,d5			;cleared, so that a match can be identified.
		cmp.w	d2,d5			;Check VLC with current d0 (only the same bits as actual VLC)
		beq.b	.got_vlc_match
		addq.l	#1,a3			;Ignore vlc data
		subq.l	#1,d1
		bne.b	.vlc_check_loop		;Loop until all vlc codes have been checked
		clr.w	(a4)+			;If no matches were found, fill table with blank
		bra.b	.next_vlc_addr		;spaces and start next address in lookup table...
.got_vlc_match	move.b	(a3)+,(a4)+		;if match was found, put data & bitlength into
		move.b	-2(a3),(a4)+		;lookup table.
.next_vlc_addr	addq.l	#1,d0
		cmp.w	a5,d0
		blt.b	.lookup_loop
		movem.l	(a7)+,d0-d7/a3-a6
		rts

*-------------------------------------------------------------------*
*------------ VLC Lookup Table Generator for dct_coeff -------------*
*-------------------------------------------------------------------*

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

GenerateDCT_VLCTable
		movem.l	d0-d7/a3-a6,-(a7)
		move.w	(a3)+,d7		;no. of entries in d7
		move.w	(a3)+,d6		;maximum vlc length in d6
		move.l	a3,a6			;start of huffman data in a6
		move.l	#-32768,d0
.lookupDCTloop	move.l	a6,a3			;start from 1st vlc code...
		moveq	#0,d1
		move.w	d7,d1
.vlc_DCT_loop	move.w	(a3)+,d2		;vlc code
		move.b	(a3)+,d3		;length of vlc code
		move.b	d6,d4			;get max vlc length
		sub.b	d3,d4			;number of shifts in d4
		lsl.w	d4,d2			;make vlc code start from MSB.
		move.l	d0,d5
		lsr.w	d4,d5
		lsl.w	d4,d5			;mask excess bits with lsr/lsl
		cmp.w	d2,d5			;Compare vlc from table with vlc of lookup address
		beq.b	.DCT_vlc_match
		addq.l	#3,a3			;Ignore vlc data (skip both run and level)
		subq	#1,d1
		bne.b	.vlc_DCT_loop		;Loop until all vlc codes have been checked
		bra.b	.DCT_vlc_next
.DCT_vlc_match	move.b	(a3),2(a4,d0.w*4)	;put 'run' data into lookup table entry 1
		move.b	(a3)+,0(a4,d0.w*4)	;put "run" data into first byte (as well)
		move.b	(a3)+,1(a4,d0.w*4)	;put 'level' data into lookup table entry 2
		move.b	-3(a3),3(a4,d0.w*4)	;put bitlength of vlc code into lookup table entry 3
		clr.b	(a4,d0.w*4)
.DCT_vlc_next	addq.l	#1,d0
		cmp.l	a5,d0
		blt.b	.lookupDCTloop
		movem.l	(a7)+,d0-d7/a3-a6
		rts



; A5: VDEC_BASE
gfx_render_p96
mpr_gray	movem.l	d0/a0,-(a7)
		moveq	#0,d0				;xstart
		moveq	#0,d1				;ystart
		move.l	width(pc),d2			;xstop
		subq.l	#1,d2
		move.l	height(pc),d3			;ystop
		subq.l	#1,d3
		move.l	y_bitmap_width(pc),d4		;bytes per row
		move.l	y_bitmap_base(pc),a2
		move.l	ScreenRastport,a0
		CALLGFX	WriteChunkyPixels
		movem.l	(a7)+,d0/a0
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

	;ifne	0
	IFNE APOLLO_YUYV

; A5: VDEC_BASE
mpr_rgbhicolor:
		move.l	a5,-(sp)	;possibly not needed

		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		
		move.l	GfxMemBase-VDEC_BASE(A5),a1	;dest ptr
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		;TODO: check whether width(pc) and y_bitmap_width actually differ
		move.l	y_bitmap_width(pc),d1	;source width (y plane) - for second line
		move.l	width(pc),d5	;output width
		and.b	#$f8,d5

		move.l	d5,d3		;output width
		and.w	#$8,d3
		lsr.w	#1,d3
		move.l	d3,a5		;
		
		move.l	d1,d3		;2*y_bitmap_width
		add.l	d3,d3		;
		sub.l	d5,d3		;-output width
		move.l	d3,a0
		
		move.l	d7,d3		; scr stride
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		add.l	d7,d3		; 2*img stride - 2*source width (16 Bit)

		lsr.l	#3,d5		;width/8

		subq.l	#1,d5		;width/8 - dbf
		move.l	d5,a6		;

		move.l	height(pc),d4
		lsr.l	#1,d4		;height/2
		subq.l	#1,d4		;-dbf
mpr_RGBHICOLOR_loop_ver:
		move.l	a6,d6
mpr_RGBHICOLOR_loop_hor:
	ifne	1
		;------------------ 8x2 pixel processing block -------------------
		;A2 - Y
		;A3 - U (Cb, actually)
		;A4 - V (Cr)
		;A1 - output ARGB32
		; R',G',B' means "dark", non-luma-corrected intensities for Red,Green,Blue (in here)
		move.l		(a4)+,d2		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;could use Dn as well
		 move.l		(a3)+,d0		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d2,E16,E17	;V0.w V1.w V2.w V3.w
		 vperm		#$84858687,d0,E16,E19	;U0.w U1.w U2.w U3.w
		 move.l		(a2)+,d0		;x x x x Y00 Y01 Y02 Y03 .b
		psubw.w		#128,E17,E17		;
		 pmul88.w	#-FIX_0_6985,E17,E20	;G'2 =V*-119 >>8
		psubw.w		#128,E19,E19		;
		 pmul88.w	#-FIX_0_3359,E19,E18	;G'1 =U*-48  >>8
		pmul88.w	#FIX_1_3711,E17,E17	;R' = (V*402)>>8
		 pmul88.w	#FIX_1_7337,E19,E19	;B' = (U*475)>>8
		paddw		E18,E20,E18		;G' = U*-48 + V*-119
		;11 cycles for 16 pixels

		 paddusb.w	#$0004,d0,d0		;dithering 0,4 0,4 (pulled up, E19 delay for TRANSHi)
		 move.l	-4(a2,d1.w),d2			;x x x x Y10 Y11 Y12 Y13 .b  (pulled up, E19 latency for TRANSHi)

		;E16 0 0 0 0 0 0 0 0
		;E17 R'0 R'1 R'2 R'3
		;E18 G'0 G'1 G'2 G'3
		;E19 B'0 B'1 B'2 B'3
		TRANSHI E16-E19,E0:E1			; E0: 000 R'0 G'0 B'0 E1: 000 R'1 G'1 B'1 .w
		 TRANSLO E16-E19,E2:E3			; E2: 000 R'2 G'2 B'2 E3: 000 R'3 G'3 B'3 .w
		paddusb.w #$0602,d2,d2			; dithering 6,2 6,2 (pulled up, E0 latency for vperm)

		;---------------------- first 4x2-pixel block ------------------------------------
		;preliminaries: load d0, load d2, paddb ...,D0
		vperm	#$84848484,d0,E0,E22		; Y00 Y00 Y00 Y00 .w
		 vperm	#$85858585,d0,E0,E23		; Y01 Y01 Y01 Y01 .w
		paddw	E0,E22,E22			; xxx R00 G00 B00 .w
		 paddw	E0,E23,E23			; xxx R01 G01 B01 .w
		packuswb E22,E23,E16			; xxx R00 G00 B00 xxx R01 G01 B01 .b

		vperm	#$84848484,d2,E0,E22		; Y10 Y10 Y10 Y10 .w
		 vperm	#$85858585,d2,E0,E23		; Y11 Y11 Y11 Y11 .w
		paddw	E0,E22,E22			; xxx R10 G10 B10 .w
		 paddw	E0,E23,E23			; xxx R11 G11 B11 .w
		packuswb E22,E23,E18			; xxx R10 G10 B10 xxx R11 G11 B11 .b
		
		vperm	#$86868686,d0,E0,E22		; Y02 Y02 Y02 Y02 .w
		 vperm	#$87878787,d0,E0,E23		; Y03 Y03 Y03 Y03 .w
		paddw	E1,E22,E22			; xxx R02 G02 B02 .w
		move.l	(a2)+,d0                        ;x x x x Y04 Y05 Y06 Y07 .b
		 paddw	E1,E23,E23			; xxx R03 G03 B03 .w
		packuswb E22,E23,E17			; xxx R02 G02 B02 xxx R03 G03 B03 .b

		vperm	#$86868686,d2,E0,E22		; Y12 Y12 Y12 Y12 .w
		 vperm	#$87878787,d2,E0,E23		; Y13 Y13 Y13 Y13 .w
		paddw	E1,E22,E22			; xxx R12 G12 B12 .w
		move.l	-4(a2,d1.w),d2			;x x x x Y14 Y15 Y16 Y17 .b 
		 paddw	E1,E23,E23			; xxx R13 G13 B13 .w
		packuswb E22,E23,E19			; xxx R12 G12 B12 xxx R13 G13 B13 .b

		 pack3216 E18,E19,(a1,d7.w)		; store 16 Bit RGB565
		pack3216 E16,E17,(a1)+			; store 16 Bit RGB565
		; 24 cycles for 8 pixels + 1 + 11/2 = 3.81 cycles/pixel

		;--------------------- second 4x2-pixel block ------------------------------------
		paddusb.w #$0004,d0,d0			; dithering 0,4 0,4
		 paddusb.w #$0602,d2,d2			; dithering 6,2 6,2

		vperm	#$84848484,d0,E0,E22		; Y04 Y04 Y04 Y04 .w
		 vperm	#$85858585,d0,E0,E23		; Y05 Y05 Y05 Y05 .w
		paddw	E2,E22,E22			; xxx R04 G04 B04 .w
		 paddw	E2,E23,E23			; xxx R05 G05 B05 .w
		packuswb E22,E23,E16			; xxx R04 G04 B04 xxx R05 G05 B05 .b

		vperm	#$84848484,d2,E0,E22		; Y14 Y14 Y14 Y14 .w
		 vperm	#$85858585,d2,E0,E23		; Y15 Y15 Y15 Y15 .w
		paddw	E2,E22,E22			; xxx R14 G14 B14 .w
		 paddw	E2,E23,E23			; xxx R15 G15 B15 .w
		packuswb E22,E23,E18			; xxx R14 G14 B14 xxx R15 G15 B15 .b
		
		vperm	#$86868686,d0,E0,E22		; Y06 Y06 Y06 Y06 .w
		 vperm	#$87878787,d0,E0,E23		; Y07 Y07 Y07 Y07 .w
		paddw	E3,E22,E22			; xxx R06 G06 B06 .w
		 paddw	E3,E23,E23			; xxx R07 G07 B07 .w
		packuswb E22,E23,E17			; xxx R06 G06 B06 xxx R07 G07 B07 .b

		vperm	#$86868686,d2,E0,E22		; Y16 Y16 Y16 Y16 .w
		 vperm	#$87878787,d2,E0,E23		; Y17 Y17 Y17 Y17 .w
		paddw	E3,E22,E22			; xxx R16 G16 B16 .w
		 paddw	E3,E23,E23			; xxx R17 G17 B17 .w
		packuswb E22,E23,E19			; xxx R16 G16 B16 xxx R17 G17 B17 .b

		 pack3216 E18,E19,(a1,d7.w)		; store 16 Bit RGB565
		pack3216 E16,E17,(a1)+			; store 16 Bit RGB565

		;total 2*24+2+11 = 61/16 = 3.8125 cycles/pixel
	else
		;------------------ 8x2 pixel processing block -------------------
		;A2 - Y
		;A3 - U (Cb, actually)
		;A4 - V (Cr)
		;A1 - output ARGB32
		; R',G',B' means "dark", non-luma-corrected intensities for Red,Green,Blue (in here)
		move.l		(a4)+,d2		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;could use Dn as well
		 move.l		(a3)+,d5		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d2,E16,E18	;V0.w V1.w V2.w V3.w
		move.l	(a2)+,d0			;x x x x Y00 Y01 Y02 Y03 .b
		 vperm		#$84858687,d5,E16,E17	;U0.w U1.w U2.w U3.w
		psubw.w		#128,E18,E18		;
		 pmul88.w	#-FIX_0_6985,E18,E20	;G'2 =V*-119 >>8
		psubw.w		#128,E17,E17		;
		 pmul88.w	#-FIX_0_3359,E17,E19	;G'1 =U*-48  >>8
		pmul88.w	#FIX_1_3711,E18,E18	;R' = (V*402)>>8
		 pmul88.w	#FIX_1_7337,E17,E17	;B' = (U*475)>>8
		paddw		E19,E20,E19		;G' = U*-48 + V*-119
		;11 cycles for 16 pixels

		;E16 0 0 0 0 0 0 0 0
		;E18 R'0 R'1 R'2 R'3
		;E19 G'0 G'1 G'2 G'3
		;E17 B'0 B'1 B'2 B'3
		;---------------------- first 4x2-pixel block ------------------------------------
		paddusb.w	#$0004,d0,d0			; dithering 0,4 0,4
		vperm	#$018923ab,E18,E19,E20		; R'0 G'0 R'1 G'1
		move.l	-4(a2,d1.w),d2			;x x x x Y10 Y11 Y12 Y13 .b
		 vperm	#$ab012389,E20,E17,E21		; B'1 R'0 G'0 B'0 (B'1 is filler, unused) (ARGB32)
		vperm	#$84848484,d0,E16,E22		; Y00 Y00 Y00 Y00
		 paddw	E21,E22,E22			; xxx R00 G00 E160
		vperm	#$85858585,d0,E16,E23		; Y01 Y01 Y01 Y01
		 paddw	E21,E23,E23			; xxx R01 G01 E161
		PACKUSWBBBD 14,15,8		;VASM E22=14,E23=15 into E0=D0+8
		paddusb.w	#$0602,d2,d2			; dithering 6,2 6,2
		;9

		vperm	#$89456789,E20,E21,E21		; B'1 R'1 G'1 B'1 (ARGB32)
		 vperm	#$86868686,d0,E16,E22		; Y02 Y02 Y02 Y02
		paddw	E21,E22,E22			; R02 G02 E162 xxx
		 vperm	#$87878787,d0,E16,E23		; Y03 Y03 Y03 Y03
		paddw	E21,E23,E23			; R03 G03 E163 xxx
		 PACKUSWBBBD 14,15,10		;VASM E22=14,E23=15 into E2=D2+8
		;6


		vperm	#$84848484,d2,E16,E22		; Y10 Y10 Y10 Y10
		move.l	(a2)+,d0			;x x x x Y04 Y05 Y06 Y07 .b
		 paddw	E21,E22,E22			; xxx R10 G10 E170
		vperm	#$85858585,d2,E16,E23		; Y11 Y11 Y11 Y11
		 paddw	E21,E23,E23			; xxx R11 G11 E171
		PACKUSWBBBD 14,15,9		;VASM E22=14,E23=15 into E1=D1+8
		;5
		 vperm	#$86868686,d2,E16,E22		; Y12 Y12 Y12 Y12
		paddw	E21,E22,E22			; R12 G12 E172 xxx
		 vperm	#$87878787,d2,E16,E23		; Y13 Y13 Y13 Y13
		paddw	E21,E23,E23			; R13 G13 E173 xxx
		 PACKUSWBBBD 14,15,11		;VASM E22=14,E23=15 into E3=D3+8
		 move.l	-4(a2,d1.w),d2			;x x x x Y14 Y15 Y16 Y17 .b
		;5
		;25 cycles = 2.875 cycles per pixel

		dc.w	$FE31,$9B07,$7000	;pack3216 E1,E3,(a1,d7.w)
		 PACK3216DDAp 8,10,1		;pack3216 E0,E2,(a1)+
		;27 cycles total incl. store(s)

		;--------------------- second 4x2-pixel block ------------------------------------
		paddusb.w	#$0004,d0,d0			; dithering 0,4 0,4
		paddusb.w	#$0602,d2,d2			; dithering 6,2 6,2

		vperm	#$45cd67ef,E18,E19,E20		; R'2 G'2 R'3 G'3
		 vperm	#$ef0123cd,E20,E17,E21		; B'3 R'2 G'2 B'2

		vperm	#$84848484,d0,E16,E22		
		 paddw	E21,E22,E22
		vperm	#$85858585,d0,E16,E23		
		 paddw	E21,E23,E23

		PACKUSWBBBD 14,15,8		;VASM: E22=14,E23=15 into E0=D0+8
		 vperm	#$89456789,E20,E21,E21
		vperm	#$86868686,d0,E16,E22
		 paddw	E21,E22,E22
		vperm	#$87878787,d0,E16,E23		
		 paddw	E21,E23,E23			
		PACKUSWBBBD 14,15,10		;VASM: E22=14,E23=15 into E2=D2+8

		vperm	#$84848484,d2,E16,E22		
		 paddw	E21,E22,E22			
		vperm	#$85858585,d2,E16,E23		
		 paddw	E21,E23,E23			
		PACKUSWBBBD 14,15,9		;VASM: E22=14,E23=15 into E1=D1+8

		vperm	#$86868686,d2,E16,E22		
		 paddw	E21,E22,E22			
		vperm	#$87878787,d2,E16,E23		
		 paddw	E21,E23,E23			
		PACKUSWBBBD 14,15,11		;VASM: E22=14,E23=15 into E3=D3+8
		;23

		 dc.w	$FE31,$9B07,$7000	;pack3216 E1,E3,(a1,d7.w)
		PACK3216DDAp 8,10,1		;pack3216 E0,E2,(a1)+
	endc
		;+2 = 25 cycles
	;------------------ 8x2 pixel processing block end ----------------

		;done: 2x8 pixels per loop
		dbf	d6,mpr_RGBHICOLOR_loop_hor

		;pointers after one line:
		; A1 = end of line (need to skip over 1 line)
		; A2 = end of line (need to skip over 1 line)
		; A3 = end of line Cb
		; A4 = end of line Cr
		add.l	a0,a2	;lea	(a2,d1.w),a2
		add.l	d3,a1
		add.l	a5,a3
		add.l	a5,a4

		dbf	d4,mpr_RGBHICOLOR_loop_ver


		move.l	(sp)+,a5	;possibly not needed
		rts
	else
; A5: VDEC_BASE

; new vars: YUYVRows,YUYVCols
;           YUYVTLOffY,YUYVTLOffC
;
mpr_rgbhicolor:
			move.l	GfxMemBase-VDEC_BASE(a5),a1
			move.l	BitmapModulo-VDEC_BASE(a5),d7

			lsl.l	#1,d7				;2*modulo
			move.l	YUYVCols.l(pc),d0		;width(pc),d0
			and.l	#$fffffffc,d0			;long align...
			move.l	d0,d6
			lsl.l	#1,d0				;no. of bytes written/line
			sub.l	d0,d7				;[2*modulo]-[width(in bytes)]
			move.l	d7,bitmap_add-VDEC_BASE(a5)

			lsr.l	#3,d0				;4 pixels/loop -> xloops = width/4
			subq.l	#1,d0
			lea	no_of_loops(pc),a2
			move.l	d0,(a2)

			move.l	y_bitmap_width(pc),d0
			lsl.l	#1,d0				;skip 2 lines!
			sub.l	d6,d0
			move.l	d0,source_y_skip-VDEC_BASE(a5)

			lsr.l	#1,d6				;no. of chrom. bytes/line
			move.l	c_bitmap_width(pc),d0
			sub.l	d6,d0
			move.l	d0,source_c_skip-VDEC_BASE(a5)

			move.l	y_bitmap_base(pc),a2
			move.l	cb_bitmap_base(pc),a3
			move.l	cr_bitmap_base(pc),a4
			add.l	YUYVTLOffY.l(pc),a2
			add.l	YUYVTLOffC.l(pc),a3
			add.l	YUYVTLOffC.l(pc),a4
			move.l	YUVtoHiColorTable-VDEC_BASE(a5),a5

			move.l	YUYVRows.l(pc),d1
			lsr.l	#1,d1				;height/2 loops (2 lines/xloop)
			move.l	d1,a6
mpr_rgbhicolor_loopy
			move.l	no_of_loops(pc),d0
mpr_rgbhicolor_loopx

;YUV to HiColor loop -> Converts 8 pixels (4x2)
;
			move.l	(a2)+,d2			;d2=[Y0,Y1,Y2,Y3]
mpr_rgbhicolor_ofst0	 move.l	160-4(a2),d3			;d2=[Y4,Y5,Y6,Y7]
			 and.l	#$fcfcfcfc,d2			;d2=[000000--][111111--][222222--][333333--]
			move.w	(a3)+,d4			;d4=[xx,xx,U0,U1]
			and.l	#$fcfcfcfc,d3			;d3=[444444--][555555--][666666--][777777--]
			 move.w	(a4)+,d5			;d5=[xx,xx,V0,V1]
			 lsl.l	#6,d4				;d4=[xxxxxxxx][xxuuuuuu][uuuuuuuu][uu------]
			rol.l	#6,d2				;d2=[Y1,Y2,Y3,Y0]
			rol.l	#6,d3				;d3=[Y5,Y6,Y7,Y4]
			 move.l	d4,d6			;F	;d6=[xxxxxxxx][xxuuuuuu][uuuuuuuu][uu------]
			 move.w	d5,d6			;F	;d6=[xxxxxxxx][xxuuuuuu][vvvvvvvv][vvvvvvvv]
			lsr.l	#4,d6				;d6=[----xxxx][xxxxxxuu][uuuuvvvv][vvvvvvvv]
			lsl.l	#8,d4				;d4=[xxuuuuuu][uuuuuuuu][uu------][--------]
			 and.l	#$0003ffc0,d6			;d6=[--------][------uu][uuuuvvvv][vv------]
			 lsl.l	#8,d5				;d5=[xxxxxxxx][vvvvvvvv][vvvvvvvv][--------]
			move.l	d6,d1
			or.b	d2,d6				;d6=[--------][------uu][uuuuvvvv][vvyyyyyy]
			 rol.l	#8,d2
			 move.w	d5,d4				;d4=[xxuuuuuu][uuuuuuuu][vvvvvvvv][vvvvvvvv]
			or.b	d2,d1
			lsr.l	#4,d4				;d4=[xxxxxxuu][uuuuuuuu][uuuuvvvv][vvvvvvvv]
			 move.l	(a5,d6.l*2),d7			;d7=[RGB0,xxxx]
			 and.b	#$c0,d6
			and.l	#$0003ffc0,d4			;d4=[--------][------uu][uuuuvvvv][vv------]
			move.w	(a5,d1.l*2),d7			;d7=[RGB0,RGB1]
			 and.b	#$c0,d1
			 move.l	d7,(a1)+
			or.b	d3,d6
			rol.l	#8,d3
			 or.b	d3,d1				;d1=[--------][------uu][uuuuvvvv][vvyyyyyy]
			 rol.l	#8,d2
			move.l	(a5,d6.l*2),d7			;d7=[RGB4,xxxx]
			or.b	d2,d4				;d4=[--------][------uu][uuuuvvvv][vvyyyyyy]
			 move.w	(a5,d1.l*2),d7			;d7=[RGB4,RGB5]
			 rol.l	#8,d2				;d2=[Y0,Y1,Y2,Y3]
mpr_rgbhicolor_ofst1	move.l	d7,640-4(a1)
			moveq	#-64,d6				;$ffffffc0
			 and.l	d4,d6
			 move.l	(a5,d4.l*2),d7			;d7=[RGB2,xxxx]
			or.b	d2,d6
			and.b	#$c0,d4				;d4=[--------][------uu][uuuuvvvv][vv------]
			 rol.l	#8,d3				;d3=[Y7,Y4,Y5,Y6]
			 moveq	#-64,d1
			and.l	d6,d1				;
			or.b	d3,d4				;d4=[--------][------uu][uuuuvvvv][vvyyyyyy]
			 move.w	(a5,d6.l*2),d7			;d7=[RGB2,RGB3]
			 rol.l	#8,d3				;d3=[Y4,Y5,Y6,Y7]
			move.l	d7,(a1)+
			or.b	d3,d1
			 move.l	(a5,d4.l*2),d7			;d7=[RGB6,xxxx]
			 ;.oO(bubble D1)
			move.w	(a5,d1.l*2),d7			;d7=[RGB6,RGB7]
mpr_rgbhicolor_ofst2	move.l	d7,640-4(a1)

			;-------------------------
			dbf	d0,mpr_rgbhicolor_loopx

			;TODO: awkward, do better
			subq.l	#1,a6
			add.l	bitmap_add,a1
			add.l	source_y_skip,a2
			add.l	source_c_skip,a3
			add.l	source_c_skip,a4

			tst.l	a6
			bne	mpr_rgbhicolor_loopy

		rts
	endc

	ifne	CODE_ALIGN
		CNOP    0,16
	endc

		;TODO: RGB24 instead of the currently executed BGR24 for 68k Builds
mpr_rgb24:
	IFNE APOLLO_YUYV
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		
		move.l	GfxMemBase-VDEC_BASE(A5),a1	;dest ptr
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		move.l	y_bitmap_width(pc),d1	;source width (y plane) - for second line
		move.l	width(pc),d5	;output width
		and.b	#$f8,d5

		move.l	d5,d3		;output width
		and.w	#$8,d3
		lsr.w	#1,d3
		move.l	d3,a5		;add to chroma pointer after loop
		
		move.l	d1,d3		;2*y_bitmap_width
		add.l	d3,d3		;
		sub.l	d5,d3		;-output width
		move.l	d3,a0		;

		move.l	d7,d3		; scr stride
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		add.l	d7,d3		; 2*img stride - 2*source width (32 Bit)

;		move.l	d1,d5		;width
		lsr.l	#3,d5		;width/8
		subq.l	#1,d5		;width/8 - dbf

		move.l	height(pc),d4
		lsr.l	#1,d4		;height/2
		subq.l	#1,d4		;-dbf
mpr_RGB24_loop_ver:
		move.l	d5,d6
mpr_RGB24_loop_hor:
		;------------------ 8x2 pixel processing block -------------------
		;A2 - Y
		;A3 - U (Cb, actually)
		;A4 - V (Cr)
		;A1 - output RGB24
		; R',G',B' means "dark", non-luma-corrected intensities for Red,Green,Blue (in here)
		; note: the register usage is sub-optimal right now. another arrangement would allow
		;       to get to ARGB much easier (using TRANS)

		;in:  A4 = V, A3 = U
		;out:
		; E16 0 0 0 0 0 0 0 0
		; E18 R'0 R'1 R'2 R'3
		; E19 G'0 G'1 G'2 G'3
		; E17 B'0 B'1 B'2 B'3
		;trash: E20
RGB24_CHROMABLOCK	macro
		move.l		(a4)+,d2		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;could use Dn as well
		 move.l		(a3)+,d0		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d2,E16,E18	;V0.w V1.w V2.w V3.w
		 vperm		#$84858687,d0,E16,E17	;U0.w U1.w U2.w U3.w
		psubw.w		#128,E18,E18		;
		 pmul88.w	#-119,E18,E20		;G'2 =V*-119 >>8
		psubw.w		#128,E17,E17		;
		 pmul88.w	#-48,E17,E19		;G'1 =U*-48  >>8
		pmul88.w	#402,E18,E18		;R' = (V*402)>>8
		 pmul88.w	#475,E17,E17		;B' = (U*475)>>8
		paddw		E19,E20,E19		;G' = U*-48 + V*-119
		;11 cycles for 16 pixels
			endm

		RGB24_CHROMABLOCK			;destroys d0,d2,E16-E20
		LOAD	(a2)+,E4			; Y00 Y01 Y02 Y03 Y04 Y05 Y06 Y07 .b (load here to avoid 1 cycle bubble due to PMUL)
		;E16 0 0 0 0 0 0 0 0
		;E18 R'0 R'1 R'2 R'3
		;E19 G'0 G'1 G'2 G'3
		;E17 B'0 B'1 B'2 B'3
		TRANSHI E16-E19,E0:E1			; E0: 000 B'0 R'0 G'0 E1: 000 B'1 R'1 G'1 .w
		TRANSLO E16-E19,E2:E3			; E2: 000 B'2 R'2 G'2 E3: 000 B'3 R'3 G'3 .w

		;---------------------- 8x2-pixels in 3 phases = 24x2 output bytes  ----------------
		LOAD	-8(a2,d1.w),E5			; Y10 Y11 Y12 Y13 Y14 Y15 Y16 Y17 .b ; "free" LOAD, if done later we'd have a 1 cycle bubble here) 

		;phase 1: RGBRGBRG
		vperm	#$45672345,E0,E1,E20		; R'0 G'0 B'0 R'0 .w
		vperm	#$6723cdef,E0,E1,E21		; G'0 B'0 R'1 G'1 .w
		vperm	#$80808081,E4,E0,E22		; Y00 Y00 Y00 Y01 .w
		vperm	#$81818282,E4,E0,E23		; Y01 Y01 Y02 Y02 .w
		paddw	E20,E22,E22			; R00 G00 B00 R01 .w
		paddw	E21,E23,E23			; G01 B01 R02 G02 .w
		packuswb E22,E23,(a1)+			; R00 G00 B00 R01 G01 B02 R02 G02 .b

		vperm	#$80808081,E5,E0,E22		; Y10 Y10 Y10 Y11 .w
		vperm	#$81818282,E5,E0,E23		; Y11 Y11 Y12 Y12 .w
		paddw	E20,E22,E22			; R10 G10 B10 R11 .w
		paddw	E21,E23,E23			; G11 B11 R12 G12 .w
		packuswb E22,E23,-8(a1,d7.w)		; R10 G10 B10 R11 G01 B02 R02 G02 .b

		;phase2: BRGBRGBR
		vperm	#$23456723,E1,E2,E6		; B'1 R'1 G'1 B'1 .w
		vperm	#$45672345,E2,E3,E21		; R'2 G'2 B'2 R'2 .w
		vperm	#$82838383,E4,E0,E22		; Y02 Y03 Y03 Y03 .w
		vperm	#$84848485,E4,E0,E23		; Y04 Y04 Y04 Y05 .w
		paddw	E6,E22,E22			; R02 G03 B03 R03 .w
		paddw	E21,E23,E23			; G04 B04 R04 G05 .w
		packuswb E22,E23,(a1)+			; B02 R03 B03 G03 R04 G04 B04 R05 .b

		vperm	#$82838383,E5,E0,E22		; Y12 Y13 Y13 Y13 .w
		vperm	#$84848485,E5,E0,E23		; Y14 Y14 Y14 Y15 .w
		paddw	E6,E22,E22			; R10 G10 B10 R11 .w
		paddw	E21,E23,E23			; G11 B11 R12 G12 .w
		packuswb E22,E23,-8(a1,d7.w)		; R10 G10 B10 R11 G01 B02 R02 G02 .b

		;phase3: GBRGBRGB
		vperm	#$6723cdef,E2,E3,E20		; G'2 B'2 R'3 G'3 .w
		vperm	#$abcdefab,E2,E3,E21		; B'3 R'3 G'3 B'3 .w

		vperm	#$85858686,E4,E0,E22		; Y05 Y05 Y06 Y06 .w
		vperm	#$86878787,E4,E0,E23		; Y06 Y07 Y07 Y07 .w
		paddw	E20,E22,E22			;
		paddw	E21,E23,E23			;
		packuswb E22,E23,(a1)+			;

		vperm	#$85858686,E5,E0,E22		; Y15 Y15 Y16 Y16 .w
		vperm	#$86878787,E5,E0,E23		; Y16 Y17 Y17 Y17 .w
		paddw	E20,E22,E22			;
		paddw	E21,E23,E23			;
		packuswb E22,E23,-8(a1,d7.w)		;
		;------------------ 8x2 pixel processing block end ----------------

		;done: 2x8 pixels per loop
		dbf	d6,mpr_RGB24_loop_hor

		;pointers after one line:
		; A1 = end of line (need to skip over 1 line)
		; A2 = end of line (need to skip over 1 line)
		; A3 = end of line Cb
		; A4 = end of line Cr
		add.l	d3,a1	;lea	(a1,d7.l),a1
		add.l	a0,a2	;lea	(a2,d1.w),a2

		dbf	d4,mpr_RGB24_loop_ver

		rts
	ENDC	;APOLLO_YUYV

mpr_bgr24:	move.l	GfxMemBase-VDEC_BASE(A5),a1
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		lsl.l	#1,d7
		move.l	YUYVCols.l(pc),d6		;width(pc),d6
		move.l	d6,d0
		and.l	#$fffffffc,d0			;align to 4-divisible width
		mulu.w	#3,d0				;this is how many bytes will be written/line!
		sub.l	d0,d7				;subtract from modulo to find no. of bytes to skip each line!
		move.l	d7,bitmap_add-VDEC_BASE(A5)

		lsr.l	#2,d6				;width/4 no. of loops (because 1 long write per loop)
		move.l	d6,no_of_loops

		move.l	d6,d1				;Calculate no. of bytes to skip in source after lines (tricky!)
		lsl.l	#2,d1				;framewidth 4-byte aligned

		move.l	y_bitmap_width(pc),d5		;d5 = no. of bytes to skip in input after each line!
		sub.l	d1,d5
		move.l	d5,d6
		add.l	y_bitmap_width(pc),d5
		move.l	d5,source_y_skip-VDEC_BASE(A5)
		lsr.l	#1,d6
		move.l	d6,source_c_skip-VDEC_BASE(A5)
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		add.l	YUYVTLOffY.l(pc),a2
		add.l	YUYVTLOffC.l(pc),a3
		add.l	YUYVTLOffC.l(pc),a4

		lea	CbTable-VDEC_BASE(A5),a6
		lea	512+Clamp256-VDEC_BASE(A5),a0
		lea	CrTable-VDEC_BASE(A5),a5

		move.l	YUYVRows.l(pc),d1			;height(pc),d1
		lsr.l	#1,d1
mpr_bgr24_loopy	move.l	d1,-(a7)
		move.l	no_of_loops(pc),d0
mpr_bgr24_loopx	;------------------;
		moveq	#0,d3
		move.b	(a4)+,d3
		move.l	(a5,d3.w*4),d4			;d4:[Cr0-G,Cr0-R]
		move.b	(a3)+,d3
		move.l	(a6,d3.w*4),d3			;d3:[Cb0-G,Cb0-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		moveq	#0,d6
		move.b	(a2),d6				;d6:[Y0]

		move.w	d6,d2
		add.w	d3,d2
		move.w	(a0,d2.w),d7			;d7:[--,--,B0,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.b	(a0,d2.w),d7			;d7:[--,--,B0,G0]
		swap	d7				;d7:[B0,G0,--,--]
		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B0,G0,R0,-]

		moveq	#0,d6
		move.b	1(a2),d6				;d6:[Y1]

		move.w	d6,d2
		add.w	d3,d2
		move.b	(a0,d2.w),d7			;d7:[B0,G0,R0,B1]

		move.l	d7,(a1)

		move.w	d6,d2
		sub.w	d5,d2
		move.w	(a0,d2.w),d7			;d7:[--,--,G1,--]
		add.w	d4,d6
		move.b	(a0,d6.w),d7			;d7:[--,--,G1,R1]
		swap	d7				;d7:[G1,R1,--,--]

		moveq	#0,d6
width0		move.b	160(a2),d6			;d6:[Y4]

		move.w	d6,d2
		add.w	d3,d2
		move.w	(a0,d2.w),d1			;d1:[--,--,B4,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.b	(a0,d2.w),d1			;d1:[--,--,B4,G4]
		swap	d1				;d1:[B4,G4,--,--]
		add.w	d4,d6
		move.w	(a0,d6.w),d1			;d1:[B4,G4,R4,--]

		moveq	#0,d6
width1		move.b	161(a2),d6			;d6:[Y5]

		move.w	d6,d2
		add.w	d3,d2
		move.b	(a0,d2.w),d1			;d1:[B4,G4,R4,B5]

modulo1		move.l	d1,160(a1)

		move.w	d6,d2
		sub.w	d5,d2
		move.w	(a0,d2.w),d1			;d1:[--,--,G5,--]
		add.w	d4,d6
		move.b	(a0,d6.w),d1			;d1:[--,--,G5,R5]
		swap	d1				;d1:[G5,R5,--,--]

		moveq	#0,d3
		move.b	(a4)+,d3
		move.l	(a5,d3.w*4),d4			;d4:[Cr1-G,Cr1-R]

		move.b	(a3)+,d3
		move.l	(a6,d3.w*4),d3			;d3:[Cb1-G,Cb1-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		moveq	#0,d6
		move.b	2(a2),d6				;d6:[Y2]

		move.w	d6,d2
		add.w	d3,d2
		move.w	(a0,d2.w),d7			;d7:[G1,R1,B2,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.b	(a0,d2.w),d7			;d7:[G1,R1,B2,G2]
		move.l	d7,4(a1)

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[--,--,R2,--]

		moveq	#0,d6
width2		move.b	162(a2),d6			;d6:[Y6]

		move.w	d6,d2
		add.w	d3,d2
		move.w	(a0,d2.w),d1			;d1:[G5,R5,B6,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.b	(a0,d2.w),d1			;d1:[G5,R5,B6,G6]
modulo2		move.l	d1,160+4(a1)

		add.w	d4,d6
		move.w	(a0,d6.w),d1			;d1:[--,--,R6,--]

		moveq	#0,d6
		move.b	3(a2),d6				;d6:[Y3]

		move.w	d6,d2
		add.w	d3,d2
		move.b	(a0,d2.w),d7			;d7:[--,--,R2,B3]
		swap	d7				;d7:[R2,B3,--,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.w	(a0,d2.w),d7			;d7:[R2,B3,G3,--]
		add.w	d4,d6
		move.b	(a0,d6.w),d7			;d7:[R2,B3,G3,R3]
		move.l	d7,8(a1)

		moveq	#0,d6
width3		move.b	163(a2),d6			;d6:[Y7]

		move.w	d6,d2
		add.w	d3,d2
		move.b	(a0,d2.w),d1			;d1:[--,--,R6,B7]
		swap	d1				;d1:[R6,B7,--,--]
		move.w	d6,d2
		sub.w	d5,d2
		move.w	(a0,d2.w),d1			;d1:[R6,B7,G7,--]
		add.w	d4,d6
		move.b	(a0,d6.w),d1			;d1:[R6,B7,G7,R7]
modulo3		move.l	d1,160+8(a1)
enda
		lea	12(a1),a1
		addq.l	#4,a2
		;--------------------
		subq.l	#1,d0
		bne.w	mpr_bgr24_loopx
		add.l	bitmap_add,a1
		add.l	source_y_skip,a2
		add.l	source_c_skip,a3
		add.l	source_c_skip,a4
		move.l	(a7)+,d1
		subq.l	#1,d1
		bne.w	mpr_bgr24_loopy
		rts

	ifne	CODE_ALIGN
		CNOP    0,16
	endc


; ----------- ARGB32 ---------------------------
mpr_argb32:
	IFNE APOLLO_YUYV
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		
		move.l	GfxMemBase-VDEC_BASE(A5),a1	;dest ptr
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		move.l	y_bitmap_width(pc),d1	;source width (y plane) - for second line
		move.l	width(pc),d5	;output width
		and.b	#$f8,d5

		move.l	d5,d3		;output width
		and.w	#$8,d3
		lsr.w	#1,d3
		move.l	d3,a5		;add to chroma pointer after loop
		
		move.l	d1,d3		;2*y_bitmap_width
		add.l	d3,d3		;
		sub.l	d5,d3		;-output width
		move.l	d3,a0		;
		
		move.l	d7,d3		; scr stride
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		sub.l	d5,d3		; - source width (y plane)
		add.l	d7,d3		; 2*img stride - 2*source width (32 Bit)

;		move.l	d1,d5		;width
		lsr.l	#3,d5		;width/8
		subq.l	#1,d5		;width/8 - dbf

		move.l	height(pc),d4
		lsr.l	#1,d4		;height/2
		subq.l	#1,d4		;-dbf
mpr_ARGB32_loop_ver:
		move.l	d5,d6
mpr_ARGB32_loop_hor:
		;------------------ 8x2 pixel processing block -------------------
		;A2 - Y
		;A3 - U (Cb, actually)
		;A4 - V (Cr)
		;A1 - output ARGB32
		; R',G',B' means "dark", non-luma-corrected intensities for Red,Green,Blue (in here)
	ifne	1
		move.l		(a4)+,d2		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;could use Dn as well
		 move.l		(a3)+,d0		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d2,E16,E17	;V0.w V1.w V2.w V3.w
		 vperm		#$84858687,d0,E16,E19	;U0.w U1.w U2.w U3.w
		 move.l	(a2)+,d0			;x x x x Y00 Y01 Y02 Y03 .b
		psubw.w		#128,E17,E17		;
		 pmul88.w	#-FIX_0_6985,E17,E20	;G'2 =V*-119 >>8
		psubw.w		#128,E19,E19		;
		 pmul88.w	#-FIX_0_3359,E19,E18	;G'1 =U*-48  >>8
		pmul88.w	#FIX_1_3711,E17,E17	;R' = (V*402)>>8
		 pmul88.w	#FIX_1_7337,E19,E19	;B' = (U*475)>>8
		paddw		E18,E20,E18		;G' = U*-48 + V*-119
		;11 cycles for 16 pixels

		;1 cycle unused (bubble)
		 move.l	-4(a2,d1.w),d2			;x x x x Y10 Y11 Y12 Y13 .b  (pulled up, E19 latency for TRANSHi)

		;E16 0 0 0 0 0 0 0 0
		;E17 R'0 R'1 R'2 R'3
		;E18 G'0 G'1 G'2 G'3
		;E19 B'0 B'1 B'2 B'3
		TRANSHI E16-E19,E0:E1			; E0: 000 R'0 G'0 B'0 E1: 000 R'1 G'1 B'1 .w
		 TRANSLO E16-E19,E2:E3			; E2: 000 R'2 G'2 B'2 E3: 000 R'3 G'3 B'3 .w

		;1 cycle bubble
		
		;---------------------- first 4x2-pixel block ------------------------------------
		;preliminaries: load d0, load d2, paddb ...,D0
		vperm	#$84848484,d0,E0,E22		; Y00 Y00 Y00 Y00 .w
		 vperm	#$85858585,d0,E0,E23		; Y01 Y01 Y01 Y01 .w
		paddw	E0,E22,E22			; xxx R00 G00 B00 .w
		 paddw	E0,E23,E23			; xxx R01 G01 B01 .w
		packuswb E22,E23,(a1)+			; xxx R00 G00 B00 xxx R01 G01 B01 .b

		vperm	#$84848484,d2,E0,E22		; Y10 Y10 Y10 Y10 .w
		 vperm	#$85858585,d2,E0,E23		; Y11 Y11 Y11 Y11 .w
		paddw	E0,E22,E22			; xxx R10 G10 B10 .w
		 paddw	E0,E23,E23			; xxx R11 G11 B11 .w
		packuswb E22,E23,-8(a1,d7.w)		; xxx R10 G10 B10 xxx R11 G11 B11 .b
		
		vperm	#$86868686,d0,E0,E22		; Y02 Y02 Y02 Y02 .w
		 vperm	#$87878787,d0,E0,E23		; Y03 Y03 Y03 Y03 .w
		paddw	E1,E22,E22			; xxx R02 G02 B02 .w
		move.l	(a2)+,d0                        ;x x x x Y04 Y05 Y06 Y07 .b
		 paddw	E1,E23,E23			; xxx R03 G03 B03 .w
		packuswb E22,E23,(a1)+			; xxx R02 G02 B02 xxx R03 G03 B03 .b

		vperm	#$86868686,d2,E0,E22		; Y12 Y12 Y12 Y12 .w
		 vperm	#$87878787,d2,E0,E23		; Y13 Y13 Y13 Y13 .w
		paddw	E1,E22,E22			; xxx R12 G12 B12 .w
		move.l	-4(a2,d1.w),d2			;x x x x Y14 Y15 Y16 Y17 .b 
		 paddw	E1,E23,E23			; xxx R13 G13 B13 .w
		packuswb E22,E23,-8(a1,d7.w)		; xxx R12 G12 B12 xxx R13 G13 B13 .b

		; 20 cycles for 8 pixels + 1 + 11/2 = 3.31 cycles/pixel

		;--------------------- second 4x2-pixel block ------------------------------------

		vperm	#$84848484,d0,E0,E22		; Y00 Y00 Y00 Y00 .w
		 vperm	#$85858585,d0,E0,E23		; Y01 Y01 Y01 Y01 .w
		paddw	E2,E22,E22			; xxx R00 G00 B00 .w
		 paddw	E2,E23,E23			; xxx R01 G01 B01 .w
		packuswb E22,E23,(a1)+			; xxx R00 G00 B00 xxx R01 G01 B01 .b

		vperm	#$84848484,d2,E0,E22		; Y10 Y10 Y10 Y10 .w
		 vperm	#$85858585,d2,E0,E23		; Y11 Y11 Y11 Y11 .w
		paddw	E2,E22,E22			; xxx R10 G10 B10 .w
		 paddw	E2,E23,E23			; xxx R11 G11 B11 .w
		packuswb E22,E23,-8(a1,d7.w)		; xxx R10 G10 B10 xxx R11 G11 B11 .b
		
		vperm	#$86868686,d0,E0,E22		; Y02 Y02 Y02 Y02 .w
		 vperm	#$87878787,d0,E0,E23		; Y03 Y03 Y03 Y03 .w
		paddw	E3,E22,E22			; xxx R02 G02 B02 .w
		 paddw	E3,E23,E23			; xxx R03 G03 B03 .w
		packuswb E22,E23,(a1)+			; xxx R02 G02 B02 xxx R03 G03 B03 .b

		vperm	#$86868686,d2,E0,E22		; Y12 Y12 Y12 Y12 .w
		 vperm	#$87878787,d2,E0,E23		; Y13 Y13 Y13 Y13 .w
		paddw	E3,E22,E22			; xxx R12 G12 B12 .w
		 paddw	E3,E23,E23			; xxx R13 G13 B13 .w
		packuswb E22,E23,-8(a1,d7.w)		; xxx R12 G12 B12 xxx R13 G13 B13 .b

	else	; ifne 1

		move.l		(a4)+,d2		;x x x x V0 V1 V2 V3 .b
		 peor		E16,E16,E16		;could use Dn as well
		 move.l		(a3)+,d0		;x x x x U0 U1 U2 U3 .b
		vperm		#$84858687,d2,E16,E18	;V0.w V1.w V2.w V3.w
		 vperm		#$84858687,d0,E16,E17	;U0.w U1.w U2.w U3.w
		psubw.w		#128,E18,E18		;
		 pmul88.w	#-119,E18,E20		;G'2 =V*-119 >>8
		psubw.w		#128,E17,E17		;
		 pmul88.w	#-48,E17,E19		;G'1 =U*-48  >>8
		pmul88.w	#402,E18,E18		;R' = (V*402)>>8
		 pmul88.w	#475,E17,E17		;B' = (U*475)>>8
		;nop					;safety net for PMUL, TBR
		paddw		E19,E20,E19		;G' = U*-48 + V*-119
		;11 cycles for 16 pixels

		;E16 0 0 0 0 0 0 0 0
		;E18 R'0 R'1 R'2 R'3
		;E19 G'0 G'1 G'2 G'3
		;E17 B'0 B'1 B'2 B'3
		;---------------------- first 4x2-pixel block ------------------------------------
		vperm	#$018923ab,E18,E19,E20		; R'0 G'0 R'1 G'1
		 move.l	(a2)+,d0			;x x x x Y00 Y01 Y02 Y03 .b
		vperm	#$ab012389,E20,E17,E21		; B'1 R'0 G'0 B'0 (B'1 is filler, unused) (ARGB32)
		vperm	#$84848484,d0,E16,E22		; Y00 Y00 Y00 Y00
		paddw	E21,E22,E22			; xxx R00 G00 E160
		vperm	#$85858585,d0,E16,E23		; Y01 Y01 Y01 Y01
		 move.l	-4(a2,d1.w),d2			;x x x x Y10 Y11 Y12 Y13 .b
		paddw	E21,E23,E23			; xxx R01 G01 E161
		packuswb E22,E23,(a1)+			; xxx R00 G00 E160 xxx R01 G01 E161 .b
		;7

		vperm	#$84848484,d2,E16,E22		; Y10 Y10 Y10 Y10
		paddw	E21,E22,E22			; xxx R10 G10 E170
;	touch	(a4,a5)
		vperm	#$85858585,d2,E16,E23		; Y11 Y11 Y11 Y11
		paddw	E21,E23,E23			; xxx R11 G11 E171
		packuswb E22,E23,-8(a1,d7.w)		; xxx R10 G10 E170 xxx R11 G11 E171 .b
		;5

		vperm	#$89456789,E20,E21,E21		; B'1 R'1 G'1 B'1 (ARGB32)
;	touch	(a3,a5)
		vperm	#$86868686,d0,E16,E22		; Y02 Y02 Y02 Y02
		paddw	E21,E22,E22			; R02 G02 E162 xxx
		vperm	#$87878787,d0,E16,E23		; Y03 Y03 Y03 Y03
		paddw	E21,E23,E23			; R03 G03 E163 xxx
		packuswb E22,E23,(a1)+			; R02 G02 E162 xxx R03 G03 E163 xx .b
		;6

		vperm	#$86868686,d2,E16,E22		; Y12 Y12 Y12 Y12
		paddw	E21,E22,E22			; R12 G12 E172 xxx
		vperm	#$87878787,d2,E16,E23		; Y13 Y13 Y13 Y13
		paddw	E21,E23,E23			; R13 G13 E173 xxx
		packuswb E22,E23,-8(a1,d7.w)		; R12 G12 E172 xxx R13 G13 E173 xx .b
		;5
		;23 cycles = 2.875 cycles per pixel

		;--------------------- second 4x2-pixel block ------------------------------------
		vperm	#$45cd67ef,E18,E19,E20		; R'2 G'2 R'3 G'3
		 move.l	(a2)+,d0			;x x x x Y04 Y05 Y06 Y07 .b
		vperm	#$ef0123cd,E20,E17,E21		; B'3 R'2 G'2 B'2

		;........... copy of the above part .............
		vperm	#$84848484,d0,E16,E22		
		paddw	E21,E22,E22			
		vperm	#$85858585,d0,E16,E23		
		 move.l	-4(a2,d1.w),d2			;x x x x Y14 Y15 Y16 Y17 .b
		paddw	E21,E23,E23			
		packuswb E22,E23,(a1)+			

		vperm	#$84848484,d2,E16,E22		
		paddw	E21,E22,E22			
;	touch		16(a2,a5)	;preload next 32 bytes after first 16 bytes, else NOP
		vperm	#$85858585,d2,E16,E23		
		paddw	E21,E23,E23			
		packuswb E22,E23,-8(a1,d7.w)		

		vperm	#$89456789,E20,E21,E21		
;	touch		16(a2,d1.w)
		vperm	#$86868686,d0,E16,E22		
		paddw	E21,E22,E22			
		vperm	#$87878787,d0,E16,E23		
		paddw	E21,E23,E23			
		packuswb E22,E23,(a1)+			

		vperm	#$86868686,d2,E16,E22		
		paddw	E21,E22,E22			
		vperm	#$87878787,d2,E16,E23		
		paddw	E21,E23,E23			
		packuswb E22,E23,-8(a1,d7.w)		
		; again 23 cycles
	endc	; ifne 1
	;------------------ 8x2 pixel processing block end ----------------


		;done: 2x8 pixels per loop
		dbf	d6,mpr_ARGB32_loop_hor

		;pointers after one line:
		; A1 = end of line (need to skip over 1 line)
		; A2 = end of line (need to skip over 1 line)
		; A3 = end of line Cb
		; A4 = end of line Cr
		add.l	d3,a1	;lea	(a1,d7.l),a1
		add.l	a0,a2	;lea	(a2,d1.w),a2

		dbf	d4,mpr_ARGB32_loop_ver

	ELSE	;APOLLO_YUYV
		move.l	GfxMemBase-VDEC_BASE(A5),a1	;dest ptr
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		lsl.l	#1,d7
		move.l	YUYVCols.l(pc),d6		;width(pc),d6
		move.l	d6,d0
		and.l	#$fffffffc,d0			;align to 4-divisible width
		asl.l	#2,d0				;this is how many bytes will be written/line! ?
		sub.l	d0,d7				;subtract from modulo to find no. of bytes to skip each line!
		move.l	d7,bitmap_add-VDEC_BASE(A5)	;this one is same like with the bgr24

		lsr.l	#2,d6				;width/4 no. of loops (because 1 long write per loop)
		move.l	d6,no_of_loops

		move.l	d6,d1				;Calculate no. of bytes to skip in source after lines (tricky!)
		lsl.l	#2,d1				;framewidth 4-byte aligned

		move.l	y_bitmap_width(pc),d5		;d5 = no. of bytes to skip in input after each line!
		sub.l	d1,d5
		move.l	d5,d6
		add.l	y_bitmap_width(pc),d5
		move.l	d5,source_y_skip-VDEC_BASE(A5)

		lsr.l	#1,d6
		move.l	d6,source_c_skip-VDEC_BASE(A5)
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		add.l	YUYVTLOffY.l(pc),a2
		add.l	YUYVTLOffC.l(pc),a3
		add.l	YUYVTLOffC.l(pc),a4

		move.l	YUYVRows.l(pc),d1	;height(pc),d1
		lsr.l	#1,d1
p96_argb32_loopy

		move.l	no_of_loops(pc),d0
p96_argb32_loopx
		move.l	d0,a5
		;------------------;free:d2,a5,a6
		moveq	#0,d3
		move.b	(a4)+,d3
		lea	CrTable,a0
		move.l	(a0,d3.w*4),d4			;d4:[Cr0-G,Cr0-R]

		move.b	(a3)+,d3
		lea	CbTable,a0
		move.l	(a0,d3.w*4),d3			;d3:[Cb0-G,Cb0-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		lea	512+Clamp256,a0
		moveq	#0,d6
		move.b	(a2),d6				;d6:[Y0]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R0]
		swap	d7
							;d7:[--,R0,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R0,G0,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R0,G0,B0]

		move.l	d7,(a1)				;store first pixel

		moveq	#0,d6
		move.b	1(a2),d6				;d6:[Y1]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R1]
		swap	d7
							;d7:[--,R1,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R1,G1,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R1,G1,B1]

		move.l	d7,4(a1)			;store 2. pixel - modulo ok!


		moveq	#0,d6
argb32width0	move.b	160(a2),d6			;d6:[Y4]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R4]
		swap	d7
							;d7:[--,R4,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R4,G4,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R4,G4,B4]

argb32modulo0	move.l	d7,160(a1)				;store 5. pixel


		moveq	#0,d6
argb32width1	move.b	161(a2),d6			;d6:[Y5]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R5]
		swap	d7
							;d7:[--,R5,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R5,G5,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R5,G5,B5]

argb32modulo1	move.l	d7,160+4(a1)				;store 6. pixel


		moveq	#0,d3
		move.b	(a4)+,d3
		lea	CrTable,a0
		move.l	(a0,d3.w*4),d4			;d4:[Cr1-G,Cr1-R]

		move.b	(a3)+,d3
		lea	CbTable,a0
		move.l	(a0,d3.w*4),d3			;d3:[Cb1-G,Cb1-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		lea	512+Clamp256,a0
		moveq	#0,d6
		move.b	2(a2),d6				;d6:[Y2]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R2]
		swap	d7
							;d7:[--,R2,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R2,G2,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R2,G2,B2]

		move.l	d7,8(a1)			;store 3. pixel - modulo ok!


		moveq	#0,d6
		move.b	3(a2),d6				;d6:[Y3]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R3]
		swap	d7
							;d7:[--,R3,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R3,G3,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R3,G3,B3]

		move.l	d7,12(a1)			;store 4. pixel - modulo ok!



		moveq	#0,d6
argb32width2	move.b	162(a2),d6			;d6:[Y6]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R6]
		swap	d7
							;d7:[--,R6,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R6,G6,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R6,G6,B6]


argb32modulo2	move.l	d7,160+8(a1)			;store 7. pixel

		moveq	#0,d6
argb32width3	move.b	163(a2),d6			;d6:[Y7]

		move.w	d6,d0
		add.w	d4,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,--,R7]
		swap	d7
							;d7:[--,R7,--,--]
		move.w	d6,d0
		sub.w	d5,d0
		move.w	(a0,d0.w),d7			;d7:[--,R7,G7,--]

		add.w	d3,d6
		move.b	(a0,d6.w),d7			;d7:[--,R7,G7,B7]

argb32modulo3	move.l	d7,160+12(a1)			;store 8. pixel
		lea	16(a1),a1			;skip 4 pixels
		addq.l	#4,a2
		;--------------------
		move.l	a5,d0
		subq.l	#1,d0
		bne.w	p96_argb32_loopx
		add.l	bitmap_add,a1
		add.l	source_y_skip,a2
		add.l	source_c_skip,a3
		add.l	source_c_skip,a4
		subq.l	#1,d1
		bne.w	p96_argb32_loopy
	ENDC	;APOLLO_YUYV

		rts


	ifne	CODE_ALIGN
		CNOP    0,16
	endc


mpr_bgra32	move.l	GfxMemBase-VDEC_BASE(A5),a1
		move.l	BitmapModulo-VDEC_BASE(A5),d7

		lsl.l	#1,d7
		move.l	YUYVCols.l(pc),d6			;width(pc),d6
		move.l	d6,d0
		and.l	#$fffffffc,d0			;align to 4-divisible width
		asl.l	#2,d0				;this is how many bytes will be written/line! ?
		sub.l	d0,d7				;subtract from modulo to find no. of bytes to skip each line!
		move.l	d7,bitmap_add-VDEC_BASE(A5)	;this one is same like with the bgr24

		lsr.l	#2,d6				;width/4 no. of loops (because 1 long write per loop)
		move.l	d6,no_of_loops

		move.l	d6,d1				;Calculate no. of bytes to skip in source after lines (tricky!)
		lsl.l	#2,d1				;framewidth 4-byte aligned

		move.l	y_bitmap_width(pc),d5		;d5 = no. of bytes to skip in input after each line!
		sub.l	d1,d5
		move.l	d5,d6
		add.l	y_bitmap_width(pc),d5
		move.l	d5,source_y_skip-VDEC_BASE(A5)
		lsr.l	#1,d6
		move.l	d6,source_c_skip-VDEC_BASE(A5)
		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		add.l	YUYVTLOffY.l(pc),a2
		add.l	YUYVTLOffC.l(pc),a3
		add.l	YUYVTLOffC.l(pc),a4

		move.l	YUYVRows.l(pc),d1			;height(pc),d1
		lsr.l	#1,d1
p96_bgra32_loopy

		move.l	no_of_loops(pc),d0
p96_bgra32_loopx
		move.l	d0,a5
		;------------------;free:d2,a5,a6
		moveq	#0,d3
		move.b	(a4)+,d3
		lea	CrTable,a0
		move.l	(a0,d3.w*4),d4			;d4:[Cr0-G,Cr0-R]

		move.b	(a3)+,d3
		lea	CbTable,a0
		move.l	(a0,d3.w*4),d3			;d3:[Cb0-G,Cb0-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		lea	512+Clamp256,a0
		moveq	#0,d6
		move.b	(a2),d6				;d6:[Y0]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B0,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B0,G0]
		swap	d7				;d7:[B0,G0,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B0,G0,R0,--]

		move.l	d7,(a1)				;store first pixel

		moveq	#0,d6
		move.b	1(a2),d6				;d6:[Y1]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B1,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B1,G1]
		swap	d7				;d7:[B1,G1,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B1,G1,R1,--]

		move.l	d7,4(a1)			;store 2. pixel - modulo ok!


		moveq	#0,d6
bgra32width0	move.b	160(a2),d6			;d6:[Y4]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B4,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B4,G4]
		swap	d7				;d7:[B4,G4,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B4,G4,R4,--]


bgra32modulo0	move.l	d7,160(a1)				;store 5. pixel


		moveq	#0,d6
bgra32width1	move.b	161(a2),d6			;d6:[Y5]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B5,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B5,G5]
		swap	d7				;d7:[B5,G5,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B5,G5,R5,--]


bgra32modulo1	move.l	d7,160+4(a1)				;store 6. pixel


		moveq	#0,d3
		move.b	(a4)+,d3
		lea	CrTable,a0
		move.l	(a0,d3.w*4),d4			;d4:[Cr1-G,Cr1-R]

		move.b	(a3)+,d3
		lea	CbTable,a0
		move.l	(a0,d3.w*4),d3			;d3:[Cb1-G,Cb1-B]
		move.l	d3,d5
		swap	d5
		move.l	d4,d6
		swap	d6
		add.w	d6,d5				;d5 = Chrom for G

		lea	512+Clamp256,a0
		moveq	#0,d6
		move.b	2(a2),d6				;d6:[Y2]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B2,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B2,G2]
		swap	d7				;d7:[B2,G2,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B2,G2,R2,--]

		move.l	d7,8(a1)			;store 3. pixel - modulo ok!


		moveq	#0,d6
		move.b	3(a2),d6				;d6:[Y3]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B3,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B3,G3]
		swap	d7				;d7:[B3,G3,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B3,G3,R3,--]

		move.l	d7,12(a1)			;store 4. pixel - modulo ok!



		moveq	#0,d6
bgra32width2	move.b	162(a2),d6			;d6:[Y6]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B6,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B6,G6]
		swap	d7				;d7:[B6,G6,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B6,G6,R6,--]



bgra32modulo2	move.l	d7,160+8(a1)			;store 7. pixel

		moveq	#0,d6
bgra32width3	move.b	163(a2),d6			;d6:[Y7]

		move.w	d6,d0
		add.w	d3,d0
		move.w	(a0,d0.w),d7			;d7:[--,--,B7,--]

		move.w	d6,d0
		sub.w	d5,d0
		move.b	(a0,d0.w),d7			;d7:[--,--,B7,G7]
		swap	d7				;d7:[B7,G7,--,--]

		add.w	d4,d6
		move.w	(a0,d6.w),d7			;d7:[B7,G7,R7,--]


bgra32modulo3	move.l	d7,160+12(a1)			;store 8. pixel
		lea	16(a1),a1			;skip 4 pixels
		addq.l	#4,a2
		;--------------------
		move.l	a5,d0
		subq.l	#1,d0
		bne.w	p96_bgra32_loopx
		add.l	bitmap_add,a1
		add.l	source_y_skip,a2
		add.l	source_c_skip,a3
		add.l	source_c_skip,a4
		subq.l	#1,d1
		bne.w	p96_bgra32_loopy
		rts

		EVEN
firsttime:	dc.l	0

	ifne	CODE_ALIGN
		CNOP    0,16
	endc












mpr_YUV422:
		tst.l	PlanarAssistance-VDEC_BASE(A5)
		beq	noplanarrenderstuff

		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	BitmapModulo-VDEC_BASE(A5),d7	;dest modulo
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4

		lea	MyYUVInfo(pc),a0
		move.l	a2,(yi_MemoryY,a0)

		move.l	a3,(yi_MemoryU,a0)
		move.l	a4,(yi_MemoryV,a0)

		move.l	y_bitmap_width(pc),d0		;y width

		move.w	d0,(yi_BytesPerRowY,a0)
		asr.l	#1,d0				;/2
		move.w	d0,(yi_BytesPerRowU,a0)
		move.w	d0,(yi_BytesPerRowV,a0)
		move.l	#YUVFB_YUV12,(yi_YUVFormat,a0)
		clr.l	(yi_Flags,a0)

		or.l	#(1<<31),(yi_Flags,a0)	; Planar Assist test flag

		moveq.l	#0,d0
		moveq.l	#0,d1
		moveq.l	#0,d2
		moveq.l	#0,d3
		move.l	y_bitmap_width(pc),d4
		move.l	height(pc),d5
		lea	MyYUVInfo(pc),a0
		move.l	p96PIPrport-VDEC_BASE(A5),a1
		suba.l	a2,a2
		movea.l	p96base,a6
		jsr	_LVOp96WriteYUVPixels(a6)

		rts


noplanarrenderstuff:

	IFNE APOLLO_YUYV

	ifne	APOLLO_NSAGABUFS

;	movem.l	required_time,d1-d2		;CIA timer value for display "appointment"
; TODO: check CIA whether time is elapsed before activating timer

	endc

	; FIXME: decision whether to enable RGB565 YUYV mode as
	;        single scan or doublescan mode
	; this iteration tries to be as close to vampiregfx driver as possible
	;
		move.w  #$0006,d1
		move.l	YUYVScrWidth,d0		;move.l	y_bitmap_width(pc),d0
		cmp.w	#499,d0
		bgt.b   .nodbl
		move.w  #$0306,d1		;double X / Y
		move.l	YUYVScrHeight,d2	;move.l	y_bitmap_height(pc),d0
		cmp.w	#299,d2
		blt.b	.nodbl
		move.w  #$0106,d1		;double X only
.nodbl:		move.w  d1,$dff1f4

		lea	VidBuf_TMRSig,a2
	ifne	APOLLO_NSAGABUFS
	 ifne	APOLLO_NSAGA_TD
		tst.b	VidBuf_TMRLock-VidBuf_TMRSig(a2)
		bne.s	.tmrlocked

		moveq	#APOLLO_NSAGABUFS-1,d1
		mulu	#VIDBUF_SIZE,d1			; buffer index * struct size
		lea     VidBuf_Store-VidBuf_TMRSig(a2,d1.l),a1
		move.l	VIDBUF_Y(a1),a1			; dest ptr
		move.l  a1,$DFF1EC               	; Update SAGA video register
.tmrlocked:
	 endc
		move.l	VidBuf_RVA-VidBuf_TMRSig(a2),d0
		move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d2
		sub.l	d2,d0
		cmp.l	#APOLLO_NSAGABUFS-APOLLO_NSAGABUFK,d0 ; total - kept buffers (i.e. don`t overwrite last shown when SAGABUFK=1)
		blt	.freebuf
		; all buffers occupied, wait now

		tst.b	VidBuf_TMRLock-VidBuf_TMRSig(a2)
		bne.s	.normaltimerwait

		; we didn't hear from audio yet (pun intended)
		; disable startup waiting and enable timer
		st	VidBuf_TMRLock-VidBuf_TMRSig(a2)
		CALLEXEC        Forbid
		bsr	CorrectVidBufTimer
		move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d2
		bsr	GetVidBufTimer
		ifne	DEBUG_TIMING
			bsr	TMRDebugD1D2
		endc
		st      VBTimerRunning-VidBuf_TMRSig(a2)
		bsr	StartYUYVTimer
		CALLEXEC        Permit

		DOUTTXT	TMRLockWait

.normaltimerwait:
		st	VBTimerWaiting-VidBuf_TMRSig(a2)
.timeron1:
		cmp.l	VidBuf_TMR-VidBuf_TMRSig(a2),d2	;timer has cleared a buffer ?
		bne.s	.freebuf

		tst.b	VBTimerRunning-VidBuf_TMRSig(a2)
		beq.s	.timeroff1			; active timer ?

		ifne	DEBUG_TIMING		
		DOUTTXT	TMRWaitBuf
		bsr	TMRDebug
		endc	;DEBUG_TIMING

		move.l	VidBuf_TMRSig-VidBuf_TMRSig(a2),d0
		or.l	#SIGBREAKF_CTRL_C,d0
                CALLEXEC Wait

		bra.s	.timeron1
.timeroff1:
		DOUTTXT	TMRoff

		; what shall we do: buffers are full but no timer running ?
		; reset buffers, start anew
		move.l	VidBuf_RVA-VidBuf_TMRSig(a2),d0
		move.l	d0,VidBuf_TMR-VidBuf_TMRSig(a2)
.freebuf:
		sf	VBTimerWaiting-VidBuf_TMRSig(a2) ; we are not waiting

		move.l	VidBuf_RVA-VidBuf_TMRSig(a2),d0 ; current "write" position
		and.l	#APOLLO_NSAGABUFS-1,d0		; 

		mulu	#VIDBUF_SIZE,d0			; buffer index * struct size
		lea     VidBuf_Store-VidBuf_TMRSig(a2,d0.l),a3
		move.l	VIDBUF_Y(a3),a1			; dest ptr
		movem.l	a2/a3,-(sp)
	else
		move.l  YUYVBufPtr1,a1                  ; dest
	endc
		add.l   YUYVScrYOffset,a1		; actually, this is both horizontal and vertical centering of drawing position
		move.l	y_bitmap_base(pc),a2		; source a2

		move.l	YUYVScrWidth,d7
		lsl.l	#1,d7

		move.l	cb_bitmap_base,a3		; Cb source ptr A3
		move.l	cr_bitmap_base,a4		; Cr source ptr A4

		; top/left offsets to center cropped pictures
		add.l	YUYVTLOffY,a2
		add.l	YUYVTLOffC,a3
		add.l	YUYVTLOffC,a4

;SCR PTR			(a1)
;SCR STRIDE - copied words	(a6)

		moveq	#-4,d5
		and.l	YUYVCols,d5	; number of columns Y
		move.l	d7,a6		; d7: screen stride total (for second line)
		add.l	d7,a6		; skip one line (two are written in total
		sub.l	d5,a6		; SCR_STRIDE screen width - number of columns copied
		sub.l	d5,a6		; two times (16 bit per pixel)

		move.l	y_bitmap_width,d1	;source width (y plane) - for second line
		move.l	d1,a5
		add.l	d1,a5
		sub.l	d5,a5		; Y stride = width - pixels copied			

		move.l	y_bitmap_width,d3	;source width (y plane)
		sub.l	d5,d3		; Y stride = width - pixels copied			
		lsr.l	#1,d3		; chroma stride
		;move.l	d3,a0		; keep chroma stride
		subq.l	#4,d3		; -4 (1 extra read in loop)
		move.l	d3,a0
		move.l	d3,a0		; keep chroma stride

		move.l	YUYVRows,d4
		lsr.l	#1,d4
		subq.l	#1,d4

		asr.l	#3,d5
		subq.l	#1,d5

mpr_YUV422_loopx
		move.l	(a3)+,d3	;x x x x U0 U1 U2 U3 (see subq.l #4,d3 above)
		move.l	d5,d6
		move.l	(a4)+,d0	;x x x x V0 V1 V2 V3 
mpr_YUV422_loopy:
		; ==== PERM64 ===========================================
		vperm	#$4567cdef,d3,d0,e17 ;VPERMiBBB $4567cdef,3,4,1  ; VPERM ...,B3,B4,B1 = U0 U1 U2 U3 V0 V1 V2 V3
		move.l	(a2)+,d0           ; Y00 Y01 Y02 Y03
		vperm	#$C0D4E1F5,e17,d0,e18 ; VPERMiBDB $C0D4E1F5,1,0,2 ; VPERM ...,B1,D0,B2 = Y00 U0 Y01 V0 Y02 U1 Y03 V1
		 move.l  -4(a2,d1.w),D2    ; Y10 Y11 Y12 Y13
		store	e18,(a1)+	   ; STOREApB   1,2
		 vperm	#$C0D4E1F5,e17,d2,e18 ;VPERMiBDB $C0D4E1F5,1,2,2 ; VPERM ...,B1,D2,B2 = Y10 U0 Y11 V0 Y12 U1 Y13 V1
		 move.l	(a2)+,d0	   ; Y04 Y05 Y06 Y07
		STORE	e18,-8(a1,d7.w)     ; STOREd8ADB -8,1,7,2
		 vperm	#$C2D6E3F7,e17,d0,e18 ;VPERM ...,B1,D0,B2 = Y04 U2 Y05 V2 Y06 U3 Y07 V3
		 move.l	-4(a2,d1.w),D2     ; Y14 Y15 Y16 Y17
		STORE	e18,(a1)+           ; STOREApB  1,2
		 vperm	#$C2D6E3F7,e17,d2,e18 ; VPERMiBDB $C2D6E3F7,1,2,2 ; VPERM ...,B1,D2,B2 = Y14 U2 Y15 V2 Y16 U3 Y17 V3
		 move.l	(a3)+,d3
		STORE	e18,-8(a1,d7.w)     ;STOREd8ADB -8,1,7,2 
		 move.l	(a4)+,d0           ; this one could well be on top of this loop but I keep it here for "d3"
		
		dbf	d6,mpr_YUV422_loopy
		
		add.l	a5,a2
		add.l	a6,a1
		 add.l	a0,a3
		 add.l	a0,a4
		
		dbf	d4,mpr_YUV422_loopx
		
		; ==============================================================
	ifne	APOLLO_NSAGABUFS
		movem.l	(sp)+,a2/a3	; a2: VidBuf_TMRSig, a3: current struct in VidBuf_Store

		GETECLOCK64 d3,d4	; current position in ext clock
	ifne	APOLLO_NSAGA_TD
		tst.b   VidBuf_TMRLock-VidBuf_TMRSig(a2)	; no timer lock on audio yet, don't shorten frame display time
		beq.s	.ok
	endc
		movem.l required_time-VidBuf_TMRSig(a2),d1-d2
		sub.l	d4,d2
		subx.l	d3,d1
		bge.s	.ok		; no dest time underflow

		move.l	frame_time-VidBuf_TMRSig(a2),d2 ;else display "shortly", frame_time/2
		lsr.l	#1,d2
		moveq	#0,d1
		add.l	d4,d2
		addx.l	d3,d1
		bra.s	.shortframe
.ok:
		movem.l	required_time-VidBuf_TMRSig(a2),d1-d2	;CIA timer value for display "appointment"
.shortframe:
		movem.l	d1-d2,VIDBUF_TimeStampH(a3)	;store dest timestamp

		moveq	#1,d1
		add.l	VidBuf_RVA-VidBuf_TMRSig(a2),d1
		move.l	d1,VidBuf_RVA-VidBuf_TMRSig(a2)	;store next index

		;start timer, if necessary
		tst.b	VBTimerRunning-VidBuf_TMRSig(a2)
		bne.s	.havetimer
		
		move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d2
		bsr	GetVidBufTimer

	ifne	APOLLO_NSAGA_TD
		;before starting timer, check for a number of conditions
		;->
		;
		tst.b	VidBuf_TMRLock-VidBuf_TMRSig(a2)
		bne.s	.normal
		tst.l	(GlobalAudioEnable.l,pc)
		seq	VidBuf_TMRLock-VidBuf_TMRSig(a2)	;audio not enabled -> skip timer lock mechanism
		beq.s	.normal
		; OK, audio is enabled, so let's wait for feedback from Audio before starting
		; timer prematurely
		tst.l	AudioIsPlaying(pc)
		beq.s	.havetimer				;audio thread did decode first block of frames ? no->don't start timer (yet)
		st	VidBuf_TMRLock-VidBuf_TMRSig(a2)	;disable startup mechanism

		CALLEXEC	Forbid
		bsr	CorrectVidBufTimer
		move.l	VidBuf_TMR-VidBuf_TMRSig(a2),d2
		bsr	GetVidBufTimer
		ifne	DEBUG_TIMING
			bsr	TMRDebugD1D2
		endc
		st	VBTimerRunning-VidBuf_TMRSig(a2)
		bsr	StartYUYVTimer
		CALLEXEC	Permit
		bra.s	.havetimer
.normal
	endc	
		ifne	DEBUG_TIMING
			OUTTXT	SPACE
			bsr	TMRDebugD1D2
		endc
		st	VBTimerRunning-VidBuf_TMRSig(a2)
		bsr	StartYUYVTimer
;TODO: verify timer, if late -> drop frames 
; 
;		tst.b	VBTimerWaiting-VidBuf_TMRSig(a2)
;		bne.s	.nosig
;		move.l	VidBuf_TMRSig-VidBuf_TMRSig(a2),d0
;		move.l	MainTask(pc),a1
;               CALLEXEC Signal
;.nosig


.havetimer:

	else
		move.l YUYVBufPtr1,a1           ; 1st FrameBuffer
		move.l YUYVBufPtr2,a2           ; 2nd FrameBuffer
		move.l a2,YUYVBufPtr1           ; 2nd FrameBuffer to first
		move.l YUYVBufPtr3,a2		; 3rd FrameBuffer
		move.l a2,YUYVBufPtr2           ; 3rd FrameBuffer to second
		move.l a1,YUYVBufPtr3		
		move.l a1,$DFF1EC               ; Update SAGA video register
	endc

	ELSE	;/* APOLLO_YUYV */

		move.l	y_bitmap_base(pc),a2		;source a2
		move.l	BitmapModulo.l(pc),d7		;dest modulo
		move.l	cb_bitmap_base(pc),a3
		move.l	cr_bitmap_base(pc),a4
		move.l	GfxMemBase.l(pc),a1		;dest

		move.l	y_bitmap_width(pc),d1			;source width (y plane)
		moveq	#1,d5
		move.l	d1,d6
		and.l	d5,d6
		add.l	d1,d6
		move.l	d6,a5			;source_y_skip
		moveq	#3,d5
		and.l	d5,d6
		add.l	d7,d6
		move.l	d6,a6			;bitmap_add

		move.l	height(pc),d4
		asr.l	#1,d4			;/2 (yuv411 miatt)

		move.l	a0,-(sp)
		lea		(a2,d1.w),a0	;source y2

		moveq.l	#$00,d5			;u/v
		 asr.l	#1,d1			;width/2

mpr_YUV422_loopx

		move.l	d1,d6			;source width/2

mpr_YUV422_loopy

		move	(a2)+,d2		;xx xx y0 y1
		move	(a0)+,d3		;xx xx y2 y3

		move.b	(a3)+,d5		;u
		 lsl.l	#8,d2			;xx y0 y1 00
		swap	d5
		move.b	(a4)+,d5		;v
		 lsl.l	#8,d3			;xx y2 y3 00

		lsr.w	#8,d2			;xx y0 00 y1
		 lsr.w	#8,d3			;xx y2 00 y3
		 
		lsl.l	#8,d2			;y0 00 y1 00
		 lsl.l	#8,d3			;y2 00 y3 00

		or.l	d5,d2			;y0 u  y1 v
		 or.l	d5,d3			;y2 u  y3 v

		move.l	d3,(a1,d7.w)	;
		move.l	d2,(a1)+		;
		 subq.l	#1,d6
		bne.b	mpr_YUV422_loopy

		adda.l	a5,a2
		 adda.l	a5,a0
		adda.l	a6,a1
		 subq.l	#1,d4
		bne.b	mpr_YUV422_loopx

		move.l	(sp)+,a0

	ENDC	;/* APOLLO_YUYV */

		rts

	ifne	DEBUG_TIMING		

TMRDebugD1D2:
		OUTTXT	TMRStart
		OUTDEC	d1
		OUTTXT	SPACE
		OUTDEC	d2
		OUTTXT	RETURN
		rts

TMRDebug:
	movem.l	d0-a6,-(sp)

	OUTTXT	TMRpos

	move.l	VBTimerIO,d0
	beq.s	.notimer
	move.l	d0,a1
	move.l	IOTV_TIME+EV_HI(a1),d1
	OUTDEC	d1
	
	OUTTXT	SPACE
	
	move.l	IOTV_TIME+EV_LO(a1),d1
	OUTDEC	d1

	OUTTXT	TMRpos2

	GETECLOCK64 d1,d2
	OUTDEC	d1
	OUTTXT	SPACE
	OUTDEC	d2

.notimer
	OUTTXT	RETURN	
	
	movem.l	(sp)+,d0-a6
	rts
TMRStart:	dc.b	"starting timer for ",10,0
TMRLockWait	dc.b	"waiting for timer lock",10,0
TMRWaitBuf	dc.b	"waiting for free buffer",10,0
TMRoff		dc.b	"timer off",10,0
TMRpos		dc.b	"tmr pos ",0
TMRpos2		dc.b	"cur_eclock ",0
	even
	endc	;DEBUG_TIMING
	
	ifeq	APOLLO_P96ONLY
		include	RendererAGAC2P.i
	endc


; constants that are accessed only once, keep out of reach of active cachelines

cos_constants:		dc.w	91,126,118,106,91,71,49,25	;cu/2*cos(0*Pi/16),cu/2*cos(1*Pi/16),etc.
intra_quant_matrix:	dc.w	8,16,19,22,26,27,29,34
			dc.w	16,16,22,24,27,29,34,37
			dc.w	19,22,26,27,29,34,34,38
			dc.w	22,22,26,27,29,34,37,40
			dc.w	22,26,27,29,32,35,40,48
			dc.w	26,27,29,32,35,40,48,58
			dc.w	26,27,29,34,38,46,56,69
			dc.w	27,29,35,38,46,56,69,83
nonintra_quant_matrix:	dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16
			dc.w	16,16,16,16,16,16,16,16

MyDragGadget		dc.l 0                 ; gg_NextGadget
			dc.w 0                 ; gg_LeftEdge
			dc.w 0                 ; gg_TopEdge
			dc.w 0                 ; gg_Width   *** is poked at start
			dc.w 0                 ; gg_Height  *** is poked at start
			dc.w GFLG_GADGHNONE    ; gg_Flags
			dc.w 0                 ; gg_Activation
			dc.w GTYP_WDRAGGING    ; gg_GadgetType
			dc.l 0                 ; gg_GadgetRender
			dc.l 0                 ; gg_SelectRender
			dc.l 0                 ; gg_GadgetText
			dc.l 0                 ; gg_MutualExclude
			dc.l 0                 ; gg_SpecialInfo
			dc.w 0                 ; gg_GadgetID
			dc.l 0                 ; gg_UserData

VBtimer_name:	 dc.b	"timer.device",0
		even


				SECTION	data,bss
STARTBSS:
dosbase:		dc.l	0
intbase:		dc.l	0
gfxbase:		dc.l	0
p96base:		dc.l	0
timerbase:		dc.l	0
aslbase:		dc.l	0
iconbase:		dc.l	0
mpegabase:		dc.l	0

WBStartMsg:		dc.l	0
filename:		dc.l	0

AslFileReq:		dc.l	0
AslFileName		dc.l	0
AslDrawer:		dc.l	0
DoAslFlag:		dc.b	0
AudioFilterBit:		dc.b	0
_UNUSED_BYTES_XXX	dc.b	0,0

EmptyDrawer:		dc.l	0

ttval_AUDIOQUALITY	dc.l	0
ttval_AUDIOFREQDIV	dc.l	0

			EVEN
p96_switch:		dc.l	0
cgx_switch:		dc.l	0
VGA_switch:		dc.l	0
AGA_switch:		dc.l	0
RTG_switch:		dc.l	0	;RTG_switch is to force WCP on AGA (for CD32 Akiko).
AHI_switch:		dc.b	0
P14_switch:		dc.b	0
P16_switch:		dc.b	0
			dc.b	0	;unused
LOOP_switch:		dc.l	0
FPS_value:		dc.l	0
NOSKIP_switch:		dc.l	0
verbose_switch:		dc.l	0
half_switch:		dc.l	0
ZOOM_value:		dc.l	0
NOVIDEO_switch:		dc.l	0
NORENDER_switch:	dc.l	0
SAVEAUDIO_name:		dc.l	0
MONOSURROUND_switch:	dc.l	0
NOP_switch:		dc.l	0
NOB_switch:		dc.l	0
BORDERLESS_switch	dc.l	0

WinFullToggle:		dc.l	0
WinNormalZoom:		dc.l	0

argers:			ds.l	33
dosarger:		dc.l	0

RiVADiskObject:		dc.l	0

StdOut:			dc.l	0
OldStdOut:		dc.l	0
filelock:		dc.l	0
filehandle:		dc.l	0
fileinfo:		dc.l	0
file_size:		dc.l	0
file_position:		dc.l	0

vbuf_active:		dc.l	0		;Current Video Buffer Base Address
abuf_active:		dc.l	0
vbuf_back:		dc.l	0		;reference (previous) vbuf
abuf_back:		dc.l	0
vbuf_position:		dc.l	0
abuf_position:		dc.l	0
vbuf_1:			dc.l	0		;Video Buffer 1 base address
vbuf_2:			dc.l	0		;Video Buffer 2 base address
abuf_1:			dc.l	0
abuf_2:			dc.l	0
vbuf_max:		dc.l	0
abuf_max:		dc.l	0

vbuf_end:		dc.l	0
last_a0:		dc.l	0
last_d4:		dc.l	0

sysbuf:			dc.l	0
sbuf_max:		dc.l	0
sbuf_pointer:		dc.l	0
SystemParseMode:	dc.l	0

vidbufnew:		dc.l	0
vidbufold:		dc.l	0

audiobuffer:		dc.l	0
audiobuffer_size:	dc.l	0

TimerPort:		dc.l	0
TimerIO:		dc.l	0
TimerClosed:		dc.l	1
TimerStruct:		dc.l	0,0

GfxSystem:		dc.l	0
p96_bitmaplock:		dc.l	0
p96RenderInfo:		ds.l	12

DitherHelpBuf		ds.l	32

			EVEN
DitherMode		dc.l	0

bitlock:		dc.l	0

GrayMode:		dc.l	0

PubScreen:		dc.l	0
PubScreenDepth:		dc.l	0
PubScreenColorFmt:	dc.l	0

p96WinPlayBitMap	dc.l	0

PIPerror:		dc.l	0		;detailed PIP error code

wincenterw:		dc.l	0
wincenterh:		dc.l	0

intra_quant_matrix_zz:		ds.w	64		;these two hold the quantization matrices in zigzag order
nonintra_quant_matrix_zz:	ds.w	64
dct_linepopulation:		ds.b	8		;highest coeff in each line of dct_zz
dct_zz:				ds.w	64


VDEC_BASE:
stream:				dc.l	0
bitoffset:			dc.l	0
pel_aspect_ratio:		dc.l	0
picture_rate:			dc.l	0		;video rate code (0-15)
vid_rate:			dc.l	0		;actual video rate (ie. 30fps)
bit_rate:			dc.l	0
vbv_buffer_size:		dc.l	0
constrained_param_flag:		dc.l	0
drop_frame_flag:		dc.l	0
time_code_hours:		dc.l	0
time_code_minutes:		dc.l	0
gop_marker_bit:			dc.l	0
time_code_seconds:		dc.l	0
time_code_pictures:		dc.l	0
closed_gop:			dc.l	0
broken_link:			dc.l	0
temporal_reference:		dc.l	0
picture_coding_type:		dc.l	0
picture_start_address:		dc.l	0
max_pic_size:			dc.l	0
vbv_delay:			dc.l	0

block_buffer_tmp_y1:		dc.l	block_ref_y1
;motion vector variables, CBP and other macroblock local stuff
full_pel_forward_vector:	dc.b	0
full_pel_backward_vector:	dc.b	0
forward_r_size:			dc.b	0
backward_r_size:		dc.b	0
DCT_P_CLEAR:			dc.b	0	;.b
macroblock_intra:		dc.b	0	;.b
coded_block_pattern:		dc.b	0	;.b
				dc.b	0	;unused

mv_fwd_xy_long:					;keep these 3 together
mv_fwd_x			dc.w	0	;
mv_fwd_y			dc.w	0	;

mv_bwd_xy_long:					;keep these 3 together
mv_bwd_x			dc.w	0	;
mv_bwd_y			dc.w	0	;

fwd_ref_frame_number:		dc.l	0
next_back_ref_frame_number:	dc.l	0
back_ref_frame_number:		dc.l	0

slice_vertical_position:	dc.l	0
quantizer_scale:		dc.l	0	;TODO: .w
MB_Type				dc.l	0
MB_Address:			dc.l	0
previous_MB_Address:		dc.l	0
last_Macroblock:		dc.l	0

	;keep these 4 offsets together
MB_y_fwd_BitmapOffset:		dc.l	0
MB_c_fwd_BitmapOffset:		dc.l	0
MB_y_bwd_BitmapOffset:		dc.l	0
MB_c_bwd_BitmapOffset:		dc.l	0

MB_address_increment:		dc.l	0
MB_total:			dc.l	0
MB_x_total:			dc.l	0
MB_Abort:			dc.l	0
MB_c_fwd_dydx:			dc.l	0		;.b current MB chroma vector: fractional components dy << 1 + dx
MB_c_bwd_dydx:			dc.l	0		;.b current MB chroma vector: fractional components dy << 1 + dx

dct_dc_y_past:			dc.l	0
dct_dc_cb_past:			dc.l	0
dct_dc_cr_past:			dc.l	0
block_count:			dc.l	0
fwd_reference_y:		dc.l	0
fwd_reference_cb:		dc.l	0
fwd_reference_cr:		dc.l	0
bwd_reference_y:		dc.l	0
bwd_reference_cb:		dc.l	0
bwd_reference_cr:		dc.l	0
queued_reference_y:		dc.l	0
queued_reference_cb:		dc.l	0
queued_reference_cr:		dc.l	0
queued_frame_number:		dc.l	0
queued_timestamp:		dc.l	0,0


add_8_rows:			dc.l	0
source_y_skip:			dc.l	0
source_c_skip:			dc.l	0
bitmap_add:			dc.l	0
BitmapModulo:			dc.l	0
doublewidth:			dc.l	0
mc_offsets_buffered:		ds.b	MC_DATA_SIZE	;perform MC into temporary buffer for later adding of coefficients
mc_offsets_direct:		ds.b	MC_DATA_SIZE	;perform MC directly into output frame

	ifne	CODE_ALIGN
		CNOP    0,16
	endc
idct_modulo_add:		dc.l	0,0,0,0,0,0

start_of_anim:			dc.l	0
pictures_played:		dc.l	0
pictures_skipped:		dc.l	0
pictures_total:			dc.l	0
frame_number:			dc.l	0
actual_frame_number:		dc.l	0
;GOP_frame_number:		dc.l	0
GOP_base_frame_number:		dc.l	0

e_count_rate:			dc.l	709379
e_clock_start:			dc.l	0
e_clock_stop:			dc.l	0
e_clock_time:			dc.l	0
e_clock_correction:		dc.l	0
e_clock_millisec:		dc.l	0
frame_time:			dc.l	0
max_lag:			dc.l	0
max_lead:			dc.l	0
last_frame_skipped:		dc.l	0
afterskip_mode:			dc.l	0
previous_relative_time:		dc.l	0

timer_data:
required_time:			dc.l	0,0	;required_time
actual_time:			dc.l	0,0	;actual_time
last_eclock:			dc.l	0,0	;last_eclock
decode_eclock:			dc.l	0,0
last_frame_decode_time:		dc.l	0,0
audio_time:			dc.l	0,0	;audio replay time
audio_timeoffset:		dc.l	0,0	;offset for audio time (at start)
time_seconds_h:			dc.l	0
time_seconds_l:			dc.l	0
average_fps_h:			dc.l	0
average_fps_l:			dc.l	0
displayed_fps_h:		dc.l	0
displayed_fps_l:		dc.l	0

PictureReconFlag:		dc.l	0

result:				dc.l	0

CLIModeRequest:			dc.l	0

P96ScreenHandle:	dc.l	0
p96PIPBitmap:		dc.l	0
p96PIPrport:		dc.l	0
PlanarAssistance:	dc.l	0
p96PIPWinHandle:	dc.l	0

			EVEN
DisplayInfoBuf:		ds.l	24

			EVEN
ScreenRastport:		dc.l	0
GfxMemBase:		dc.l	0
Palette:		ds.l	2+3*256

MainWindow:		dc.l	0
apollo_active:		dc.b	0		;apollo_active.b != 0 means the move.l (a4),B0  command was executed successfully
PauseFlag		dc.b	0		;0=no pause, !=0 = pause
UNUSED_BYTES111:	ds.b	2		;free to use for something

AbortFlag:		dc.l	0
CtrlCAbort:		dc.l	0

	ifne	CODE_ALIGN
		CNOP    0,16
	endc
; Aligned Framebuffers
FrameBuffer1:			dc.l	0
FrameBuffer1_Cb:		dc.l	0
FrameBuffer1_Cr:		dc.l	0
FrameBuffer2:			dc.l	0
FrameBuffer2_Cb:		dc.l	0
FrameBuffer2_Cr:		dc.l	0
FrameBuffer3:			dc.l	0
FrameBuffer3_Cb:		dc.l	0
FrameBuffer3_Cr:		dc.l	0
total_bitmap_size:		dc.l	0
framebuf_toggle:		dc.l	0
FrameBufferPointers:		ds.l	3	; allocated pointers, the aligned ones are not suitable for FreeMem
FrameBufferAllocSize:		dc.l	0

Addr_LookupTables:		dc.l	0
lookup_MB_address:		dc.l	0
lookup_MB_type_P:		dc.l	0
lookup_MB_type_B:		dc.l	0
lookup_block_pattern:		dc.l	0
lookup_motion_vector:		dc.l	0
lookup_DCT_size_lum:		dc.l	0
lookup_DCT_size_chrom:		dc.l	0
lookup_DCT_coeff:		dc.l	0
convert_to_bitmap:		dc.l	0
predict_clamp:			dc.l	0
YUVtoHiColorTable:		dc.l	0
YUVtoBGGRTable:			dc.l	0
YUV_BG_Table:			dc.l	0
YUV_GR_Table:			dc.l	0



		EVEN
YUYVScrWidth	dc.l	0               ; SAGA Screen Width
YUYVScrHeight	dc.l	0		; SAGA Screen Height
YUYVScrYOffset	dc.l	0		; SAGA Screen YOffset for Vertical Centering
YUYVBufPtr      dc.l    0               ; SAGA Triple Buffer Address (MemAlloc)
YUYVBufSize     dc.l    0               ; SAGA Triple Buffer Size in bytes (MemAlloc)
	ifeq	APOLLO_NSAGABUFS
YUYVBufPtr1     dc.l    0               ; 32-Bytes Aligned FrameBuffer #1
YUYVBufPtr2     dc.l    0               ; 32-Bytes Aligned FrameBuffer #2
YUYVBufPtr3     dc.l    0               ; 32-Bytes Aligned FrameBuffer #3
	endc

		DCT_COUNTERS	; MacrosDCTCount.m

YUYVTLOffY	dc.l	0		; if video is bigger than screen, offset into picture
YUYVTLOffC	dc.l	0		; for luma/chroma
YUYVCols	dc.l	0		; number of columns to write on screen
YUYVRows	dc.l	0		; number of rows to write on screen

		ifne	APOLLO_NSAGABUFS
VidBuf_TMRLock:	ds.b	1		;timer locked in
		ds.b	3		;unused
VidBuf_TMRSig:	ds.l	1		;signal from timer to main thread that a buffer just got free
VidBuf_TMR:	ds.l	1		;position of "timer" in video buffer (count)
VidBuf_RVA:	ds.l	1		;position of main thread in video buffer (count)
VidBuf_Store:	ds.b	VIDBUF_SIZE*APOLLO_NSAGABUFS ; video buffer structures for queued SAGA output (static in here)
		ds.b	VIDBUF_SIZE

	*---------- Datenbereich für Timer.device Unterstützung -----------*
VBTimerDevice:	 ds.l	1
VBTimerPort:	 ds.l	1		;	MP_SIZE
VBTimerIO:	 ds.l	1		;VBTimerRequest	 ds.b	IOTV_SIZE
VBTimer_Lasttime ds.l	2		;EClock TimerVal Struktur
VBTimer_Timerval ds.l	1		;aktuelle Zeit
VBTimerInt:	 ds.b	IS_SIZE		;
VBTimerInit:	 ds.b	1
VBTimerRunning:	 ds.b	1		;timer is running ? 0xff/0x00
VBTimerIntFlag:	 ds.b	1		;0=off,1=on,2=stopped
VBTimerWaiting:	 ds.b	1		;
		 cnop	0,4
		endc

pcmptr0		dc.l	0
pcmptr1		dc.l	0
pcm_buffer_1	ds.w	MPEGA_PCM_SIZE	;keep this size (see copyAudioDev)
pcm_buffer_2	ds.w	MPEGA_PCM_SIZE
		ds.b	128		;better safe than sorry

block_ref_y1:			ds.b	64		;block_ref is to contain the referenced area from reference frame
block_ref_y2:			ds.b	64		;no direct reference to these lower 5 blocks in the code (!)
block_ref_y3:			ds.b	64
block_ref_y4:			ds.b	64
block_ref_cb:			ds.b	64
block_ref_cr:			ds.b	64

;BASE2:

my_easystruct:  ds.l    1       ;EASY-Request
                ds.l    1
		ds.l    1
	        ds.l    1
	        ds.l    1

PIPZoomData:		dc.w	0,0,0,0
PIPtitle:		ds.b	7			;dc.b	"RiVA - "
Windowfilename:		ds.b	250

filenamestring:			ds.b	4096		;to store asl filename with full path
WBProgName:			ds.b	256		;progdir & filename

EmptyPointer:	ds.w	4				;empty mouse pointer

;
; AUDIO TASK RELATED DATA
;
audiofile:			dc.l	0


; software YCbCr to RGB tables
CbTable:			ds.l	256
CrTable:			ds.l	256
Clamp256:			ds.b	1024

MB_y_OffsetTable:		ds.b	65536*4		;What the #$&%" ? THIS WILL FALL ON OUR FEET SOMEDAY ! -- bax
MB_c_OffsetTable:		ds.b	65536

ENDBSS:

