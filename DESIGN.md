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
- [ ] Faster output: direct RGB565 / fullscreen RTG / port `RendererCGXInit.i`
- [ ] MintAMP audio backend + audio-master A/V sync
- [ ] AGA C2P + dither output (port from `RendererAGAC2P.i`)
- [ ] MJPEG decoder reusing RiVA's 68k IDCT
- [ ] seek/loop
- [ ] Moon-shot heavier codec, gated to fast/PiStorm machines
```
