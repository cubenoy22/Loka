#!/usr/bin/env python3
"""Refresh and read-only headroom contracts using actual MacBinary fixtures."""

import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

from Retro68SizeReportTest import PROJECT_DIR, baseline_for, make_macbinary

sys.path.insert(0, str(PROJECT_DIR / "tools" / "ci"))
import retro68_size_bank_refresh as refresh


class SizeBankRefreshTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        (self.root / "CMakeCache.txt").write_text(
            "CMAKE_BUILD_TYPE:STRING=Release\nRETRO68_CPU:STRING=m68k\n"
            "LOKA_TOOLBOX_MULTIVERSAL_INTERFACES:BOOL=ON\n")
        self.artifact = self.root / "example" / "Fixture68K.bin"
        self.artifact.parent.mkdir()
        self.artifact.write_bytes(make_macbinary([
            ("CODE", b"code"), ("DATA", b"data"), ("RELA", b"rela")
        ]))
        self.baseline = baseline_for("example/Fixture68K.bin", self.artifact.stat().st_size - 128,
                                     3, 5, 4, allowance=256)
        self.baseline["identity"] = {"source_commit": "a" * 40, "preset": "retro68-68k-release"}
        self.path = self.root / "baseline.json"
        self.path.write_text(json.dumps(self.baseline))
        self.original = self.path.read_bytes()

    def run_tool(self, *args):
        with contextlib.redirect_stdout(io.StringIO()) as output, contextlib.redirect_stderr(io.StringIO()):
            status = refresh.main([str(self.root), "--baseline", str(self.path), *args])
        return status, output.getvalue()

    def test_headroom_is_strict_and_never_writes(self):
        for threshold, expected in [(128, 0), (129, 2), (0, 0), (-1, 1)]:
            status, output = self.run_tool("--check-headroom", str(threshold))
            self.assertEqual(status, expected)
            self.assertEqual(self.path.read_bytes(), self.original)
            if expected == 2:
                self.assertIn("| Fixture68K | 128 |", output)

    def test_measurement_error_is_not_low_headroom(self):
        self.artifact.unlink()
        self.assertEqual(self.run_tool("--check-headroom", "1024")[0], 1)
        self.artifact.write_bytes(b"truncated")
        self.assertEqual(self.run_tool("--check-headroom", "1024")[0], 1)
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_refresh_identity_and_all_four_measurements(self):
        body = self.root / "body.md"
        with mock.patch.object(refresh, "git", side_effect=["b" * 40,
                "One (#12)\nDuplicate (#12)\nIgnore #99\nTwo (#3)\nNot suffix (#44) trailing"]):
            status, output = self.run_tool("--body", str(body))
        self.assertEqual(status, 0)
        updated = json.loads(self.path.read_text())
        self.assertEqual(updated["artifacts"][0]["baseline"],
                         dict(total=self.artifact.stat().st_size, CODE=4, DATA=4, RELA=4))
        self.assertEqual(updated["identity"]["source_commit"], "b" * 40)
        reference = updated["identity"]["component_reference"]
        self.assertIn("Multiversal Interfaces build", reference)
        self.assertIn("accumulated #3, #12 since", reference)
        self.assertNotIn("#99", reference)
        self.assertEqual(updated["schema_version"], 1)
        self.assertEqual(updated["material_growth_bytes"], 256)
        self.assertIn("| Fixture68K |", body.read_text())
        self.assertIn("+128 |", body.read_text())
        self.assertIn("Allow GitHub Actions to create", body.read_text())

    def test_failed_candidate_preserves_bank(self):
        extra = self.artifact.with_name("Unbanked68K.bin")
        extra.write_bytes(self.artifact.read_bytes())
        with mock.patch.object(refresh, "git", side_effect=["b" * 40, ""]):
            self.assertEqual(self.run_tool()[0], 1)
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_refresh_reads_universal_provenance_and_empty_history(self):
        cache = self.root / "CMakeCache.txt"
        cache.write_text(cache.read_text().replace("=ON", "=OFF"))
        with mock.patch.object(refresh, "git", side_effect=["b" * 40, ""]):
            self.assertEqual(self.run_tool()[0], 0)
        reference = json.loads(self.path.read_text())["identity"]["component_reference"]
        self.assertIn("local Universal Interfaces", reference)
        self.assertIn("no merged PR suffixes", reference)

    def test_invalid_cache_or_missing_history_preserves_bank(self):
        cache = self.root / "CMakeCache.txt"
        original_cache = cache.read_text()
        for invalid in [original_cache.replace("Release", "Debug"),
                        original_cache.replace("=ON", "=UNKNOWN")]:
            cache.write_text(invalid)
            self.assertEqual(self.run_tool()[0], 1)
            self.assertEqual(self.path.read_bytes(), self.original)
        cache.write_text(original_cache)
        with mock.patch.object(refresh, "git", side_effect=refresh.subprocess.CalledProcessError(128, "git")):
            self.assertEqual(self.run_tool()[0], 1)
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_invalid_arguments_do_not_signal_headroom(self):
        self.assertEqual(self.run_tool("--check-headroom", "oops")[0], 1)
        self.assertEqual(self.run_tool("--check-headroom", "0", "--body", str(self.path))[0], 1)
        self.assertEqual(self.run_tool("--help")[0], 0)
        self.assertEqual(self.path.read_bytes(), self.original)

    def test_git_reads_repository_head(self):
        self.assertRegex(refresh.git("rev-parse", "HEAD"), r"^[0-9a-f]{40}$")

    def test_zero_verification_rejects_component_only_difference(self):
        baseline = self.baseline
        baseline["artifacts"][0]["baseline"]["total"] = self.artifact.stat().st_size
        with contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(refresh.size_report.SizeReportError):
                refresh.verify_zero(self.root, baseline)


if __name__ == "__main__":
    unittest.main()
