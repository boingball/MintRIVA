# MintRIVA — design & roadmap

A codec-agnostic video player for 68k AmigaOS, in the spirit of **MintAMP**
(the libhelix-based audio player): small, portable C at the core, thin
Amiga-specific layers around it, and audio handled by MintAMP itself.

The repository started as the source of **RiVA 0.54** (Fellner / Török /
Richter) — the fastest 68k MPEG-1 player, written in hand-tuned 68k/AMMX
assembly. RiVA is kept as **reference** (`src/`, `RiVA.guide`): its renderers
(AGA C2P, CyberGraphX/P96 chunky) and IDCT/motion macros are a goldmine. The
new player is a *fresh* codebase, not an extension of that assembly.

## Why not "just extend RiVA"?

MintAMP was easy because libhelix is portable C — you wrap a codec. RiVA has no
C codec to wrap: its MPEG-1 video decoder *is* 22k lines of assembly, and
MPEG-1 is its ceiling. To go beyond MPEG-1 we need a decoder we can actually
plug in and swap, so the core is portable C with a decoder vtable
(`player/core/mr_codec.h`).

## The hardware spread drives everything

One player, one binary, must span a huge range:

| Tier | Machine | Reality |
|------|---------|---------|
| Floor | A600 / stock AGA, 68030 | No FPU, no SIMD. Decode must be cheap: LUTs and block copies, small frames, dither to chunky. |
| Mid | 68040/060 + RTG | Real integer throughput; MPEG-1 / MJPEG viable at modest sizes. |
| Moon | PiStorm / Emu68 + RTG | An ARM runs the 68k. Benchmarks like a very fast 68080. Heavier codecs become physically possible. |

Design consequence: **codec choice is a tier, not a fixed decision.** The
player picks/loads a decoder; a weak machine sticks to Cinepak, a PiStorm can
run something much heavier from the *same* player.

## Codec strategy

"Beyond MPEG-1 on a 68030" does **not** mean a newer codec — H.264/VP8/etc.
cost *more* CPU, not less. On weak hardware you win by matching the codec to
the CPU, which usually means an older, decode-cheap codec.

- **Cinepak (CVID)** — base tier. Vector quantisation: codebook lookups + block
  copies, no DCT. Designed for 386/68030-class CD-ROM playback. **Implemented
  and validated** (`player/core/mr_cinepak.c`).
- **Motion-JPEG** — mid tier. Intra-only; core is 8×8 IDCT + Huffman, and
  RiVA's hand-tuned 68k IDCT (`src/MacrosIDCT68k.m`) is directly reusable as the
  hot path. Better quality, heavier than Cinepak.
- **MPEG-1** — mid tier. Could wrap a portable decoder, or bridge to RiVA's
  engine as reference.
- **Moon-shot** — a portable decoder (e.g. Theora, or a minimal H.264-baseline)
  cross-compiled to 68k, gated to fast/PiStorm machines to see what is
  physically possible.

Container: **AVI** (RIFF) and **QuickTime MOV** are both implemented behind one
auto-detecting front end (`mr_demux.h`), so the player is container-blind —
`mr_avi.c`, `mr_mov.c`. Adding a container is a backend, like adding a codec.

## Architecture

```
             +------------------ platform (Amiga) ------------------+
  file/async | RTG chunky blit   | AGA C2P + dither | MintAMP audio |
   reader ---+-------------------+------------------+---------------+
      |            ^ frames             ^ frames          ^ pcm
      v            |                    |                 |
  +--------+   +-----------------------------+     +--------------+
  |  demux | ->|  decoder (vtable, per-codec)| ... |  a/v sync    |
  | mr_avi |   |  cinepak / mjpeg / ...       |     | (audio clock)|
  +--------+   +-----------------------------+     +--------------+
       \__________________ portable core (C) __________________/
```

- **Portable core** (`player/core/`): demux, decoder registry + decoders, pixel
  formats. Builds and is tested on the dev host; no Amiga dependencies.
- **Platform layer** (planned, `player/amiga/`): screen/RTG setup and chunky
  blit, AGA C2P + dither (port RiVA's renderers), file/async IO, and the
  MintAMP audio backend.
- **Sync**: audio is the master clock (MintAMP drives playback rate); video
  drops/‌repeats frames to track it — same principle RiVA settled on.

## Validation approach

Because there is no Amiga toolchain on the dev host, the portable core is
proven against **ffmpeg** on the host: decode the same clip and compare to
ffmpeg's own decoder frame-by-frame. Cinepak currently matches to a worst
per-frame mean-absolute-error of **~0.13/255** (last-LSB YUV→RGB rounding).
`cd player && make check`.

## Roadmap

- [x] Decoder vtable + registry (`mr_codec.h`)
- [x] Container-agnostic demux front end (`mr_demux.h`)
- [x] AVI demuxer (video + audio stream discovery) (`mr_avi.c`)
- [x] QuickTime MOV demuxer (stbl sample-table frames) (`mr_mov.c`)
- [x] Cinepak decoder, ffmpeg-validated on AVI + MOV (`mr_cinepak.c`)
- [x] Amiga (m68k) build + verified decoding on real hardware
- [x] `mrplay`: RTG window output via cybergraphics WritePixelArray
      (`player/amiga/`) - **video playing on real hardware**
- [x] AGA fallback: custom screen via WritePixelArray8, auto RTG->AGA
      selection by screenmode (`display.c`, `display_aga.c`) - on hardware
- [x] AGA colour modes: 256-colour ordered dither (`mr_dither.c`) and
      HAM8/HAM6 near-truecolour (`mr_ham.c`); 2x scaling (`mr_scale.c`).
      Encoders host-validated (HAM8 round-trip MAE 2.05/255 vs dither 6.77)
- [x] Encoder speed: divide-free dither/HAM via lookup tables (2fps -> 6fps
      on 030); fast 2x by doubling the chunky, not the RGB (`mr_scale2x_u8`)
- [~] AGA C2P: built-in 8x8-transpose chunky->planar straight to bitplanes
      (`mr_c2p.c`), default over WritePixelArray8 (--wpa to compare). Transpose
      + round-trip host-verified; on-hardware speed measured with --time
- [x] Faster HAM encoder (divide-free, table-driven set error); --time splits
      encode vs blit. Measured: for HAM8 the encode dominates, blit is minor
- [x] Dirty-row rendering: the Cinepak decoder reports the changed-row span
      (host-verified to cover every changed pixel); the display re-encodes and
      re-blits only those rows, so mostly-static video skips most of the encode
- [x] Playback controls: pause (space), loop (--loop), quit; input is now an
      event stream (`display_poll_event`) so seek (cursor keys) can slot in
- [ ] Seek (needs a keyframe index in the demuxers)
- [ ] Optional asm C2P hot loop; CD32 Akiko hardware C2P path (--akiko)
- [x] MJPEG decoder (picojpeg adapter, `mr_mjpeg.c`) - ffmpeg-validated
      (worst MAE 0.5/255); proves the codec plugin design with a 2nd codec
- [x] AGA auto-fit: oversized clips (e.g. 640x480) are integer box-downscaled
      to fit a non-interlaced screen (`mr_scale_down_rgb24`); bad frames skip
      instead of stopping playback
- [x] MPEG-1 decode via pl_mpeg (`mr_mpeg1.c`, MIT single-file lib) -
      ffmpeg-validated on host (worst MAE ~0.9/255). .mpg is a self-contained
      stream so it gets a source wrapper, not the demux+codec split.
- [x] MPEG-1 in the Amiga player: .mpg/.mpeg play through pl_mpeg (video + MP2
      audio -> Paula), reusing the display/audio backends. The 68k build links
      libm + soft-float for this (MPEG-1 is a fast-machine codec). pause/loop
      apply. (Cinepak/MJPEG path stays integer.)
- [x] Microsoft MPEG-4 v2 (`MP42`) in AVI: separate H.263-derived decoder
      plugin with I/P pictures, slice/DC/AC prediction, skip macroblocks and
      half-pel motion compensation. Host-validated against ffmpeg on the full
      1,983-frame BFHL sample (worst per-frame RGB MAE 1.83/255).
- [ ] Modern codecs (Theora / H.264-baseline) gated to fast/PiStorm machines,
      likely by wrapping a portable decoder library
- [ ] Internet streaming (reuse MintAMP's radio_stream + AmiSSL HTTP stack)
- [ ] Faster output: direct RGB565 / fullscreen RTG / port `RendererCGXInit.i`
- [~] Paula audio backend + audio-master A/V sync (`audio_paula.c`) - PCM,
      MP2, MP3-in-AVI and AAC-LC-in-MP4. MP3/AAC use the pinned MintAMP/Helix
      sources through `player/audio/mr_audio_decode.c`; host regression tests
      pass, pending on-hardware verification.
- [ ] AGA C2P + dither output (port from `RendererAGAC2P.i`)
- [ ] MJPEG decoder reusing RiVA's 68k IDCT
- [ ] seek/loop
- [ ] Moon-shot heavier codec, gated to fast/PiStorm machines
```
