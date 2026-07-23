# MintRIVA

A codec-agnostic video player for 68k AmigaOS — built in the spirit of
MintAMP (the libhelix audio player): a small, portable C core with thin
Amiga-specific layers, and audio handled by MintAMP.

The goal is to go **beyond MPEG-1** on real Amiga hardware — from a stock
A600/AGA up to a PiStorm/RTG machine — by matching the codec to the CPU rather
than chasing heavier modern formats. See **[DESIGN.md](DESIGN.md)** for the full
architecture and roadmap.

This repository began as the source of **RiVA 0.54**, the fastest 68k MPEG-1
player (Stephen Fellner, László Török, Henryk Richter). That assembly is kept
as reference material — see `src/`, the original `README`, and `RiVA.guide`.

## Status

| Component | State |
|-----------|-------|
| Decoder plugin interface + registry | ✅ |
| Container-agnostic demux (auto-detect) | ✅ |
| File-backed AVI, QuickTime MOV/MP4 and MPEG-TS/M2TS demuxers | ✅ packet-streamed; no whole-file allocation |
| Cinepak (CVID) decoder | ✅ ffmpeg-validated (AVI + MOV) |
| Runs on real 68k hardware | ✅ decode verified |
| MJPEG / MPEG-1 / MPEG-4 Part 2 / Microsoft MP42/DIV2 decoders | ✅ ffmpeg-validated |
| MPEG-2 Main Profile video | ✅ libmpeg2; TS + B-frames ffmpeg-validated |
| H.264 High Profile (`avc1`, CABAC, B-frames) | ✅ libavc; ffmpeg-validated |
| MPEG-TS/M2TS MPEG-1/2 or H.264 + ADTS AAC | ✅ 188/192-byte packets; ffmpeg-validated |
| Raw MJPEG + raw MPEG-4 Visual streams | ✅ |
| Amiga RTG / AGA output | ✅ |
| PCM / MP2 / MP3 / AAC-LC audio to Paula | ✅ host-validated; hardware test pending for MP3/AAC |

## Building & testing the portable core (dev host)

The `player/core` code is plain C99 with no Amiga dependencies, so it builds and
is validated on a normal machine before it ever meets a 68k toolchain.

The H.264 tier uses GCC (including the m68k GCC build); the legacy vbcc target
continues to build the lighter codecs without libavc.

`mrplay` carries a `$STACK:320000` AmigaOS stack cookie because libavc needs
substantially more stack than the classic Shell default. On systems that do
not honour stack cookies, run `Stack 320000` before starting the player.

```sh
git submodule update --init --recursive
cd player
make            # builds ./mr_decode
make check      # decodes a Cinepak clip and diffs against ffmpeg (needs ffmpeg)
make check-audio # decodes MP3-in-AVI and AAC-LC-in-MP4 through MintAMP/Helix
```

Inspect or dump any AVI/MOV/MP4/TS/M2TS:

```sh
./mr_decode file.avi                 # stream info + frame count
./mr_decode file.avi --ppm outdir    # write decoded frames as PPM
```

`mrplay` streams AVI, MOV/MP4 and MPEG-TS/M2TS packets from disk. Its RAM use is
therefore set by container metadata, the largest compressed packet, and the
active decoder/display buffers rather than by the media file size. TS currently
supports MPEG-1/2 or AVC/H.264 video with ADTS AAC audio; AC3 is not decoded.
Raw MJPEG/M4V and MPEG-1 program streams still use the original whole-file
input path.

## Layout

```
src/                 RiVA 0.54 assembly (reference)
RiVA.guide           RiVA manual (reference)
DESIGN.md            architecture & roadmap
player/core/         portable C core: demux + video decoders
player/audio/        packet adapter for MintAMP's MP3/AAC Helix decoders
player/amiga/        RTG/AGA display, Paula output and player frontend
player/tests/        host test harness + fixtures
player/vendor/       pinned/vendored build dependencies
```

## Licensing

RiVA is GPL-2.0 (`src/gpl-2.0.txt`); its AGA/CGX renderers are dual GPL/MIT. New
MintRIVA code inherits GPL-2.0 to stay compatible with the RiVA reference it
draws on. The vendored VideoLAN libmpeg2 core is GPL-2.0-or-later. MintAMP/Helix
and Apache-2.0 Ittiam libavc remain separately licensed in their pinned
submodules; retain their notices when distributing source or binaries.
