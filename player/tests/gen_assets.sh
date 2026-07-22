#!/bin/sh
# Regenerate the test fixtures used by `make check`.
# Requires ffmpeg. The committed assets are produced by this exact command so
# the Cinepak conformance check is reproducible.
set -e
cd "$(dirname "$0")/assets"

# 128x96, 12 fps, 2 s => 24 frames, two keyframes (GOP 12): exercises intra,
# inter-with-skip and selective codebook updates across a keyframe boundary.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -c:v cinepak test_cinepak.avi -y
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -c:v mjpeg -q:v 5 test_mjpeg.avi -y
# MPEG-1 program stream (25 fps - MPEG-1 only allows standard rates).
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=2 \
    -c:v mpeg1video -b:v 800k -f mpeg test_mpeg1.mpg -y
# Same Cinepak content in a QuickTime MOV, with PCM audio, to exercise the
# MOV demuxer (sample-table frame reconstruction).
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -f lavfi -i sine=frequency=440:sample_rate=22050:duration=2 \
    -c:v cinepak -c:a pcm_s16le test_cinepak.mov -y
# MPEG-4 Part 2 (DivX/Xvid, fourcc FMP4): intra-only (I-VOP path) and Simple
# Profile (I+P with 4MV). ASP tools (B-frames/qpel/GMC) are a later stage.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=1 \
    -c:v mpeg4 -g 1 -qscale:v 4 test_mp4v_intra.avi -y
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -c:v mpeg4 -bf 0 -flags +mv4 -qscale:v 4 test_mp4v_sp.avi -y

# Ground-truth frames, decoded by ffmpeg's own Cinepak decoder (per container,
# since ffmpeg re-encodes the Cinepak stream separately for each).
rm -rf ref_cinepak && mkdir -p ref_cinepak
ffmpeg -v error -i test_cinepak.avi ref_cinepak/f%03d.ppm -y
rm -rf ref_mov && mkdir -p ref_mov
ffmpeg -v error -i test_cinepak.mov ref_mov/f%03d.ppm -y
rm -rf ref_mjpeg && mkdir -p ref_mjpeg
ffmpeg -v error -i test_mjpeg.avi ref_mjpeg/f%03d.ppm -y
rm -rf ref_mpeg1 && mkdir -p ref_mpeg1
ffmpeg -v error -i test_mpeg1.mpg ref_mpeg1/f%03d.ppm -y
rm -rf ref_mp4v_intra && mkdir -p ref_mp4v_intra
ffmpeg -v error -i test_mp4v_intra.avi ref_mp4v_intra/f%03d.ppm -y
rm -rf ref_mp4v_sp && mkdir -p ref_mp4v_sp
ffmpeg -v error -i test_mp4v_sp.avi ref_mp4v_sp/f%03d.ppm -y

echo "fixtures regenerated in $(pwd)"
