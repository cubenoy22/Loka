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


FLOW_SCRIPT = os.path.join(PROJECT_DIR, "scripts", "macos-standalone-flow.sh")

STUB_LIPO = """#!/usr/bin/env bash
# Behaves like the Xcode 9 cctools lipo: -archs is an unknown flag, so any
# regression back to `lipo -archs` fails every test in this class.
if [ "$1" = "-archs" ]; then
  echo "fatal error: lipo: unknown flag: -archs" >&2
  exit 1
fi
if [ "$1" != "-info" ] || [ -z "${2:-}" ]; then
  echo "stub lipo: unexpected arguments: $*" >&2
  exit 1
fi
printf '%s\\n' "$LOKA_TEST_LIPO_INFO_OUTPUT"
"""


class BinaryArchCheckTest(unittest.TestCase):
    """Pins the lipo -info parse behind loka_binary_contains_arch."""

    def run_check(self, info_output, arch):
        with tempfile.TemporaryDirectory(prefix="lipo-arch-check-") as directory:
            stub = os.path.join(directory, "lipo")
            with open(stub, "w", encoding="utf-8") as handle:
                handle.write(STUB_LIPO)
            os.chmod(stub, 0o755)
            environment = os.environ.copy()
            environment["LOKA_LIPO_BIN"] = stub
            environment["LOKA_TEST_LIPO_INFO_OUTPUT"] = info_output
            return subprocess.run(
                [
                    "bash",
                    "-c",
                    'set -euo pipefail; . "$1"; loka_binary_contains_arch ignored.bin "$2"',
                    "binary-arch-check-test",
                    STAGE_HELPER,
                    arch,
                ],
                env=environment,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

    def test_thin_wording_matches_its_architecture(self):
        thin = "Non-fat file: build/app.bin is architecture: x86_64"
        self.assertEqual(self.run_check(thin, "x86_64").returncode, 0)

    def test_thin_wording_refuses_a_different_architecture(self):
        thin = "Non-fat file: build/app.bin is architecture: x86_64"
        self.assertNotEqual(self.run_check(thin, "arm64").returncode, 0)

    def test_fat_wording_matches_every_listed_architecture(self):
        fat = "Architectures in the fat file: build/app.bin are: i386 x86_64"
        self.assertEqual(self.run_check(fat, "i386").returncode, 0)
        self.assertEqual(self.run_check(fat, "x86_64").returncode, 0)
        self.assertNotEqual(self.run_check(fat, "ppc").returncode, 0)

    def test_binary_path_containing_colon_space_still_parses(self):
        thin = "Non-fat file: stage: 3/app.bin is architecture: arm64"
        self.assertEqual(self.run_check(thin, "arm64").returncode, 0)
        self.assertNotEqual(self.run_check(thin, "3/app.bin").returncode, 0)

    def test_flow_script_routes_the_arch_check_through_the_shared_door(self):
        with open(FLOW_SCRIPT, encoding="utf-8") as handle:
            flow = handle.read()
        self.assertIn("loka_binary_contains_arch", flow)
        self.assertNotIn("-archs", flow)



if __name__ == "__main__":
    unittest.main()
