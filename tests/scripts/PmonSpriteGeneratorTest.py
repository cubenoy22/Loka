#!/usr/bin/env python3

import binascii
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib


SPRITE_COUNT = 3
SPRITE_SIZE = 96
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def png_chunk(kind, data):
    return (
        struct.pack(">I", len(data))
        + kind
        + data
        + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)
    )


def write_palette_png(path, sprite_index):
    palette = bytes((255, 255, 255, 0, 0, 0, 64, 128, 192))
    rows = []
    for vertical in range(SPRITE_SIZE):
        row = bytes(
            (horizontal // 16 + vertical // 16 + sprite_index) % 3
            for horizontal in range(SPRITE_SIZE)
        )
        rows.append(b"\0" + row)
    encoded = (
        PNG_SIGNATURE
        + png_chunk(
            b"IHDR", struct.pack(">IIBBBBB", SPRITE_SIZE, SPRITE_SIZE, 8, 3, 0, 0, 0)
        )
        + png_chunk(b"PLTE", palette)
        + png_chunk(b"IDAT", zlib.compress(b"".join(rows)))
        + png_chunk(b"IEND", b"")
    )
    path.write_bytes(encoded)


def fail(message):
    raise AssertionError(message)


def parse_bags(manifest):
    bags = []
    for line in manifest.splitlines():
        if not line or line.startswith("#"):
            continue
        if line.startswith("bag "):
            bags.append((line[4:], []))
        elif line.startswith("asset "):
            if not bags:
                fail("manifest contains an asset before its bag")
            bags[-1][1].append(line)
        else:
            fail("unexpected manifest line: {}".format(line))
    return bags


def main():
    repo_dir = Path(__file__).resolve().parents[2]
    generator = repo_dir / "example/ScrapbookUI/assets/make_pmonsprite_lrp.py"
    badge = repo_dir / "example/ScrapbookUI/assets/ngbadge.pict"

    with tempfile.TemporaryDirectory(prefix="loka-pmonsprite-test-") as temporary:
        work_dir = Path(temporary)
        sprites_dir = work_dir / "sprites"
        output_dir = work_dir / "output"
        lrpc_build_dir = work_dir / "lrpc-build"
        sprites_dir.mkdir()
        for index in range(1, SPRITE_COUNT + 1):
            write_palette_png(sprites_dir / "{}.png".format(index), index)

        subprocess.run(
            [
                "cmake",
                "-S",
                str(repo_dir / "tools/lrpc"),
                "-B",
                str(lrpc_build_dir),
                "-DCMAKE_BUILD_TYPE=Debug",
            ],
            check=True,
        )
        subprocess.run(
            ["cmake", "--build", str(lrpc_build_dir), "--parallel", "2"],
            check=True,
        )
        lrpc = lrpc_build_dir / "lrpc"
        if not lrpc.is_file():
            fail("host lrpc build did not produce {}".format(lrpc))

        subprocess.run(
            [
                sys.executable,
                str(generator),
                "--sprites-dir",
                str(sprites_dir),
                "--sprite-count",
                str(SPRITE_COUNT),
                "--output-dir",
                str(output_dir),
                "--lrpc",
                str(lrpc),
            ],
            check=True,
        )

        bags = parse_bags((output_dir / "manifest.txt").read_text(encoding="ascii"))
        expected_ui = (
            "ui",
            ["asset 9001 image UI/RefusedBadge ngbadge.pict"],
        )
        if not bags or bags[0] != expected_ui:
            fail(
                "bag 0 is not the exact ui badge bag: {!r}".format(
                    bags[0] if bags else None
                )
            )
        if len(bags) != SPRITE_COUNT + 1:
            fail("expected {} bags, got {}".format(SPRITE_COUNT + 1, len(bags)))
        for index in range(1, SPRITE_COUNT + 1):
            expected = (
                "pmon-{}".format(index),
                [
                    "asset {} image Pages/Sprite{} pmon-{}.pict".format(
                        1000 + index, index, index
                    )
                ],
            )
            if bags[index] != expected:
                fail("bag {} differs: {!r}".format(index, bags[index]))

        if (output_dir / "ngbadge.pict").read_bytes() != badge.read_bytes():
            fail("staged ngbadge.pict differs from the committed source")
        header = (output_dir / "R.hpp").read_text(encoding="ascii")
        if "const std::size_t AssetCount = {};".format(SPRITE_COUNT) not in header:
            fail("R.hpp does not expose the generated Pages asset count")
        if "const AssetRef Sprite1 = {1001UL, 1," not in header:
            fail("R.hpp does not expose the first generated sprite")
        if not (output_dir / "ASSETS.LRP").is_file():
            fail("lrpc did not produce ASSETS.LRP")

    print("ok: hermetic PMonSprite generator layout and pack")


if __name__ == "__main__":
    main()
