#!/usr/bin/env python3
"""Build a development-only PMonSprite scrapbook package from palette PNGs."""

import argparse
from collections import namedtuple
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import urllib.error
import urllib.request
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
SPRITE_COUNT = 12
SPRITE_SIZE = 96
ROW_BYTES = SPRITE_SIZE // 8
INDEXED_ROW_BYTES = SPRITE_SIZE
FRAME = (0, 0, 150, 200)
BITMAP_BOUNDS = (0, 0, SPRITE_SIZE, SPRITE_SIZE)
DESTINATION = (27, 52, 123, 148)
PACK_BITS_RECT_OFFSET = 564
SPRITE_URL = (
    "https://raw.githubusercontent.com/PokeAPI/sprites/master/"
    "sprites/pokemon/{index}.png"
)
DecodedSprite = namedtuple(
    "DecodedSprite", ("palette", "transparency", "indices", "luminance")
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


def paeth_predictor(left, above, upper_left):
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def reverse_filters(filtered, height, stride):
    expected = height * (stride + 1)
    if len(filtered) != expected:
        raise ValueError(
            "PNG scanline data has {} bytes; expected {}".format(
                len(filtered), expected
            )
        )

    rows = []
    cursor = 0
    for _ in range(height):
        filter_kind = filtered[cursor]
        cursor += 1
        row = bytearray(filtered[cursor : cursor + stride])
        cursor += stride
        previous = rows[-1] if rows else bytes(stride)

        for column in range(stride):
            left = row[column - 1] if column > 0 else 0
            above = previous[column]
            upper_left = previous[column - 1] if column > 0 else 0
            if filter_kind == 0:
                predictor = 0
            elif filter_kind == 1:
                predictor = left
            elif filter_kind == 2:
                predictor = above
            elif filter_kind == 3:
                predictor = (left + above) // 2
            elif filter_kind == 4:
                predictor = paeth_predictor(left, above, upper_left)
            else:
                raise ValueError("unsupported PNG scanline filter {}".format(filter_kind))
            row[column] = (row[column] + predictor) & 0xFF
        rows.append(row)
    return rows


def decode_palette_png(path):
    encoded = path.read_bytes()
    if not encoded.startswith(PNG_SIGNATURE):
        raise ValueError("{} is not a PNG file".format(path))

    cursor = len(PNG_SIGNATURE)
    header = None
    palette = None
    transparency = b""
    compressed = bytearray()
    saw_end = False

    while cursor < len(encoded):
        if cursor + 12 > len(encoded):
            raise ValueError("{} has a truncated PNG chunk".format(path))
        length = struct.unpack(">I", encoded[cursor : cursor + 4])[0]
        chunk_kind = encoded[cursor + 4 : cursor + 8]
        data_start = cursor + 8
        data_end = data_start + length
        chunk_end = data_end + 4
        if chunk_end > len(encoded):
            raise ValueError("{} has a truncated PNG chunk".format(path))
        chunk_data = encoded[data_start:data_end]
        expected_crc = struct.unpack(">I", encoded[data_end:chunk_end])[0]
        actual_crc = zlib.crc32(chunk_kind + chunk_data) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError("{} has a bad PNG chunk checksum".format(path))

        if chunk_kind == b"IHDR":
            if header is not None or length != 13:
                raise ValueError("{} has an invalid IHDR chunk".format(path))
            header = struct.unpack(">IIBBBBB", chunk_data)
        elif chunk_kind == b"PLTE":
            if length == 0 or length % 3 != 0 or length > 256 * 3:
                raise ValueError("{} has an invalid PLTE chunk".format(path))
            palette = [
                tuple(chunk_data[offset : offset + 3])
                for offset in range(0, length, 3)
            ]
        elif chunk_kind == b"tRNS":
            transparency = chunk_data
        elif chunk_kind == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_kind == b"IEND":
            if length != 0:
                raise ValueError("{} has an invalid IEND chunk".format(path))
            saw_end = True
            cursor = chunk_end
            break
        cursor = chunk_end

    if header is None or palette is None or not compressed or not saw_end:
        raise ValueError("{} is missing required palette PNG chunks".format(path))

    width, height, bit_depth, color_type, compression, filtering, interlace = header
    if width != SPRITE_SIZE or height != SPRITE_SIZE:
        raise ValueError(
            "{} is {}x{}; expected {}x{}".format(
                path, width, height, SPRITE_SIZE, SPRITE_SIZE
            )
        )
    if color_type != 3 or bit_depth not in (4, 8):
        raise ValueError(
            "{} must be a 4-bit or 8-bit palette PNG".format(path)
        )
    if compression != 0 or filtering != 0 or interlace != 0:
        raise ValueError(
            "{} must use standard PNG compression/filtering and no interlace".format(
                path
            )
        )
    if len(transparency) > len(palette):
        raise ValueError("{} has more alpha entries than palette entries".format(path))

    stride = (width * bit_depth + 7) // 8
    try:
        filtered = zlib.decompress(bytes(compressed))
    except zlib.error as error:
        raise ValueError("{} has invalid compressed PNG data: {}".format(path, error))
    rows = reverse_filters(filtered, height, stride)

    indices = []
    luminance = []
    for row in rows:
        index_row = bytearray()
        output_row = []
        for horizontal in range(width):
            if bit_depth == 8:
                palette_index = row[horizontal]
            else:
                packed = row[horizontal // 2]
                palette_index = (
                    packed >> 4 if horizontal % 2 == 0 else packed & 0x0F
                )
            if palette_index >= len(palette):
                raise ValueError("{} uses an undefined palette entry".format(path))
            index_row.append(palette_index)
            red, green, blue = palette[palette_index]
            alpha = (
                transparency[palette_index]
                if palette_index < len(transparency)
                else 255
            )
            red = (red * alpha + 255 * (255 - alpha) + 127) // 255
            green = (green * alpha + 255 * (255 - alpha) + 127) // 255
            blue = (blue * alpha + 255 * (255 - alpha) + 127) // 255
            output_row.append((299 * red + 587 * green + 114 * blue + 500) // 1000)
        indices.append(bytes(index_row))
        luminance.append(output_row)
    return DecodedSprite(
        tuple(palette), bytes(transparency), tuple(indices), luminance
    )


def dither_to_bitmap(luminance):
    working = [[float(value) for value in row] for row in luminance]
    bitmap = bytearray(SPRITE_SIZE * ROW_BYTES)

    for vertical in range(SPRITE_SIZE):
        for horizontal in range(SPRITE_SIZE):
            old_value = max(0.0, min(255.0, working[vertical][horizontal]))
            black = old_value < 128.0
            new_value = 0.0 if black else 255.0
            if black:
                byte_index = vertical * ROW_BYTES + horizontal // 8
                bitmap[byte_index] |= 1 << (7 - horizontal % 8)
            error = old_value - new_value
            if horizontal + 1 < SPRITE_SIZE:
                working[vertical][horizontal + 1] += error * 7.0 / 16.0
            if vertical + 1 < SPRITE_SIZE:
                if horizontal > 0:
                    working[vertical + 1][horizontal - 1] += error * 3.0 / 16.0
                working[vertical + 1][horizontal] += error * 5.0 / 16.0
                if horizontal + 1 < SPRITE_SIZE:
                    working[vertical + 1][horizontal + 1] += error / 16.0
    return bytes(bitmap)


def pack_bits(row):
    """Apple PackBits: repeat runs as (1 - count) & 0xFF + byte, literal runs
    as (count - 1) + bytes; runs of three or more repeats are worth packing."""
    out = bytearray()
    i = 0
    length = len(row)
    while i < length:
        run = 1
        while i + run < length and row[i + run] == row[i] and run < 128:
            run += 1
        if run >= 3:
            out.append((1 - run) & 0xFF)
            out.append(row[i])
            i += run
            continue
        literal_start = i
        i += run
        while i < length and i - literal_start < 128:
            run = 1
            while i + run < length and row[i + run] == row[i] and run < 128:
                run += 1
            if run >= 3:
                break
            i += run
        if i - literal_start > 128:
            i = literal_start + 128
        out.append(i - literal_start - 1)
        out += row[literal_start:i]
    return bytes(out)


def unpack_pack_bits(packed):
    """Reference PackBits decoder used by the indexed PICT self-check."""
    out = bytearray()
    cursor = 0
    while cursor < len(packed):
        control = packed[cursor]
        cursor += 1
        if control <= 127:
            count = control + 1
            if cursor + count > len(packed):
                raise ValueError("truncated PackBits literal")
            out += packed[cursor : cursor + count]
            cursor += count
        elif control >= 129:
            count = 257 - control
            if cursor >= len(packed):
                raise ValueError("truncated PackBits repeat")
            out.extend([packed[cursor]] * count)
            cursor += 1
    return bytes(out)


def assemble_picture(pack_bits_rect):
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
            *FRAME,
            0,
        ),
    )

    # An initial clip is not optional decoration: DrawPicture maps the
    # playback clip to the destination, and without a clip opcode the wide-open
    # default overflows 16-bit coordinates as soon as the picture is drawn
    # scaled, clipping every op to nothing. OpenPicture-recorded pictures get
    # this opcode from the recording port; a generated stream must carry it
    # itself. Region data: 2-byte region size (10) + bounding rect.
    operations += padded_op(0x0001, word(10) + rect(FRAME))  # Clip

    operations += padded_op(0x0098, pack_bits_rect)
    operations += word(0x00FF)  # OpEndPic

    picture = word(10 + len(operations)) + rect(FRAME) + operations
    if len(picture) & 1:
        raise AssertionError("version 2 picture must remain word-aligned")
    return bytes(512) + picture


def build_picture(bitmap):
    if len(bitmap) != SPRITE_SIZE * ROW_BYTES:
        raise ValueError("1-bit sprite data has the wrong size")

    # PackBitsRect, not BitsRect: QuickDraw's picture player keys its "is the
    # data packed" decision on rowBytes, not on the opcode -- recording never
    # emits an unpacked bitmap once rowBytes reaches 8, so playback assumes
    # rowBytes >= 8 means PackBits rows. An unpacked 0x0090 stream at
    # rowBytes 12 is "decompressed" into tiled garbage. Each scanline is a
    # 1-byte packed length (rowBytes <= 250) followed by its PackBits data.
    packed_rows = bytearray()
    for row_start in range(0, len(bitmap), ROW_BYTES):
        packed = pack_bits(bitmap[row_start:row_start + ROW_BYTES])
        if len(packed) > 250:
            raise AssertionError("packed scanline exceeds a 1-byte length")
        packed_rows.append(len(packed))
        packed_rows += packed
    return assemble_picture(
        word(ROW_BYTES)
        + rect(BITMAP_BOUNDS)
        + rect(BITMAP_BOUNDS)
        + rect(DESTINATION)
        + word(0)
        + packed_rows
    )


def indexed_sprite(decoded):
    palette = list(decoded.palette)
    transparent_indices = {
        index
        for index, alpha in enumerate(decoded.transparency)
        if alpha != 255
    }
    if not transparent_indices:
        return tuple(palette), decoded.indices

    white = (255, 255, 255)
    try:
        white_index = palette.index(white)
    except ValueError:
        if len(palette) == 256:
            raise ValueError("transparent pixels need a white palette entry")
        white_index = len(palette)
        palette.append(white)

    remapped_rows = []
    for row in decoded.indices:
        remapped_rows.append(
            bytes(
                white_index
                if palette_index in transparent_indices
                else palette_index
                for palette_index in row
            )
        )
    return tuple(palette), tuple(remapped_rows)


def build_indexed_picture(palette, rows):
    if not 1 <= len(palette) <= 256:
        raise ValueError("indexed sprite palette must have 1 through 256 entries")
    if len(rows) != SPRITE_SIZE or any(
        len(row) != INDEXED_ROW_BYTES for row in rows
    ):
        raise ValueError("8-bit sprite data has the wrong size")

    color_table = bytearray()
    color_table += struct.pack(">IHH", 0, 0x0000, len(palette) - 1)
    for index, (red, green, blue) in enumerate(palette):
        color_table += struct.pack(
            ">HHHH",
            index,
            red * 0x0101,
            green * 0x0101,
            blue * 0x0101,
        )

    packed_rows = bytearray()
    for row in rows:
        packed = pack_bits(row)
        if len(packed) > 255:
            raise AssertionError("packed scanline exceeds a 1-byte length")
        packed_rows.append(len(packed))
        packed_rows += packed

    # DirectBitsRect carries a baseAddr before rowBytes; PackBitsRect's PixMap
    # variant starts at rowBytes, so this record deliberately omits baseAddr.
    pixmap = (
        word(0x8000 | INDEXED_ROW_BYTES)
        + rect(BITMAP_BOUNDS)
        + struct.pack(
            ">HHIIIHHHHIII",
            0,          # pmVersion
            0,          # packType
            0,          # packSize
            72 << 16,   # hRes
            72 << 16,   # vRes
            0,          # pixelType: chunky
            8,          # pixelSize
            1,          # cmpCount
            8,          # cmpSize
            0,          # planeBytes
            0,          # pmTable
            0,          # pmReserved
        )
    )
    return assemble_picture(
        pixmap
        + color_table
        + rect(BITMAP_BOUNDS)
        + rect(DESTINATION)
        + word(0)
        + packed_rows
    )


def check_picture(picture, depth, color_count=None, indexed_rows=None):
    if picture[:512] != bytes(512):
        raise AssertionError("PICT file header must be 512 zero bytes")
    picture_size = struct.unpack(">H", picture[512:514])[0]
    if picture_size != len(picture) - 512:
        raise AssertionError("PICT picSize does not match the encoded picture")
    if picture[514:522] != rect(FRAME):
        raise AssertionError("PICT picFrame is incorrect")
    if picture[522:526] != word(0x0011) + word(0x02FF):
        raise AssertionError("PICT version opcode is incorrect")
    if picture[552:564] != word(0x0001) + word(10) + rect(FRAME):
        raise AssertionError("PICT initial clip opcode is missing or malformed")
    if depth == 1:
        expected = word(0x0098) + word(ROW_BYTES) + rect(BITMAP_BOUNDS)
        actual = picture[
            PACK_BITS_RECT_OFFSET : PACK_BITS_RECT_OFFSET + len(expected)
        ]
        if actual != expected:
            raise AssertionError("1-bit PackBitsRect header is malformed")
    elif depth == 256:
        if color_count is None or indexed_rows is None:
            raise AssertionError("indexed PICT check needs palette and source rows")
        expected = (
            word(0x0098)
            + word(0x8000 | INDEXED_ROW_BYTES)
            + rect(BITMAP_BOUNDS)
        )
        actual = picture[
            PACK_BITS_RECT_OFFSET : PACK_BITS_RECT_OFFSET + len(expected)
        ]
        if actual != expected:
            raise AssertionError("8-bit PackBitsRect PixMap header is malformed")

        pixel_fields_offset = PACK_BITS_RECT_OFFSET + 28
        if picture[pixel_fields_offset : pixel_fields_offset + 8] != (
            word(0) + word(8) + word(1) + word(8)
        ):
            raise AssertionError("8-bit PixMap pixel fields are malformed")

        color_table_offset = PACK_BITS_RECT_OFFSET + 48
        expected_table_header = struct.pack(">IHH", 0, 0x0000, color_count - 1)
        actual_table_header = picture[
            color_table_offset : color_table_offset + 8
        ]
        if actual_table_header != expected_table_header:
            raise AssertionError("8-bit PixMap ColorTable header is malformed")

        row_data_offset = color_table_offset + 8 + color_count * 8 + 18
        checked_row = max(
            range(len(indexed_rows)),
            key=lambda index: len(set(indexed_rows[index])),
        )
        packed_row_offset = row_data_offset
        for _ in range(checked_row):
            packed_row_offset += 1 + picture[packed_row_offset]
        packed_length = picture[packed_row_offset]
        packed_row = picture[
            packed_row_offset + 1 : packed_row_offset + 1 + packed_length
        ]
        if unpack_pack_bits(packed_row) != indexed_rows[checked_row]:
            raise AssertionError(
                "8-bit PackBits scanline does not round-trip to source indices"
            )
    else:
        raise AssertionError("unsupported PICT check depth")
    if len(picture) & 1:
        raise AssertionError("PICT file is not word-aligned")


def ensure_sprite(path, index):
    if path.is_file():
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    url = SPRITE_URL.format(index=index)
    print("{} is missing; downloading {}".format(path, url))
    try:
        with urllib.request.urlopen(url) as response:
            encoded = response.read()
        path.write_bytes(encoded)
    except (OSError, urllib.error.URLError) as error:
        raise RuntimeError(
            "{} is missing and could not be downloaded (offline?): {}".format(
                path, error
            )
        )


def manifest_contents(depth, sprite_count=SPRITE_COUNT):
    if depth not in (1, 256):
        raise ValueError("unsupported manifest depth")
    # Bag 0 must be ui because ScrapbookPackage.hpp pins
    # kUiBagIndex = 0 / kFirstPageBagIndex = 1.
    lines = [
        "# One sprite per bag: a page flip reads exactly one independently verifiable bag.",
        "",
        "bag ui",
        "asset 9001 image UI/RefusedBadge ngbadge.pict",
    ]
    for index in range(1, sprite_count + 1):
        lines.extend(
            [
                "",
                "bag pmon-{}".format(index),
                "asset {} image PMON_SPRITE_{} pmon-{}.pict".format(
                    1000 + index, index, index
                ),
            ]
        )
    return "\n".join(lines) + "\n"


def write_manifest(path, depth, sprite_count=SPRITE_COUNT):
    path.write_text(manifest_contents(depth, sprite_count), encoding="ascii")


def pack_assets(lrpc, manifest, package, stamp_path, requirements, page_count):
    command = [
        str(lrpc),
        "pack",
        str(manifest),
        "-o",
        str(package),
        "--stamp",
        str(stamp_path),
        "--require",
        str(requirements),
        "--require-pages",
        str(page_count),
    ]
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.stdout:
        print(completed.stdout, end="")
    if completed.stderr:
        print(completed.stderr, file=sys.stderr, end="")
    if completed.returncode != 0:
        raise RuntimeError("lrpc failed with exit code {}".format(completed.returncode))

    match = re.search(r"\bstamp ([0-9]+)\)", completed.stdout)
    if match is None:
        raise RuntimeError("lrpc output did not report the derived id-space stamp")
    printed_stamp = match.group(1)
    written_stamp = stamp_path.read_text(encoding="ascii").strip()
    if written_stamp != printed_stamp:
        raise RuntimeError(
            "lrpc printed stamp {} but wrote {}".format(
                printed_stamp, written_stamp
            )
        )
    # lrpc staged and atomically committed that file itself; rewriting the
    # same bytes here could only make things worse (a failed rewrite leaves a
    # truncated stamp beside a good package).
    return printed_stamp


def parse_arguments(repo_root):
    parser = argparse.ArgumentParser(
        description="Build the development-only PMonSprite ScrapbookUI package.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=(
            "Depth 1 (the default) emits the current dithered 1-bit BitMap package\n"
            "under build/pmonsprite-lrp/. Depth 256 emits an 8-bit indexed PixMap\n"
            "package under build/pmonsprite-lrp-256/ without dithering.\n\n"
            "On the current black-and-white window, a 256-color picture degrades to\n"
            "QuickDraw's basic-port color mapping. Full fidelity waits for the\n"
            "color-window arm."
        ),
    )
    parser.add_argument(
        "--sprites-dir",
        type=Path,
        default=repo_root / "build" / "pmonsprite-staging",
        help="directory containing consecutively numbered PNGs starting at 1",
    )
    parser.add_argument(
        "--sprite-count",
        type=int,
        default=SPRITE_COUNT,
        help="number of consecutively numbered sprite PNGs (default: 12)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="output directory (default: the depth-specific repository build dir)",
    )
    parser.add_argument(
        "--lrpc",
        type=Path,
        default=repo_root / "build" / "host" / "lrpc" / "lrpc",
        help="path to the host lrpc executable",
    )
    parser.add_argument(
        "--depth",
        type=int,
        choices=(1, 256),
        default=1,
        help=(
            "sprite depth: dithered 1-bit BitMap or indexed 8-bit PixMap "
            "(default: 1)"
        ),
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="report the built-in PICT header self-check",
    )
    return parser.parse_args()


def main():
    repo_root = Path(__file__).resolve().parents[3]
    arguments = parse_arguments(repo_root)
    if arguments.sprite_count < 1:
        raise ValueError("sprite count must be positive")
    output_dir_name = (
        "pmonsprite-lrp" if arguments.depth == 1 else "pmonsprite-lrp-256"
    )
    output_dir = arguments.output_dir or repo_root / "build" / output_dir_name
    output_dir.mkdir(parents=True, exist_ok=True)

    for index in range(1, arguments.sprite_count + 1):
        sprite_path = arguments.sprites_dir / "{}.png".format(index)
        ensure_sprite(sprite_path, index)
        decoded = decode_palette_png(sprite_path)
        if arguments.depth == 1:
            picture = build_picture(dither_to_bitmap(decoded.luminance))
            check_picture(picture, arguments.depth)
        else:
            palette, indexed_rows = indexed_sprite(decoded)
            picture = build_indexed_picture(palette, indexed_rows)
            check_picture(
                picture,
                arguments.depth,
                color_count=len(palette),
                indexed_rows=indexed_rows,
            )
        output_path = output_dir / "pmon-{}.pict".format(index)
        output_path.write_bytes(picture)
        print("{}: {} bytes".format(output_path, len(picture)))

    if arguments.check:
        if manifest_contents(1, arguments.sprite_count) != manifest_contents(
            256, arguments.sprite_count
        ):
            raise AssertionError("depth-specific manifests must remain identical")
        if arguments.depth == 1:
            detail = "1-bit PackBitsRect rowBytes/bounds"
        else:
            detail = (
                "8-bit PixMap rowBytes/pixelSize/ColorTable and PackBits round-trip"
            )
        print(
            "PICT byte-level self-check: OK "
            "(picSize, frame, version, initial clip, {})".format(detail)
        )

    manifest = output_dir / "manifest.txt"
    package = output_dir / "ASSETS.LRP"
    stamp_path = output_dir / "stamp.txt"
    shutil.copyfile(
        Path(__file__).resolve().with_name("ngbadge.pict"),
        output_dir / "ngbadge.pict",
    )
    write_manifest(manifest, arguments.depth, arguments.sprite_count)
    requirements = Path(__file__).resolve().with_name("scrapbook.pkgreq")
    page_count = arguments.sprite_count
    stamp = pack_assets(
        arguments.lrpc,
        manifest,
        package,
        stamp_path,
        requirements,
        page_count,
    )
    # The build step reads these instead of hardcoding the values, so a
    # changed sprite roster cannot leave the app compiled against a package
    # its open() checks are guaranteed to reject.
    (output_dir / "page-count.txt").write_text(
        "{}\n".format(page_count), encoding="ascii"
    )

    if arguments.check:
        other_depth = 256 if arguments.depth == 1 else 1
        if manifest.read_text(encoding="ascii") != manifest_contents(
            other_depth, arguments.sprite_count
        ):
            raise AssertionError("written manifests differ between depths")
        if arguments.output_dir is None:
            other_output = repo_root / "build" / (
                "pmonsprite-lrp-256"
                if arguments.depth == 1
                else "pmonsprite-lrp"
            )
            other_manifest_path = other_output / "manifest.txt"
            if (
                other_manifest_path.is_file()
                and other_manifest_path.read_text(encoding="ascii")
                != manifest.read_text(encoding="ascii")
            ):
                raise AssertionError("generated manifests differ between depths")
            other_stamp_path = other_output / "stamp.txt"
            if other_stamp_path.is_file():
                other_stamp = other_stamp_path.read_text(encoding="ascii").strip()
                if other_stamp != stamp:
                    raise AssertionError(
                        "depth stamps differ: {} versus {}".format(
                            stamp, other_stamp
                        )
                    )
        print(
            "Depth-independent manifest/stamp self-check: OK "
            "(depth 1 = depth 256 = {})".format(stamp)
        )

    print("Package: {}".format(package))
    print("Stamp: {}".format(stamp))
    print(
        "Build the {}-page ScrapbookUI app from the repository root with:".format(
            arguments.sprite_count
        )
    )
    print(
        "  cmake -S . -B build/retro68-pmon -G Ninja "
        "-DCMAKE_BUILD_TYPE=Release "
        "-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Retro68.cmake "
        "-DRETRO68_CPU=m68k -DLOKA_WARNINGS_AS_ERRORS=ON "
        "-DLOKA_SCRAPBOOK_PAGE_COUNT={} "
        "-DLOKA_SCRAPBOOK_ID_SPACE_STAMP={}".format(
            arguments.sprite_count, stamp
        )
    )
    print(
        "  cmake --build build/retro68-pmon "
        "--target ScrapbookUI68K_APPL"
    )


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, RuntimeError) as error:
        print("make_pmonsprite_lrp.py: error: {}".format(error), file=sys.stderr)
        sys.exit(1)
