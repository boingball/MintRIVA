#!/usr/bin/env python3
"""Turn an ffmpeg MPEG-4 AVI into a VOL-less early OpenDivX test fixture."""

import sys
from pathlib import Path


def find_video_header(data: bytearray) -> int:
    pos = 0
    while True:
        pos = data.find(b"strh", pos)
        if pos < 0:
            raise ValueError("video strh chunk not found")
        if pos + 16 <= len(data) and data[pos + 8 : pos + 12] == b"vids":
            return pos
        pos += 4


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {sys.argv[0]} SOURCE.avi DEST.avi")

    data = bytearray(Path(sys.argv[1]).read_bytes())
    strh = find_video_header(data)
    strf = data.find(b"strf", strh)
    movi = data.find(b"movi", strh)
    if strf < 0 or movi < 0 or strf > movi:
        raise ValueError("video strf/movi chunks not found")

    strf_size = int.from_bytes(data[strf + 4 : strf + 8], "little")
    if strf_size < 40:
        raise ValueError("video strf is not a BITMAPINFOHEADER")

    # Match the contradictory headers used by early OpenDivX AVIs:
    # fccHandler='divx', but biCompression is the numeric value 4.
    data[strh + 12 : strh + 16] = b"divx"
    data[strf + 24 : strf + 28] = (4).to_bytes(4, "little")

    chunk = data.find(b"00dc", movi + 4)
    if chunk < 0 or chunk + 8 > len(data):
        raise ValueError("first video chunk not found")
    chunk_size = int.from_bytes(data[chunk + 4 : chunk + 8], "little")
    payload = chunk + 8
    end = payload + chunk_size
    if end > len(data):
        raise ValueError("truncated first video chunk")
    vop = data.find(b"\x00\x00\x01\xb6", payload, end)
    if vop < 0:
        raise ValueError("first VOP start code not found")

    # Old files provide no VOL. Keep packet sizes/offsets stable by replacing
    # the generated setup headers with zero padding rather than removing them.
    data[payload:vop] = b"\x00" * (vop - payload)
    Path(sys.argv[2]).write_bytes(data)


if __name__ == "__main__":
    main()
