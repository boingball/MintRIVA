# MintRIVA portable core - host build (for development & codec validation).
# The Amiga build uses a separate toolchain (vbcc / bebbo-gcc); this Makefile
# only proves the portable core on the dev host.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -g
CORE     = core/mr_codec.c core/mr_demux.c core/mr_avi.c core/mr_mov.c \
           core/mr_cinepak.c
TESTSRC  = tests/mr_decode.c
HEADERS  = core/mr_codec.h core/mr_demux.h core/mr_avi.h core/mr_mov.h \
           core/mr_types.h

all: mr_decode

mr_decode: $(TESTSRC) $(CORE) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(TESTSRC) $(CORE)

# Run the Cinepak conformance check against ffmpeg's own decoder output, for
# both the AVI and MOV containers. Regenerates reference PPMs if not present.
check: mr_decode
	@test -d tests/assets/ref_cinepak && test -d tests/assets/ref_mov \
	    || sh tests/gen_assets.sh
	@echo "[AVI]"
	./mr_decode tests/assets/test_cinepak.avi --check tests/assets/ref_cinepak
	@echo "[MOV]"
	./mr_decode tests/assets/test_cinepak.mov --check tests/assets/ref_mov

clean:
	rm -f mr_decode

.PHONY: all check clean
