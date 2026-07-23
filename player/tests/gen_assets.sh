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
# Raw MJPEG is a concatenated sequence of JPEG images with no container.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=1 \
    -c:v mjpeg -q:v 5 -f mjpeg test_raw_mjpeg.mjpeg -y
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
# Microsoft MPEG-4 v2 in AVI: separate H.263-derived MP42 bitstream.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -c:v msmpeg4v2 -g 12 -qscale:v 4 test_mp42.avi -y
# DIV2 is an alternate AVI FourCC for the same Microsoft v2 bitstream. Remux
# the identical packets so both codec tags share one ffmpeg reference set.
ffmpeg -v error -i test_mp42.avi -c copy -tag:v DIV2 test_div2.avi -y
# H.264 High Profile in MP4: CABAC, 8x8 transform-capable profile, B-frame
# reordering and avcC/length-prefixed NAL handling. AAC-LC exercises the same
# container's interleaved compressed-audio samples.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -f lavfi -i sine=frequency=660:sample_rate=22050:duration=2 \
    -c:v libx264 -profile:v high -level:v 2.0 -pix_fmt yuv420p \
    -g 12 -bf 2 -refs 1 -crf 22 \
    -c:a aac -profile:a aac_low -b:a 64k -shortest test_h264_high.mp4 -y
# Remux the exact H.264/AAC packets into 188-byte broadcast TS and 192-byte
# Blu-ray-style M2TS. These exercise Annex-B/PES assembly and ADTS AAC without
# introducing another encoder reference.
ffmpeg -v error -i test_h264_high.mp4 -c copy \
    -f mpegts test_h264_aac.ts -y
ffmpeg -v error -i test_h264_high.mp4 -c copy -mpegts_m2ts_mode 1 \
    -f mpegts test_h264_aac.m2ts -y
# MPEG-2 Main Profile with B-frame reordering in a transport stream. This also
# verifies that the decoder drains both delayed reference pictures at EOF.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=2 \
    -c:v mpeg2video -profile:v main -pix_fmt yuv420p \
    -g 12 -bf 2 -qscale:v 4 -an -f mpegts test_mpeg2.ts -y
# Early OpenDivX AVI variant: numeric biCompression=4, 'divx' handler, and no
# VOL header in the bitstream. This reproduces Xmen-OpenDivX-200-slow.avi.
python3 ../make_legacy_opendivx.py test_mp4v_sp.avi test_opendivx_legacy.avi
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=1 \
    -c:v mpeg4 -bf 0 -flags +qpel -qscale:v 4 test_mp4v_qpel.avi -y
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=12:duration=2 \
    -c:v mpeg4 -bf 2 -qscale:v 4 test_mp4v_b.avi -y
# Raw MPEG-4 Visual elementary stream: VOL + one VOP sequence, no container.
ffmpeg -v error -f lavfi -i testsrc2=size=128x96:rate=25:duration=1 \
    -c:v mpeg4 -bf 2 -qscale:v 4 -f m4v test_raw_mpeg4.m4v -y

# Ground-truth frames, decoded by ffmpeg's own Cinepak decoder (per container,
# since ffmpeg re-encodes the Cinepak stream separately for each).
rm -rf ref_cinepak && mkdir -p ref_cinepak
ffmpeg -v error -i test_cinepak.avi ref_cinepak/f%03d.ppm -y
rm -rf ref_mov && mkdir -p ref_mov
ffmpeg -v error -i test_cinepak.mov ref_mov/f%03d.ppm -y
rm -rf ref_mjpeg && mkdir -p ref_mjpeg
ffmpeg -v error -i test_mjpeg.avi ref_mjpeg/f%03d.ppm -y
rm -rf ref_raw_mjpeg && mkdir -p ref_raw_mjpeg
ffmpeg -v error -f mjpeg -framerate 25 -i test_raw_mjpeg.mjpeg \
    ref_raw_mjpeg/f%03d.ppm -y
rm -rf ref_mpeg1 && mkdir -p ref_mpeg1
ffmpeg -v error -i test_mpeg1.mpg ref_mpeg1/f%03d.ppm -y
rm -rf ref_mp4v_intra && mkdir -p ref_mp4v_intra
ffmpeg -v error -i test_mp4v_intra.avi ref_mp4v_intra/f%03d.ppm -y
rm -rf ref_mp4v_sp && mkdir -p ref_mp4v_sp
ffmpeg -v error -i test_mp4v_sp.avi ref_mp4v_sp/f%03d.ppm -y
rm -rf ref_mp42 && mkdir -p ref_mp42
ffmpeg -v error -i test_mp42.avi ref_mp42/f%03d.ppm -y
rm -rf ref_h264_high && mkdir -p ref_h264_high
ffmpeg -v error -i test_h264_high.mp4 ref_h264_high/f%03d.ppm -y
rm -rf ref_mpeg2_ts && mkdir -p ref_mpeg2_ts
ffmpeg -v error -i test_mpeg2.ts ref_mpeg2_ts/f%03d.ppm -y
rm -rf ref_opendivx_legacy && mkdir -p ref_opendivx_legacy
ffmpeg -v error -i test_opendivx_legacy.avi ref_opendivx_legacy/f%03d.ppm -y
rm -rf ref_mp4v_qpel && mkdir -p ref_mp4v_qpel
ffmpeg -v error -i test_mp4v_qpel.avi ref_mp4v_qpel/f%03d.ppm -y
rm -rf ref_mp4v_b && mkdir -p ref_mp4v_b
ffmpeg -v error -i test_mp4v_b.avi ref_mp4v_b/f%03d.ppm -y
rm -rf ref_raw_mpeg4 && mkdir -p ref_raw_mpeg4
ffmpeg -v error -f m4v -framerate 25 -i test_raw_mpeg4.m4v \
    ref_raw_mpeg4/f%03d.ppm -y

echo "fixtures regenerated in $(pwd)"
