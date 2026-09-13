#!/usr/bin/env python3
"""Path policy and workflow integration contracts; standard library only."""

import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/ci/ci_paths.py"
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("ci_paths", TOOL)
CI = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CI)
JOBS = ("toolbox-build", "macos", "win32")
WORKFLOWS = ("toolbox", "macos", "windows")
GUARD = "github.event_name != 'pull_request' || steps.changes.outputs.run == 'true'"


class CiPathsTest(unittest.TestCase):
    def check_paths(self, paths, expected):
        for job, run in zip(JOBS, expected):
            with self.subTest(paths=paths, job=job):
                actual, reason = CI.classify(job, paths)
                self.assertEqual(actual, run, reason)
                self.assertTrue(reason)

    def test_shared_must_run_paths(self):
        for path in ("common/core/State.hpp", "example/hello/main.cpp",
                     "tests/toolbox/scenario.json", "tests/macos/driver.mm",
                     "tests/UnitTest.cpp", "tests/scripts/CiPathsTest.py",
                     "cmake/LokaTestSources.cmake", "CMakeLists.txt"):
            self.check_paths([path], (True, True, True))

    def test_platform_trees(self):
        for path, expected in (
            ("apple/toolbox/src/Control.cpp", (True, False, False)),
            ("apple/macos/src/Control.mm", (False, True, False)),
            ("apple/shared.hpp", (True, True, False)),
            ("win32/src/Control.cpp", (False, False, True)),
        ):
            self.check_paths([path], expected)

    def test_docs_only(self):
        self.check_paths(["docs/guide.md", "docs/nested/image.png"],
                         (False, False, False))

    def test_plans_only_has_only_toolbox_allowance(self):
        self.check_paths(["plans/proposal.md"], (False, True, True))

    def test_workflow_self_change_overrides_skip(self):
        for index, workflow in enumerate(WORKFLOWS):
            self.check_paths(["docs/guide.md",
                              ".github/workflows/" + workflow + ".yml"],
                             tuple(i == index for i in range(3)))
        self.check_paths([".github/workflows/linux.yml"], (False, False, False))
        self.check_paths([".github/workflows/size-bank-refresh.yml"],
                         (False, False, False))

    def test_bank_json_only(self):
        self.check_paths(["tools/ci/retro68_68k_size_baseline.json"],
                         (True, False, False))

    def test_retro68_tools(self):
        for path in ("tools/ci/retro68_size_report.py",
                     "tools/ci/retro68_size_bank_refresh.py"):
            self.check_paths([path], (True, False, False))

    def test_mixed_diff_cannot_hide_run_path(self):
        for paths, expected in (
            (["docs/guide.md", "common/core/State.hpp"], (True, True, True)),
            (["common/core/State.hpp", "docs/guide.md"], (True, True, True)),
            (["apple/toolbox/a.cpp", "win32/a.cpp"], (True, False, True)),
        ):
            self.check_paths(paths, expected)

    def test_empty_diff_runs(self):
        self.check_paths([], (True, True, True))

    def test_unknown_paths_run(self):
        for path in ("CMakePresets.json", "README.md", "assets/icon.png",
                     "tools/ci/ci_paths.py", "tools/lrpc/main.cpp",
                     "docs-extra/file.md", "", '"docs/quoted\\nname.md"'):
            self.check_paths([path], (True, True, True))

    def test_reason_records_rule_or_conservative_fallback(self):
        self.assertIn("must-run common/*", CI.classify("macos", ["common/a"])[1])
        self.assertIn("outside may-skip", CI.classify("macos", ["unknown"])[1])
        self.assertIn("own workflow", CI.classify(
            "macos", [".github/workflows/macos.yml"])[1])
        self.assertIn("may-skip: docs/*, win32/*", CI.classify(
            "macos", ["docs/a", "win32/b"])[1])
        self.assertNotIn("\n", CI.classify("macos", ["unknown\nrun=false"])[1])

    def test_cli_argv_stdin_multiple_jobs_and_summary(self):
        with tempfile.TemporaryDirectory() as directory:
            summary = Path(directory) / "summary"
            command = [sys.executable, str(TOOL), "--summary", str(summary)]
            for job in JOBS:
                command += ["--job", job]
            argv = subprocess.check_output(command + ["docs/guide.md"], text=True)
            stdin = subprocess.check_output(command, input="docs/guide.md\n", text=True)
            self.assertEqual(argv, stdin)
            self.assertEqual(argv.count("run=false\n"), 3)
            self.assertEqual(len(summary.read_text().splitlines()), 6)
            empty = subprocess.check_output(command, input="", text=True)
            self.assertEqual(empty.count("run=true\n"), 3)
            self.assertEqual(len(summary.read_text().splitlines()), 6)
            bad = subprocess.run([sys.executable, str(TOOL), "--job", "typo"],
                                 input="docs/a\n", capture_output=True, text=True)
            self.assertNotEqual(bad.returncode, 0)
            self.assertNotIn("run=false", bad.stdout)

    def test_workflow_guards_preserve_existing_conditions(self):
        for name, job in zip(WORKFLOWS, JOBS):
            workflow = (ROOT / ".github/workflows" / (name + ".yml")).read_text()
            steps = re.split(r"^      - ", workflow.split("    steps:\n", 1)[1],
                             flags=re.MULTILINE)[1:]
            self.assertIn("fetch-depth: 0", steps[0])
            self.assertNotIn("        if:", steps[0])
            self.assertEqual(steps[1].count("        if:"), 1)
            self.assertIn("id: changes", steps[1])
            self.assertIn("if: ${{ github.event_name == 'pull_request' }}", steps[1])
            self.assertIn("shell: bash", steps[1])
            self.assertIn("--job " + job, steps[1])
            for step in steps[2:]:
                with self.subTest(workflow=name, step=step.splitlines()[0]):
                    self.assertEqual(step.count("        if:"), 1)
                    self.assertIn(GUARD, step)
            if name == "macos":
                self.assertIn("failure() && (" + GUARD + ")", steps[-1])
            if name == "toolbox":
                self.assertIn("!cancelled() && github.event_name == 'pull_request'", steps[-2])
                self.assertIn("--check-headroom 1024", steps[-2])
                self.assertIn("steps.headroom.outcome != 'skipped'", steps[-1])
                self.assertIn("github.event.pull_request.head.repo.full_name == github.repository", steps[-1])

    def test_workflow_diff_handles_deletion_rename_and_failure(self):
        # Execute the actual Bash step against a local git history, including
        # a rename from a must-run tree into docs and a missing base ref.
        workflow = (ROOT / ".github/workflows/macos.yml").read_text()
        step = workflow.split("      - name: Classify PR paths\n", 1)[1]
        script = re.search(r"        run: \|\n((?:          .*\n)+)", step)[1]
        script = "\n".join(line[10:] for line in script.splitlines())
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=repo,
                                               stderr=subprocess.STDOUT, text=True)
            git("init", "-q")
            git("config", "user.name", "CI Test")
            git("config", "user.email", "ci-test@example.invalid")
            (repo / "common").mkdir()
            (repo / "common/input").write_text("fixture\n")
            git("add", ".")
            git("commit", "-qm", "base")
            git("update-ref", "refs/remotes/origin/main", "HEAD")
            (repo / "tools/ci").mkdir(parents=True)
            (repo / "tools/ci/ci_paths.py").write_text(TOOL.read_text())
            (repo / "docs").mkdir()
            (repo / "docs/guide").write_text("guide\n")
            git("add", "docs")
            git("commit", "-qm", "docs")
            env = dict(os.environ, BASE="main", RUNNER_TEMP=directory,
                       GITHUB_OUTPUT=str(repo / "output"),
                       GITHUB_STEP_SUMMARY=str(repo / "summary"))
            def run():
                (repo / "output").write_text("")
                return subprocess.run(["bash", "--noprofile", "--norc", "-eo",
                                       "pipefail", "-c", script], cwd=repo,
                                      env=env, capture_output=True, text=True)
            self.assertEqual(run().returncode, 0)
            self.assertIn("run=false\n", (repo / "output").read_text())
            self.assertEqual(len((repo / "summary").read_text().splitlines()), 1)
            git("mv", "common/input", "docs/input")
            git("commit", "-qm", "move to docs")
            self.assertEqual(run().returncode, 0)
            self.assertIn("run=true\n", (repo / "output").read_text())
            self.assertIn("common/input", (repo / "ci-paths.txt").read_text())
            env["BASE"] = "missing"
            self.assertNotEqual(run().returncode, 0)
            self.assertEqual((repo / "output").read_text(), "")


if __name__ == "__main__":
    unittest.main()
