#!/usr/bin/env python3
"""Stage a Scrapbook LRPK package, optionally corrupting one validated bag."""

import argparse
import os
import struct
import sys


class PackageError(ValueError):
    pass


def corruption_offset(package, target_bag):
    if len(package) < 512 or package[:4] != b"LRPK" or package[8:12] != b"HEAD":
        raise PackageError("not an LRPK package with a fixed 512-byte HEAD")
    if struct.unpack_from(">I", package, 4)[0] + 8 != len(package):
        raise PackageError("LRPK form length does not match the staged file")

    index = None
    data_start = None
    data_size = None
    cursor = 512
    while cursor < len(package):
        if cursor + 8 > len(package):
            raise PackageError("truncated LRPK chunk header")
        tag = bytes(package[cursor : cursor + 4])
        payload_size = struct.unpack_from(">I", package, cursor + 4)[0]
        payload_start = cursor + 8
        padded_size = (payload_size + 3) & ~3
        if payload_start + padded_size > len(package):
            raise PackageError("truncated LRPK chunk payload")
        if tag == b"INDX":
            index = bytes(package[payload_start : payload_start + payload_size])
        elif tag == b"DATA":
            data_start = payload_start
            data_size = payload_size
        cursor = payload_start + padded_size

    if index is None or data_start is None or data_size is None:
        raise PackageError("LRPK is missing INDX or DATA")
    if len(index) < 8:
        raise PackageError("LRPK INDX is too short")
    bag_count, asset_count = struct.unpack_from(">II", index, 0)
    if len(index) != 8 + bag_count * 20 + asset_count * 16:
        raise PackageError("LRPK INDX row counts do not match its size")
    if target_bag < 0 or target_bag >= bag_count:
        raise PackageError("requested bag is outside the LRPK bag table")

    row = 8 + target_bag * 20
    data_offset, stored_size = struct.unpack_from(">II", index, row)
    if data_offset % 4 != 0 or stored_size == 0 or stored_size > data_size - data_offset:
        raise PackageError("target bag has invalid stored payload bounds")
    payload_start = data_start + data_offset
    payload_end = payload_start + stored_size
    flip_offset = payload_start + stored_size // 2
    if not (data_start <= payload_start <= flip_offset < payload_end <= data_start + data_size <= len(package)):
        raise PackageError("computed corruption byte is outside the target bag payload")
    return flip_offset, payload_start, payload_end


def stage(source, destination, target_bag):
    if os.path.abspath(source) == os.path.abspath(destination):
        raise PackageError("source and destination must differ")
    with open(source, "rb") as handle:
        package = bytearray(handle.read())
    validation_bag = 0 if target_bag is None else target_bag
    flip_offset, payload_start, payload_end = corruption_offset(package, validation_bag)
    if target_bag is not None:
        package[flip_offset] ^= 0x01
    with open(destination, "wb") as handle:
        handle.write(package)
    if target_bag is not None:
        print(
            "bag {} payload [{}, {}), flipped offset {}".format(
                target_bag, payload_start, payload_end, flip_offset
            )
        )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("destination")
    parser.add_argument("--corrupt-bag", type=int)
    arguments = parser.parse_args(argv)
    try:
        stage(arguments.source, arguments.destination, arguments.corrupt_bag)
        return 0
    except (OSError, PackageError, struct.error) as error:
        print("stage-scrapbook-package: error: {}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
