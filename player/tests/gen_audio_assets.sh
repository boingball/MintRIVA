#!/bin/sh
# Generate compressed-audio integration fixtures for `make check-audio`.
set -e
cd "$(dirname "$0")/assets"

# MP3 exercises AVI WAVE tag 0x55 and packet joins; AAC exercises mp4a/esds
# AudioSpecificConfig plus one raw access unit per MP4 sample. 44.1 kHz also
# covers Paula's 2:1 output decimation.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=2 \
    -f lavfi -i sine=frequency=523:sample_rate=44100:duration=2 \
    -c:v mpeg4 -bf 0 -qscale:v 5 -c:a libmp3lame -b:a 96k \
    -shortest test_mp3.avi -y
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=2 \
    -f lavfi -i sine=frequency=659:sample_rate=44100:duration=2 \
    -c:v mpeg4 -bf 0 -qscale:v 5 -c:a aac -b:a 96k \
    -shortest test_aac.mp4 -y

echo "audio fixtures regenerated in $(pwd)"
