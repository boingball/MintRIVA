# Kalms 68030 C2P backend

`--kalms-c2p` opts into Mikael Kalms' `c2p1x1_8_c5_030` for an AGA
256-colour screen. `--wpa` remains the default, `--c2p` remains the portable C
fallback, and `--riva-c2p` remains available for comparisons. CD32/Akiko has
precedence over every CPU converter.

The implementation converts the complete persistent chunky frame. Its stride
is padded to the screen width, with the encoded picture placed at the centred
screen offset, because this Kalms routine has no row-modulo or horizontal-offset
ABI. The self-modifying initialiser is run once after the screen opens.

Before enabling the routine, MintRIVA checks the real `BitMap->Planes[]` array:
the screen must have eight planes, its row length must match the padded chunky
width, and every plane must be at the same positive `bplsize` displacement from
plane zero. Kalms documents a 16 KiB maximum `bplsize`. An incompatible layout
falls back to `WritePixelArray8`; `--time` prints the layout result and reason.
This deliberately avoids assumptions about how graphics.library allocated the
planes.

## Attribution and licence

The converter is from **Mikael Kalms' C2P collection**. Kalms' upstream
`readme.txt` states that all files outside its `others` directory are **Public
Domain**, usable, modifiable, and redistributable for commercial and
non-commercial purposes. MintRIVA vendors the authoritative assembly and
register-ABI header unchanged under `player/vendor/kalms-c2p/normal/`.

## Verification

`make -f Makefile.amiga kalms_c2p_check` uses `vasmm68k_mot` to assemble the
authoritative Motorola/Devpac-syntax source unchanged into an ELF object, then
builds an Amiga-side differential
test covering several 32-pixel-aligned widths and multi-row heights against
`mr_c2p8`. Building `mrplay` links the assembly directly; `nm mrplay` can be
used to confirm `c2p1x1_8_c5_030`, `c2p1x1_8_c5_030_smcinit`, and their
underscore aliases are present.

The build normally finds vasm beside `m68k-amigaos-gcc` and the assembly NDK
includes below `AMIGA_ROOT`. Alternative SDK layouts can set `KALMS_VASM` and
`KALMS_ASM_INCLUDE` explicitly; the latter names the directory containing
`lvo/exec_lib.i`.
