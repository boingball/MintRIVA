# CLAUDE.md — working notes for this repo

## What this is
MintRIVA: a codec-agnostic 68k AmigaOS video player. New code is portable C in
`player/`. The RiVA 0.54 assembly in `src/` is **reference only** (renderers +
IDCT/motion macros worth porting) — do not try to extend that 22k-line `.s`.
Read `DESIGN.md` before making structural decisions.

## Core principles
- **Portable core, thin platform layer.** `player/core/` must stay
  Amiga-independent and host-buildable (C99, fixed-width ints, big-endian-safe
  via `mr_rl*`/`mr_rb*` helpers). Amiga-specific output/audio/IO goes in a
  separate `player/amiga/` layer (not yet written).
- **Codecs plug in behind `mr_codec.h`.** Add a decoder, register it in
  `mr_codec.c` — never special-case a codec in the player skeleton.
- **Audio is MintAMP.** Do not add an in-tree audio codec; the audio backend
  will call MintAMP/libhelix. Audio is the master clock for A/V sync.

## Validate against ffmpeg — always
There is no m68k toolchain on the dev host, so correctness is proven by
decoding on the host and diffing against ffmpeg frame-by-frame:
```sh
cd player && make check      # Cinepak vs ffmpeg, expect worst MAE < ~0.2/255
```
When adding a decoder, add an equivalent `make check` path with an
ffmpeg-generated fixture (`player/tests/gen_assets.sh`). ffmpeg is the oracle.

## Cinepak notes (hard-won)
Chunk-id flag bits live in the **high** byte: `0x0100`=selective/inter,
`0x0200`=V1-codebook / V4-only-vectors, `0x0400`=grayscale. Codebooks and the
output framebuffer **persist across frames** (inter frames patch in place and
may selectively update codebooks). Getting these bit positions wrong shows up as
error that *accumulates* between keyframes, not as an immediate failure.

## Build / test commands
- `cd player && make` — build host harness `mr_decode`
- `cd player && make check` — Cinepak conformance
- `./mr_decode <avi>` / `--ppm <dir>` / `--check <refdir>`

## Git
Work happens on branch `claude/amiga-video-player-riva-9pz78q`. Commit with
clear messages; do not open a PR unless asked.
