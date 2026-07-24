#!/bin/sh
set -eu

decoder=${1:-./mr_decode}
mode=${2:-http}
tmpdir=$(mktemp -d)
server_pid=

cleanup()
{
    if test -n "$server_pid"; then
        kill "$server_pid" 2>/dev/null || true
        wait "$server_pid" 2>/dev/null || true
    fi
    rm -rf "$tmpdir" tests/assets/hls
}
trap cleanup EXIT INT TERM

# Derive an HLS VOD fixture by slicing the MPEG-TS test asset into TS-packet-
# aligned segments; concatenated, they are the original stream, so the HLS
# source must decode identically to it.
python3 - <<'PY'
import os
d=open('tests/assets/test_mpeg2.ts','rb').read(); PKT=188; n=len(d)//PKT
os.makedirs('tests/assets/hls',exist_ok=True)
b1=(n//3)*PKT; b2=(2*n//3)*PKT; parts=[d[:b1],d[b1:b2],d[b2:]]
m=["#EXTM3U","#EXT-X-VERSION:3","#EXT-X-TARGETDURATION:2","#EXT-X-MEDIA-SEQUENCE:0"]
for i,p in enumerate(parts):
    open(f'tests/assets/hls/seg{i}.ts','wb').write(p); m+=["#EXTINF:2.0,",f"seg{i}.ts"]
m.append("#EXT-X-ENDLIST")
open('tests/assets/hls/media.m3u8','w').write("\n".join(m)+"\n")
open('tests/assets/hls/master.m3u8','w').write(
 "#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1200000\nmissing.m3u8\n"
 "#EXT-X-STREAM-INF:BANDWIDTH=300000\nmedia.m3u8\n")
PY

server_args="--root tests/assets --port-file $tmpdir/port --range-marker $tmpdir/range-used"
scheme=http
if test "$mode" = https; then
    scheme=https
    openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
        -subj /CN=localhost \
        -keyout "$tmpdir/key.pem" -out "$tmpdir/cert.pem" \
        >/dev/null 2>&1
    server_args="$server_args --cert $tmpdir/cert.pem --key $tmpdir/key.pem"
fi

python3 tests/http_fixture_server.py $server_args \
    >"$tmpdir/server.log" 2>&1 &
server_pid=$!

tries=0
while test ! -s "$tmpdir/port"; do
    tries=$((tries + 1))
    if test "$tries" -ge 100; then
        cat "$tmpdir/server.log"
        exit 1
    fi
    sleep 0.05
done
port=$(cat "$tmpdir/port")
base="$scheme://127.0.0.1:$port"

"$decoder" "$base/media/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/redirect/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/media/test_h264_high.mp4" \
    --check tests/assets/ref_h264_high
"$decoder" "$base/media/test_mp42.avi" \
    --check tests/assets/ref_mp42
"$decoder" "$base/chunked/media/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/chunked/redirect/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/chunked/media/test_h264_high.mp4" \
    --check tests/assets/ref_h264_high
"$decoder" "$base/chunked/media/test_mp42.avi" \
    --check tests/assets/ref_mp42

"$decoder" "$base/chunked-head/media/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/chunked-head/media/test_h264_high.mp4" \
    --check tests/assets/ref_h264_high

# Length-less forward-only stream: no Content-Length, Range ignored. Only the
# sequential MPEG-TS path can play this; the frames must still match exactly.
"$decoder" "$base/stream/media/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/stream/redirect/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts

# Seekable server that drops each transfer after a small cap: the client must
# reconnect with a Range at a non-zero offset to finish (the byte-range resume
# path). The frames still match, and this is what touches the range marker
# asserted below - read-ahead now caches whole small files, so a plain
# sequential fetch never needs a mid-file Range on its own.
"$decoder" "$base/drop/media/test_h264_high.mp4" \
    --check tests/assets/ref_h264_high
"$decoder" "$base/drop/media/test_mpeg2.ts" \
    --check tests/assets/ref_mpeg2_ts

# HLS VOD: the media playlist's segments concatenate back to the TS asset, and a
# master playlist must resolve to the sole reachable variant. Both decode to the
# same reference frames.
"$decoder" "$base/media/hls/media.m3u8" \
    --check tests/assets/ref_mpeg2_ts
"$decoder" "$base/media/hls/master.m3u8" \
    --check tests/assets/ref_mpeg2_ts

test -f "$tmpdir/range-used"
echo "$mode URL checks passed"
