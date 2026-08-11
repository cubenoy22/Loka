#!/usr/bin/env python3
"""Characterization tests for the OS-neutral scenario runner tools."""

import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCENARIO_DIR = os.path.join(PROJECT_DIR, "tests", "scenarios")
SNAP_TOOL = os.path.join(SCENARIO_DIR, "snaprecord.py")
PNG_TOOL = os.path.join(SCENARIO_DIR, "pngtool.py")
PACKAGE_TOOL = os.path.join(SCENARIO_DIR, "stage-scrapbook-package.py")
ASSETS = os.path.join(PROJECT_DIR, "example", "ScrapbookUI", "ASSETS.LRP")


def run_tool(*arguments):
    return subprocess.run(
        [sys.executable] + list(arguments),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def png_chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_rgb_png(path, width, height, pixels):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixels[y * width + x])
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(rows)))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as handle:
        handle.write(payload)


def record(**overrides):
    values = {
        "format_version": "1",
        "schema_version": "1",
        "scenario_version": "1",
        "test": "ScrapbookUI",
        "step": "startup",
        "node": "MainNode",
        "tick": "1",
        "status": "ok",
        "caption": "line\\twith\\\\slash\\nnext",
    }
    values.update(overrides)
    return "".join("{}\t{}\n".format(key, values[key]) for key in sorted(values)) + "\n"


class SnapRecordToolTest(unittest.TestCase):
    def test_full_compare_is_order_independent_and_get_decodes_values(self):
        with tempfile.TemporaryDirectory(prefix="scenario-snap-") as directory:
            expected = os.path.join(directory, "expected.snap")
            actual = os.path.join(directory, "actual.snap")
            expected_text = record()
            actual_text = "".join(reversed(expected_text.splitlines(keepends=True)[:-1])) + "\n"
            with open(expected, "w", encoding="utf-8", newline="") as handle:
                handle.write(expected_text)
            with open(actual, "w", encoding="utf-8", newline="") as handle:
                handle.write(actual_text)

            compared = run_tool(SNAP_TOOL, "compare", expected, actual)
            self.assertEqual(compared.returncode, 0, compared.stderr)
            fetched = run_tool(SNAP_TOOL, "get", actual, "caption")
            self.assertEqual(fetched.returncode, 0, fetched.stderr)
            self.assertEqual(fetched.stdout, "line\twith\\slash\nnext\n")

    def test_compare_reports_changed_missing_and_extra_keys(self):
        with tempfile.TemporaryDirectory(prefix="scenario-snap-") as directory:
            expected = os.path.join(directory, "expected.snap")
            actual = os.path.join(directory, "actual.snap")
            with open(expected, "w", encoding="utf-8") as handle:
                handle.write(record(caption="expected"))
            with open(actual, "w", encoding="utf-8") as handle:
                handle.write(record(caption="actual", unexpected="value"))

            compared = run_tool(SNAP_TOOL, "compare", expected, actual)
            self.assertNotEqual(compared.returncode, 0)
            self.assertIn("caption", compared.stderr)
            self.assertIn("unexpected", compared.stderr)

    def test_duplicate_key_and_malformed_escape_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix="scenario-snap-") as directory:
            duplicate = os.path.join(directory, "duplicate.snap")
            malformed = os.path.join(directory, "malformed.snap")
            with open(duplicate, "w", encoding="utf-8") as handle:
                handle.write(record().rstrip("\n") + "\nstatus\tok\n\n")
            with open(malformed, "w", encoding="utf-8") as handle:
                handle.write(record(caption="bad\\qescape"))

            duplicate_result = run_tool(SNAP_TOOL, "get", duplicate, "status")
            malformed_result = run_tool(SNAP_TOOL, "get", malformed, "caption")
            self.assertNotEqual(duplicate_result.returncode, 0)
            self.assertIn("duplicate", duplicate_result.stderr)
            self.assertNotEqual(malformed_result.returncode, 0)
            self.assertIn("escape", malformed_result.stderr)


class ExpectedRecordPinsTest(unittest.TestCase):
    def test_shared_pins_cover_registered_scenarios_and_use_app_identity(self):
        scenarios = (
            "startup",
            "open-first-page",
            "open-first-page-refused",
            "flip-forward-back",
            "refused-flip-keeps-page",
            "open-text-page",
            "open-text-page-refused",
        )
        expected_dir = os.path.join(SCENARIO_DIR, "expected", "scrapbook")
        for scenario in scenarios:
            path = os.path.join(expected_dir, scenario + ".snap")
            identity = run_tool(SNAP_TOOL, "get", path, "test")
            step = run_tool(SNAP_TOOL, "get", path, "step")
            self.assertEqual(identity.returncode, 0, identity.stderr)
            self.assertEqual(identity.stdout, "ScrapbookUI\n")
            self.assertEqual(step.returncode, 0, step.stderr)
            self.assertEqual(step.stdout, scenario + "\n")


class PngToolTest(unittest.TestCase):
    def test_exact_compare_and_diff_pin_one_pixel_change(self):
        with tempfile.TemporaryDirectory(prefix="scenario-png-") as directory:
            expected = os.path.join(directory, "expected.png")
            actual = os.path.join(directory, "actual.png")
            difference = os.path.join(directory, "diff.png")
            write_rgb_png(expected, 2, 2, [(1, 2, 3)] * 4)
            write_rgb_png(actual, 2, 2, [(1, 2, 3), (9, 8, 7), (1, 2, 3), (1, 2, 3)])

            same = run_tool(PNG_TOOL, "compare", expected, expected)
            changed = run_tool(PNG_TOOL, "compare", expected, actual)
            diffed = run_tool(PNG_TOOL, "diff", expected, actual, difference)
            self.assertEqual(same.returncode, 0, same.stderr)
            self.assertNotEqual(changed.returncode, 0)
            self.assertIn("differing pixels: 1", changed.stdout)
            self.assertEqual(diffed.returncode, 1)
            self.assertTrue(os.path.isfile(difference))
            self.assertGreater(os.path.getsize(difference), 0)


class PackageStagingToolTest(unittest.TestCase):
    def test_stage_copy_and_validated_corruption_leave_source_unchanged(self):
        with tempfile.TemporaryDirectory(prefix="scenario-package-") as directory:
            plain = os.path.join(directory, "plain.LRP")
            corrupt = os.path.join(directory, "corrupt.LRP")
            with open(ASSETS, "rb") as handle:
                original = handle.read()

            copied = run_tool(PACKAGE_TOOL, ASSETS, plain)
            changed = run_tool(PACKAGE_TOOL, ASSETS, corrupt, "--corrupt-bag", "1")
            self.assertEqual(copied.returncode, 0, copied.stderr)
            self.assertEqual(changed.returncode, 0, changed.stderr)
            with open(plain, "rb") as handle:
                self.assertEqual(handle.read(), original)
            with open(corrupt, "rb") as handle:
                self.assertNotEqual(handle.read(), original)
            with open(ASSETS, "rb") as handle:
                self.assertEqual(handle.read(), original)

    def test_invalid_package_is_refused_even_without_corruption(self):
        with tempfile.TemporaryDirectory(prefix="scenario-package-") as directory:
            source = os.path.join(directory, "invalid.LRP")
            destination = os.path.join(directory, "staged.LRP")
            with open(source, "wb") as handle:
                handle.write(b"not an LRPK")
            result = run_tool(PACKAGE_TOOL, source, destination)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("not an LRPK", result.stderr)
            self.assertFalse(os.path.exists(destination))


if __name__ == "__main__":
    unittest.main()
