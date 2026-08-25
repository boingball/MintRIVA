#!/bin/sh
# Reject hardware floating-point instructions in generated Amiga assembly.
#
# QuickJS implements JavaScript Number and therefore needs floating-point
# arithmetic, but MintVID's classic releases must remain usable on CPUs with no
# FPU (including PiStorm configurations).  Each QuickJS source is compiled to
# assembly with the release flags and checked before the normal link.
#
# Do not scan the linked Amiga Hunk with objdump: Bebbo places constant tables
# and the embedded EJS bytecode in code hunks, so a linear disassembler decodes
# data as convincing but bogus FPU instructions.
set -eu

if [ "$#" -lt 1 ]; then
    echo "usage: $0 <generated-assembly> [...]" >&2
    exit 2
fi

matches=$(mktemp "${TMPDIR:-/tmp}/mintvid-no-fpu-matches.XXXXXX")
trap 'rm -f "$matches"' EXIT HUP INT TERM

# No integer m68k instruction mnemonic begins with lower-case 'f'.  Requiring
# whitespace after it excludes labels such as "fail:".
awk '
    tolower($0) ~ /^[[:space:]]*f[a-z][a-z0-9.]*[[:space:]]/ {
        print FILENAME ":" FNR ":" $0
    }
' "$@" > "$matches"
if [ -s "$matches" ]; then
    echo "ERROR: hardware FPU instructions found in generated assembly:" >&2
    sed -n '1,40p' "$matches" >&2
    exit 1
fi

echo "No hardware FPU instructions emitted for QuickJS"
