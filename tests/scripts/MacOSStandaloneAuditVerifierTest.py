#!/usr/bin/env python3
"""End-to-end audit checks for the packaged macOS standalone verifier."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_DIR = Path(__file__).resolve().parents[2]
EXPECTED_AUDIT = (
    PROJECT_DIR / "tests/scenarios/expected/scrapbook/standalone-tour.audit"
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
        shutil.copy2(EXPECTED_AUDIT, self.stage / "standalone-tour.audit")

        binary = (
            self.stage
            / "LokaScrapbookStandaloneFlowMacOS.app"
            / "Contents/MacOS/LokaScrapbookStandaloneFlowMacOS"
        )
        binary.parent.mkdir(parents=True)
        binary.write_text(
            """#!/usr/bin/env bash
set -euo pipefail
cp "$LOKA_STANDALONE_AUDIT_FIXTURE" LOG.TXT
while :; do sleep 1; done
""",
            encoding="utf-8",
        )
        binary.chmod(0o755)
        resources = binary.parents[1] / "Resources"
        resources.mkdir()
        (resources / "ASSETS.LRP").write_bytes(b"fixture-assets")

    def tearDown(self):
        self.temporary_directory.cleanup()

    def run_verifier(self, audit):
        environment = os.environ.copy()
        environment["LOKA_STANDALONE_AUDIT_FIXTURE"] = str(audit)
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
        result = self.run_verifier(EXPECTED_AUDIT)

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Runtime-verified macOS Standalone Flow", result.stdout)

    def test_refuses_bogus_verdict_body_with_valid_structural_lines(self):
        lines = EXPECTED_AUDIT.read_text(encoding="utf-8").splitlines()
        lines[13:33] = ["bogus_{:02d}".format(index) for index in range(1, 21)]
        bogus = self.stage / "bogus.audit"
        bogus.write_text("\n".join(lines) + "\n", encoding="utf-8")

        result = self.run_verifier(bogus)

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("does not match", result.stderr)


if __name__ == "__main__":
    unittest.main()
