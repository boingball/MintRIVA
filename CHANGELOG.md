# MintVID changelog

## 1.1.0 - 2026-08-28

### Added

- Separate **TurboGT** H.264 performance mode for aggressive playback on
  PiStorm/Emu68-class systems without changing the existing Turbo+ policy.
- CPU-aware Kalms C2P selection: the 68030 release keeps the 030 kernel, while
  the 68040 and 68060 releases use Kalms' kernel designed for those CPUs.
- Fused Kalms 2x2 scaling and C2P for eligible eight-plane display geometry.
- Direct H.264 YUV420P-to-indexed input for the fused Kalms 2x2 path, avoiding
  both the RGB24 intermediate and a separate RGB-to-indexed pass.
- Direct six-plane Kalms HAM6 output in the 68040 and 68060 releases.
- AmigaOS `$VER:` identities for every shipped executable.

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
- The P96 RTG backend's direct-lock fast path (previously 24-bit BGR only)
  now also covers 16-bit RGB565 and 32-bit ARGB screens, and tries depths in
  16/24/32 preference order when opening its private screen - so it engages
  on more real boards/modes, including the 16-bit depth this project already
  prefers for lower RTG memory traffic.
- The CGX RTG backend gains an equivalent direct-lock fast path for its own
  private fullscreen screen, using cybergraphics.library's
  `LockBitMapTagList()` directly rather than Picasso96API.library - so
  genuine CyberGraphX-only boards that never had Picasso96API.library also
  skip `WritePixelArray`'s per-pixel colour-space conversion, not just P96-
  compatible ones.

### Compatibility

- Unsupported Kalms geometry or bitmap layouts continue to fall back safely
  to `WritePixelArray8`.
- The public feature set remains available in the 68030, 68040, and 68060
  release drawers; performance-specific assembly is selected at build time.
- Both new RTG direct-lock paths fail closed onto the existing
  `WritePixelArray` path for any unrecognised screen format or geometry
  (windowed mode, or a shared Workbench screen), never onto garbled output.

## 1.0.0 - 2026-08-23

- Initial public MintVID release.
