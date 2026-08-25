#!/bin/sh
# Reference-only host probe: measure how often current yt-dlp itself receives
# YouTube WEB_SAFARI HLS on the same video/IP as MintVID's resolver tests.
# This does not build or execute any MintVID code.
set -eu

COUNT=${1:-50}
URL=${2:-}
YTDLP=${YTDLP:-yt-dlp}

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
if ! command -v "$YTDLP" >/dev/null 2>&1; then
    echo "yt-dlp was not found in PATH (override with YTDLP=/path/to/yt-dlp)" >&2
    exit 2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 was not found in PATH" >&2
    exit 2
fi

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PLAYER=$(CDPATH= cd -- "$HERE/.." && pwd)
LOG="$PLAYER/ytdlp-safari-reference.log"
TMP_JSON="$PLAYER/.ytdlp-safari-reference.json"
TMP_ERR="$PLAYER/.ytdlp-safari-reference.err"
trap 'rm -f "$TMP_JSON" "$TMP_ERR"' EXIT HUP INT TERM

: > "$LOG"
HITS=0
ERRORS=0
i=1
while [ "$i" -le "$COUNT" ]; do
    echo "===== yt-dlp attempt $i/$COUNT =====" | tee -a "$LOG"
    rm -f "$TMP_JSON" "$TMP_ERR"
    if "$YTDLP" \
        --no-playlist --skip-download --dump-single-json \
        --extractor-args "youtube:player_client=default,web_safari" \
        "$URL" >"$TMP_JSON" 2>"$TMP_ERR"; then
        HLS_COUNT=$(python3 - "$TMP_JSON" <<'PY'
import json, sys
with open(sys.argv[1], 'r', encoding='utf-8') as f:
    info = json.load(f)
formats = info.get('formats') or []
hls = []
for fmt in formats:
    protocol = str(fmt.get('protocol') or '').lower()
    url = str(fmt.get('url') or '').lower()
    manifest = str(fmt.get('manifest_url') or '').lower()
    if 'm3u8' in protocol or '.m3u8' in url or 'manifest/hls' in url or '.m3u8' in manifest or 'manifest/hls' in manifest:
        hls.append(fmt)
print(len(hls))
PY
)
        if [ "$HLS_COUNT" -gt 0 ]; then
            HITS=$((HITS + 1))
            echo "RESULT YTDLP_SAFARI_HLS formats=$HLS_COUNT" | tee -a "$LOG"
        else
            echo "RESULT YTDLP_NO_SAFARI_HLS formats=0" | tee -a "$LOG"
        fi
    else
        ERRORS=$((ERRORS + 1))
        echo "RESULT YTDLP_ERROR" | tee -a "$LOG"
        # Keep only concise diagnostics; no cookies or signed media URLs are logged.
        grep -E '^(ERROR|WARNING):' "$TMP_ERR" | head -n 8 | tee -a "$LOG" || true
    fi
    i=$((i + 1))
done

printf '\n===== yt-dlp reference summary =====\n' | tee -a "$LOG"
printf 'attempts:        %s\n' "$COUNT" | tee -a "$LOG"
printf 'Safari HLS:      %s/%s\n' "$HITS" "$COUNT" | tee -a "$LOG"
printf 'errors:          %s\n' "$ERRORS" | tee -a "$LOG"
awk -v hits="$HITS" -v total="$COUNT" 'BEGIN { printf "HLS hit rate:    %.1f%%\n", (100.0 * hits) / total }' | tee -a "$LOG"
printf 'full log:        %s\n' "$LOG" | tee -a "$LOG"
