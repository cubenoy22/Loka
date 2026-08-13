#!/usr/bin/env python3
"""Fail-closed tests for the release archive assembler."""

import hashlib
import importlib.util
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


PROJECT_DIR = pathlib.Path(__file__).resolve().parents[2]
ASSEMBLER = PROJECT_DIR / "scripts" / "release" / "assemble.py"
sys.dont_write_bytecode = True


def run(*arguments, cwd=None):
    return subprocess.run(
        list(arguments),
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def load_assembler():
    spec = importlib.util.spec_from_file_location("release_assembler_test", ASSEMBLER)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class ReleaseAssemblerTest(unittest.TestCase):
    def make_repository(self, directory):
        root = pathlib.Path(directory)
        script = root / "scripts" / "release" / "assemble.py"
        script.parent.mkdir(parents=True)
        shutil.copy2(ASSEMBLER, script)
        (root / "tracked.txt").write_text("tracked release file\n", encoding="utf-8")
        (root / "ASSETS.LRP").write_bytes(b"tracked package")
        (root / "make-fixtures.py").write_text(
            """#!/usr/bin/env python3
import pathlib

stage = pathlib.Path("build/stage")
stage.mkdir(parents=True)
(stage / "release.bin").write_bytes(b"release")
(stage / "LokaDev.hd").write_bytes(b"development disk")
(stage / "scratch.dsk").write_bytes(b"development disk")
(stage / ".DS_Store").write_bytes(b"finder metadata")
(stage / "generated.LRPK").write_bytes(b"untracked package")
""",
            encoding="utf-8",
        )
        (root / "modify-tracked.py").write_text(
            """#!/usr/bin/env python3
import pathlib

pathlib.Path("tracked.txt").write_text("modified by build\\n", encoding="utf-8")
""",
            encoding="utf-8",
        )
        commands = (
            ("git", "init", "-b", "main"),
            ("git", "config", "user.name", "Release Assembler Test"),
            ("git", "config", "user.email", "release-test@example.invalid"),
            ("git", "config", "commit.gpgsign", "false"),
            ("git", "config", "tag.gpgsign", "false"),
            ("git", "add", "."),
            ("git", "commit", "-m", "fixture"),
            ("git", "tag", "-a", "v1.0.0", "-m", "fixture release"),
        )
        for command in commands:
            result = run(*command, cwd=root)
            self.assertEqual(result.returncode, 0, result.stderr)
        return root, script

    def invoke(self, root, script, allowlist, archive, *extra):
        return run(
            sys.executable,
            str(script),
            "--tag",
            "v1.0.0",
            "--allowlist",
            str(allowlist),
            "--archive",
            str(archive),
            *extra,
            cwd=root,
        )

    def test_unlisted_archive_member_is_refused(self):
        assembler = load_assembler()
        with tempfile.TemporaryDirectory(prefix="release-archive-check-") as directory:
            archive = pathlib.Path(directory) / "swept.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("listed.txt", b"listed")
                output.writestr("unlisted.txt", b"swept in")
            hashes = {"listed.txt": hashlib.sha256(b"listed").hexdigest()}
            with self.assertRaises(assembler.ReleaseError) as caught:
                assembler.verify_archive(archive, hashes)
            self.assertIn("unlisted archive member", str(caught.exception))

    def test_listed_missing_file_is_refused_without_outputs(self):
        with tempfile.TemporaryDirectory(prefix="release-missing-") as directory:
            root, script = self.make_repository(directory)
            allowlist = root / "allowlist.txt"
            archive = root / "out" / "release.zip"
            allowlist.write_text("git\tmissing.txt\n", encoding="utf-8")
            result = self.invoke(root, script, allowlist, archive)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("listed file is missing", result.stderr)
            self.assertFalse(archive.exists())
            self.assertFalse(pathlib.Path(str(archive) + ".manifest.txt").exists())

    def test_only_allowlisted_build_output_enters_archive(self):
        with tempfile.TemporaryDirectory(prefix="release-dev-disk-") as directory:
            root, script = self.make_repository(directory)
            allowlist = root / "allowlist.txt"
            archive = root / "out" / "release.zip"
            allowlist.write_text(
                "git\ttracked.txt\tdocs/tracked.txt\n"
                "git\tASSETS.LRP\tassets/ASSETS.LRP\n"
                "build\tbuild/stage/release.bin\tbin/release.bin\n",
                encoding="utf-8",
            )
            result = self.invoke(
                root,
                script,
                allowlist,
                archive,
                "--build-command",
                f"{sys.executable} make-fixtures.py",
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            with zipfile.ZipFile(archive, "r") as assembled:
                self.assertEqual(
                    assembled.namelist(),
                    ["assets/ASSETS.LRP", "bin/release.bin", "docs/tracked.txt"],
                )
                self.assertNotIn("LokaDev.hd", "\n".join(assembled.namelist()))
                self.assertNotIn("scratch.dsk", "\n".join(assembled.namelist()))
                self.assertNotIn(".DS_Store", "\n".join(assembled.namelist()))
            manifest = pathlib.Path(str(archive) + ".manifest.txt").read_text(encoding="utf-8")
            self.assertIn("checkout_path=/tmp/loka-release-assembly-worktree", manifest)
            self.assertIn("provenance=git-tracked:", manifest)
            self.assertIn(
                "provenance=build-output:build/stage/release.bin  bin/release.bin",
                manifest,
            )
            self.assertEqual(manifest.count("artifact_sha256="), 3)

    def test_git_provenance_refuses_file_modified_by_build(self):
        with tempfile.TemporaryDirectory(prefix="release-modified-") as directory:
            root, script = self.make_repository(directory)
            allowlist = root / "allowlist.txt"
            archive = root / "out" / "release.zip"
            allowlist.write_text("git\ttracked.txt\n", encoding="utf-8")
            result = self.invoke(
                root,
                script,
                allowlist,
                archive,
                "--build-command",
                f"{sys.executable} modify-tracked.py",
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("listed git file differs", result.stderr)
            self.assertFalse(archive.exists())

    def test_untracked_lrpk_build_output_is_refused(self):
        with tempfile.TemporaryDirectory(prefix="release-lrpk-") as directory:
            root, script = self.make_repository(directory)
            allowlist = root / "allowlist.txt"
            archive = root / "out" / "release.zip"
            allowlist.write_text("build\tbuild/stage/generated.LRPK\n", encoding="utf-8")
            result = self.invoke(
                root,
                script,
                allowlist,
                archive,
                "--build-command",
                f"{sys.executable} make-fixtures.py",
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("LRPK package must be git-tracked", result.stderr)
            self.assertFalse(archive.exists())


if __name__ == "__main__":
    unittest.main()
