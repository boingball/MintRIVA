//------------------------------------------------------------------------------
// Small picojpeg-compatible baseline JPEG decoder for MintAMP artwork.
//
// This file implements the public picojpeg entrypoints used by the Amiga GUI:
// pjpeg_decode_init() parses an in-memory JPEG supplied by the callback and
// pjpeg_decode_mcu() returns one decoded MCU at a time in the picojpeg MCU
// buffers.  It supports baseline sequential 8-bit grayscale and YCbCr JPEGs
// using H1V1, H2V1, H1V2, and H2V2 sampling, which matches the artwork path.
//------------------------------------------------------------------------------
#include "picojpeg.h"

#include <stdlib.h>
#include <string.h>
#include <stdlib.h>  /* MintRIVA: was miniamp_memguard.h (memguard macros unused) */

#define PJPG_MAX_COMPONENTS 4
#define PJPG_MAX_TABLES 4
#define PJPG_IN_CHUNK 128
#define PJPG_MARKER_NONE 0

typedef signed short pj_i16;
typedef unsigned short pj_u16;
typedef unsigned char pj_u8;

typedef struct {
	pj_u8 valid;
	pj_u8 bits[16];
	pj_u8 vals[256];
	pj_u16 firstCode[16];
	pj_u16 firstSym[16];
	pj_u16 numVals;
} PjHuffTable;

typedef struct {
	pj_u8 id;
	pj_u8 h;
	pj_u8 v;
	pj_u8 tq;
	pj_u8 td;
	pj_u8 ta;
	pj_i16 dc;
} PjComponent;


static const pj_u8 gZigZag[64] = {
	0, 1, 8,16, 9, 2, 3,10,17,24,32,25,18,11, 4, 5,
	12,19,26,33,40,48,41,34,27,20,13, 6, 7,14,21,28,
	35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
	58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
};

#define PJ_IDCT_CONST_BITS 8
#define PJ_IDCT_PASS1_BITS 2
#define PJ_AAN_SCALE_BITS 14
#define PJ_AAN_QUANT_BITS (PJ_AAN_SCALE_BITS - PJ_IDCT_PASS1_BITS)

static const int gAANScale[64] = {
	16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
	22725, 31521, 29692, 26722, 22725, 17855, 12299,  6270,
	21407, 29692, 27969, 25172, 21407, 16819, 11585,  5906,
	19266, 26722, 25172, 22654, 19266, 15137, 10426,  5315,
	16384, 22725, 21407, 19266, 16384, 12873,  8867,  4520,
	12873, 17855, 16819, 15137, 12873, 10114,  6967,  3552,
	 8867, 12299, 11585, 10426,  8867,  6967,  4799,  2446,
	 4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247
};

static const int gIdctFix1082 = 277;
static const int gIdctFix1414 = 362;
static const int gIdctFix1848 = 473;
static const int gIdctFix2613 = 669;

static pj_u8 *gJpegData;
static unsigned long gJpegSize;
static unsigned long gPos;
static unsigned long gScanPos;
static int gBitBuf;
static int gBitsLeft;
static int gMarker;
static int gWidth;
static int gHeight;
static int gNumComps;
static int gMaxH;
static int gMaxV;
static int gMcuW;
static int gMcuH;
static int gMcuPerRow;
static int gMcuPerCol;
static int gMcuIndex;
static int gRestartInterval;
static int gRestartLeft;
static int gNextRestart;
static int gReduce;
/* Adobe APP14 colour-transform flag: -1 none, 0 = CMYK (unchanged), 1 = YCbCr,
 * 2 = YCCK.  Only consulted for 4-component images to pick the CMYK vs YCCK
 * conversion; 3-component JPEGs stay plain YCbCr as before. */
static int gAppTransform;
static PjComponent gComp[PJPG_MAX_COMPONENTS];
static int gQuant[PJPG_MAX_TABLES][64];
static pj_u8 gQuantValid[PJPG_MAX_TABLES];
static PjHuffTable gHuff[2][PJPG_MAX_TABLES];
static pj_u8 gMCUBufR[256];
static pj_u8 gMCUBufG[256];
static pj_u8 gMCUBufB[256];

static int pj_read_byte(void)
{
	if (gPos >= gJpegSize)
		return -1;
	return gJpegData[gPos++];
}

static int pj_read_be16(void)
{
	int a = pj_read_byte();
	int b = pj_read_byte();
	if (a < 0 || b < 0)
		return -1;
	return (a << 8) | b;
}

static int pj_skip(unsigned long n)
{
	if (n > gJpegSize - gPos)
		return -1;
	gPos += n;
	return 0;
}

static int pj_next_marker(void)
{
	int c;
	do {
		c = pj_read_byte();
		if (c < 0)
			return -1;
	} while (c != 0xFF);
	do {
		c = pj_read_byte();
		if (c < 0)
			return -1;
	} while (c == 0xFF);
	while (c == 0) {
		do {
			c = pj_read_byte();
			if (c < 0)
				return -1;
		} while (c != 0xFF);
		do {
			c = pj_read_byte();
			if (c < 0)
				return -1;
		} while (c == 0xFF);
	}
	return c;
}

static int pj_read_entropy_byte(void)
{
	int c;
	if (gScanPos >= gJpegSize)
		return -1;
	c = gJpegData[gScanPos++];
	if (c == 0xFF) {
		int n;
		do {
			if (gScanPos >= gJpegSize)
				return -1;
			n = gJpegData[gScanPos++];
		} while (n == 0xFF);
		if (n != 0) {
			gMarker = n;
			return -1;
		}
	}
	return c;
}

static int pj_get_bit(void)
{
	int c;
	if (!gBitsLeft) {
		c = pj_read_entropy_byte();
		if (c < 0)
			return -1;
		gBitBuf = c;
		gBitsLeft = 8;
	}
	c = (gBitBuf >> 7) & 1;
	gBitBuf = (gBitBuf << 1) & 0xFF;
	gBitsLeft--;
	return c;
}

static int pj_get_bits(int n)
{
	int v = 0;
	while (n-- > 0) {
		int b = pj_get_bit();
		if (b < 0)
			return -1;
		v = (v << 1) | b;
	}
	return v;
}

static int pj_extend(int v, int t)
{
	int vt;
	if (!t)
		return 0;
	vt = 1 << (t - 1);
	if (v < vt)
		v += -(1 << t) + 1;   /* MintRIVA: was (-1<<t); same value, no UB warning */
	return v;
}

static unsigned char pj_build_huff(PjHuffTable *ht)
{
	int i;
	int code = 0;
	int sym = 0;
	for (i = 0; i < 16; i++) {
		ht->firstCode[i] = (pj_u16)code;
		ht->firstSym[i] = (pj_u16)sym;
		sym += ht->bits[i];
		code = (code + ht->bits[i]) << 1;
		if (code > 0x1FFFF)
			return PJPG_BAD_DHT_COUNTS;
	}
	ht->numVals = (pj_u16)sym;
	ht->valid = 1;
	return 0;
}

static int pj_huff_decode(const PjHuffTable *ht)
{
	int len;
	int code = 0;
	if (!ht->valid)
		return -1;
	for (len = 1; len <= 16; len++) {
		int b = pj_get_bit();
		int count;
		int first;
		int idx;
		if (b < 0)
			return -1;
		code = (code << 1) | b;
		count = ht->bits[len - 1];
		first = ht->firstCode[len - 1];
		if (count && code >= first && code < first + count) {
			idx = ht->firstSym[len - 1] + code - first;
			if (idx < 0 || idx >= ht->numVals)
				return -1;
			return ht->vals[idx];
		}
	}
	return -1;
}

static unsigned char pj_read_dqt(void)
{
	int len = pj_read_be16();
	if (len < 2)
		return PJPG_BAD_DQT_MARKER;
	len -= 2;
	while (len > 0) {
		int pqtq = pj_read_byte();
		int prec;
		int tq;
		int i;
		if (pqtq < 0)
			return PJPG_BAD_DQT_MARKER;
		prec = pqtq >> 4;
		tq = pqtq & 15;
		if (tq >= PJPG_MAX_TABLES)
			return PJPG_BAD_DQT_TABLE;
		if (len < 1 + 64 * (prec ? 2 : 1))
			return PJPG_BAD_DQT_LENGTH;
		for (i = 0; i < 64; i++) {
			int idx = gZigZag[i];
			int v = prec ? pj_read_be16() : pj_read_byte();
			int scaled;

			if (v < 0)
				return PJPG_BAD_DQT_MARKER;
			scaled = (int)(((long)v * gAANScale[idx] +
				(1L << (PJ_AAN_QUANT_BITS - 1))) >> PJ_AAN_QUANT_BITS);
			gQuant[tq][idx] = (scaled || !v) ? scaled : 1;
		}
		gQuantValid[tq] = 1;
		len -= 1 + 64 * (prec ? 2 : 1);
	}
	return len == 0 ? 0 : PJPG_BAD_DQT_LENGTH;
}

static unsigned char pj_read_dht(void)
{
	int len = pj_read_be16();
	if (len < 2)
		return PJPG_BAD_DHT_MARKER;
	len -= 2;
	while (len > 0) {
		int tc_th = pj_read_byte();
		int tc;
		int th;
		int i;
		int count = 0;
		PjHuffTable *ht;
		if (tc_th < 0)
			return PJPG_BAD_DHT_MARKER;
		tc = tc_th >> 4;
		th = tc_th & 15;
		if (tc > 1 || th >= PJPG_MAX_TABLES)
			return PJPG_BAD_DHT_INDEX;
		ht = &gHuff[tc][th];
		memset(ht, 0, sizeof(*ht));
		if (len < 17)
			return PJPG_BAD_DHT_MARKER;
		for (i = 0; i < 16; i++) {
			int b = pj_read_byte();
			if (b < 0)
				return PJPG_BAD_DHT_MARKER;
			ht->bits[i] = (pj_u8)b;
			count += b;
		}
		if (count > (tc ? 255 : 16) || len < 17 + count)
			return PJPG_BAD_DHT_COUNTS;
		for (i = 0; i < count; i++) {
			int v = pj_read_byte();
			if (v < 0)
				return PJPG_BAD_DHT_MARKER;
			ht->vals[i] = (pj_u8)v;
		}
		len -= 17 + count;
		if (pj_build_huff(ht))
			return PJPG_BAD_DHT_COUNTS;
	}
	return len == 0 ? 0 : PJPG_BAD_DHT_MARKER;
}

static unsigned char pj_read_sof0(void)
{
	int len = pj_read_be16();
	int precision;
	int i;
	if (len < 8)
		return PJPG_BAD_SOF_LENGTH;
	precision = pj_read_byte();
	gHeight = pj_read_be16();
	gWidth = pj_read_be16();
	gNumComps = pj_read_byte();
	if (precision != 8)
		return PJPG_BAD_PRECISION;
	if (gWidth <= 0)
		return PJPG_BAD_WIDTH;
	if (gHeight <= 0)
		return PJPG_BAD_HEIGHT;
	if (gNumComps != 1 && gNumComps != 3 && gNumComps != 4)
		return PJPG_UNSUPPORTED_COLORSPACE;
	if (len != 8 + 3 * gNumComps)
		return PJPG_BAD_SOF_LENGTH;
	gMaxH = 1;
	gMaxV = 1;
	for (i = 0; i < gNumComps; i++) {
		int hv;
		gComp[i].id = (pj_u8)pj_read_byte();
		hv = pj_read_byte();
		gComp[i].h = (pj_u8)(hv >> 4);
		gComp[i].v = (pj_u8)(hv & 15);
		gComp[i].tq = (pj_u8)pj_read_byte();
		gComp[i].dc = 0;
		if (!gComp[i].h || !gComp[i].v || gComp[i].tq >= PJPG_MAX_TABLES)
			return PJPG_UNSUPPORTED_SAMP_FACTORS;
		if (gComp[i].h > gMaxH)
			gMaxH = gComp[i].h;
		if (gComp[i].v > gMaxV)
			gMaxV = gComp[i].v;
	}
	if (gNumComps == 1) {
		if (gComp[0].h != 1 || gComp[0].v != 1)
			return PJPG_UNSUPPORTED_SAMP_FACTORS;
	} else if (gNumComps == 4) {
		/* CMYK/YCCK: only the simple, unsubsampled case (every component
		 * 1x1, one 8x8 block each) is handled -- this keeps the MCU decode a
		 * flat 4-block loop and covers the print-quality CMYK JPEGs that
		 * appear as album art.  Subsampled 4-component images are rejected. */
		if (gComp[0].h != 1 || gComp[0].v != 1 ||
			gComp[1].h != 1 || gComp[1].v != 1 ||
			gComp[2].h != 1 || gComp[2].v != 1 ||
			gComp[3].h != 1 || gComp[3].v != 1)
			return PJPG_UNSUPPORTED_SAMP_FACTORS;
	} else {
		if (gComp[1].h != 1 || gComp[1].v != 1 ||
			gComp[2].h != 1 || gComp[2].v != 1)
			return PJPG_UNSUPPORTED_SAMP_FACTORS;
		if (!((gComp[0].h == 1 && gComp[0].v == 1) ||
			(gComp[0].h == 2 && gComp[0].v == 1) ||
			(gComp[0].h == 1 && gComp[0].v == 2) ||
			(gComp[0].h == 2 && gComp[0].v == 2)))
			return PJPG_UNSUPPORTED_SAMP_FACTORS;
	}
	gMcuW = gMaxH * 8;
	gMcuH = gMaxV * 8;
	gMcuPerRow = (gWidth + gMcuW - 1) / gMcuW;
	gMcuPerCol = (gHeight + gMcuH - 1) / gMcuH;
	return 0;
}

static int pj_find_comp(int id)
{
	int i;
	for (i = 0; i < gNumComps; i++)
		if (gComp[i].id == id)
			return i;
	return -1;
}

static unsigned char pj_read_sos(void)
{
	int len = pj_read_be16();
	int comps;
	int i;
	if (len < 6)
		return PJPG_BAD_SOS_LENGTH;
	comps = pj_read_byte();
	if (comps != gNumComps || len != 6 + 2 * comps)
		return PJPG_NOT_SINGLE_SCAN;
	for (i = 0; i < comps; i++) {
		int id = pj_read_byte();
		int tabs = pj_read_byte();
		int ci = pj_find_comp(id);
		if (ci < 0)
			return PJPG_BAD_SOS_COMP_ID;
		gComp[ci].td = (pj_u8)(tabs >> 4);
		gComp[ci].ta = (pj_u8)(tabs & 15);
		if (gComp[ci].td >= PJPG_MAX_TABLES || gComp[ci].ta >= PJPG_MAX_TABLES)
			return PJPG_UNDEFINED_HUFF_TABLE;
	}
	if (pj_read_byte() != 0 || pj_read_byte() != 63 || pj_read_byte() != 0)
		return PJPG_UNSUPPORTED_MODE;
	gScanPos = gPos;
	gBitBuf = 0;
	gBitsLeft = 0;
	gMarker = PJPG_MARKER_NONE;
	gRestartLeft = gRestartInterval;
	gNextRestart = 0;
	return 0;
}

static unsigned char pj_read_dri(void)
{
	int len = pj_read_be16();
	if (len != 4)
		return PJPG_BAD_DRI_LENGTH;
	gRestartInterval = pj_read_be16();
	if (gRestartInterval < 0)
		return PJPG_BAD_DRI_LENGTH;
	return 0;
}

/* Reads an APP14 marker, capturing the Adobe colour-transform flag when the
 * segment is an "Adobe" marker (needed to tell CMYK from YCCK on 4-component
 * images).  Non-Adobe APP14 segments are simply skipped. */
static unsigned char pj_read_app14(void)
{
	int len = pj_read_be16();
	if (len < 2)
		return PJPG_BAD_VARIABLE_MARKER;
	if (len >= 14) {
		pj_u8 sig[5];
		int i;
		for (i = 0; i < 5; i++)
			sig[i] = (pj_u8)pj_read_byte();
		if (sig[0] == 'A' && sig[1] == 'd' && sig[2] == 'o' &&
			sig[3] == 'b' && sig[4] == 'e') {
			if (pj_skip(6)) /* version(2) + flags0(2) + flags1(2) */
				return PJPG_BAD_VARIABLE_MARKER;
			gAppTransform = pj_read_byte();
			if (len > 14 && pj_skip((unsigned long)(len - 14)))
				return PJPG_BAD_VARIABLE_MARKER;
			return 0;
		}
		if (pj_skip((unsigned long)(len - 2 - 5)))
			return PJPG_BAD_VARIABLE_MARKER;
		return 0;
	}
	if (pj_skip((unsigned long)(len - 2)))
		return PJPG_BAD_VARIABLE_MARKER;
	return 0;
}

static int pj_idct_descale(int x, int n)
{
	/* n is 3 for reduced DC output and 5 for the final IDCT pass,
	 * so the rounding add is at most 16 and cannot overflow here. */
	return (x + (1 << (n - 1))) >> n;
}

static int pj_idct_multiply(int x, int c)
{
	/* The fixed-point constants are bounded by +/-gIdctFix2613 (669).
	 * AAN dequantisation keeps pass-1 inputs within the IDCT guard-bit
	 * budget, so x * c fits in a signed 32-bit int without overflow. */
	return (x * c) >> PJ_IDCT_CONST_BITS;
}

static pj_u8 pj_idct_clamp(int x)
{
	if (x < 0)
		return 0;
	if (x > 255)
		return 255;
	return (pj_u8)x;
}

static void pj_idct_block(const int *coef, pj_u8 *out)
{
	int workspace[64];
	int ctr;
	const int *inptr = coef;
	int *wsptr = workspace;

	for (ctr = 0; ctr < 8; ctr++) {
		int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		int tmp10, tmp11, tmp12, tmp13;
		int z5, z10, z11, z12, z13;

		if ((inptr[8] | inptr[16] | inptr[24] | inptr[32] |
			inptr[40] | inptr[48] | inptr[56]) == 0) {
			int dcval = inptr[0];

			wsptr[0] = dcval;
			wsptr[8] = dcval;
			wsptr[16] = dcval;
			wsptr[24] = dcval;
			wsptr[32] = dcval;
			wsptr[40] = dcval;
			wsptr[48] = dcval;
			wsptr[56] = dcval;
			inptr++;
			wsptr++;
			continue;
		}

		tmp0 = inptr[0];
		tmp1 = inptr[16];
		tmp2 = inptr[32];
		tmp3 = inptr[48];

		tmp10 = tmp0 + tmp2;
		tmp11 = tmp0 - tmp2;
		tmp13 = tmp1 + tmp3;
		tmp12 = pj_idct_multiply(tmp1 - tmp3, gIdctFix1414) - tmp13;
		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		tmp4 = inptr[8];
		tmp5 = inptr[24];
		tmp6 = inptr[40];
		tmp7 = inptr[56];
		z13 = tmp6 + tmp5;
		z10 = tmp6 - tmp5;
		z11 = tmp4 + tmp7;
		z12 = tmp4 - tmp7;
		tmp7 = z11 + z13;
		tmp11 = pj_idct_multiply(z11 - z13, gIdctFix1414);
		z5 = pj_idct_multiply(z10 + z12, gIdctFix1848);
		tmp10 = pj_idct_multiply(z12, gIdctFix1082) - z5;
		tmp12 = pj_idct_multiply(z10, -gIdctFix2613) + z5;
		tmp6 = tmp12 - tmp7;
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		wsptr[0] = tmp0 + tmp7;
		wsptr[56] = tmp0 - tmp7;
		wsptr[8] = tmp1 + tmp6;
		wsptr[48] = tmp1 - tmp6;
		wsptr[16] = tmp2 + tmp5;
		wsptr[40] = tmp2 - tmp5;
		wsptr[32] = tmp3 + tmp4;
		wsptr[24] = tmp3 - tmp4;
		inptr++;
		wsptr++;
	}

	wsptr = workspace;
	for (ctr = 0; ctr < 8; ctr++) {
		int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
		int tmp10, tmp11, tmp12, tmp13;
		int z5, z10, z11, z12, z13;

		if ((wsptr[1] | wsptr[2] | wsptr[3] | wsptr[4] |
			wsptr[5] | wsptr[6] | wsptr[7]) == 0) {
			pj_u8 dcval = pj_idct_clamp(pj_idct_descale(wsptr[0],
				PJ_IDCT_PASS1_BITS + 3) + 128);

			out[0] = dcval;
			out[1] = dcval;
			out[2] = dcval;
			out[3] = dcval;
			out[4] = dcval;
			out[5] = dcval;
			out[6] = dcval;
			out[7] = dcval;
			wsptr += 8;
			out += 8;
			continue;
		}

		tmp10 = wsptr[0] + wsptr[4];
		tmp11 = wsptr[0] - wsptr[4];
		tmp13 = wsptr[2] + wsptr[6];
		tmp12 = pj_idct_multiply(wsptr[2] - wsptr[6], gIdctFix1414) -
			tmp13;
		tmp0 = tmp10 + tmp13;
		tmp3 = tmp10 - tmp13;
		tmp1 = tmp11 + tmp12;
		tmp2 = tmp11 - tmp12;

		z13 = wsptr[5] + wsptr[3];
		z10 = wsptr[5] - wsptr[3];
		z11 = wsptr[1] + wsptr[7];
		z12 = wsptr[1] - wsptr[7];
		tmp7 = z11 + z13;
		tmp11 = pj_idct_multiply(z11 - z13, gIdctFix1414);
		z5 = pj_idct_multiply(z10 + z12, gIdctFix1848);
		tmp10 = pj_idct_multiply(z12, gIdctFix1082) - z5;
		tmp12 = pj_idct_multiply(z10, -gIdctFix2613) + z5;
		tmp6 = tmp12 - tmp7;
		tmp5 = tmp11 - tmp6;
		tmp4 = tmp10 + tmp5;

		out[0] = pj_idct_clamp(pj_idct_descale(tmp0 + tmp7,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[7] = pj_idct_clamp(pj_idct_descale(tmp0 - tmp7,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[1] = pj_idct_clamp(pj_idct_descale(tmp1 + tmp6,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[6] = pj_idct_clamp(pj_idct_descale(tmp1 - tmp6,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[2] = pj_idct_clamp(pj_idct_descale(tmp2 + tmp5,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[5] = pj_idct_clamp(pj_idct_descale(tmp2 - tmp5,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[4] = pj_idct_clamp(pj_idct_descale(tmp3 + tmp4,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		out[3] = pj_idct_clamp(pj_idct_descale(tmp3 - tmp4,
			PJ_IDCT_PASS1_BITS + 3) + 128);
		wsptr += 8;
		out += 8;
	}
}

static unsigned char pj_decode_block(int ci, pj_u8 *out)
{
	int coef[64];
	PjComponent *c = &gComp[ci];
	int s;
	int diff;
	int k;
	if (!gReduce)
		memset(coef, 0, sizeof(coef));
	if (!gQuantValid[c->tq] || !gHuff[0][c->td].valid || !gHuff[1][c->ta].valid)
		return PJPG_UNDEFINED_HUFF_TABLE;
	s = pj_huff_decode(&gHuff[0][c->td]);
	if (s < 0 || s > 15)
		return PJPG_DECODE_ERROR;
	diff = pj_get_bits(s);
	if (diff < 0)
		return PJPG_DECODE_ERROR;
	c->dc = (pj_i16)(c->dc + pj_extend(diff, s));
	coef[0] = c->dc * gQuant[c->tq][0];
	k = 1;
	while (k < 64) {
		int rs = pj_huff_decode(&gHuff[1][c->ta]);
		int r;
		if (rs < 0)
			return PJPG_DECODE_ERROR;
		r = rs >> 4;
		s = rs & 15;
		if (s == 0) {
			if (r == 15) {
				k += 16;
				continue;
			}
			break;
		}
		k += r;
		if (k >= 64)
			return PJPG_DECODE_ERROR;
		diff = pj_get_bits(s);
		if (diff < 0)
			return PJPG_DECODE_ERROR;
		if (!gReduce)
			coef[gZigZag[k]] = pj_extend(diff, s) * gQuant[c->tq][gZigZag[k]];
		k++;
	}
	if (gReduce) {
		pj_u8 dcval = pj_idct_clamp(pj_idct_descale(coef[0], 3) + 128);

		memset(out, dcval, 64);
	} else {
		pj_idct_block(coef, out);
	}
	return 0;
}

static int pj_clamp(int x)
{
	if (x < 0)
		return 0;
	if (x > 255)
		return 255;
	return x;
}

static void pj_ycbcr_to_rgb(int y, int cb, int cr, pj_u8 *r, pj_u8 *g, pj_u8 *b)
{
	int cbb = cb - 128;
	int crr = cr - 128;
	*r = (pj_u8)pj_clamp(y + ((359 * crr) >> 8));
	*g = (pj_u8)pj_clamp(y - (( 88 * cbb + 183 * crr) >> 8));
	*b = (pj_u8)pj_clamp(y + ((454 * cbb) >> 8));
}

static void pj_reset_dc(void)
{
	int i;
	for (i = 0; i < PJPG_MAX_COMPONENTS; i++)
		gComp[i].dc = 0;
}

static unsigned char pj_consume_restart(void)
{
	int marker;
	gBitsLeft = 0;
	gBitBuf = 0;
	if (gMarker)
		marker = gMarker;
	else {
		do {
			if (gScanPos >= gJpegSize)
				return PJPG_BAD_RESTART_MARKER;
			marker = gJpegData[gScanPos++];
		} while (marker != 0xFF);
		do {
			if (gScanPos >= gJpegSize)
				return PJPG_BAD_RESTART_MARKER;
			marker = gJpegData[gScanPos++];
		} while (marker == 0xFF);
	}
	gMarker = PJPG_MARKER_NONE;
	if (marker != 0xD0 + gNextRestart)
		return PJPG_BAD_RESTART_MARKER;
	gNextRestart = (gNextRestart + 1) & 7;
	gRestartLeft = gRestartInterval;
	pj_reset_dc();
	return 0;
}

static unsigned char pj_parse_jpeg(void)
{
	unsigned char status;
	int marker;
	int sawSOF = 0;
	if (pj_read_byte() != 0xFF || pj_read_byte() != 0xD8)
		return PJPG_NOT_JPEG;
	for (;;) {
		marker = pj_next_marker();
		if (marker < 0)
			return PJPG_UNEXPECTED_MARKER;
		switch (marker) {
		case 0xC0:
			status = pj_read_sof0();
			if (status)
				return status;
			sawSOF = 1;
			break;
		case 0xC2:
			return PJPG_UNSUPPORTED_MODE;
		case 0xC4:
			status = pj_read_dht();
			if (status)
				return status;
			break;
		case 0xDB:
			status = pj_read_dqt();
			if (status)
				return status;
			break;
		case 0xDD:
			status = pj_read_dri();
			if (status)
				return status;
			break;
		case 0xDA:
			if (!sawSOF)
				return PJPG_UNEXPECTED_MARKER;
			return pj_read_sos();
		case 0xD9:
			return PJPG_UNEXPECTED_MARKER;
		case 0xEE:
			status = pj_read_app14();
			if (status)
				return status;
			break;
		default:
			if ((marker >= 0xD0 && marker <= 0xD7) || marker == 0x01)
				return PJPG_UNEXPECTED_MARKER;
			{
				int len = pj_read_be16();
				if (len < 2 || pj_skip((unsigned long)(len - 2)))
					return PJPG_BAD_VARIABLE_MARKER;
			}
			break;
		}
	}
}

unsigned char pjpeg_decode_init(pjpeg_image_info_t *pInfo,
	pjpeg_need_bytes_callback_t pNeed_bytes_callback, void *pCallback_data,
	unsigned char reduce)
{
	unsigned char tmp[PJPG_IN_CHUNK];
	unsigned char got;
	unsigned char status;
	unsigned long cap = 0;
	gReduce = reduce ? 1 : 0;
	if (!pInfo || !pNeed_bytes_callback)
		return PJPG_NOT_JPEG;
	pjpeg_decode_free();
	memset(pInfo, 0, sizeof(*pInfo));
	memset(gQuantValid, 0, sizeof(gQuantValid));
	memset(gHuff, 0, sizeof(gHuff));
	memset(gComp, 0, sizeof(gComp));
	gJpegSize = 0;
	gPos = 0;
	gMcuIndex = 0;
	gRestartInterval = 0;
	gAppTransform = -1;
	for (;;) {
		status = pNeed_bytes_callback(tmp, sizeof(tmp), &got, pCallback_data);
		if (status)
			return PJPG_STREAM_READ_ERROR;
		if (!got)
			break;
		if (gJpegSize + got > cap) {
			unsigned long newCap = cap ? cap * 2 : 4096;
			pj_u8 *p;
			while (newCap < gJpegSize + got)
				newCap *= 2;
			p = (pj_u8 *)realloc(gJpegData, newCap);
			if (!p)
				return PJPG_NOTENOUGHMEM;
			gJpegData = p;
			cap = newCap;
		}
		memcpy(gJpegData + gJpegSize, tmp, got);
		gJpegSize += got;
	}
	status = pj_parse_jpeg();
	if (status)
		return status;
	pInfo->m_width = gWidth;
	pInfo->m_height = gHeight;
	pInfo->m_comps = gNumComps;
	pInfo->m_MCUSPerRow = gMcuPerRow;
	pInfo->m_MCUSPerCol = gMcuPerCol;
	pInfo->m_MCUWidth = gMcuW;
	pInfo->m_MCUHeight = gMcuH;
	if (gNumComps == 1)
		pInfo->m_scanType = PJPG_GRAYSCALE;
	else if (gMaxH == 1 && gMaxV == 1)
		pInfo->m_scanType = PJPG_YH1V1;
	else if (gMaxH == 2 && gMaxV == 1)
		pInfo->m_scanType = PJPG_YH2V1;
	else if (gMaxH == 1 && gMaxV == 2)
		pInfo->m_scanType = PJPG_YH1V2;
	else
		pInfo->m_scanType = PJPG_YH2V2;
	pInfo->m_pMCUBufR = gMCUBufR;
	pInfo->m_pMCUBufG = gMCUBufG;
	pInfo->m_pMCUBufB = gMCUBufB;
	pj_reset_dc();
	return 0;
}

unsigned char pjpeg_decode_mcu(void)
{
	pj_u8 yBlocks[4][64];
	pj_u8 cbBlock[64];
	pj_u8 crBlock[64];
	int bx, by, y, x, i;
	unsigned char status;
	if (gMcuIndex >= gMcuPerRow * gMcuPerCol)
		return PJPG_NO_MORE_BLOCKS;
	if (gRestartInterval && gRestartLeft == 0) {
		status = pj_consume_restart();
		if (status)
			return status;
	}
	memset(gMCUBufR, 0, sizeof(gMCUBufR));
	memset(gMCUBufG, 0, sizeof(gMCUBufG));
	memset(gMCUBufB, 0, sizeof(gMCUBufB));
	if (gNumComps == 1) {
		status = pj_decode_block(0, gMCUBufR);
		if (status)
			return status;
	} else if (gNumComps == 4) {
		/* CMYK/YCCK, all components 1x1 (enforced in pj_read_sof0): decode the
		 * four 8x8 blocks and convert to RGB.  Adobe CMYK data is stored
		 * inverted, so with stored samples C,M,Y,K the result is simply
		 * R=C*K/255, G=M*K/255, B=Y*K/255.  For YCCK (Adobe transform 2) the
		 * first three components are the inverted CMY carried as YCbCr, so run
		 * the YCbCr->RGB step first and treat its output as C,M,Y. */
		for (i = 0; i < 4; i++) {
			status = pj_decode_block(i, yBlocks[i]);
			if (status)
				return status;
		}
		for (i = 0; i < 64; i++) {
			int c, m, yv, k = yBlocks[3][i];
			if (gAppTransform == 2) {
				pj_u8 rr, gg, bb;
				pj_ycbcr_to_rgb(yBlocks[0][i], yBlocks[1][i], yBlocks[2][i],
					&rr, &gg, &bb);
				c = rr; m = gg; yv = bb;
			} else {
				c = yBlocks[0][i]; m = yBlocks[1][i]; yv = yBlocks[2][i];
			}
			gMCUBufR[i] = (pj_u8)((c * k + 127) / 255);
			gMCUBufG[i] = (pj_u8)((m * k + 127) / 255);
			gMCUBufB[i] = (pj_u8)((yv * k + 127) / 255);
		}
	} else {
		int yCount = gComp[0].h * gComp[0].v;
		int bi = 0;
		for (by = 0; by < gComp[0].v; by++) {
			for (bx = 0; bx < gComp[0].h; bx++) {
				status = pj_decode_block(0, yBlocks[bi++]);
				if (status)
					return status;
			}
		}
		(void)yCount;
		status = pj_decode_block(1, cbBlock);
		if (status)
			return status;
		status = pj_decode_block(2, crBlock);
		if (status)
			return status;
		for (y = 0; y < gMcuH; y++) {
			for (x = 0; x < gMcuW; x++) {
				int yBlockX = x / 8;
				int yBlockY = y / 8;
				int yBlock = yBlockY * gComp[0].h + yBlockX;
				int ySample = yBlocks[yBlock][(y & 7) * 8 + (x & 7)];
				int cX = (x * 8) / gMcuW;
				int cY = (y * 8) / gMcuH;
				int off = (y / 8) * (gMcuW / 8) * 64 + (x / 8) * 64 +
					(y & 7) * 8 + (x & 7);
				pj_ycbcr_to_rgb(ySample, cbBlock[cY * 8 + cX],
					crBlock[cY * 8 + cX], &gMCUBufR[off], &gMCUBufG[off],
					&gMCUBufB[off]);
			}
		}
	}
	gMcuIndex++;
	if (gRestartInterval)
		gRestartLeft--;
	return 0;
}

void pjpeg_decode_free(void)
{
	if (gJpegData) {
		free(gJpegData);
		gJpegData = 0;
	}
}
