#!/usr/bin/env python3
"""Generate original, deterministic monochrome PICT fixtures under build/ only."""
import argparse
from pathlib import Path
import random
import struct

ROOT = Path(__file__).resolve().parents[2]


def pict(requested):
    # Version 2 BitsRect: a plain BitMap, srcCopy, no packing or colour table.
    row_bytes = 64
    height = round((requested - 584) / row_bytes)
    if not 1 <= height <= 32767:
        raise ValueError("requested size is outside the bitmap range")
    width = row_bytes * 8
    rect = struct.pack(">hhhh", 0, 0, height, width)
    rng = random.Random(7)
    pixels = bytearray(row_bytes * height)
    for y in range(height):
        for x in range(width):
            ink = rng.randrange(width) < x
            if width // 8 < x < width // 3 and height // 5 < y < height // 2:
                ink = True
            if width // 2 < x < width * 7 // 8 and height // 2 < y < height * 4 // 5:
                ink = False
            if ink:
                pixels[y * row_bytes + x // 8] |= 128 >> (x % 8)
    header = b'\x0c\0' + struct.pack('>iIIII', -1, 0, 0, width << 16, height << 16) + bytes(4)
    stream = (b'\0\0' + rect + b'\0\x11\x02\xff' + header
              + b'\0\x90' + struct.pack('>H', row_bytes) + rect
              + rect + rect + b'\0\0' + pixels + b'\0\xff')
    stream = struct.pack('>H', len(stream) if len(stream) <= 65535 else 0) + stream[2:]
    return bytes(512) + stream


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bytes', type=int, required=True)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    build = ROOT / 'build'
    if build.is_symlink() or build not in output.parents:
        parser.error('output must resolve beneath the repository build/ directory')
    try:
        data = pict(args.bytes)
        if abs(len(data) - args.bytes) > args.bytes * .02:
            raise ValueError('cannot meet requested size within 2%')
        output.parent.mkdir(parents=True, exist_ok=True)
        # A hard link could alias a tracked file despite a safe resolved path.
        if output.exists():
            output.unlink()
        output.write_bytes(data)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
