#!/usr/bin/env python3
"""Locate release CODE in a saved heap, then map an A6 chain and raw stack scan.

Addresses are hexadecimal. Requires readelf and c++filt on PATH. Raw matches
are return-address candidates, not proof that a word is a return address.
"""

import argparse
import bisect
from pathlib import Path
import re
import struct
import subprocess
import unittest


def readelf(elf, option):
    return subprocess.check_output(['readelf', option, str(elf)], text=True)


def locate_code(elf, heap, zone):
    raw = elf.read_bytes()
    sections = {}
    for line in readelf(elf, '-SW').splitlines():
        match = re.match(r'\s*\[\s*(\d+)\]\s+(\.code\S*)\s+PROGBITS\s+'
                         r'([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)', line)
        if match:
            index, name, va, offset, size = match.groups()
            sections[name] = (index, int(va, 16), int(offset, 16), int(size, 16))
    relocations = {name: set() for name in sections}
    current = None
    for line in readelf(elf, '-rW').splitlines():
        if line.startswith('Relocation section'):
            current = re.sub(r'^\.rela?', '', line.split("'")[1])
        elif current in sections:
            fields = line.split()
            if fields and re.fullmatch(r'[0-9a-fA-F]{8,16}', fields[0]):
                # Retro68 relocation offsets are ELF virtual addresses. Four
                # bytes conservatively covers the 68K relocation widths.
                offset = int(fields[0], 16) - sections[current][1]
                relocations[current].update(range(offset, offset + 4))
    located = []
    for name, (index, va, offset, size) in sections.items():
        body = raw[offset:offset + size]
        start_address = None
        for start in range(0, len(body) - 31, 2):
            if any(byte in relocations[name] for byte in range(start, start + 32)):
                continue
            window = body[start:start + 32]
            if window.count(window[:2]) > 8:
                continue
            hit = heap.find(window)
            if hit < 0 or heap.find(window, hit + 1) >= 0:
                continue
            base = hit - start
            if base < 0 or base + size > len(heap):
                continue
            start_address = zone + base
            located.append((name, index, va, size, start_address, body))
            break
        address = f'{start_address:08x}' if start_address is not None else 'NOT FOUND'
        print(f'{name}: vaddr={va:08x} size={size:x} runtime={address}')
    if not located:
        raise ValueError('no CODE section could be located in the saved heap')
    return located


def symbolizer(elf, located):
    symbols = {}
    for line in readelf(elf, '-sW').splitlines():
        fields = line.split()
        if len(fields) >= 8 and fields[3] == 'FUNC' and fields[6].isdigit():
            symbols.setdefault(fields[6], []).append(
                (int(fields[1], 16), int(fields[2]), fields[7]))
    names = sorted({row[2] for rows in symbols.values() for row in rows})
    demangled = subprocess.run(['c++filt'], input='\n'.join(names) + '\n',
                               text=True, capture_output=True, check=True).stdout.splitlines()
    names = dict(zip(names, demangled))
    lookup = {}
    for index, rows in symbols.items():
        rows.sort()
        lookup[index] = ([row[0] for row in rows], rows)

    def describe(address):
        for name, index, va, size, base, _ in located:
            if base <= address < base + size:
                value = va + address - base
                starts, rows = lookup.get(index, ([], []))
                pos = bisect.bisect_right(starts, value) - 1
                symbol = '?'
                if pos >= 0:
                    begin, length, mangled = rows[pos]
                    if value < begin + max(length, 1):
                        symbol = f'{names[mangled]}+0x{value - begin:x}'
                return f'{name}+0x{address - base:x} {symbol}'
        return None
    return describe


def classify_return(code, offset):
    """Classify an ELF section offset by the call encodings ending there.

    This suffix check does not establish instruction boundaries or prove that
    a saved value belongs to a live call. The raw scanner rejects odd addresses.
    """
    if not 0 <= offset <= len(code):
        return 'non-call'
    if offset >= 2:
        word = struct.unpack_from('>H', code, offset - 2)[0]
        if (word & 0xF000 == 0xA000 or 0x4E90 <= word <= 0x4E97
                or word & 0xFF00 == 0x6100 and word & 0xFF not in (0, 0xFF)):
            return 'call'
    if offset >= 4:
        word = struct.unpack_from('>H', code, offset - 4)[0]
        if 0x4EA8 <= word <= 0x4EAF or word in (0x4EB8, 0x6100):
            return 'call'
    if offset >= 6:
        word = struct.unpack_from('>H', code, offset - 6)[0]
        if word in (0x4EB9, 0x61FF):
            return 'call'
    return 'non-call'


class CallClassifierTests(unittest.TestCase):
    def test_calls(self):
        patterns = {
            'trap': ['a000', 'afff'],
            'jsr (An)': [f'{word:04x}' for word in range(0x4E90, 0x4E98)],
            'jsr abs.l': ['4eb9 12345678'],
            'jsr d16(An)': [f'{word:04x} 1234' for word in range(0x4EA8, 0x4EB0)],
            'jsr abs.w': ['4eb8 1234'],
            'bsr.w': ['6100 1234'],
            'bsr.s': [f'61{byte:02x}' for byte in range(1, 0xFF)],
            'bsr.l': ['61ff 12345678'],
        }
        for name, encodings in patterns.items():
            for encoding in encodings:
                with self.subTest(name=name, encoding=encoding):
                    code = bytes.fromhex(encoding)
                    self.assertEqual(classify_return(code, len(code)), 'call')
                    # The address can lie inside a section, not just at its end.
                    self.assertEqual(classify_return(b'\x4e\x71' + code + b'\x4e\x71',
                                                     len(code) + 2), 'call')

    def test_non_calls_and_bounds(self):
        for encoding in ('', '4e', '4e71', '4e75', '4e98', '4ea7 1234',
                         '4eb0 1234', '6002', '6100', '61ff', '61ff 1234',
                         '4eb8', '4eb9 1234'):
            with self.subTest(encoding=encoding):
                code = bytes.fromhex(encoding)
                self.assertEqual(classify_return(code, len(code)), 'non-call')
        code = bytes.fromhex('a000')
        for offset in (-1, 0, 1, 3):
            with self.subTest(offset=offset):
                self.assertEqual(classify_return(code, offset), 'non-call')


def map_stack(stack, a7, a6, describe, located, show_all=False):
    print('--- A6 chain')
    if a6 is None:
        print('not requested (supply a6 to walk frames)')
    else:
        frame = a6
        depth = 0
        while a7 <= frame and frame + 8 <= a7 + len(stack) and frame % 2 == 0:
            next_frame, ret = struct.unpack_from('>II', stack, frame - a7)
            print(f'#{depth:03d} fp={frame:08x} ret={ret:08x} {describe(ret) or "unmapped"}')
            depth += 1
            if next_frame <= frame:
                print(f'stop: non-increasing next frame {next_frame:08x}')
                break
            frame = next_frame
        else:
            print(f'stop: frame {frame:08x} outside saved stack or unaligned')
        print(f'frames: {depth}')
    print(f'--- raw scan from a7={a7:08x} (candidates only)')
    for offset in range(0, len(stack) - 3, 2):
        address = struct.unpack_from('>I', stack, offset)[0]
        if address % 2:
            continue
        for _, _, _, size, base, code in located:
            if base <= address < base + size:
                classification = classify_return(code, address - base)
                if show_all or classification == 'call':
                    print(f'a7+{offset:04x}: {address:08x} {classification} {describe(address)}')
                break


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', nargs='?', type=Path, help='matching release *.code.bin.gdb')
    parser.add_argument('heap', nargs='?', type=Path)
    parser.add_argument('zone', nargs='?', type=lambda value: int(value, 16))
    parser.add_argument('stack', nargs='?', type=Path)
    parser.add_argument('a7', nargs='?', type=lambda value: int(value, 16))
    parser.add_argument('a6', nargs='?', type=lambda value: int(value, 16))
    parser.add_argument('--all', action='store_true', help='show non-call even CODE candidates too')
    parser.add_argument('--self-test', action='store_true', help='test call encodings without input files')
    args = parser.parse_args()
    if args.self_test:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(CallClassifierTests)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        parser.exit(0 if result.wasSuccessful() else 1)
    if any(getattr(args, name) is None for name in ('elf', 'heap', 'zone', 'stack', 'a7')):
        parser.error('elf, heap, zone, stack and a7 are required unless using --self-test')
    try:
        located = locate_code(args.elf, args.heap.read_bytes(), args.zone)
        map_stack(args.stack.read_bytes(), args.a7, args.a6,
                  symbolizer(args.elf, located), located, args.all)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{parser.prog}: {error}\n')


if __name__ == '__main__':
    main()
