#!/bin/sh
# Build and repeatedly launch the host YouTube Low resolver probe.
# Each attempt is a fresh process, matching separate Amiga mrplay launches.
set -eu

COUNT=${1:-50}
URL=${2:-}

if [ -z "$URL" ]; then
    echo "usage: $0 [count] <youtube-url>" >&2
    exit 2
fi

case "$COUNT" in
    *[!0-9]*|'') echo "count must be a positive integer" >&2; exit 2 ;;
esac
if [ "$COUNT" -lt 1 ]; then
    echo "count must be at least 1" >&2
    exit 2
fi

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PLAYER=$(CDPATH= cd -- "$HERE/.." && pwd)
cd "$PLAYER"

CC=${CC:-cc}
OUT=mr_youtube_live_check
LOG=youtube-live-stress.log

QUICKJS_ROOT=vendor/quickjs

$CC -O2 -Wall -Wextra -std=c99 -g \
    -D_GNU_SOURCE -DMR_QUICKJS_NO_THREADS \
    -DCONFIG_VERSION=\"2026-06-04\" -fwrapv \
    -DMR_HTTP_HAVE_OPENSSL -I"$QUICKJS_ROOT" \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-sign-compare \
    -Wno-implicit-fallthrough -Wno-maybe-uninitialized -Wno-type-limits \
    -o "$OUT" \
    tests/mr_youtube_live_check.c \
    core/mr_youtube.c core/mr_youtube_nsig.c \
    core/mr_http.c core/mr_source.c core/mr_hls.c \
    "$QUICKJS_ROOT/quickjs.c" "$QUICKJS_ROOT/dtoa.c \
    "$QUICKJS_ROOT/libregexp.c" "$QUICKJS_ROOT/libunicode.c \
    "$QUICKJS_ROOT/cutils.c" \
    vendor/yt-dlp-ejs/yt_solver_lib_le.c \
    vendor/yt-dlp-ejs/yt_solver_core_le.c \
    -lm -lssl -lcrypto

: > "$LOG"
i=1
while [ "$i" -le "$COUNT" ]; do
    echo "===== attempt $i/$COUNT =====" | tee -a "$LOG"
    if ./$OUT "$URL" 2>&1 | tee -a "$LOG"; then
        :
    else
        echo "probe process failed" | tee -a "$LOG"
    fi
    i=$((i + 1))
done

HLS=$(grep -c '^RESULT SAFARI_HLS ' "$LOG" || true)
ADAPTIVE=$(grep -c '^RESULT ADAPTIVE_144 ' "$LOG" || true)
FALLBACK=$(grep -c '^RESULT FALLBACK_360 ' "$LOG" || true)
ERRORS=$(grep -c '^RESULT ERROR ' "$LOG" || true)
SAFARI_HEADERS=$(grep -c '^YouTube HTTP session: adding WEB_SAFARI API headers$' "$LOG" || true)
VISITOR_HEADERS=$(grep -c '^YouTube HTTP session: forwarding watch-page visitor header$' "$LOG" || true)
LOW=$((HLS + ADAPTIVE))

printf '\n===== summary =====\n'
printf 'attempts:       %s\n' "$COUNT"
printf 'Safari headers: %s/%s\n' "$SAFARI_HEADERS" "$COUNT"
printf 'visitor header: %s/%s\n' "$VISITOR_HEADERS" "$COUNT"
printf 'Safari HLS:     %s\n' "$HLS"
printf 'adaptive 144:   %s\n' "$ADAPTIVE"
printf 'total 144:      %s/%s\n' "$LOW" "$COUNT"
printf 'fallback 360:   %s\n' "$FALLBACK"
printf 'errors:         %s\n' "$ERRORS"
awk -v low="$LOW" -v total="$COUNT" 'BEGIN { printf "144 hit rate:    %.1f%%\n", (100.0 * low) / total }'
printf 'full log:       %s/%s\n' "$PLAYER" "$LOG"
