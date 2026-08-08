#!/usr/bin/env python3
"""Generate deterministic Scrapbook images for classic and modern packages."""

from pathlib import Path
import struct
import zlib


FRAME = (0, 0, 150, 200)
BADGE_FRAME = (0, 0, 16, 16)
PAGES = (
    ("1", "CHECKER", bytes.fromhex("aa55aa55aa55aa55")),
    ("2", "STRIPES", bytes.fromhex("f0f0f0f0f0f0f0f0")),
    ("3", "BLOCKS", bytes.fromhex("cc33cc33cc33cc33")),
    ("4", "DIAGONAL", bytes.fromhex("8040201008040201")),
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PNG_WIDTH = FRAME[3] - FRAME[1]
PNG_HEIGHT = FRAME[2] - FRAME[0]
DIGITS = {
    "1": (0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110),
    "2": (0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111),
    "3": (0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110),
    "4": (0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010),
}


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


def png_chunk(kind, payload):
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def encode_rgb_png(width, height, pixels):
    if len(pixels) != width * height * 3:
        raise ValueError("RGB payload does not match the PNG dimensions")
    scanlines = bytearray()
    stride = width * 3
    for vertical in range(height):
        scanlines.append(0)
        start = vertical * stride
        scanlines.extend(pixels[start : start + stride])
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
        + png_chunk(b"IEND", b"")
    )


def set_rgb(pixels, width, horizontal, vertical, value):
    offset = (vertical * width + horizontal) * 3
    pixels[offset : offset + 3] = bytes((value, value, value))


def build_png_page(number, pattern):
    pixels = bytearray(PNG_WIDTH * PNG_HEIGHT * 3)
    for vertical in range(PNG_HEIGHT):
        for horizontal in range(PNG_WIDTH):
            patterned = pattern[vertical % 8] & (1 << (7 - horizontal % 8))
            value = 45 if patterned else 225
            if horizontal in (0, PNG_WIDTH - 1) or vertical in (0, PNG_HEIGHT - 1):
                value = 0
            set_rgb(pixels, PNG_WIDTH, horizontal, vertical, value)

    # A generated five-by-seven digit keeps every page identifiable without
    # importing a font or third-party artwork into the repository.
    scale = 10
    glyph = DIGITS[number]
    glyph_left = (PNG_WIDTH - 5 * scale) // 2
    glyph_top = (PNG_HEIGHT - 7 * scale) // 2
    for vertical in range(glyph_top - 8, glyph_top + 7 * scale + 8):
        for horizontal in range(glyph_left - 8, glyph_left + 5 * scale + 8):
            set_rgb(pixels, PNG_WIDTH, horizontal, vertical, 255)
    for row, bits in enumerate(glyph):
        for column in range(5):
            if bits & (1 << (4 - column)):
                for dy in range(scale):
                    for dx in range(scale):
                        set_rgb(
                            pixels,
                            PNG_WIDTH,
                            glyph_left + column * scale + dx,
                            glyph_top + row * scale + dy,
                            0,
                        )
    return encode_rgb_png(PNG_WIDTH, PNG_HEIGHT, pixels)


def build_png_refused_badge():
    width = BADGE_FRAME[3] - BADGE_FRAME[1]
    height = BADGE_FRAME[2] - BADGE_FRAME[0]
    pixels = bytearray([255] * (width * height * 3))
    for vertical in range(height):
        for horizontal in range(width):
            dx = horizontal - (width - 1) / 2.0
            dy = vertical - (height - 1) / 2.0
            if dx * dx + dy * dy <= 7.0 * 7.0:
                set_rgb(pixels, width, horizontal, vertical, 0)
            if 3 <= horizontal <= 12 and (
                abs(horizontal - vertical) <= 1
                or abs((width - 1 - horizontal) - vertical) <= 1
            ):
                set_rgb(pixels, width, horizontal, vertical, 255)
    return encode_rgb_png(width, height, pixels)


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
        png_payload = build_png_page(number, pattern)
        (output_dir / f"page{index}.png").write_bytes(png_payload)
        print(f"page{index}.png: {len(png_payload)} bytes")
    badge = build_refused_badge()
    (output_dir / "ngbadge.pict").write_bytes(badge)
    print(f"ngbadge.pict: {len(badge)} bytes")
    png_badge = build_png_refused_badge()
    (output_dir / "ngbadge.png").write_bytes(png_badge)
    print(f"ngbadge.png: {len(png_badge)} bytes")


if __name__ == "__main__":
    main()
