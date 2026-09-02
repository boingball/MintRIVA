# MintVID changelog

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
