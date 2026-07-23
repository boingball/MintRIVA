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
    rm -rf "$tmpdir"
}
trap cleanup EXIT INT TERM

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

test -f "$tmpdir/range-used"
echo "$mode URL checks passed"
