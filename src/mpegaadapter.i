; adapter macros: mpega doesn't provide ASM includes and everyone built his own ones...

        ifnd	MPAACC_FUNC
MPAACC_FUNC		EQU	MPA_func
MPAACC_READ_BUFFER	EQU	MPAR_buffer
MPAACC_READ_NUM_BYTES	EQU	MPAR_num_bytes
LVO_MPEGA_open		EQU	_LVOMPEGA_open
LVO_MPEGA_decode_frame	EQU	_LVOMPEGA_decode_frame
LVO_MPEGA_close		EQU	_LVOMPEGA_close

MPASTRM_FREQUENCY	EQU	MPS_frequency
MPASTRM_NORM		EQU	MPS_norm
MPASTRM_LAYER		EQU	MPS_layer
MPASTRM_MODE		EQU	MPS_mode
MPASTRM_BITRATE		EQU	MPS_bitrate
	endc


