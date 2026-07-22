# MintRIVA portable core - host build (for development & codec validation).
# The Amiga build uses a separate toolchain (vbcc / bebbo-gcc); this Makefile
# only proves the portable core on the dev host.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c99 -g
CORE     = core/mr_codec.c core/mr_avi.c core/mr_cinepak.c
TESTSRC  = tests/mr_decode.c

all: mr_decode

mr_decode: $(TESTSRC) $(CORE) core/mr_codec.h core/mr_avi.h core/mr_types.h
	$(CC) $(CFLAGS) -o $@ $(TESTSRC) $(CORE)

# Run the Cinepak conformance check against ffmpeg's own decoder output.
# Regenerates the reference PPMs (via ffmpeg) if they are not present.
check: mr_decode
	@test -d tests/assets/ref_cinepak || sh tests/gen_assets.sh
	./mr_decode tests/assets/test_cinepak.avi --check tests/assets/ref_cinepak

clean:
	rm -f mr_decode

.PHONY: all check clean
