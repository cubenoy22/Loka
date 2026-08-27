#!/usr/bin/env python3
"""End-to-end audit checks for the packaged macOS standalone verifier."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_DIR = Path(__file__).resolve().parents[2]
CATALOG = (
    (
        "scrapbook",
        "LokaScrapbookStandaloneFlowMacOS",
        "tests/scenarios/expected/scrapbook/standalone-tour.audit",
    ),
    (
        "helloworld",
        "LokaHelloWorldStandaloneFlowMacOS",
        "tests/scenarios/expected/helloworld/toggle-action-probe.audit",
    ),
    (
        "tutorial",
        "LokaTutorialStandaloneFlowMacOS",
        "tests/scenarios/expected/tutorial/increment-summary-toggle.audit",
    ),
    (
        "minesweeper",
        "LokaMineSweeperStandaloneFlowMacOS",
        "tests/scenarios/expected/minesweeper/new-game-twice.audit",
    ),
    (
        "floppybird",
        "LokaFloppyBirdStandaloneFlowMacOS",
        "tests/scenarios/expected/floppybird/fixed-step-flaps.audit",
    ),
)


class MacOSStandaloneAuditVerifierTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="macos-standalone-audit-"
        )
        self.stage = Path(self.temporary_directory.name)
        shutil.copy2(
            PROJECT_DIR / "scripts/macos-standalone-flow.sh",
            self.stage / "Verify-StandaloneFlow.sh",
        )
        shutil.copy2(
            PROJECT_DIR / "scripts/presentation-stage.sh",
            self.stage / "presentation-stage.sh",
        )
        expected_directory = self.stage / "expected"
        expected_directory.mkdir()
        catalog_lines = [
            "# SimpleViewer is excluded from automation: interactive file chooser."
        ]
        for key, target, expected_path in CATALOG:
            expected = PROJECT_DIR / expected_path
            shutil.copy2(expected, expected_directory / (key + ".audit"))
            catalog_lines.append("\t".join((key, target, expected_path)))
            binary = self.stage / (target + ".app") / "Contents/MacOS" / target
            binary.parent.mkdir(parents=True)
            binary.write_text(
                """#!/usr/bin/env bash
set -euo pipefail
cp "$LOKA_STANDALONE_AUDIT_FIXTURE_DIR/%s.audit" LOG.TXT
while :; do sleep 1; done
"""
                % key,
                encoding="utf-8",
            )
            binary.chmod(0o755)
        (self.stage / "standalone-flow-catalog.tsv").write_text(
            "\n".join(catalog_lines) + "\n", encoding="utf-8"
        )

        self.audit_fixtures = self.stage / "fixtures"
        shutil.copytree(expected_directory, self.audit_fixtures)

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_verifier(self):
        environment = os.environ.copy()
        environment["LOKA_STANDALONE_AUDIT_FIXTURE_DIR"] = str(
            self.audit_fixtures
        )
        return subprocess.run(
            ["bash", str(self.stage / "Verify-StandaloneFlow.sh"), "Verify"],
            cwd=self.stage,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=10,
            check=False,
        )

    def test_accepts_the_tracked_audit_byte_for_byte(self):
        result = self.run_verifier()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(
            result.stdout.count("Runtime-verified macOS Standalone Flow:"), 5
        )
        for key, _target, _expected in CATALOG:
            self.assertTrue((self.stage / "actual" / (key + ".audit")).is_file())

    def test_refuses_bogus_verdict_body_with_valid_structural_lines(self):
        tutorial = self.audit_fixtures / "tutorial.audit"
        lines = tutorial.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            if line.startswith("text.value\t"):
                lines[index] = "text.value\tbogus"
                break
        else:
            self.fail("tutorial fixture has no text.value verdict field")
        tutorial.write_text("\n".join(lines) + "\n", encoding="utf-8")

        result = self.run_verifier()

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not match", result.stderr)
        self.assertTrue((self.stage / "actual/scrapbook.audit").is_file())
        self.assertTrue((self.stage / "actual/helloworld.audit").is_file())
        self.assertTrue((self.stage / "actual/tutorial.audit").is_file())
        self.assertFalse((self.stage / "actual/minesweeper.audit").exists())

    def test_refuses_a_catalog_that_silently_omits_one_application(self):
        catalog = self.stage / "standalone-flow-catalog.tsv"
        lines = catalog.read_text(encoding="utf-8").splitlines()
        catalog.write_text("\n".join(lines[:-1]) + "\n", encoding="utf-8")

        result = self.run_verifier()

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("must contain five runnable applications", result.stderr)
        self.assertFalse((self.stage / "actual").exists())

    def test_failed_rerun_clears_later_success_evidence(self):
        first_result = self.run_verifier()
        self.assertEqual(first_result.returncode, 0, first_result.stderr)

        scrapbook = self.audit_fixtures / "scrapbook.audit"
        lines = scrapbook.read_text(encoding="utf-8").splitlines()
        for index, line in enumerate(lines):
            if line.startswith("view.target.present\t"):
                lines[index] = "view.target.present\t0"
                break
        else:
            self.fail("scrapbook fixture has no view.target.present verdict field")
        scrapbook.write_text("\n".join(lines) + "\n", encoding="utf-8")

        second_result = self.run_verifier()

        self.assertNotEqual(second_result.returncode, 0)
        self.assertTrue((self.stage / "actual/scrapbook.audit").is_file())
        for key in ("helloworld", "tutorial", "minesweeper", "floppybird"):
            self.assertFalse((self.stage / "actual" / (key + ".audit")).exists())


if __name__ == "__main__":
    unittest.main()
