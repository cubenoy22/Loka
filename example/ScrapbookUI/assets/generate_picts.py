#!/usr/bin/env python3
"""Generate the deterministic, 1-bit-friendly Scrapbook PICT assets."""

from pathlib import Path
import struct


FRAME = (0, 0, 150, 200)
BADGE_FRAME = (0, 0, 16, 16)
PAGES = (
    ("1", "CHECKER", bytes.fromhex("aa55aa55aa55aa55")),
    ("2", "STRIPES", bytes.fromhex("f0f0f0f0f0f0f0f0")),
    ("3", "BLOCKS", bytes.fromhex("cc33cc33cc33cc33")),
    ("4", "DIAGONAL", bytes.fromhex("8040201008040201")),
)


def word(value):
    return struct.pack(">H", value)


def rect(value):
    return struct.pack(">hhhh", *value)


def padded_op(opcode, payload=b""):
    encoded = word(opcode) + payload
    if len(encoded) & 1:
        encoded += b"\0"
    return encoded


def long_text(vertical, horizontal, text):
    encoded = text.encode("ascii")
    if len(encoded) > 255:
        raise ValueError("PICT text is limited to 255 bytes")
    return padded_op(
        0x0028,
        struct.pack(">hhB", vertical, horizontal, len(encoded)) + encoded,
    )


def picture_preamble(frame):
    operations = bytearray()
    operations += word(0x0011)
    operations += word(0x02FF)
    operations += padded_op(
        0x0C00,
        struct.pack(
            ">hhllhhhhL",
            -2,
            0,
            72 << 16,
            72 << 16,
            *frame,
            0,
        ),
    )

    # An initial clip is not optional decoration: DrawPicture maps the
    # playback clip to the destination, and without a clip opcode the wide-open
    # default overflows 16-bit coordinates as soon as the picture is drawn
    # scaled, clipping every op to nothing. OpenPicture-recorded pictures get
    # this opcode from the recording port; a generated stream must carry it
    # itself. Region data: 2-byte region size (10) + bounding rect.
    operations += padded_op(0x0001, word(10) + rect(frame))  # Clip
    return operations


def finish_picture(frame, operations):
    operations += word(0x00FF)  # OpEndPic
    picture = word(10 + len(operations)) + rect(frame) + operations
    if len(picture) & 1:
        raise AssertionError("version 2 picture must remain word-aligned")
    return bytes(512) + picture


def build_picture(number, label, pattern):
    # The identifying sequence for a version 2 picture, followed by the
    # extended header carrying 72 dpi resolution and the source rectangle.
    operations = picture_preamble(FRAME)

    operations += padded_op(0x000A, pattern)  # FillPat
    operations += padded_op(0x0034, rect((5, 5, 145, 195)))  # FillRect
    operations += padded_op(0x0030, rect(FRAME))  # FrameRect

    # Clear two small fields so the black text remains legible over every
    # pattern on a 1-bit display.
    operations += padded_op(0x0032, rect((35, 65, 100, 135)))  # EraseRect
    operations += padded_op(0x0032, rect((105, 35, 138, 165)))
    operations += padded_op(0x0003, word(0))  # TxFont: system font
    operations += padded_op(0x000D, word(48))  # TxSize
    operations += long_text(88, 85, number)
    operations += padded_op(0x000D, word(18))
    operations += long_text(130, 55, label)
    return finish_picture(FRAME, operations)


def build_refused_badge():
    operations = picture_preamble(BADGE_FRAME)
    operations += padded_op(0x0051, rect((1, 1, 15, 15)))  # PaintOval

    # Erasing paired 2x2 squares makes a bold white X without relying on a
    # font or copied artwork. The stepped edges stay distinct at 1-bit depth.
    for coordinate in range(4, 12, 2):
        operations += padded_op(
            0x0032,
            rect((coordinate, coordinate, coordinate + 2, coordinate + 2)),
        )
        operations += padded_op(
            0x0032,
            rect((coordinate, 14 - coordinate, coordinate + 2, 16 - coordinate)),
        )
    return finish_picture(BADGE_FRAME, operations)


def main():
    output_dir = Path(__file__).resolve().parent
    for index, (number, label, pattern) in enumerate(PAGES, start=1):
        payload = build_picture(number, label, pattern)
        (output_dir / f"page{index}.pict").write_bytes(payload)
        print(f"page{index}.pict: {len(payload)} bytes")
    badge = build_refused_badge()
    (output_dir / "ngbadge.pict").write_bytes(badge)
    print(f"ngbadge.pict: {len(badge)} bytes")


if __name__ == "__main__":
    main()
