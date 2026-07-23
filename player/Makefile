# MintRIVA portable core - host build (for development & codec validation).
# The Amiga build uses a separate toolchain (vbcc / bebbo-gcc); this Makefile
# only proves the portable core on the dev host.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -g
CORE     = core/mr_codec.c core/mr_demux.c core/mr_avi.c core/mr_mov.c \
           core/mr_raw_mjpeg.c core/mr_raw_mpeg4.c \
           core/mr_cinepak.c core/mr_dither.c core/mr_ham.c core/mr_scale.c \
           core/mr_c2p.c core/mr_mjpeg.c core/picojpeg.c core/mr_mpeg1.c \
           core/mr_mpeg4.c
TESTSRC  = tests/mr_decode.c
MINTAMP_ROOT ?= vendor/MintAMP
MINTAMP_MP3 = $(MINTAMP_ROOT)/mp3dec.c $(MINTAMP_ROOT)/mp3tabs.c \
              $(wildcard $(MINTAMP_ROOT)/real/*.c)
MINTAMP_AAC_ALL = $(wildcard $(MINTAMP_ROOT)/decoders/aac/*.c)
MINTAMP_AAC = $(filter-out $(wildcard $(MINTAMP_ROOT)/decoders/aac/sbr*.c), \
                           $(MINTAMP_AAC_ALL))
MINTAMP_FLAGS = -DAMIGA_M68K -DARDUINO -DESP8266 \
                -I$(MINTAMP_ROOT)/pub -I$(MINTAMP_ROOT)/real \
                -I$(MINTAMP_ROOT)/decoders/aac \
                -I$(MINTAMP_ROOT)/decoders/aac-arduino-shim
HEADERS  = core/mr_codec.h core/mr_demux.h core/mr_avi.h core/mr_mov.h \
           core/mr_raw_mjpeg.h core/mr_raw_mpeg4.h \
           core/mr_dither.h core/mr_ham.h core/mr_scale.h core/mr_c2p.h \
           core/mr_mpeg1.h core/mr_mpeg4.h core/mr_mpeg4_tables.inc \
           core/mr_types.h

all: mr_decode

# pl_mpeg (in mr_mpeg1.c) uses libm; MR_HAVE_MPEG1 enables the harness path.
mr_decode: $(TESTSRC) $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -DMR_HAVE_MPEG1 -o $@ $(TESTSRC) $(CORE) -lm

mr_audio_check: tests/mr_audio_check.c audio/mr_audio_decode.c \
                audio/mr_audio_decode.h $(CORE)
	@test -f $(MINTAMP_ROOT)/pub/mp3dec.h -a \
	      -f $(MINTAMP_ROOT)/decoders/aac/aacdec.h || { \
	  echo "ERROR: initialise the MintAMP submodule first:"; \
	  echo "  git submodule update --init --recursive"; exit 1; }
	$(CC) $(CFLAGS) $(MINTAMP_FLAGS) -o $@ \
	    tests/mr_audio_check.c audio/mr_audio_decode.c $(CORE) \
	    $(MINTAMP_MP3) $(MINTAMP_AAC) -lm

# Run the Cinepak conformance check against ffmpeg's own decoder output, for
# both the AVI and MOV containers. Regenerates reference PPMs if not present.
check: mr_decode
	@test -d tests/assets/ref_cinepak && test -d tests/assets/ref_mov \
	    && test -d tests/assets/ref_mjpeg && test -d tests/assets/ref_raw_mjpeg \
	    && test -d tests/assets/ref_mpeg1 \
	    && test -d tests/assets/ref_mp4v_intra && test -d tests/assets/ref_mp4v_sp \
	    && test -d tests/assets/ref_opendivx_legacy \
	    && test -d tests/assets/ref_mp4v_qpel && test -d tests/assets/ref_mp4v_b \
	    && test -d tests/assets/ref_raw_mpeg4 \
	    || sh tests/gen_assets.sh
	@echo "[Cinepak AVI]"
	./mr_decode tests/assets/test_cinepak.avi --check tests/assets/ref_cinepak
	@echo "[Cinepak MOV]"
	./mr_decode tests/assets/test_cinepak.mov --check tests/assets/ref_mov
	@echo "[MJPEG AVI]"
	./mr_decode tests/assets/test_mjpeg.avi --check tests/assets/ref_mjpeg
	@echo "[raw MJPEG]"
	./mr_decode tests/assets/test_raw_mjpeg.mjpeg --check tests/assets/ref_raw_mjpeg
	@echo "[MPEG-1 PS]"
	./mr_decode tests/assets/test_mpeg1.mpg --check tests/assets/ref_mpeg1
	@echo "[MPEG-4 Part 2 intra]"
	./mr_decode tests/assets/test_mp4v_intra.avi --check tests/assets/ref_mp4v_intra
	@echo "[MPEG-4 Part 2 Simple Profile]"
	./mr_decode tests/assets/test_mp4v_sp.avi --check tests/assets/ref_mp4v_sp
	@echo "[MPEG-4 Part 2 legacy OpenDivX AVI]"
	./mr_decode tests/assets/test_opendivx_legacy.avi --check tests/assets/ref_opendivx_legacy
	@echo "[MPEG-4 Part 2 quarter-pel]"
	./mr_decode tests/assets/test_mp4v_qpel.avi --check tests/assets/ref_mp4v_qpel
	@echo "[MPEG-4 Part 2 B-VOPs]"
	./mr_decode tests/assets/test_mp4v_b.avi --check tests/assets/ref_mp4v_b
	@echo "[raw MPEG-4 Part 2]"
	./mr_decode tests/assets/test_raw_mpeg4.m4v --check tests/assets/ref_raw_mpeg4

check-audio: mr_audio_check
	@test -f tests/assets/test_mp3.avi -a -f tests/assets/test_aac.mp4 \
	    || sh tests/gen_audio_assets.sh
	./mr_audio_check tests/assets/test_mp3.avi mp3
	./mr_audio_check tests/assets/test_aac.mp4 aac

clean:
	rm -f mr_decode mr_audio_check

.PHONY: all check check-audio clean
