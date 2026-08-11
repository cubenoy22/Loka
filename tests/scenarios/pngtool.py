#!/usr/bin/env python3
"""Crop, compare, and diff 8-bit RGB/RGBA PNGs using the standard library."""

import argparse
import os
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
            scanlines.extend(image.rgba[pixel_start : pixel_start + 3])
    with open(path, "wb") as handle:
        handle.write(_build_png(image.width, image.height, 2, bytes(scanlines)))


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


def compare_images(first, second, report=True):
    first_difference = None
    difference_count = 0
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

    if first_difference is None:
        return True
    if report:
        if first.width != second.width or first.height != second.height:
            print(
                "dimensions differ: {}x{} != {}x{}".format(
                    first.width, first.height, second.width, second.height
                )
            )
        x, y, first_pixel, second_pixel = first_difference
        print(
            "first differing pixel ({}, {}): {} != {}; differing pixels: {}".format(
                x, y, first_pixel, second_pixel, difference_count
            )
        )
    return False


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
            pixels.extend(((x * 31 + y * 7) & 0xFF, (x * 11 + y * 43) & 0xFF, (x * 5 + y * 19) & 0xFF, 255))
    source = Image(width, height, pixels)

    with tempfile.TemporaryDirectory(prefix="pngcrop-selftest-") as directory:
        filtered_path = os.path.join(directory, "filtered.png")
        cropped_path = os.path.join(directory, "cropped.png")
        roundtrip_path = os.path.join(directory, "roundtrip.png")
        _write_rgba_with_filters(filtered_path, source)
        decoded = read_png(filtered_path)
        if not compare_images(source, decoded, report=False):
            raise PngError("RGBA/filter decode selftest failed")

        expected_crop = crop_image(source, 1, 1, 6, 5)
        write_png(cropped_path, expected_crop)
        decoded_crop = read_png(cropped_path)
        write_png(roundtrip_path, decoded_crop)
        if not compare_images(expected_crop, decoded_crop, report=False):
            raise PngError("crop/write/read selftest failed")
        if not compare_images(read_png(cropped_path), read_png(roundtrip_path), report=False):
            raise PngError("compare round-trip selftest failed")

        changed = bytearray(decoded_crop.rgba)
        changed[0] ^= 1
        changed_image = Image(decoded_crop.width, decoded_crop.height, changed)
        if compare_images(decoded_crop, changed_image, report=False):
            raise PngError("compare mismatch selftest failed")
        difference = difference_image(decoded_crop, changed_image)
        if difference.rgba[:4] != bytes((255, 0, 255, 255)):
            raise PngError("difference image mismatch selftest failed")

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

    compare_parser = subparsers.add_parser("compare", help="compare decoded pixels")
    compare_parser.add_argument("first")
    compare_parser.add_argument("second")

    diff_parser = subparsers.add_parser("diff", help="write a visual exact-pixel diff")
    diff_parser.add_argument("expected")
    diff_parser.add_argument("actual")
    diff_parser.add_argument("output")

    subparsers.add_parser("selftest", help="exercise decode, crop, write, and compare")
    arguments = parser.parse_args(argv)

    try:
        if arguments.command == "crop":
            image = read_png(arguments.input)
            write_png(
                arguments.output,
                crop_image(image, arguments.left, arguments.top, arguments.right, arguments.bottom),
            )
            return 0
        if arguments.command == "compare":
            return 0 if compare_images(read_png(arguments.first), read_png(arguments.second)) else 1
        if arguments.command == "diff":
            expected = read_png(arguments.expected)
            actual = read_png(arguments.actual)
            equal = compare_images(expected, actual)
            write_png(arguments.output, difference_image(expected, actual))
            return 0 if equal else 1
        return selftest()
    except (OSError, PngError, struct.error, zlib.error) as error:
        print("pngtool: error: {}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
