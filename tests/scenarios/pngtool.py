#!/usr/bin/env python3
"""Crop, compare, diff, and animate 8-bit RGB/RGBA PNGs using the standard library."""

import argparse
import collections
import os
import re
import struct
import sys
import tempfile
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class PngError(ValueError):
    pass


class Image:
    def __init__(self, width, height, rgba):
        if width <= 0 or height <= 0:
            raise PngError("image dimensions must be positive")
        if len(rgba) != width * height * 4:
            raise PngError("pixel buffer does not match image dimensions")
        self.width = width
        self.height = height
        self.rgba = bytes(rgba)


ImageDifference = collections.namedtuple(
    "ImageDifference", ("pixel_count", "column_count", "first_difference")
)


def _paeth(left, above, upper_left):
    estimate = left + above - upper_left
    left_distance = abs(estimate - left)
    above_distance = abs(estimate - above)
    upper_left_distance = abs(estimate - upper_left)
    if left_distance <= above_distance and left_distance <= upper_left_distance:
        return left
    if above_distance <= upper_left_distance:
        return above
    return upper_left


def _chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def _build_png(width, height, color_type, scanlines):
    header = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    return (
        PNG_SIGNATURE
        + _chunk(b"IHDR", header)
        + _chunk(b"IDAT", zlib.compress(scanlines))
        + _chunk(b"IEND", b"")
    )


def read_png(path):
    with open(path, "rb") as handle:
        payload = handle.read()
    if not payload.startswith(PNG_SIGNATURE):
        raise PngError("not a PNG file")

    offset = len(PNG_SIGNATURE)
    width = height = color_type = None
    compressed = bytearray()
    saw_iend = False
    while offset < len(payload):
        if offset + 12 > len(payload):
            raise PngError("truncated PNG chunk")
        length = struct.unpack(">I", payload[offset : offset + 4])[0]
        kind = payload[offset + 4 : offset + 8]
        data_start = offset + 8
        data_end = data_start + length
        crc_end = data_end + 4
        if crc_end > len(payload):
            raise PngError("truncated PNG chunk payload")
        data = payload[data_start:data_end]
        expected_crc = struct.unpack(">I", payload[data_end:crc_end])[0]
        actual_crc = zlib.crc32(kind)
        actual_crc = zlib.crc32(data, actual_crc) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise PngError("CRC mismatch in {} chunk".format(kind.decode("ascii", "replace")))

        if kind == b"IHDR":
            if width is not None or length != 13:
                raise PngError("invalid IHDR chunk")
            width, height, bit_depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", data
            )
            if width == 0 or height == 0:
                raise PngError("image dimensions must be positive")
            if bit_depth != 8 or color_type not in (2, 6):
                raise PngError("only 8-bit RGB and RGBA PNG files are supported")
            if compression != 0 or filtering != 0 or interlace != 0:
                raise PngError("unsupported PNG compression, filter method, or interlace mode")
        elif kind == b"IDAT":
            if width is None:
                raise PngError("IDAT appears before IHDR")
            compressed.extend(data)
        elif kind == b"PLTE":
            if width is None:
                raise PngError("PLTE appears before IHDR")
        elif kind == b"IEND":
            if length != 0:
                raise PngError("invalid IEND chunk")
            saw_iend = True
            offset = crc_end
            break
        elif kind[:1].isupper():
            raise PngError("unsupported critical PNG chunk {}".format(kind.decode("ascii", "replace")))
        offset = crc_end

    if width is None or not compressed or not saw_iend:
        raise PngError("PNG is missing IHDR, IDAT, or IEND")
    if offset != len(payload):
        raise PngError("data follows IEND")

    channels = 3 if color_type == 2 else 4
    row_bytes = width * channels
    expected_size = height * (row_bytes + 1)
    raw = zlib.decompress(bytes(compressed))
    if len(raw) != expected_size:
        raise PngError("decompressed image data has the wrong size")

    decoded_rows = []
    previous = bytearray(row_bytes)
    raw_offset = 0
    for _ in range(height):
        filter_type = raw[raw_offset]
        raw_offset += 1
        filtered = raw[raw_offset : raw_offset + row_bytes]
        raw_offset += row_bytes
        if filter_type > 4:
            raise PngError("unsupported PNG row filter {}".format(filter_type))

        row = bytearray(row_bytes)
        for column, value in enumerate(filtered):
            left = row[column - channels] if column >= channels else 0
            above = previous[column]
            upper_left = previous[column - channels] if column >= channels else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            else:
                predictor = _paeth(left, above, upper_left)
            row[column] = (value + predictor) & 0xFF
        decoded_rows.append(row)
        previous = row

    rgba = bytearray(width * height * 4)
    destination = 0
    for row in decoded_rows:
        for source in range(0, len(row), channels):
            rgba[destination : destination + 3] = row[source : source + 3]
            rgba[destination + 3] = row[source + 3] if channels == 4 else 255
            destination += 4
    return Image(width, height, rgba)


def write_png(path, image):
    scanlines = bytearray()
    row_rgba_bytes = image.width * 4
    for y in range(image.height):
        scanlines.append(0)
        row_start = y * row_rgba_bytes
        for x in range(image.width):
            pixel_start = row_start + x * 4
            scanlines.extend(image.rgba[pixel_start : pixel_start + 4])
    with open(path, "wb") as handle:
        handle.write(_build_png(image.width, image.height, 6, bytes(scanlines)))


def normalize_image(image, left, top, right, bottom):
    if not (0 <= left < right <= image.width and 0 <= top < bottom <= image.height):
        raise PngError(
            "capture rectangle ({}, {}, {}, {}) is outside {}x{}".format(
                left, top, right, bottom, image.width, image.height
            )
        )
    normalized = bytearray(image.width * image.height * 4)
    for y in range(top, bottom):
        for x in range(left, right):
            offset = (y * image.width + x) * 4
            normalized[offset : offset + 3] = image.rgba[offset : offset + 3]
            normalized[offset + 3] = 255
    return Image(image.width, image.height, normalized)


def read_capture_rectangle(path):
    with open(path, "rb") as handle:
        payload = handle.read()
    if re.fullmatch(rb"-?\d+ -?\d+ -?\d+ -?\d+\n", payload) is None:
        raise PngError("capture rectangle must be one 'left top right bottom' line")
    return tuple(int(field) for field in payload.split())


def alpha_mask_bounds(image):
    left = image.width
    top = image.height
    right = 0
    bottom = 0
    for y in range(image.height):
        for x in range(image.width):
            alpha = image.rgba[(y * image.width + x) * 4 + 3]
            if alpha not in (0, 255):
                raise PngError("golden alpha mask contains a non-binary alpha value")
            if alpha == 255:
                left = min(left, x)
                top = min(top, y)
                right = max(right, x + 1)
                bottom = max(bottom, y + 1)
    if right == 0 or bottom == 0:
        raise PngError("golden alpha mask is empty")
    for y in range(image.height):
        for x in range(image.width):
            expected = 255 if left <= x < right and top <= y < bottom else 0
            if image.rgba[(y * image.width + x) * 4 + 3] != expected:
                raise PngError("golden alpha mask is not one rectangular region")
    return left, top, right, bottom


def crop_image(image, left, top, right, bottom):
    if not (0 <= left < right <= image.width and 0 <= top < bottom <= image.height):
        raise PngError(
            "crop rectangle ({}, {}, {}, {}) is outside {}x{}".format(
                left, top, right, bottom, image.width, image.height
            )
        )
    width = right - left
    height = bottom - top
    cropped = bytearray(width * height * 4)
    destination = 0
    for y in range(top, bottom):
        source = (y * image.width + left) * 4
        count = width * 4
        cropped[destination : destination + count] = image.rgba[source : source + count]
        destination += count
    return Image(width, height, cropped)


def measure_image_difference(first, second):
    first_difference = None
    difference_count = 0
    differing_columns = set()
    for y in range(max(first.height, second.height)):
        for x in range(max(first.width, second.width)):
            first_offset = (y * first.width + x) * 4
            second_offset = (y * second.width + x) * 4
            first_pixel = (
                tuple(first.rgba[first_offset : first_offset + 4])
                if x < first.width and y < first.height
                else None
            )
            second_pixel = (
                tuple(second.rgba[second_offset : second_offset + 4])
                if x < second.width and y < second.height
                else None
            )
            if first_pixel != second_pixel:
                if first_difference is None:
                    first_difference = (x, y, first_pixel, second_pixel)
                difference_count += 1
                differing_columns.add(x)

    return ImageDifference(difference_count, len(differing_columns), first_difference)


def report_image_difference(first, second, difference):
    first_difference = difference.first_difference
    if first_difference is not None and (
        first.width != second.width or first.height != second.height
    ):
        print(
            "dimensions differ: {}x{} != {}x{}".format(
                first.width, first.height, second.width, second.height
            )
        )
    if first_difference is not None:
        x, y, first_pixel, second_pixel = first_difference
        print(
            "first differing pixel ({}, {}): {} != {}; differing pixels: {}".format(
                x, y, first_pixel, second_pixel, difference.pixel_count
            )
        )


def compare_images(first, second, report=True):
    difference = measure_image_difference(first, second)
    if report:
        report_image_difference(first, second, difference)
    return difference.pixel_count


def difference_image(expected, actual):
    width = max(expected.width, actual.width)
    height = max(expected.height, actual.height)
    pixels = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            output = (y * width + x) * 4
            expected_offset = (y * expected.width + x) * 4
            actual_offset = (y * actual.width + x) * 4
            expected_pixel = (
                expected.rgba[expected_offset : expected_offset + 4]
                if x < expected.width and y < expected.height
                else None
            )
            actual_pixel = (
                actual.rgba[actual_offset : actual_offset + 4]
                if x < actual.width and y < actual.height
                else None
            )
            if expected_pixel == actual_pixel and actual_pixel is not None:
                red, green, blue, _ = actual_pixel
                gray = (red * 30 + green * 59 + blue * 11) // 100
                pixels[output : output + 4] = bytes((gray, gray, gray, 255))
            else:
                pixels[output : output + 4] = bytes((255, 0, 255, 255))
    return Image(width, height, pixels)



# --- GIF89a writer -------------------------------------------------------
#
# MAME captures evidence as a PNG per frame. Turning a capture run into
# something watchable in a pull request needs an animation, and the rigs this
# repository is driven from have no ffmpeg, no pip, and no image library --
# so the assembler has to live here, beside the PNG reader that already
# decodes those frames with nothing but zlib.


GIF_MAX_CODE = 4096


class _BitWriter:
    """LSB-first variable-width code packer, as GIF image data requires."""

    def __init__(self):
        self._bytes = bytearray()
        self._bits = 0
        self._count = 0

    def write(self, code, width):
        self._bits |= code << self._count
        self._count += width
        while self._count >= 8:
            self._bytes.append(self._bits & 0xFF)
            self._bits >>= 8
            self._count -= 8

    def finish(self):
        if self._count > 0:
            self._bytes.append(self._bits & 0xFF)
            self._bits = 0
            self._count = 0
        return bytes(self._bytes)


def _gif_lzw_encode(indices, min_code_size):
    clear_code = 1 << min_code_size
    end_code = clear_code + 1
    writer = _BitWriter()
    table = {}
    next_code = end_code + 1
    code_size = min_code_size + 1
    writer.write(clear_code, code_size)
    if not indices:
        writer.write(end_code, code_size)
        return writer.finish()

    prefix = indices[0]
    for value in indices[1:]:
        key = (prefix, value)
        if key in table:
            prefix = table[key]
            continue
        writer.write(prefix, code_size)
        if next_code < GIF_MAX_CODE:
            table[key] = next_code
            next_code += 1
            # One past the width, not at it: a decoder only adds its entry on
            # the following code, so it is always one behind the encoder here.
            # Bumping at (1 << code_size) desynchronises the two by one code.
            if next_code == (1 << code_size) + 1 and code_size < 12:
                code_size += 1
        else:
            writer.write(clear_code, code_size)
            table = {}
            next_code = end_code + 1
            code_size = min_code_size + 1
        prefix = value
    writer.write(prefix, code_size)
    writer.write(end_code, code_size)
    return writer.finish()


def _gif_lzw_decode(payload, min_code_size):
    """Decoder used only by selftest: an encoder nothing here can view has to
    be checked against a reader, not against inspection."""
    clear_code = 1 << min_code_size
    end_code = clear_code + 1
    code_size = min_code_size + 1
    table = None
    previous = None
    output = []
    bit = 0
    total_bits = len(payload) * 8
    while bit + code_size <= total_bits:
        code = 0
        for offset in range(code_size):
            index = bit + offset
            if payload[index >> 3] & (1 << (index & 7)):
                code |= 1 << offset
        bit += code_size
        if code == clear_code:
            table = [[i] for i in range(clear_code)] + [[], []]
            code_size = min_code_size + 1
            previous = None
            continue
        if code == end_code:
            break
        if table is None:
            raise PngError("GIF data did not start with a clear code")
        if code < len(table):
            entry = list(table[code])
        elif previous is not None:
            entry = list(previous) + [previous[0]]
        else:
            raise PngError("GIF data referenced an undefined code")
        output.extend(entry)
        if previous is not None:
            table.append(list(previous) + [entry[0]])
            if len(table) == (1 << code_size) and code_size < 12:
                code_size += 1
        previous = entry
    return output


def _gif_blocks(payload):
    blocks = bytearray()
    position = 0
    while position < len(payload):
        chunk = payload[position:position + 255]
        blocks.append(len(chunk))
        blocks.extend(chunk)
        position += len(chunk)
    blocks.append(0)
    return bytes(blocks)


def scale_image(image, factor):
    """Nearest-neighbour downscale. Averaging would invent colours a 1-bit
    Classic screen never had and inflate the palette; nearest keeps both the
    palette and the pixel grid honest."""
    if factor <= 1:
        return image
    width = image.width // factor
    height = image.height // factor
    if width <= 0 or height <= 0:
        raise PngError("scale factor is larger than the image")
    source = image.rgba
    scaled = bytearray(width * height * 4)
    for y in range(height):
        source_row = (y * factor) * image.width
        target_row = y * width
        for x in range(width):
            source_offset = (source_row + x * factor) * 4
            target_offset = (target_row + x) * 4
            scaled[target_offset:target_offset + 4] = source[source_offset:source_offset + 4]
    return Image(width, height, bytes(scaled))


def _palette_for(images):
    """One palette for every frame, so the animation cannot shift colour
    between frames. Exact while the capture fits 256 colours; a Classic screen
    capture always does."""
    colours = {}
    for image in images:
        pixels = image.rgba
        for offset in range(0, len(pixels), 4):
            key = pixels[offset:offset + 3]
            if key not in colours:
                colours[key] = len(colours)
                if len(colours) > 256:
                    return None
    return colours


def _quantized_palette():
    colours = {}
    for red in range(8):
        for green in range(8):
            for blue in range(4):
                key = bytes((red * 255 // 7, green * 255 // 7, blue * 255 // 3))
                colours.setdefault(key, len(colours))
    return colours


def _index_frame(image, colours, quantized):
    pixels = image.rgba
    indices = []
    append = indices.append
    for offset in range(0, len(pixels), 4):
        key = pixels[offset:offset + 3]
        if quantized:
            key = bytes((
                (key[0] >> 5) * 255 // 7,
                (key[1] >> 5) * 255 // 7,
                (key[2] >> 6) * 255 // 3,
            ))
        append(colours[key])
    return indices


def write_gif(path, images, delay_centiseconds):
    if not images:
        raise PngError("an animation needs at least one frame")
    width = images[0].width
    height = images[0].height
    for image in images:
        if image.width != width or image.height != height:
            raise PngError("every frame must share the animation's dimensions")

    colours = _palette_for(images)
    quantized = colours is None
    if quantized:
        colours = _quantized_palette()

    table_bits = max(1, (max(1, len(colours)) - 1).bit_length())
    table_size = 1 << table_bits
    palette = bytearray(table_size * 3)
    for key, index in colours.items():
        palette[index * 3:index * 3 + 3] = key

    min_code_size = max(2, table_bits)
    packed = 0x80 | ((table_bits - 1) << 4) | (table_bits - 1)

    payload = bytearray()
    payload.extend(b"GIF89a")
    payload.extend(struct.pack("<HHBBB", width, height, packed, 0, 0))
    payload.extend(palette)
    # Loop forever: the reel it records does.
    payload.extend(b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00")
    for image in images:
        payload.extend(b"\x21\xf9\x04\x04")
        payload.extend(struct.pack("<HBB", delay_centiseconds, 0, 0))
        payload.extend(b"\x2c")
        payload.extend(struct.pack("<HHHHB", 0, 0, width, height, 0))
        payload.append(min_code_size)
        indices = _index_frame(image, colours, quantized)
        payload.extend(_gif_blocks(_gif_lzw_encode(indices, min_code_size)))
    payload.extend(b"\x3b")

    with open(path, "wb") as handle:
        handle.write(bytes(payload))
    return len(payload)


def _filtered_scanline(row, previous, channels, filter_type):
    filtered = bytearray(len(row))
    for column, value in enumerate(row):
        left = row[column - channels] if column >= channels else 0
        above = previous[column]
        upper_left = previous[column - channels] if column >= channels else 0
        if filter_type == 0:
            predictor = 0
        elif filter_type == 1:
            predictor = left
        elif filter_type == 2:
            predictor = above
        elif filter_type == 3:
            predictor = (left + above) // 2
        else:
            predictor = _paeth(left, above, upper_left)
        filtered[column] = (value - predictor) & 0xFF
    return filtered


def _write_rgba_with_filters(path, image):
    row_bytes = image.width * 4
    previous = bytearray(row_bytes)
    scanlines = bytearray()
    for y in range(image.height):
        row = image.rgba[y * row_bytes : (y + 1) * row_bytes]
        filter_type = y % 5
        scanlines.append(filter_type)
        scanlines.extend(_filtered_scanline(row, previous, 4, filter_type))
        previous = row
    with open(path, "wb") as handle:
        handle.write(_build_png(image.width, image.height, 6, bytes(scanlines)))


def selftest():
    width = 7
    height = 5
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels.extend(
                (
                    (x * 31 + y * 7) & 0xFF,
                    (x * 11 + y * 43) & 0xFF,
                    (x * 5 + y * 19) & 0xFF,
                    (x * 47 + y * 29) & 0xFF,
                )
            )
    source = Image(width, height, pixels)

    normalized = normalize_image(source, 2, 1, 6, 4)
    for y in range(height):
        for x in range(width):
            offset = (y * width + x) * 4
            pixel = normalized.rgba[offset : offset + 4]
            if 2 <= x < 6 and 1 <= y < 4:
                expected = source.rgba[offset : offset + 3] + bytes((255,))
            else:
                expected = bytes((0, 0, 0, 0))
            if pixel != expected:
                raise PngError("window-mask normalization selftest failed")
    if alpha_mask_bounds(normalized) != (2, 1, 6, 4):
        raise PngError("window-mask alpha bounds selftest failed")
    outside_changed = bytearray(source.rgba)
    outside_changed[0:4] = bytes((255, 255, 255, 255))
    if compare_images(
        normalized,
        normalize_image(Image(width, height, outside_changed), 2, 1, 6, 4),
        report=False,
    ) != 0:
        raise PngError("outside-window difference survived normalization")

    with tempfile.TemporaryDirectory(prefix="pngcrop-selftest-") as directory:
        filtered_path = os.path.join(directory, "filtered.png")
        cropped_path = os.path.join(directory, "cropped.png")
        roundtrip_path = os.path.join(directory, "roundtrip.png")
        _write_rgba_with_filters(filtered_path, source)
        decoded = read_png(filtered_path)
        if compare_images(source, decoded, report=False) != 0:
            raise PngError("RGBA/filter decode selftest failed")

        expected_crop = crop_image(source, 1, 1, 6, 5)
        write_png(cropped_path, expected_crop)
        decoded_crop = read_png(cropped_path)
        write_png(roundtrip_path, decoded_crop)
        if decoded_crop.rgba[3::4] != expected_crop.rgba[3::4]:
            raise PngError("write_png alpha round-trip selftest failed")
        if compare_images(expected_crop, decoded_crop, report=False) != 0:
            raise PngError("crop/write/read selftest failed")
        if compare_images(
            read_png(cropped_path), read_png(roundtrip_path), report=False
        ) != 0:
            raise PngError("compare round-trip selftest failed")

        changed = bytearray(decoded_crop.rgba)
        changed[0] ^= 1
        changed_image = Image(decoded_crop.width, decoded_crop.height, changed)
        if compare_images(decoded_crop, changed_image, report=False) == 0:
            raise PngError("compare mismatch selftest failed")
        difference = difference_image(decoded_crop, changed_image)
        if difference.rgba[:4] != bytes((255, 0, 255, 255)):
            raise PngError("difference image mismatch selftest failed")

    # An encoder whose output nothing on this host can open has to be checked
    # against a reader, not against inspection.
    for min_code_size in (2, 4, 8):
        alphabet = 1 << min_code_size
        symbols = list(range(alphabet)) * 7
        # Every symbol must stay inside the alphabet: a value at or above the
        # clear code is not a pixel index, and emitting one would collide with
        # the reserved codes rather than exercise the encoder.
        symbols += [value % alphabet for value in (0, 0, 0, 1, 1, 2, 3, 3, 3, 3, 5)]
        decoded = _gif_lzw_decode(_gif_lzw_encode(symbols, min_code_size), min_code_size)
        if decoded != symbols:
            raise PngError("GIF LZW round-trip failed at min_code_size {}".format(min_code_size))

    # A run long enough to exhaust the 12-bit table and force a mid-stream
    # clear, which is where an encoder that miscounts code width breaks.
    long_symbols = [(index * 7 + index // 251) % 256 for index in range(60000)]
    if _gif_lzw_decode(_gif_lzw_encode(long_symbols, 8), 8) != long_symbols:
        raise PngError("GIF LZW round-trip failed across a table reset")

    with tempfile.TemporaryDirectory() as directory:
        frame_a = Image(4, 2, bytes([255, 0, 0, 255] * 4 + [0, 0, 255, 255] * 4))
        frame_b = Image(4, 2, bytes([0, 0, 255, 255] * 8))
        gif_path = os.path.join(directory, "selftest.gif")
        written = write_gif(gif_path, [frame_a, frame_b], 10)
        with open(gif_path, "rb") as handle:
            payload = handle.read()
        if not payload.startswith(b"GIF89a") or not payload.endswith(b"\x3b"):
            raise PngError("GIF container is malformed")
        if written != len(payload):
            raise PngError("GIF byte count disagrees with the file")
        if payload.count(b"\x21\xf9\x04") != 2:
            raise PngError("GIF is missing a per-frame graphic control block")

    scaled = scale_image(Image(4, 2, bytes([9, 8, 7, 255] * 8)), 2)
    if scaled.width != 2 or scaled.height != 1:
        raise PngError("scale_image produced the wrong dimensions")

    print("pngcrop selftest: ok")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    crop_parser = subparsers.add_parser("crop", help="crop a PNG rectangle")
    crop_parser.add_argument("input")
    crop_parser.add_argument("left", type=int)
    crop_parser.add_argument("top", type=int)
    crop_parser.add_argument("right", type=int)
    crop_parser.add_argument("bottom", type=int)
    crop_parser.add_argument("output")

    normalize_parser = subparsers.add_parser(
        "normalize", help="mask a full-screen PNG to a capture rectangle record"
    )
    normalize_parser.add_argument("input")
    normalize_parser.add_argument("rectangle_record")
    normalize_parser.add_argument("output")

    bounds_parser = subparsers.add_parser(
        "alpha-bounds", help="print and validate the rectangular alpha mask"
    )
    bounds_parser.add_argument("input")

    compare_parser = subparsers.add_parser("compare", help="compare decoded pixels")
    compare_parser.add_argument("--max-diff-px", type=int, default=0)
    compare_parser.add_argument("--max-diff-columns", type=int)
    compare_parser.add_argument("first")
    compare_parser.add_argument("second")

    diff_parser = subparsers.add_parser("diff", help="write a visual exact-pixel diff")
    diff_parser.add_argument("expected")
    diff_parser.add_argument("actual")
    diff_parser.add_argument("output")

    gif_parser = subparsers.add_parser("gif", help="assemble PNG frames into an animated GIF")
    gif_parser.add_argument("output")
    gif_parser.add_argument("inputs", nargs="+")
    gif_parser.add_argument("--delay", type=int, default=10,
                            help="frame delay in centiseconds (default 10)")
    gif_parser.add_argument("--scale", type=int, default=1,
                            help="integer nearest-neighbour downscale factor (default 1)")
    gif_parser.add_argument("--stride", type=int, default=1,
                            help="keep every Nth input frame (default 1)")

    subparsers.add_parser("selftest", help="exercise decode, crop, write, compare, and animate")
    arguments = parser.parse_args(argv)

    try:
        if arguments.command == "crop":
            image = read_png(arguments.input)
            write_png(
                arguments.output,
                crop_image(image, arguments.left, arguments.top, arguments.right, arguments.bottom),
            )
            return 0
        if arguments.command == "normalize":
            image = read_png(arguments.input)
            rectangle = read_capture_rectangle(arguments.rectangle_record)
            write_png(arguments.output, normalize_image(image, *rectangle))
            print("{} {} {} {}".format(*rectangle))
            return 0
        if arguments.command == "alpha-bounds":
            rectangle = alpha_mask_bounds(read_png(arguments.input))
            print("{} {} {} {}".format(*rectangle))
            return 0
        if arguments.command == "compare":
            if arguments.max_diff_px < 0:
                raise PngError("--max-diff-px must not be negative")
            if arguments.max_diff_columns is not None and arguments.max_diff_columns < 0:
                raise PngError("--max-diff-columns must not be negative")
            expected = read_png(arguments.first)
            actual = read_png(arguments.second)
            difference = measure_image_difference(expected, actual)
            report_image_difference(expected, actual, difference)
            dimensions_differ = (
                expected.width != actual.width or expected.height != actual.height
            )
            max_diff_columns = (
                "unbounded"
                if arguments.max_diff_columns is None
                else str(arguments.max_diff_columns)
            )
            if dimensions_differ:
                print(
                    "compare result: dimension mismatch; result: fail; differing pixels: {}; "
                    "differing columns: {}; max-diff-px: {}; max-diff-columns: {}".format(
                        difference.pixel_count,
                        difference.column_count,
                        arguments.max_diff_px,
                        max_diff_columns,
                    )
                )
                return 1
            passed = difference.pixel_count <= arguments.max_diff_px and (
                arguments.max_diff_columns is None
                or difference.column_count <= arguments.max_diff_columns
            )
            print(
                "compare result: differing pixels: {}; differing columns: {}; "
                "max-diff-px: {}; max-diff-columns: {}; result: {}".format(
                    difference.pixel_count,
                    difference.column_count,
                    arguments.max_diff_px,
                    max_diff_columns,
                    "pass" if passed else "fail",
                )
            )
            return 0 if passed else 1
        if arguments.command == "diff":
            expected = read_png(arguments.expected)
            actual = read_png(arguments.actual)
            difference_count = compare_images(expected, actual)
            write_png(arguments.output, difference_image(expected, actual))
            return 0 if difference_count == 0 else 1
        if arguments.command == "gif":
            if arguments.stride < 1:
                raise PngError("--stride must be at least 1")
            if arguments.delay < 1:
                raise PngError("--delay must be at least 1")
            selected = arguments.inputs[::arguments.stride]
            frames = [scale_image(read_png(path), arguments.scale) for path in selected]
            written = write_gif(arguments.output, frames, arguments.delay)
            print("pngtool: wrote {} frames, {} bytes".format(len(frames), written))
            return 0
        return selftest()
    except (OSError, PngError, struct.error, zlib.error) as error:
        print("pngtool: error: {}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
