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
| AVI, QuickTime MOV/MP4 and MPEG-TS/M2TS demuxers | ✅ packet-streamed from disk or HTTP(S); no whole-file allocation |
| HTTP/HTTPS URL input | ✅ redirects, byte-range seeking and 256 KiB rewind cache |
| Cinepak (CVID) decoder | ✅ ffmpeg-validated (AVI + MOV) |
| Microsoft Video 1 — MSVC/CRAM AVI | ✅ native 8/16-bit RGB24 decoder; compatible WHAM streams accepted |
| Microsoft RLE8 — palettised AVI | ✅ native palette and delta-frame decoder (RLE4 deferred) |
| Raw UYVY422 (`2vuy`/`UYVY`) | ✅ uncompressed QuickTime/MOV video |
| Runs on real 68k hardware | ✅ decode verified |
| MJPEG / MPEG-1 / MPEG-4 Part 2 / Microsoft MP42/DIV2 decoders | ✅ ffmpeg-validated |
| MPEG-2 Main Profile video | ✅ libmpeg2; TS + B-frames ffmpeg-validated |
| H.264 High Profile (`avc1`, CABAC, B-frames) | ✅ libavc; ffmpeg-validated |
| MPEG-TS/M2TS MPEG-1/2 or H.264 + ADTS AAC | ✅ 188/192-byte packets; ffmpeg-validated |
| Raw MJPEG + raw MPEG-4 Visual streams | ✅ |
| Amiga RTG / AGA output | ✅ |
| Separate ReAction controller (`mrgui`) | ✅ file picker, mode/options and transport controls |
| IPTV directory core | ✅ bounded iptv-org JSON/M3U parsing, joining and local filters |
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
make check-http # local HTTP range/redirect integration tests
make check-https # the same tests over TLS (needs OpenSSL development files)
```

Inspect or dump any AVI/MOV/MP4/TS/M2TS:

```sh
./mr_decode file.avi                 # stream info + frame count
./mr_decode file.avi --ppm outdir    # write decoded frames as PPM
```

`mrplay` streams AVI, MOV/MP4 and MPEG-TS/M2TS packets from disk or a direct
`http://`/`https://` file URL. Its RAM use is therefore set by container
metadata, the largest compressed packet, a 256 KiB network rewind cache, and
the active decoder/display buffers rather than by the media file size. HTTP
redirects and byte-range seeking are supported:

```sh
mrplay "http://example.net/video.avi"
mrplay "https://example.net/video.mp4"
```

Plain HTTP is present in the normal Amiga build. HTTPS uses
`amisslmaster.library`/AmiSSL v5 and must be enabled when compiling:

```sh
make -f Makefile.amiga mrplay SSL=1
```

For compatibility with typical classic Amiga AmiSSL installations, that mode
uses TLS and SNI but does not verify the server certificate by default. Build
with `SSL=1 SSLCERTS=1` to enable the default CA roots and hostname
verification.

## ReAction controller

The Amiga build also creates `mrgui`, a separate Workbench-friendly controller
in the style of MintAMP's ReAction interface. Keep `mrgui` and `mrplay` in the
same directory (or put `mrplay` on the command path), run `mrgui`, choose a
movie and select **AGA**, **HAM6**, **HAM8**, or **CGX**. **Laced** and **2x**
apply to the three chipset modes; CGX playback opens a size-gadget window and
scales the video as that window is resized. The **C2P** chooser selects the
standard graphics.library path, CD32 Akiko hardware, or the Kalms converter for
chipset playback, and is disabled for CGX. Play starts the selected movie,
Pause toggles playback, Stop exits it, and Fast forward toggles unpaced decode.

The controller's file gadget identifies the selected file. On launch, `mrplay`
also reports the container type, video codec/FourCC, dimensions, frame rate and
audio format to its console, which is useful metadata when testing unfamiliar
files.

URL input currently means a finite, directly addressable media file: the
server must supply `Content-Length` or `Content-Range`, and must honour byte
ranges when the container seeks. Chunked live streams, HLS playlists and
fragmented MP4 are not supported yet.

TS currently supports MPEG-1/2 or AVC/H.264 video with ADTS AAC audio; AC3 is
not decoded. Raw MJPEG/M4V and MPEG-1 program streams still use the original
whole-file input path and therefore do not accept URLs.

### IPTV browser

The ReAction controller includes an **IPTV...** launcher for the separate
`iptvgui` directory window.  Its default public directory is iptv-org
(`channels.json`, `streams.json`, `countries.json`, and `categories.json`).
MintRIVA does not host or redistribute television channels: iptv-org is a
collection of publicly available links, and individual links may be offline,
geo-blocked, or require request headers.

The directory reader is bounded and retains only the metadata used for local
country/category/search filtering.  Cached JSON is used immediately, refreshed
after 24 hours, and replaced only after a complete download parses successfully;
a failed refresh leaves the prior cache intact.  Manual HTTP/HTTPS media URLs,
M3U8 playlists, and simple `#EXTM3U` lists use the normal MintRIVA URL/player
pipeline.  Playback still depends on the existing demuxers and codecs. HLS
prefers supported low-resolution variants (maximum width 640 by default), and
cannot make DRM, login-only, unsupported-codec, or dead streams playable.

Per-stream `Referer` and `User-Agent` values are retained by the IPTV model.
Builds whose HTTP source cannot attach those headers report that limitation
rather than leaking a channel's headers into later playback.

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

## VLC-era video compatibility (wave 1)

H.263 baseline video in AVI and QuickTime MOV is supported for QCIF and CIF,
including intra/inter pictures, skipped macroblocks, half-pixel motion
compensation and persistent reference frames. The decoder rejects malformed or
truncated syntax and refuses H.263+ tools rather than producing corrupt output.
H.263+ is therefore **partial**: UMV, SAC, advanced prediction, PB/improved-PB,
deblocking, slice structure, reference-picture selection, independent segments,
alternative inter VLC, modified quantisation, data partitioning, custom clock
frequency and scalability remain explicitly unsupported. H.261 is not yet
supported. Indeo 3, Sorenson Video 1, WMV1/2 and VP3/Theora are planned.

The FourCC audit below is deliberately conservative. “Registry” means the alias
is covered by the deterministic routing test; a named clip means its bitstream
was also decoded by the existing conformance suite.

| FourCC | Codec family | MintRIVA decoder | Status | Tested sample |
|---|---|---|---|---|
| `DIVX`, `DX50`, `XVID`, `xvid`, `FMP4`, `MP4V`, `mp4v` | ISO MPEG-4 Part 2 | `mpeg4` | accepted | `test_mp4v_sp.avi`; registry |
| `3IV2`, `3iv2`, `3IVX` | 3ivX / ISO MPEG-4 Part 2 | `mpeg4` | accepted | registry; upstream sample inspection pending |
| `RMP4`, `BLZ0`, `SEDG`, `M4S2`, `MP4S` | ISO MPEG-4 Part 2 vendor aliases | `mpeg4` | accepted | registry |
| `DIV2`, `MP42` | Microsoft MPEG-4 v2 | `msmpeg4v2` | accepted, kept separate | `test_div2.avi`, `test_mp42.avi` |
| `DIV1`, `MP41` | Microsoft MPEG-4 v1 | none | unsupported | registry rejection |
| `DIV3`, `MP43`, `AP41`, `COL1`, `COL0` | Microsoft MPEG-4 v3 / DivX 3 | none | unsupported; never routed to ISO ASP | registry rejection |
| `DIV4`, `DIV5`, `DIV6` | ambiguous DivX-era vendor tags | none | unsupported pending sample verification | registry rejection |
| `H263`, `h263`, `I263`, `i263` | H.263 | `H.263 baseline` | accepted for baseline QCIF/CIF | registry; `h263.mov` conformance pending |
| `U263`, `u263`, `T263`, `X263` | vendor H.263 / frequently H.263+ | `H.263 baseline` | registered, but annex flags are rejected | registry; upstream sample inspection pending |
