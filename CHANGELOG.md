# MintVID changelog

## 1.1.1 - 2026-09-03

### Added

- Native WMV1 / Windows Media Video 7 decoding for AVI files, including the
  codec's selectable run/level, DC and motion-vector VLC tables, coded-block
  prediction and adaptive escape coding.
- Native WMV2 / Windows Media Video 8 decoding for AVI files, including its
  extension header, bitplane skip coding, adaptive motion prediction, MSPEL,
  adaptive block transforms and in-loop deblocking.
- WMV1 and WMV2 decoder coverage in the portable and big-endian m68k/QEMU
  conformance suites against ffmpeg-generated reference output.

### Fixed

- Live HLS ESC/Stop shutdown is stabilised by restoring the released 1.0.0
  single-next-segment lookahead policy. This removes the aggressive three-
  segment scheduling introduced late in 1.1.0 while retaining the hard worker
  join/no-abandon shutdown protection, preventing the `MintVID HLS fetch`
  `#80000004` worker failure seen on real A1200/WinUAE testing.
- H.264 Turbo+ now applies TurboGT's all-picture degradation (disabling
  I-frame deblocking) to the keyframes it still fully decodes, instead of
  keeping them at Fast's non-key-only degrade policy. Turbo+ skips every P-
  and B-frame, so the keyframe decode is the one blocking call left between
  displayed frames; on a slow CPU (reported on a stock 66 MHz 68060 A1200)
  that call could run long enough to drain Paula's hardware buffer with no
  audio service in between, heard as a laggy half-rate echo. Shortening the
  keyframe decode keeps audio fed through it.

### Improved

- The shared GUI About requester now calls out local/HLS/IPTV/YouTube playback
  and the WMV7/WMV8 additions alongside the existing MPEG/H.264 codec family.
- Release metadata, Aminet text and licence index are refreshed for 1.1.1.

### Compatibility notes

- WMV1's low-bitrate spatial intra/inter prediction mode is deliberately
  rejected rather than decoded approximately.
- WMV2 IntraX8 (J-frame) coding is not supported and is likewise rejected
  cleanly. Normal WMV2 I/P streams remain supported.
- The HLS worker still contains the newer lifecycle hardening from 1.1.0; only
  the number of future compressed segments actively hinted by HLS is returned
  to one for release stability.

## 1.1.0 - 2026-09-02

### Added

- Separate **TurboGT** H.264 performance mode. TurboGT keeps Turbo's B-frame
  skip policy and P-frame reference chain, but applies the strongest practical
  libavc degradation policy to every decoded picture.
- CPU-aware Kalms C2P selection: the 68030 release keeps the 030 kernel, while
  the 68040 and 68060 releases use Kalms' kernel designed for those CPUs.
- Fused Kalms 2x2 scaling and C2P for eligible eight-plane display geometry.
- Direct H.264 YUV420P-to-indexed input for the fused Kalms 2x2 path, avoiding
  both the RGB24 intermediate and a separate RGB-to-indexed pass.
- Direct six-plane Kalms HAM6 output in the 68040 and 68060 releases.
- AmigaOS `$VER:` identities for every shipped executable.

### Changed defaults

- TurboGT is now the default H.264 policy in the GUIs and for bare `mrplay`
  Auto mode. Explicit Quality, Balanced, Fast, Turbo, and Turbo+ choices remain
  available.
- CPU-matched Kalms conversion is now the default AGA/HAM C2P path. Unsupported
  geometry or bitmap layouts still fall back safely to `WritePixelArray8`;
  RTG playback ignores the C2P setting.

### Improved

- H.264 decoding now uses libavc shared display buffers, decoding luma directly
  into the display picture and avoiding one full-frame luma copy per output.
- Kalms conversion now processes only dirty row bands instead of converting
  the entire persistent chunky frame for every update.
- Kalms input buffers are explicitly 16-byte aligned.
- ReAction and GadTools GUIs expose the supported Kalms paths consistently;
  040/060 builds permit AGA, HAM8, and HAM6, while 030 correctly excludes the
  040-only HAM6 kernel.
- `--time` identifies the active implementation as `kalms-030`, `kalms-040`,
  `kalms-2x2`, or `kalms-ham6`.
- YouTube requests for 720p/1080p/Best continue searching other clients after
  an early 360p-only response, retaining 360p as a final fallback.
- General AGA H.264 upscaling reuses each repeated source pixel's YUV-to-RGB
  result while retaining destination-specific dithering; 256-to-640 fitting
  now performs 256 colour conversions per row instead of 640.
- Live HLS playback can buffer several compressed segments ahead instead of
  only one. This provides substantially more network-jitter margin per byte
  than storing the same duration as decoded RGB frames.
- Network playback grows its decoded-frame queue from available RAM while
  retaining the existing safety limits for large frames or tight systems.
- H.264 frames already more than one frame period late can skip their expensive
  RGB conversion/display output while still decoding reference state, allowing
  demux and audio work to catch up.
- Live resync now aims to return about 2.5 seconds behind the live edge, trading
  a little latency for useful margin against the next segment-fetch stall.
- Paula hardware requests grow from 100 ms to 200 ms per buffer, and the audio
  rescue entry, target, and time budget are retuned to match.
- The P96 RTG backend's direct-lock fast path now covers 16-bit RGB565 and
  32-bit ARGB as well as 24-bit BGR, and tries depths in 16/24/32 preference
  order when opening its private screen.
- The CGX RTG backend gains an equivalent direct-lock fast path for its own
  private fullscreen screen through cybergraphics.library, including genuine
  CyberGraphX-only boards without Picasso96API.library.

### Fixed

- P96 fullscreen mode selection now prefers a screen with the video's aspect
  ratio before the smallest spare area, so 854x480 no longer selects a 4:3
  1024x768 scanout when a 16:9 1280x720 mode is available.
- P96 presentation now rebuilds its fitted rectangle from live decoded-frame
  dimensions when an HLS segment changes size, preventing stale metadata from
  stretching a 16:9 frame to 4:3.
- ESC/Stop now joins the in-process HLS fetch worker before `mrplay` exits,
  preventing the worker from continuing in an unloaded code segment and
  raising an `#80000004` illegal-instruction alert. Pending lookahead is no
  longer promoted during shutdown.
- AmiSSL shutdown no longer calls both `CleanupAmiSSL()` and `CloseAmiSSL()`
  for a session opened with `AmiSSL_InitAmiSSL=TRUE`, avoiding duplicate TLS
  cleanup when the HLS worker exits.

### Compatibility

- The public feature set remains available in the 68030, 68040, and 68060
  release drawers; performance-specific assembly is selected at build time.
- Both new RTG direct-lock paths fail closed onto the existing
  `WritePixelArray` path for any unrecognised screen format or geometry
  (windowed mode, or a shared Workbench screen), never onto garbled output.
- Turbo+ remains available for last-resort keyframe-only playback, but TurboGT
  is the normal aggressive setting because it preserves the P-frame chain.

## 1.0.0 - 2026-08-23

- Initial public MintVID release.
