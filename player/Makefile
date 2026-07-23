# MintRIVA portable core - host build (for development & codec validation).
# The Amiga build uses a separate toolchain (vbcc / bebbo-gcc); this Makefile
# only proves the portable core on the dev host.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -g
CORE     = core/mr_codec.c core/mr_demux.c core/mr_avi.c core/mr_mov.c \
           core/mr_cinepak.c core/mr_dither.c core/mr_ham.c core/mr_scale.c \
           core/mr_c2p.c core/mr_mjpeg.c core/picojpeg.c core/mr_mpeg1.c \
           core/mr_mpeg4.c
TESTSRC  = tests/mr_decode.c
HEADERS  = core/mr_codec.h core/mr_demux.h core/mr_avi.h core/mr_mov.h \
           core/mr_dither.h core/mr_ham.h core/mr_scale.h core/mr_c2p.h \
           core/mr_mpeg1.h core/mr_mpeg4.h core/mr_mpeg4_tables.inc \
           core/mr_types.h

all: mr_decode

# pl_mpeg (in mr_mpeg1.c) uses libm; MR_HAVE_MPEG1 enables the harness path.
mr_decode: $(TESTSRC) $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DMR_HAVE_MPEG1 -o $@ $(TESTSRC) $(CORE) -lm

# Run the Cinepak conformance check against ffmpeg's own decoder output, for
# both the AVI and MOV containers. Regenerates reference PPMs if not present.
check: mr_decode
	@test -d tests/assets/ref_cinepak && test -d tests/assets/ref_mov \
	    && test -d tests/assets/ref_mjpeg && test -d tests/assets/ref_mpeg1 \
	    && test -d tests/assets/ref_mp4v_intra && test -d tests/assets/ref_mp4v_sp \
	    && test -d tests/assets/ref_mp4v_qpel && test -d tests/assets/ref_mp4v_b \
	    || sh tests/gen_assets.sh
	@echo "[Cinepak AVI]"
	./mr_decode tests/assets/test_cinepak.avi --check tests/assets/ref_cinepak
	@echo "[Cinepak MOV]"
	./mr_decode tests/assets/test_cinepak.mov --check tests/assets/ref_mov
	@echo "[MJPEG AVI]"
	./mr_decode tests/assets/test_mjpeg.avi --check tests/assets/ref_mjpeg
	@echo "[MPEG-1 PS]"
	./mr_decode tests/assets/test_mpeg1.mpg --check tests/assets/ref_mpeg1
	@echo "[MPEG-4 Part 2 intra]"
	./mr_decode tests/assets/test_mp4v_intra.avi --check tests/assets/ref_mp4v_intra
	@echo "[MPEG-4 Part 2 Simple Profile]"
	./mr_decode tests/assets/test_mp4v_sp.avi --check tests/assets/ref_mp4v_sp
	@echo "[MPEG-4 Part 2 quarter-pel]"
	./mr_decode tests/assets/test_mp4v_qpel.avi --check tests/assets/ref_mp4v_qpel
	@echo "[MPEG-4 Part 2 B-VOPs]"
	./mr_decode tests/assets/test_mp4v_b.avi --check tests/assets/ref_mp4v_b

clean:
	rm -f mr_decode

.PHONY: all check clean
