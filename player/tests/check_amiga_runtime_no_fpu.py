#!/usr/bin/env python3
"""Reject hardware-FPU opcodes in linked QuickJS math helper functions.

Bebbo's 68040/68060 libgcc/libm multilibs can add Line-F instructions after
all MintVID translation units have passed an assembly-level no-FPU check.  The
Amiga Hunk executable retains symbols, so inspect the exact ranges occupied by
the compiler double helpers and the small libm surface used by QuickJS.

Only those symbol ranges are decoded here.  Scanning a complete Hunk CODE
segment is invalid for this binary because the linker also places constants and
the embedded EJS bytecode in CODE hunks.
"""

from __future__ import annotations

import re
import struct
import sys
from pathlib import Path

HUNK_HEADER = 0x3F3
HUNK_NAME = 0x3E8
HUNK_CODE = 0x3E9
HUNK_DATA = 0x3EA
HUNK_BSS = 0x3EB
HUNK_RELOC32 = 0x3EC
HUNK_RELOC16 = 0x3ED
HUNK_RELOC8 = 0x3EE
HUNK_EXT = 0x3EF
HUNK_SYMBOL = 0x3F0
HUNK_DEBUG = 0x3F1
HUNK_END = 0x3F2
HUNK_DREL32 = 0x3F7

GCC_FLOAT_HELPER = re.compile(
    r"^___(?:add|sub|mul|div|neg|cmp|eq|ne|ge|gt|le|lt|unord|"
    r"float|fix|extend|trunc).*(?:df|sf)"
)
LIBM_HELPERS = {"_fmod", "_floor", "_ceil", "_trunc", "_round"}


class HunkReader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.offset = 0

    def u32(self) -> int:
        if self.offset + 4 > len(self.data):
            raise ValueError("truncated Hunk executable")
        value = struct.unpack_from(">I", self.data, self.offset)[0]
        self.offset += 4
        return value

    def skip_longs(self, count: int) -> None:
        self.offset += count * 4
        if self.offset > len(self.data):
            raise ValueError("truncated Hunk executable")


def read_hunks(path: Path) -> tuple[bytes, dict[int, tuple[int, int]],
                                    dict[int, dict[str, int]]]:
    data = path.read_bytes()
    reader = HunkReader(data)
    if reader.u32() != HUNK_HEADER:
        raise ValueError("not an Amiga Hunk executable")

    while True:  # resident library names
        length = reader.u32()
        if not length:
            break
        reader.skip_longs(length)

    table_size = reader.u32()
    reader.u32()  # first hunk
    reader.u32()  # last hunk
    reader.skip_longs(table_size)

    hunk_index = -1
    code_ranges: dict[int, tuple[int, int]] = {}
    symbols: dict[int, dict[str, int]] = {}

    while reader.offset < len(data):
        hunk_type = reader.u32() & 0x3FFFFFFF
        if hunk_type in (HUNK_CODE, HUNK_DATA, HUNK_BSS):
            hunk_index += 1
            byte_length = reader.u32() * 4
            start = reader.offset
            if hunk_type != HUNK_BSS:
                reader.offset += byte_length
            if hunk_type == HUNK_CODE:
                code_ranges[hunk_index] = (start, byte_length)
        elif hunk_type == HUNK_NAME:
            reader.skip_longs(reader.u32())
        elif hunk_type in (HUNK_RELOC32, HUNK_RELOC16,
                           HUNK_RELOC8, HUNK_DREL32):
            while True:
                count = reader.u32()
                if not count:
                    break
                reader.u32()  # target hunk
                reader.skip_longs(count)
        elif hunk_type == HUNK_SYMBOL:
            table = symbols.setdefault(hunk_index, {})
            while True:
                name_longs = reader.u32()
                if not name_longs:
                    break
                raw_name = data[reader.offset:reader.offset + name_longs * 4]
                reader.skip_longs(name_longs)
                name = raw_name.rstrip(b"\0").decode("latin-1")
                table[name] = reader.u32()
        elif hunk_type == HUNK_DEBUG:
            reader.skip_longs(reader.u32())
        elif hunk_type == HUNK_EXT:
            while True:
                descriptor = reader.u32()
                if not descriptor:
                    break
                ext_type = descriptor >> 24
                reader.skip_longs(descriptor & 0xFFFFFF)  # name
                if ext_type in (0, 1, 2, 3):
                    reader.u32()  # definition value
                elif ext_type >= 129:
                    reader.skip_longs(reader.u32())  # references
                else:
                    raise ValueError(f"unsupported HUNK_EXT type {ext_type}")
        elif hunk_type == HUNK_END:
            continue
        else:
            raise ValueError(f"unsupported Hunk type 0x{hunk_type:x}")

    return data, code_ranges, symbols


def is_float_helper(name: str) -> bool:
    return bool(GCC_FLOAT_HELPER.match(name)) or name in LIBM_HELPERS


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {Path(sys.argv[0]).name} <Amiga executable>",
              file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    try:
        data, code_ranges, hunk_symbols = read_hunks(path)
    except (OSError, ValueError) as error:
        print(f"ERROR: cannot inspect {path}: {error}", file=sys.stderr)
        return 2

    checked: list[str] = []
    failures: list[tuple[str, int]] = []
    for hunk, symbol_table in hunk_symbols.items():
        if hunk not in code_ranges:
            continue
        code_start, code_length = code_ranges[hunk]
        ordered_offsets = sorted(set(symbol_table.values()) | {code_length})
        next_offset = {
            value: ordered_offsets[index + 1]
            for index, value in enumerate(ordered_offsets[:-1])
        }
        for name, start in sorted(symbol_table.items(), key=lambda item: item[1]):
            if not is_float_helper(name):
                continue
            end = next_offset.get(start, code_length)
            function = data[code_start + start:code_start + end]
            checked.append(name)
            # Motorola's FPU is coprocessor ID 1.  Its complete Line-F opcode
            # family has 1111 001 in bits 15..9 (F200..F3FF).
            for offset in range(0, len(function) - 1, 2):
                word = struct.unpack_from(">H", function, offset)[0]
                if word & 0xFE00 == 0xF200:
                    failures.append((name, offset))

    if not checked:
        print("ERROR: no linked QuickJS floating-point helpers found",
              file=sys.stderr)
        return 2
    if failures:
        print("ERROR: linked QuickJS runtime contains hardware-FPU opcodes:",
              file=sys.stderr)
        for name, offset in failures:
            print(f"  {name}+0x{offset:x}", file=sys.stderr)
        return 1

    print(f"No hardware FPU opcodes in {len(checked)} linked QuickJS helpers")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
