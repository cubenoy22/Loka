#!/usr/bin/env python3
"""Generate original, deterministic monochrome PICT fixtures under build/ only."""
import argparse
from pathlib import Path
import random
import struct

ROOT = Path(__file__).resolve().parents[2]


def packbits_literal(row):
    """PackBits with literal runs only: QuickDraw requires packed rows once
    rowBytes reaches 8, and a literal run keeps the pixels byte-exact."""
    out = bytearray()
    for start in range(0, len(row), 128):
        chunk = row[start:start + 128]
        out.append(len(chunk) - 1)
        out += chunk
    return bytes(out)


def pict(requested):
    # Version 2 BitsRect: a plain BitMap, srcCopy, no colour table. rowBytes is
    # 64 (>= 8), so every row is a PackBits-packed run with a one-byte count.
    row_bytes = 64
    packed_row = row_bytes + 1
    height = round((requested - 584) / packed_row)
    if not 1 <= height <= 32767:
        raise ValueError("requested size is outside the bitmap range")
    width = row_bytes * 8
    rect = struct.pack(">hhhh", 0, 0, height, width)
    rng = random.Random(7)
    rows = []
    for y in range(height):
        row = bytearray(row_bytes)
        for x in range(width):
            ink = rng.randrange(width) < x
            if width // 8 < x < width // 3 and height // 5 < y < height // 2:
                ink = True
            if width // 2 < x < width * 7 // 8 and height // 2 < y < height * 4 // 5:
                ink = False
            if ink:
                row[x // 8] |= 128 >> (x % 8)
        rows.append(packbits_literal(bytes(row)))
    pixels = b''.join(rows)
    dpi = 72 << 16
    # Version -2 picture header: 72 dpi and the source rectangle, as the
    # pictures QuickDraw itself writes carry.
    header = b'\x0c\0' + struct.pack('>hHII', -2, 0, dpi, dpi) + rect + bytes(4)
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
