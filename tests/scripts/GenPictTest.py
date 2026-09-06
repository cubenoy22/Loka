#!/usr/bin/env python3
"""PICT fixture shape, determinism and output confinement pins."""
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / 'tools/scenario/gen_pict.py'


class GenPictTest(unittest.TestCase):
    def test_sizes_headers_and_determinism(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            for size in (12288, 51200):
                paths = [Path(directory) / name for name in ('a.pict', 'b.pict')]
                for path in paths:
                    subprocess.run(['python3', str(GEN), '--bytes', str(size), str(path)], check=True)
                data = paths[0].read_bytes()
                self.assertEqual(data, paths[1].read_bytes())
                self.assertLessEqual(abs(len(data) - size), size * .02)
                self.assertEqual(data[:512], bytes(512))
                self.assertEqual(struct.unpack_from('>H', data, 512)[0], len(data) - 512)
                top, left, height, width = struct.unpack_from('>hhhh', data, 514)
                self.assertEqual((top, left, width), (0, 0, 512))
                self.assertGreater(height, 0)
                self.assertEqual(data[522:526], b'\0\x11\x02\xff')
                self.assertEqual(struct.unpack_from('>H', data, 554)[0], 64)
                self.assertEqual(data[526:532], b'\x0c\0\xff\xff\xff\xff')
                self.assertEqual(data[552:554], b'\0\x90')
                for offset in (556, 564, 572):
                    self.assertEqual(data[offset:offset+8], data[514:522])
                self.assertEqual(data[580:582], b'\0\0')
                self.assertEqual(len(data), 584 + 64 * height)
                self.assertEqual(data[-2:], b'\0\xff')

    def test_refuses_symlink_escape(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'build') as directory:
            link = Path(directory) / 'escape'
            link.symlink_to(ROOT, target_is_directory=True)
            result = subprocess.run(['python3', str(GEN), '--bytes', '12288',
                                     str(link / 'README.md')], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'beneath', result.stderr)

    def test_refuses_tracked_and_escaped_paths(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        for path in (ROOT / 'README.md', ROOT / 'build/../README.md'):
            before = path.read_bytes()
            result = subprocess.run(['python3', str(GEN), '--bytes', '12288', str(path)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'beneath', result.stderr)
            self.assertEqual(path.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
