#!/bin/sh
# Reject hardware floating-point instructions in a finished Amiga executable.
#
# QuickJS implements JavaScript Number and therefore needs floating-point
# arithmetic, but MintVID's classic releases must remain usable on CPUs with no
# FPU (including PiStorm configurations).  -msoft-float protects code generated
# from MintVID/QuickJS sources; this linked-image check also catches an FPU
# implementation pulled from libgcc, libm, or another static archive.
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 <amiga-executable> <m68k-objdump>" >&2
    exit 2
fi

binary=$1
objdump=$2
listing=$(mktemp "${TMPDIR:-/tmp}/mintvid-no-fpu.XXXXXX")
matches=$(mktemp "${TMPDIR:-/tmp}/mintvid-no-fpu-matches.XXXXXX")
trap 'rm -f "$listing" "$matches"' EXIT HUP INT TERM

"$objdump" -d "$binary" > "$listing"

# Binutils writes an instruction as an address, encoded words, then a mnemonic.
# No integer m68k mnemonic begins with lower-case 'f'; symbol names are enclosed
# in angle brackets and therefore do not match the whitespace boundary here.
if grep -E '^[[:space:]]*[0-9a-fA-F]+:.*[[:space:]]f[a-z][a-z0-9.]*([[:space:]]|$)' \
        "$listing" > "$matches"; then
    echo "ERROR: hardware FPU instructions found in $binary:" >&2
    sed -n '1,40p' "$matches" >&2
    exit 1
fi

echo "No hardware FPU instructions found in $binary"
