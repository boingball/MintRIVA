# CPU-aware Kalms C2P backend

`--kalms-c2p` opts into Mikael Kalms' public-domain converters. `--wpa`
remains the default, `--c2p` remains the portable C fallback, and
`--riva-c2p` remains available for comparisons. CD32/Akiko has precedence over
every CPU converter.

The release profiles now select a kernel that matches their CPU:

- `.030` uses `c2p1x1_8_c5_030`.
- `.040` and `.060` use `c2p1x1_8_c5_040`, which Kalms designed for both CPUs.
- Exact 2x output at 8 planes uses `c2p2x2_8_c5_bm` on every profile. This
  fuses encoded-pixel enlargement and C2P, removing `mr_scale2x_u8()` and the
  doubled chunky framebuffer traversal.
- HAM6 on `.040` and `.060` uses `c2p1x1_6_c5_bm_040`, writing directly to the
  screen's real `BitMap`. The `.030` HAM6 path continues to fall back safely.

All Kalms input allocations are explicitly aligned to 16 bytes. Geometry that
does not meet a kernel's documented width, offset, depth, plane-layout, or
row-length requirements falls back to `WritePixelArray8`.

## Dirty-row conversion

The 1x1 indexed/HAM8 path still keeps a persistent screen-width chunky buffer,
because the normal 8-plane kernels have no horizontal source modulo. It no
longer converts that whole buffer unconditionally. Before each conversion,
MintVID reinitialises the kernel for `[dy0,dy1)` and passes the first dirty row.
The 030 build performs the cache-flushing SMC initialisation only once when the
screen opens; per-frame dirty geometry uses Kalms' non-SMC init.

The bitmap 2x2 and HAM6 routines already accept destination offsets, so they
also receive only the dirty source rows. This keeps decoder, encoder, and C2P
dirty-row knowledge intact end to end.

## Diagnostics

With `--time`, the `AGA path:` line identifies the selected implementation:

- `c2p=kalms-030`
- `c2p=kalms-040` (used by both `.040` and `.060`)
- `c2p=kalms-2x2`
- `c2p=kalms-ham6`

The final `Kalms conversion: ... ms` total remains directly comparable with
the other AGA backends. No per-frame diagnostic output is added to the hot
path.

## Attribution and licence

The converters are from **Mikael Kalms' C2P collection**. Kalms' upstream
`readme.txt` states that all files outside its `others` directory are **Public
Domain**, usable, modifiable, and redistributable for commercial and
non-commercial purposes. MintVID vendors the authoritative assembly and
register-ABI headers unchanged under `player/vendor/kalms-c2p/`.

## Verification

`make -f Makefile.amiga kalms_c2p_check CPU=68030` differentially checks the
030 1x1 routine and fused 2x2 routine against MintVID's portable C2P.

The same target with `CPU=68040` or `CPU=68060` checks the 040/060 1x1 routine,
including a plane larger than the 030 kernel's 16 KiB limit, plus dirty-band,
fused 2x2, and six-plane bitmap output. The normal CI Amiga compiler job builds
all three release profiles as well.

The build normally finds vasm beside `m68k-amigaos-gcc` and the assembly NDK
includes below `AMIGA_ROOT`. Alternative SDK layouts can set `KALMS_VASM` and
`KALMS_ASM_INCLUDE` explicitly; the latter names the directory containing
`lvo/exec_lib.i` and `graphics/gfx.i`.
