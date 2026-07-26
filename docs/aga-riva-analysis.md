# RiVA AGA display-path analysis

This note records what can safely be reused from RiVA without coupling the
MintRIVA decoders to a display format. The reference is `RendererAGAInit.i` and
`RendererAGAC2P.i`; RiVA's MPEG decoder itself remains untouched.

## What RiVA actually does

* The AGA screen is requested with `SA_Interleaved`, but the renderer obtains
  the bitmap/plane address once and writes the planar result itself. Its C2P is
  a CPU butterfly transpose in 32-pixel groups; it is **not** a blitter C2P.
* The C2P consumes decoder-owned luma/chroma buffers with separately calculated
  end-of-line corrections. HAM6 and HAM8 combine colour conversion, HAM command
  selection, pixel doubling and planar output. That fused input contract is not
  compatible with MintRIVA's codec-neutral RGB24 frame contract.
* The destination plane offsets are patched into the assembly during setup and
  `CacheClearU` follows the self-modification. This is not a general requirement
  for writing bitmap data. It is required because RiVA modifies instructions.
* The final partial 32-pixel group is masked. Source line padding and destination
  plane size/modulo are calculated independently. The routines can also skip an
  input row for their half-height mode.
* The AGA path writes the screen bitmap directly. It neither allocates screen
  buffers nor calls `ChangeScreenBuffer`, and it does not use the blitter. Thus
  there is no RiVA double-buffer swap protocol to port.
* There are Apollo-specific renderers elsewhere, but no safe runtime-selected
  020/030/040/060 AGA C2P family in this path. Importing a self-modifying CPU
  routine into the baseline 68030 binary would add cache and ABI risks before
  it has been benchmarked on the target CPUs.

## Safe first optimisation

`--riva-c2p` selects a conservative backend inside `display_aga.c`. Like RiVA,
it works in aligned 32-pixel groups and writes directly to each bitmap plane.
It retains MintRIVA's already verified chunky transpose, but combines four
adjacent plane bytes into one aligned longword store. This reduces expensive
Chip RAM transactions without adding a Fast-to-Chip staging bitmap or assuming
that all planes are one contiguous allocation.

The first hardware benchmark motivated a 32-bit-only revision. For 176 frames
of a 150x118 Cinepak clip the measurements were:

| backend | display | blit |
| --- | ---: | ---: |
| `--wpa` | 7025 ms | 3272 ms |
| `--c2p` | 8649 ms | 4891 ms |
| `--riva-c2p` | 8914 ms | 5169 ms |

Conversion time was effectively identical, isolating the regression to the
C2P/blit stage. The first experimental routine used a `uint64_t` transpose;
multiword masks and shifts are costly on a 68030. The revised routine constructs
the same plane bits with 32-bit operations only, retains aligned longword plane
writes, and is differentially tested against `mr_c2p8` using random rows,
multiple 32-pixel groups, source strides, and partial dirty-row offsets.

The current `WritePixelArray8` default and `--c2p` renderer remain available as
fallbacks. Akiko continues to take precedence when selected. HAM6, HAM8,
ordered-colour, exact 2x scaling, arbitrary resize, laced modes, and dirty row
ranges all feed the same persistent chunky buffer and therefore keep their
existing semantics. Both source stride and bitmap `BytesPerRow` are honoured;
no PAL, non-laced, or laced modulo is hard-coded.

Conversion still happens in Fast RAM and the C2P result goes straight to Chip
RAM. A blitter pass would add another transfer and cannot perform the chunky
bit transpose, so it is not used.

## Deferred until hardware measurements

* **Double buffering:** it prevents visible tearing, but is not automatically a
  speed optimisation. With dirty frames, alternating buffers do not contain the
  same history. Correctness requires converting a complete persistent chunky
  frame for every swap, mirroring every dirty update into both bitmaps, or
  copying unchanged planar data. Each option can cost more than direct partial
  updates, so none is silently enabled.
* **CPU assembly variants:** a 68020-safe implementation and separately tuned
  030/040/060 routines need an ABI wrapper, runtime CPU detection, alignment
  tests, and real-hardware results. Self-modifying offsets would additionally
  require instruction-cache flushing.
* **Cache management:** ordinary CPU stores into Chip RAM need no version of
  RiVA's `CacheClearU`; that call follows its instruction patching. DMA-visible
  Fast RAM or a future blitter/bitmap ownership scheme would need explicit
  cache coherency review on 040/060.
* **Interleaved-layout shortcuts:** the selected implementation uses the plane
  pointers and `BytesPerRow` published by graphics.library rather than assuming
  RiVA's allocation layout. A later interleaved fast path should first validate
  those pointers and modulo on real PAL, laced, and non-laced screens.

`--time` reports conversion and blit time for every frame and retains the final
accumulated totals. This makes the opt-in backend directly comparable with
`--wpa`, `--c2p`, and `--cd32` on each target machine.
