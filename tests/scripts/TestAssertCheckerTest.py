#!/usr/bin/env python3
"""Exercise the assert checker through its CLI on isolated C++ source corpora."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


CHECKER = Path(__file__).resolve().parents[2] / "tools/ci/check_test_asserts.py"


class TestAssertCheckerTest(unittest.TestCase):
    def run_checker(self, sources):
        with tempfile.TemporaryDirectory(prefix="loka-assert-check-") as directory:
            root = Path(directory)
            script = root / "tools/ci/check_test_asserts.py"
            script.parent.mkdir(parents=True)
            shutil.copyfile(CHECKER, script)
            for name, text in sources.items():
                path = root / "tests" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text, encoding="utf-8")
            return subprocess.run(
                [sys.executable, str(script)], text=True, capture_output=True,
                check=False,
            )

    def test_verified_query_does_not_taint_other_files(self):
        result = self.run_checker({
            "Flow.cpp": "LOKA_VERIFY(tracker.phase() == TRACKER_IDLE);\n",
            "Commit.cpp": "assert(tracker.phase() == TRACKER_IDLE);\n",
            "Allocation.hpp": "assert(write.tracker->phase() == expected);\n",
        })
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_local_violation_is_detected_without_reporting_foreign_query(self):
        result = self.run_checker({
            "Driver.mm": "LOKA_VERIFY(driver.advance());\nassert(driver.advance());\n",
            "Query.cpp": "assert(iterator.advance());\n",
        })
        self.assertEqual(result.returncode, 1)
        self.assertIn("Driver.mm:2: advance", result.stdout)
        self.assertNotIn("Query.cpp", result.stdout)
        self.assertIn("1 finding(s)", result.stdout)

    def test_same_file_operation_is_detected_before_its_verify(self):
        result = self.run_checker({"Flush.cpp":
            "assert(scene.flushInvalidation());\n"
            "LOKA_VERIFY(scene.flushInvalidation());\n"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("Flush.cpp:1: flushInvalidation", result.stdout)
        self.assertIn("1 finding(s)", result.stdout)

    def test_accessor_exclusion_and_reasoned_escape_remain_available(self):
        result = self.run_checker({"Queries.cpp":
            "LOKA_VERIFY(capture.get(output));\n"
            "assert(state.get() == 3);\n"
            "LOKA_VERIFY(driver.advance());\n"
            "assert(query.advance()); // loka-assert-ok: fixture query is pure\n"})
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_find_output_operation_is_detected_but_lookup_is_not(self):
        result = self.run_checker({"Find.cpp":
            "LOKA_VERIFY(registry.find(&key, output));\n"
            "assert(registry.find(&key, output));\n"
            "assert(registry.find(&key) != 0);\n"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("Find.cpp:2: find", result.stdout)
        self.assertIn("1 finding(s)", result.stdout)

    def test_missing_corpus_is_refused(self):
        result = self.run_checker({"Empty.cpp": "assert(value == 3);\n"})
        self.assertEqual(result.returncode, 1)
        self.assertIn("no LOKA_VERIFY corpus found", result.stderr)


if __name__ == "__main__":
    unittest.main()
