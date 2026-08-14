#!/usr/bin/env python3
"""Tests for failure-atomic presentation directory replacement."""

import os
import subprocess
import tempfile
import unittest


PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
STAGE_HELPER = os.path.join(PROJECT_DIR, "scripts", "presentation-stage.sh")


def run_stage(script, stage):
    return subprocess.run(
        ["bash", "-c", script, "presentation-stage-test", stage, STAGE_HELPER],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


class PresentationStageTest(unittest.TestCase):
    def test_completed_directory_replaces_old_stage(self):
        with tempfile.TemporaryDirectory(prefix="presentation-stage-") as directory:
            stage = os.path.join(directory, "stage with spaces")
            os.mkdir(stage)
            with open(os.path.join(stage, "old"), "w", encoding="utf-8") as handle:
                handle.write("old")

            result = run_stage(
                r'''
set -euo pipefail
stage="$1"
. "$2"
populate() {
  printf 'new' >"$1/new"
}
loka_replace_stage_directory "$stage" populate
''',
                stage,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertFalse(os.path.exists(os.path.join(stage, "old")))
            with open(os.path.join(stage, "new"), encoding="utf-8") as handle:
                self.assertEqual(handle.read(), "new")
            self.assertEqual(os.listdir(directory), ["stage with spaces"])

    def test_populate_failure_preserves_old_stage(self):
        with tempfile.TemporaryDirectory(prefix="presentation-stage-") as directory:
            stage = os.path.join(directory, "stage")
            os.mkdir(stage)
            with open(os.path.join(stage, "old"), "w", encoding="utf-8") as handle:
                handle.write("old")

            result = run_stage(
                r'''
set -euo pipefail
stage="$1"
. "$2"
populate() {
  printf 'incomplete' >"$1/new"
  return 7
}
loka_replace_stage_directory "$stage" populate
''',
                stage,
            )

            self.assertEqual(result.returncode, 7, result.stderr)
            self.assertFalse(os.path.exists(os.path.join(stage, "new")))
            with open(os.path.join(stage, "old"), encoding="utf-8") as handle:
                self.assertEqual(handle.read(), "old")
            self.assertEqual(os.listdir(directory), ["stage"])

    def test_unsafe_root_path_is_refused_before_populate(self):
        result = run_stage(
            r'''
set -euo pipefail
. "$2"
populate() {
  return 99
}
loka_replace_stage_directory / populate
''',
            "/",
        )

        self.assertEqual(result.returncode, 2)
        self.assertIn("Refusing unsafe presentation stage path", result.stderr)


if __name__ == "__main__":
    unittest.main()
