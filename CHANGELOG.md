# MintVID changelog

## 1.1.0 - 2026-08-28

### Added

- Separate **TurboGT** H.264 performance mode for aggressive playback on
  PiStorm/Emu68-class systems without changing the existing Turbo+ policy.
- CPU-aware Kalms C2P selection: the 68030 release keeps the 030 kernel, while
  the 68040 and 68060 releases use Kalms' kernel designed for those CPUs.
- Fused Kalms 2x2 scaling and C2P for eligible eight-plane display geometry.
- Direct six-plane Kalms HAM6 output in the 68040 and 68060 releases.
- AmigaOS `$VER:` identities for every shipped executable.

### Improved

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

### Compatibility

- Unsupported Kalms geometry or bitmap layouts continue to fall back safely
  to `WritePixelArray8`.
- The public feature set remains available in the 68030, 68040, and 68060
  release drawers; performance-specific assembly is selected at build time.

## 1.0.0 - 2026-08-23

- Initial public MintVID release.
