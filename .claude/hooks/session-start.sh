#!/bin/bash
# SessionStart hook for Claude Code on the web.
#
# MintVID's player/ has no dependency manifest (it's a plain C project with
# git submodules), so there is nothing for a package manager to install.
# What actually blocks `make check` / `make check-audio` / `make check-m68k`
# from an otherwise-fresh clone is:
#   1. Two git submodules (libavc, MintAMP) that ship as empty directories
#      until initialised.
#   2. ffmpeg, needed to (re)generate the conformance fixtures under
#      player/tests/assets/ - `make check` regenerates them itself if
#      missing, but only once ffmpeg exists on PATH.
#   3. A real m68k cross-gcc + qemu-user, for `make check-m68k`: this repo
#      ships and is built for m68k/AmigaOS, and the host running this hook
#      is x86-64/little-endian, so host-only testing cannot catch an
#      endianness or 68030-alignment bug. There is no AmigaOS toolchain
#      package (see CLAUDE.md) - m68k-linux-gnu-gcc + qemu-m68k is the closest
#      available substitute: real big-endian m68k code generation and
#      execution, just not AmigaOS's ABI/libraries, so it cannot build
#      mrplay.c itself, only the portable player/core + libavc pieces.
set -uo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-.}"

echo "== session-start: initialising git submodules =="
# The esp8266audio submodule (pulled in for MintAMP's AAC decoder) has its
# own nested submodules (TinySoundFont, opus) that MintAMP does not actually
# need here and that this network's proxy cannot reach (gitlab.xiph.org).
# Init libavc/MintAMP non-recursively first, each on its own line with `||`,
# so a blocked nested dependency this project doesn't use can never abort
# the rest of the hook.
git submodule update --init player/vendor/libavc || \
    echo "WARN: libavc submodule init failed - make check will not work"
git submodule update --init player/vendor/MintAMP || \
    echo "WARN: MintAMP submodule init failed - make check-audio will not work"
git -C player/vendor/MintAMP submodule update --init decoders/esp8266audio || \
    echo "WARN: esp8266audio submodule init failed - make check-audio will not work"

echo "== session-start: installing ffmpeg + m68k cross toolchain =="
if command -v apt-get >/dev/null 2>&1; then
    SUDO=""
    if [ "$(id -u)" != "0" ] && command -v sudo >/dev/null 2>&1; then
        SUDO="sudo -n"
    fi
    $SUDO apt-get update -qq || true
    # ffmpeg: oracle for player/tests/gen_assets.sh and `make check`.
    # gcc-m68k-linux-gnu/binutils-m68k-linux-gnu/qemu-user: real m68k
    # (big-endian) codegen + execution for `make check-m68k` - see
    # player/tests/run_m68k_check.sh for what that buys over host-only
    # testing.
    $SUDO apt-get install -y -qq \
        ffmpeg \
        gcc-m68k-linux-gnu binutils-m68k-linux-gnu qemu-user \
        || echo "WARN: apt-get install failed - some make check targets may be unavailable"
else
    echo "WARN: apt-get not found - skipping ffmpeg/m68k toolchain install"
fi

echo "== session-start: done =="
