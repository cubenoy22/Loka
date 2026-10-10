#!/usr/bin/env python3
"""Copy a MacBinary with SIZE(-1)'s preferred/minimum partition set in KiB."""

import argparse
from pathlib import Path
import struct


def size_resource(blob):
    """Return the absolute offset of the ten-byte SIZE(-1) payload."""
    def require(condition, message):
        if not condition:
            raise ValueError(message)

    def within(offset, length, begin, end):
        require(begin <= offset <= end and 0 <= length <= end - offset,
                "truncated or invalid MacBinary/resource fork")

    within(0, 128, 0, len(blob))
    require(blob[0] == 0 and 1 <= blob[1] <= 63 and blob[74] == blob[82] == 0,
            "invalid MacBinary header")
    data_length, resource_length = struct.unpack_from('>II', blob, 83)
    secondary_length = struct.unpack_from('>H', blob, 120)[0]
    data_start = 128 + ((secondary_length + 127) // 128) * 128
    within(data_start, data_length, 0, len(blob))
    fork = data_start + ((data_length + 127) // 128) * 128
    end = fork + resource_length
    within(fork, resource_length, 0, len(blob))
    within(fork, 16, fork, end)
    data_offset, map_offset, data_size, map_size = struct.unpack_from('>IIII', blob, fork)
    data = fork + data_offset
    rmap = fork + map_offset
    within(data, data_size, fork + 16, end)
    within(rmap, map_size, fork + 16, end)
    within(rmap, 28, rmap, rmap + map_size)
    types = rmap + struct.unpack_from('>H', blob, rmap + 24)[0]
    within(types, 2, rmap + 28, rmap + map_size)
    count = struct.unpack_from('>H', blob, types)[0]
    count = 0 if count == 0xFFFF else count + 1
    within(types + 2, count * 8, rmap, rmap + map_size)
    for index in range(count):
        entry = types + 2 + index * 8
        kind, last, refs_offset = struct.unpack_from('>4sHH', blob, entry)
        if kind != b'SIZE':
            continue
        refs = types + refs_offset
        within(refs, (last + 1) * 12, rmap, rmap + map_size)
        for number in range(last + 1):
            ref = refs + number * 12
            if struct.unpack_from('>h', blob, ref)[0] != -1:
                continue
            payload = data + int.from_bytes(blob[ref + 5:ref + 8], 'big')
            within(payload, 4, data, data + data_size)
            length = struct.unpack_from('>I', blob, payload)[0]
            within(payload + 4, length, data, data + data_size)
            require(length == 10, "SIZE(-1) must contain 10 bytes")
            return payload + 4
    raise ValueError("missing SIZE resource with id -1")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', nargs='?', type=Path)
    parser.add_argument('partition', nargs='?', type=int, help='positive partition size in KiB')
    parser.add_argument('--preferred-k', action='store_true',
                        help='read preferred KiB instead of patching')
    args = parser.parse_args()
    try:
        if args.preferred_k:
            if args.destination is not None or args.partition is not None:
                raise ValueError('--preferred-k accepts only the source')
            blob = args.source.read_bytes()
            offset = size_resource(blob)
            print(struct.unpack_from('>I', blob, offset + 2)[0] / 1024)
            return
        if args.destination is None or args.partition is None:
            raise ValueError('patching requires destination and partition')
        if not 1 <= args.partition <= 0x7FFFFFFF // 1024:
            raise ValueError('partition must be 1..2097151 KiB')
        if args.source.resolve() == args.destination.resolve() or (
                args.destination.exists() and args.source.samefile(args.destination)):
            raise ValueError('destination must be a copy, not the source')
        blob = bytearray(args.source.read_bytes())
        offset = size_resource(blob)
        size = args.partition * 1024
        struct.pack_into('>II', blob, offset + 2, size, size)
        args.destination.write_bytes(blob)
        print(f'SIZE(-1): preferred={size} minimum={size} bytes; {args.destination}')
    except (OSError, ValueError) as error:
        parser.exit(1, f'{parser.prog}: {error}\n')


if __name__ == '__main__':
    main()
