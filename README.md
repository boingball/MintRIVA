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
| AVI, QuickTime MOV/MP4, Matroska/MKV and MPEG-TS/M2TS demuxers | ✅ packet-streamed from disk or HTTP(S); no whole-file allocation |
| HTTP/HTTPS URL input | ✅ redirects, byte-range seeking and 256 KiB rewind cache |
| Public YouTube URLs | ✅ live HLS plus experimental muxed 360p/720p H.264/AAC playback for compatible uploads |
| ReAction YouTube search | ✅ no-key search browser with live-only filter and native playback handoff |
| Cinepak (CVID) decoder | ✅ ffmpeg-validated (AVI + MOV) |
| Microsoft Video 1 — MSVC/CRAM AVI | ✅ native 8/16-bit RGB24 decoder; compatible WHAM streams accepted |
| Microsoft RLE8 — palettised AVI | ✅ native palette and delta-frame decoder (RLE4 deferred) |
| Raw UYVY422 (`2vuy`/`UYVY`) | ✅ uncompressed QuickTime/MOV video |
| Runs on real 68k hardware | ✅ decode verified |
| MJPEG / MPEG-1 / MPEG-4 Part 2 / Microsoft MP42/DIV2 decoders | ✅ ffmpeg-validated |
| MPEG-2 Main Profile video | ✅ libmpeg2; TS + B-frames ffmpeg-validated |
| H.264 High Profile (`avc1`, CABAC, B-frames) | ✅ libavc; ffmpeg-validated |
| MPEG-TS/M2TS MPEG-1/2 or H.264 + AAC/MP2/AC-3 | ✅ ADTS or LATM AAC; 188/192-byte packets; ffmpeg-validated |
| Matroska/MKV | ✅ H.264/MPEG-4/MPEG-2/MJPEG video; AAC/MP3/MP2/AC-3/PCM audio; common lacing supported |
| Raw MJPEG + raw MPEG-4 Visual streams | ✅ |
| Amiga RTG / AGA output | ✅ |
| Separate ReAction controller (`mrgui`) | ✅ file picker, mode/options and transport controls |
| IPTV directory core | ✅ bounded iptv-org JSON/M3U parsing, joining and local filters |
| PCM / MP2 / MP3 / AAC-LC / AC-3 audio to Paula | ✅ host-validated; AC-3 uses fixed-point stereo downmix |

## Building & testing the portable core (dev host)

The `player/core` code is plain C99 with no Amiga dependencies, so it builds and
is validated on a normal machine before it ever meets a 68k toolchain.

The H.264 tier uses GCC (including the m68k GCC build); the legacy vbcc target
continues to build the lighter codecs without libavc.

`mrplay` carries a `$STACK:320000` AmigaOS stack cookie because libavc needs
substantially more stack than the classic Shell default. On systems that do
not honour stack cookies, run `Stack 320000` before starting the player.

The normal Amiga build remains 68030-compatible. Optimised 68040 and 68060
builds can be selected explicitly, or packaged together in `player/release/`:

```sh
cd player
make -f Makefile.amiga all SSL=1 CPU=68030
make -f Makefile.amiga all SSL=1 CPU=68040
make -f Makefile.amiga all SSL=1 CPU=68060
make -f Makefile.amiga release SSL=1   # release/MintRIVA030, 040 and 060
```

Use the `.040` build for a PiStorm configured as a 68040 and `.060` only on a
68060-compatible CPU. `release/MintRIVA030`, `release/MintRIVA040` and
`release/MintRIVA060` are ready-to-run sets with ordinary unsuffixed program
names. Each contains `mrplay`, `mrgui`, `iptvgui` and `ytgui`, so the GUI can
launch its companion programs without any renaming, plus the command-line
`mr_decode` codec probe/test harness. The release target finishes by restoring
the working binaries to the universal 030 build.

```sh
git submodule update --init --recursive
cd player
make            # builds ./mr_decode
make check      # decodes a Cinepak clip and diffs against ffmpeg (needs ffmpeg)
make check-audio # MP3, AAC ADTS/LATM and fixed-point AC-3 decoder checks
make check-http # local HTTP range/redirect integration tests
make check-https # the same tests over TLS (needs OpenSSL development files)
```

Inspect or dump any AVI/MOV/MP4/MKV/TS/M2TS:

```sh
./mr_decode file.avi                 # stream info + frame count
./mr_decode file.avi --ppm outdir    # write decoded frames as PPM
```

`mrplay` streams AVI, MOV/MP4, Matroska/MKV and MPEG-TS/M2TS packets from disk or a direct
`http://`/`https://` file URL. Its RAM use is therefore set by container
metadata, the largest compressed packet, a 256 KiB network rewind cache, and
the active decoder/display buffers rather than by the media file size. HTTP
redirects and byte-range seeking are supported:

```sh
mrplay "http://example.net/video.avi"
mrplay "https://example.net/video.mp4"
mrplay --user-agent "Mozilla/5.0" --referer "https://example.net/" \
  "https://example.net/live/master.m3u8"
mrplay --hls-max-width=640 --hls-max-height=360 \
  "https://www.youtube.com/watch?v=LIVE_STREAM_ID"
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

### Live streaming resilience

Live HLS (`.m3u8`) playback on constrained hardware has a few extra controls.
The AmiSSL library, TLS context, and TLS session are initialised once and reused
across segments, so each segment boundary reconnects with an abbreviated
handshake instead of the full per-segment bring-up.

- `--net-queue=N` — hold up to `N` decoded frames ahead (default 1 for network,
  clamped to 32). A few frames absorb per-frame decode jitter; a deep buffer
  (e.g. `--net-queue=24`) lets video sit ahead of the audio clock and present in
  order, keeps the loop demuxing so the audio FIFO stays fed, and rides across a
  segment-boundary refetch. Costs one RGB frame of RAM per used slot.
- `--live-resync` — recover from big disruptions. If a stall leaves playback
  more than ~4 s behind the wall clock, it fast-consumes the buffered backlog
  (decode reference-only, discard audio) and re-primes near the live edge; and
  if the stream drops out entirely it reopens the URL and resumes rather than
  ending. It never fires in normal playback. GUI-launched playback (`mrgui`, the
  IPTV browser) enables this by default since IPTV streams are always live; a
  direct `mrplay <url>` leaves it off. Use `--no-live-resync` to opt out.

```sh
mrplay --net-queue=24 --live-resync "https://example.net/live/master.m3u8"
```

### YouTube

`mrplay` can resolve a public YouTube Live watch/share URL natively. It fetches
the watch page with a browser user agent, extracts and JSON-decodes the signed
`hlsManifestUrl`, validates that it is an HTTPS `manifest.googlevideo.com`
playlist, then hands it to the normal HLS variant/segment pipeline. Resolution
happens on the Amiga itself, so IP-bound signed URLs are not borrowed from a
remote service. `--hls-low` and the HLS quality ceilings still apply.
The HTTP/HLS path accepts signed URLs up to 4095 bytes, since current YouTube
manifest URLs can exceed the older 1 KiB media-URL limit.

For ordinary uploads, the resolver also experiments with YouTube's muxed
360p MP4 (`itag 18`) and, where still supplied, muxed 720p MP4 (`itag 22`).
Those formats contain H.264 video and AAC audio together,
so it can use MintRIVA's existing seekable HTTP/MP4 path without downloading or
merging separate streams. Only a direct signed HTTPS Google Video URL is
accepted; ciphered URLs and unresolved player `n` challenges are rejected.
Selecting 720p, 1080p, or Best makes recorded playback try 720p first and fall
back to 360p automatically. Low, 360p, and 480p retain the 360p muxed format.
There is no standard muxed 480p or 1080p target here: dependable higher
resolutions require separate adaptive video and audio streams and are deferred
to the next phase.

This remains intentionally narrow: age/login/region-restricted videos, DRM,
uploads without a usable muxed 360p/720p format, and private-schema changes can
all produce a clean unsupported error. YouTube can change these internal clients
and responses, so the resolver may require maintenance.

The ReAction controller's **YouTube...** button opens the separate `ytgui`
search window. It searches YouTube's public results page without an API key,
shows the title and channel, and starts the selected result through the same
native resolver. The **Quality** button cycles through Low, 360p, 480p, 720p,
1080p, and unrestricted Best. For recorded videos, 720p/1080p/Best try the
compatible muxed 720p format and fall back to 360p; the other choices use 360p.
**Live only** is enabled by default; untick it to search and play ordinary
uploads. Build
with `SSL=1` and keep `ytgui` beside `mrgui` and `mrplay`. As with watch-page
resolution, this deliberately small parser may need maintenance if YouTube
changes its private page schema.

Selecting a result and pressing **Channel videos** follows its bounded channel
ID to the public channel `/videos` page and lists that channel's uploads. The
transport row controls the separate player process: Play first cleanly replaces
the current video, Pause and Fast toggle their modes, Vol -/+ adjusts Paula in
steps, Fullscreen toggles the RTG window, and Stop exits the player.

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
In RTG/CGX mode, `F` switches the live player between its resizeable window and
a borderless public-screen-sized view without restarting decoding; `--fullscreen`
starts in that view. Press `F` again—or use ytgui's **Fullscreen** button—to
restore the previous window geometry. AGA display modes remain hotkey-driven
and ignore the RTG-only fullscreen command. True timeline seeking is not yet
implemented; it needs a demux keyframe/sample seek API rather than pretending
that fast decode is a seek operation.

The controller's file gadget identifies the selected file. On launch, `mrplay`
also reports the container type, video codec/FourCC, dimensions, frame rate and
audio format to its console, which is useful metadata when testing unfamiliar
files.

Direct AVI/MOV/MP4 URL input still needs a finite, byte-addressable resource:
the server must supply `Content-Length` or `Content-Range`, and must honour byte
ranges when the container seeks. MPEG-TS also accepts a forward-only chunked
response, while HLS playlists use the dedicated live/VOD source. Fragmented MP4
is not supported yet.

TS currently supports MPEG-1/2 or AVC/H.264 video with MP2, ADTS/LATM AAC or
AC-3 audio. Raw MJPEG/M4V and MPEG-1 program streams still use the original
whole-file input path and therefore do not accept URLs.

### IPTV browser

The ReAction controller includes an **IPTV...** launcher for the separate
`iptvgui` directory window. Build it together with the controller using
`make -f Makefile.amiga all SSL=1 SSLCERTS=1`; keep `mrgui`, `iptvgui`,
`ytgui`, and `mrplay` together. `SSL=1` enables AmiSSL for YouTube searches and
the iptv-org directory download; `SSLCERTS=1` enables certificate verification.
A browser built
without HTTPS support remains usable for cached data and manual URLs, but a
refresh explicitly reports that it must be rebuilt with `SSL=1`.
The browser immediately reads valid cached `channels.json` and `streams.json`
from `PROGDIR:Cache/IPTV/`. Its default public directory is iptv-org
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

Cached JSON is processed incrementally with a 16 KiB buffer. Only the selected
country is held in RAM; unrelated global streams are validated and discarded.
Changing country rebuilds the compact directory from cache without downloading
the API files again. Each channel retains at most four preferred stream URLs.
Country filtering uses the directory's own codes (`UK` for United Kingdom and
`US` for United States), rather than deriving ISO codes from display labels.

Per-stream `Referer` and `User-Agent` values are retained by the IPTV model and
passed as typed, bounded `mrplay` options. They follow redirects, HLS variant
playlists, live-playlist refreshes, segments, and range reconnects. Values with
CR/LF or values exceeding their fixed limits are rejected, and the options are
owned by one playback source so they cannot leak into a later channel. The IPTV
window's **Next Stream** button advances through the retained alternatives
without silently looping.

IPTV playback inherits a snapshot of the ReAction controller's display, C2P,
lace, and 2x selections when **IPTV...** is pressed. The IPTV window shows that
snapshot beside its status; close and reopen it after changing controller
settings. A Shell-launched `iptvgui` uses safe AGA/Standard, lace-off, 2x-off,
low-bandwidth HLS defaults. The shared bounded argument builder is also used by
the main controller's ordinary **Play** action, so both paths map identical
settings to identical `mrplay` display flags.

For a real-hardware fragmentation check, record both `AvailMem(MEMF_FAST)` and
`AvailMem(MEMF_FAST|MEMF_LARGEST)`, then open `iptvgui`, wait for the list, and
close it ten times. The loader prints those values around each loading phase and
after ListBrowser construction; neither total Fast RAM nor the largest block
should show a meaningful downward trend across completed open/close cycles.

## Layout

```
src/                 RiVA 0.54 assembly (reference)
RiVA.guide           RiVA manual (reference)
DESIGN.md            architecture & roadmap
player/core/         portable C core: demux + video decoders
player/audio/        packet adapter for MP2, MintAMP MP3/AAC and fixed AC-3
player/amiga/        RTG/AGA display, Paula output and player frontend
player/tests/        host test harness + fixtures
player/vendor/       pinned/vendored build dependencies
```

## Licensing

RiVA is GPL-2.0 (`src/gpl-2.0.txt`); its AGA/CGX renderers are dual GPL/MIT. New
MintRIVA code inherits GPL-2.0 to stay compatible with the RiVA reference it
draws on. The vendored VideoLAN libmpeg2 core and fixed-point Rockbox/a52dec
AC-3 core are GPL-2.0-or-later. MintAMP/Helix
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
