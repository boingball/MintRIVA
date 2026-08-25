#!/bin/sh
# Build the host YouTube Low resolver probe and run repeated resolves inside
# ONE process.  The anonymous youtube.com cookie jar therefore stays alive
# between attempts, unlike run_youtube_live_stress.sh's cold-process samples.
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
LOG=youtube-live-warm.log
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
    "$QUICKJS_ROOT/quickjs.c" \
    "$QUICKJS_ROOT/dtoa.c" \
    "$QUICKJS_ROOT/libregexp.c" \
    "$QUICKJS_ROOT/libunicode.c" \
    "$QUICKJS_ROOT/cutils.c" \
    vendor/yt-dlp-ejs/yt_solver_lib_le.c \
    vendor/yt-dlp-ejs/yt_solver_core_le.c \
    -lm -lssl -lcrypto

: > "$LOG"
if ./$OUT --repeat "$COUNT" "$URL" 2>&1 | tee -a "$LOG"; then
    :
else
    echo "warm probe reported one or more resolver errors" | tee -a "$LOG"
fi

HLS=$(grep -c '^RESULT SAFARI_HLS ' "$LOG" || true)
ADAPTIVE=$(grep -c '^RESULT ADAPTIVE_144 ' "$LOG" || true)
FALLBACK=$(grep -c '^RESULT FALLBACK_360 ' "$LOG" || true)
ERRORS=$(grep -c '^RESULT ERROR ' "$LOG" || true)
LOW=$((HLS + ADAPTIVE))

printf '\n===== warm-session summary =====\n'
printf 'attempts:      %s\n' "$COUNT"
printf 'Safari HLS:    %s\n' "$HLS"
printf 'adaptive 144:  %s\n' "$ADAPTIVE"
printf 'total 144:     %s/%s\n' "$LOW" "$COUNT"
printf 'fallback 360:  %s\n' "$FALLBACK"
printf 'errors:        %s\n' "$ERRORS"
awk -v low="$LOW" -v total="$COUNT" 'BEGIN { printf "144 hit rate:   %.1f%%\n", (100.0 * low) / total }'

printf '\n===== warm-up trend (blocks of 10) =====\n'
awk '
/^RESULT / {
    attempt = 0
    for (i = 1; i <= NF; i++) {
        if ($i ~ /^attempt=/) {
            split($i, a, "=")
            attempt = a[2] + 0
            break
        }
    }
    if (!attempt) next
    block = int((attempt - 1) / 10) + 1
    seen[block]++
    if ($2 == "SAFARI_HLS" || $2 == "ADAPTIVE_144") hit[block]++
    if (attempt > max_attempt) max_attempt = attempt
}
END {
    blocks = int((max_attempt - 1) / 10) + 1
    for (b = 1; b <= blocks; b++) {
        first = (b - 1) * 10 + 1
        last = b * 10
        if (last > max_attempt) last = max_attempt
        pct = seen[b] ? (100.0 * hit[b] / seen[b]) : 0
        printf "attempts %d-%d: %d/%d (%.1f%%) 144p\n", first, last, hit[b] + 0, seen[b] + 0, pct
    }
}' "$LOG"

printf '\nfull log:      %s/%s\n' "$PLAYER" "$LOG"
