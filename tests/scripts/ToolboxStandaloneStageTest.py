#!/usr/bin/env python3
"""Tests for the transportable Toolbox standalone Flow stage."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


PROJECT_DIR = Path(__file__).resolve().parents[2]


class ToolboxStandaloneStageTest(unittest.TestCase):
    def setUp(self):
        self.temporary_directory = tempfile.TemporaryDirectory(
            prefix="toolbox-standalone-stage-"
        )
        self.fixture = Path(self.temporary_directory.name)
        (self.fixture / "scripts").mkdir()
        (self.fixture / "docs").mkdir()
        (self.fixture / "example" / "ScrapbookUI").mkdir(parents=True)
        (self.fixture / "tools").mkdir()

        for name in (
            "toolbox-standalone-flow.sh",
            "presentation-stage.sh",
            "retro68-cmake.sh",
            "retro68-env.sh",
            "env-file.sh",
        ):
            shutil.copy2(PROJECT_DIR / "scripts" / name, self.fixture / "scripts" / name)
        shutil.copy2(
            PROJECT_DIR / "docs" / "TOOLBOX_STANDALONE_FLOW.md",
            self.fixture / "docs" / "TOOLBOX_STANDALONE_FLOW.md",
        )
        (self.fixture / "example" / "ScrapbookUI" / "ASSETS.LRP").write_bytes(
            b"fixture-assets\x00\xff"
        )

        self._write_tool(
            "cmake",
            r'''#!/usr/bin/env bash
set -euo pipefail
cpu=68k
suffix=68K
if [[ " $* " == *" retro68-ppc-standalone-release "* ]]; then
  cpu=ppc
  suffix=PPC
fi
if [[ " $* " == *" --build "* ]]; then
  output="$PWD/build/retro68/$cpu/Standalone/Release/tests/toolbox"
  mkdir -p "$output"
  if [[ " $* " == *" LokaStandaloneLoop${suffix}All"* ]]; then
    for base in \
      LokaScrapbookStandaloneLoop \
      LokaHelloStandaloneLoop \
      LokaTutorialStandaloneLoop \
      LokaMineStandaloneLoop \
      LokaFloppyStandaloneLoop; do
      name="$base$suffix"
      printf 'fixture-macbinary-%s' "$name" >"$output/$name.bin"
      printf '%s\n' "$name" >"$output/$name.dsk"
    done
    simple="$PWD/build/retro68/$cpu/Standalone/Release/example/SimpleViewer"
    mkdir -p "$simple"
    printf 'fixture-simpleviewer' >"$simple/LokaSimpleViewer$suffix.bin"
    printf 'LokaSimpleViewer%s\n' "$suffix" >"$simple/LokaSimpleViewer$suffix.dsk"
  else
    printf 'fixture-macbinary' >"$output/LokaScrapbookStandaloneFlow$suffix.bin"
    printf 'LokaScrapbookStandaloneFlow%s\n' "$suffix" \
      >"$output/LokaScrapbookStandaloneFlow$suffix.dsk"
  fi
  cp "$PWD/example/ScrapbookUI/ASSETS.LRP" "$output/ASSETS.LRP"
fi
''',
        )
        self._write_tool(
            "ninja",
            r'''#!/usr/bin/env bash
exit 0
''',
        )
        self._write_tool(
            "hmount",
            r'''#!/usr/bin/env bash
set -euo pipefail
printf '%s' "$1" >"$HOME/mounted-disk"
''',
        )
        self._write_tool(
            "hcopy",
            r'''#!/usr/bin/env bash
set -euo pipefail
source_path="$2"
destination="$3"
if [[ "$source_path" == ':ASSETS.LRP' ]]; then
  cp "$HOME/disk-assets" "$destination"
  exit 0
fi
if [[ "${FAKE_HCOPY_FAIL:-0}" == 1 ]]; then
  exit 17
fi
cp "$source_path" "$HOME/disk-assets"
printf 'ASSETS.LRP\n' >>"$(cat "$HOME/mounted-disk")"
''',
        )
        self._write_tool(
            "hls",
            r'''#!/usr/bin/env bash
set -euo pipefail
grep -q '^ASSETS.LRP$' "$(cat "$HOME/mounted-disk")"
cat "$(cat "$HOME/mounted-disk")"
''',
        )
        self._write_tool(
            "humount",
            r'''#!/usr/bin/env bash
set -euo pipefail
rm -f "$HOME/mounted-disk"
''',
        )

    def tearDown(self):
        self.temporary_directory.cleanup()

    def _write_tool(self, name, body):
        path = self.fixture / "tools" / name
        path.write_text(body, encoding="utf-8")
        path.chmod(0o755)

    def _run_stage(self, cpu=None, **environment_overrides):
        environment = os.environ.copy()
        environment["PATH"] = str(self.fixture / "tools") + os.pathsep + environment["PATH"]
        environment["RETRO68_TOOLCHAIN_BIN"] = str(self.fixture / "tools")
        environment.update(environment_overrides)
        command = ["bash", "scripts/toolbox-standalone-flow.sh", "Stage"]
        if cpu is not None:
            command.append(cpu)
        return subprocess.run(
            command,
            cwd=self.fixture,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

    def _run_release(self, cpu=None, **environment_overrides):
        environment = os.environ.copy()
        environment["PATH"] = str(self.fixture / "tools") + os.pathsep + environment["PATH"]
        environment["RETRO68_TOOLCHAIN_BIN"] = str(self.fixture / "tools")
        environment.update(environment_overrides)
        command = ["bash", "scripts/toolbox-standalone-flow.sh", "Release"]
        if cpu is not None:
            command.append(cpu)
        return subprocess.run(
            command,
            cwd=self.fixture,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

    def test_stage_contains_transport_and_self_contained_disk_artifacts(self):
        result = self._run_stage()
        self.assertEqual(result.returncode, 0, result.stderr)

        stage = self.fixture / "build" / "presentation" / "toolbox-68k-release"
        self.assertEqual(
            sorted(path.name for path in stage.iterdir()),
            [
                "ASSETS.LRP",
                "LokaScrapbookStandaloneFlow68K.bin",
                "LokaScrapbookStandaloneFlow68K.dsk",
                "README.md",
            ],
        )
        self.assertEqual((stage / "ASSETS.LRP").read_bytes(), b"fixture-assets\x00\xff")
        self.assertIn(
            b"ASSETS.LRP\n",
            (stage / "LokaScrapbookStandaloneFlow68K.dsk").read_bytes(),
        )

    def test_disk_population_failure_preserves_completed_stage(self):
        first = self._run_stage()
        self.assertEqual(first.returncode, 0, first.stderr)
        stage = self.fixture / "build" / "presentation" / "toolbox-68k-release"
        previous_disk = (stage / "LokaScrapbookStandaloneFlow68K.dsk").read_bytes()
        (stage / "completed-stage").write_text("keep", encoding="utf-8")

        failed = self._run_stage(FAKE_HCOPY_FAIL="1")

        self.assertEqual(failed.returncode, 17)
        self.assertEqual((stage / "completed-stage").read_text(encoding="utf-8"), "keep")
        self.assertEqual(
            (stage / "LokaScrapbookStandaloneFlow68K.dsk").read_bytes(), previous_disk
        )

    def test_ppc_stage_contains_transport_and_self_contained_disk_artifacts(self):
        result = self._run_stage("ppc")
        self.assertEqual(result.returncode, 0, result.stderr)

        stage = self.fixture / "build" / "presentation" / "toolbox-ppc-release"
        self.assertEqual(
            sorted(path.name for path in stage.iterdir()),
            [
                "ASSETS.LRP",
                "LokaScrapbookStandaloneFlowPPC.bin",
                "LokaScrapbookStandaloneFlowPPC.dsk",
                "README.md",
            ],
        )
        self.assertEqual((stage / "ASSETS.LRP").read_bytes(), b"fixture-assets\x00\xff")
        self.assertIn(
            b"ASSETS.LRP\n",
            (stage / "LokaScrapbookStandaloneFlowPPC.dsk").read_bytes(),
        )

    def test_release_isolates_each_application_on_both_cpus(self):
        applications = {
            "scrapbook": "LokaScrapbookStandaloneLoop",
            "helloworld": "LokaHelloStandaloneLoop",
            "tutorial": "LokaTutorialStandaloneLoop",
            "minesweeper": "LokaMineStandaloneLoop",
            "floppybird": "LokaFloppyStandaloneLoop",
            "simpleviewer": "LokaSimpleViewer",
        }
        for cpu, suffix in (("68k", "68K"), ("ppc", "PPC")):
            with self.subTest(cpu=cpu):
                result = self._run_release(cpu)
                self.assertEqual(result.returncode, 0, result.stderr)
                release = self.fixture / "build" / "release" / ("toolbox-" + cpu)
                self.assertEqual(
                    {path.name for path in release.iterdir()},
                    set(applications) | {"README.md"},
                )
                for key, base in applications.items():
                    name = base + suffix
                    directory = release / key
                    expected = {name + ".bin", name + ".dsk"}
                    if key == "scrapbook":
                        expected.add("ASSETS.LRP")
                        self.assertEqual(
                            (directory / "ASSETS.LRP").read_bytes(),
                            b"fixture-assets\x00\xff",
                        )
                        self.assertIn(
                            b"ASSETS.LRP\n", (directory / (name + ".dsk")).read_bytes()
                        )
                    self.assertEqual({path.name for path in directory.iterdir()}, expected)
                    payload = (b"fixture-simpleviewer" if key == "simpleviewer"
                               else ("fixture-macbinary-" + name).encode())
                    self.assertEqual((directory / (name + ".bin")).read_bytes(), payload)

    def test_release_failure_preserves_previous_layout_and_retry_replaces_it(self):
        for cpu in ("68k", "ppc"):
            with self.subTest(cpu=cpu):
                release = self.fixture / "build" / "release" / ("toolbox-" + cpu)
                release.mkdir(parents=True)
                # Simulate the old flat release and an audit left by its user.
                (release / "old.bin").write_bytes(b"old application")
                (release / "ASSETS.LRP").write_bytes(b"old assets")
                (release / "LOG.TXT").write_bytes(b"old audit")
                previous = {p.name: p.read_bytes() for p in release.iterdir()}
                failed = self._run_release(cpu, FAKE_HCOPY_FAIL="1")
                self.assertNotEqual(failed.returncode, 0)
                self.assertEqual(
                    {p.name: p.read_bytes() for p in release.iterdir()}, previous
                )
                retry = self._run_release(cpu)
                self.assertEqual(retry.returncode, 0, retry.stderr)
                self.assertTrue((release / "scrapbook" / "ASSETS.LRP").is_file())
                for old_name in previous:
                    self.assertFalse((release / old_name).exists())
                self.assertEqual(list(release.parent.glob(".*.staging.*")), [])
                self.assertEqual(list(release.parent.glob(".*.previous.*")), [])

    def test_vscode_tasks_use_the_completed_stage_for_scsi(self):
        tasks_document = json.loads((PROJECT_DIR / ".vscode" / "tasks.json").read_text())
        tasks = {task["label"]: task for task in tasks_document["tasks"]}
        inputs = {entry["id"]: entry for entry in tasks_document["inputs"]}

        self.assertEqual(
            tasks["Build & Start in MAME via SCSI"]["dependsOn"],
            [
                "Build & Prepare SCSI Dev Disk",
                "MAME: Start",
            ],
        )
        self.assertEqual(
            tasks["Build & Prepare SCSI Dev Disk"]["args"],
            ["--build-and-prepare", "${input:lokaScsiApp}"],
        )
        self.assertIn(
            "ScrapbookUI",
            inputs["lokaScsiApp"]["options"],
        )
        self.assertNotIn(
            "HelloWorldScenarioLoop",
            inputs["lokaScsiApp"]["options"],
        )
        self.assertIn(
            "HelloWorldScenarioLoop",
            inputs["lokaScsiLoop"]["options"],
        )
        self.assertEqual(
            tasks["Build & Start Loop in MAME via SCSI"]["dependsOn"],
            ["Build & Prepare SCSI Loop Disk", "MAME: Start"],
        )
        self.assertEqual(
            [
                task["label"]
                for task in tasks_document["tasks"]
                if "SCSI" in task["label"] and not task.get("hide", False)
            ],
            [
                "Build & Start in MAME via SCSI",
                "Build & Start Loop in MAME via SCSI",
            ],
        )
        wrapper = (PROJECT_DIR / "scripts" / "mame-dev-disk-app.sh").read_text()
        self.assertIn('if [ "$key" = "ScrapbookStandaloneFlow" ]; then', wrapper)
        self.assertIn('"$script_dir/toolbox-standalone-flow.sh" Stage', wrapper)
        self.assertIn('if [ "$mode" = "--build-and-prepare" ]; then', wrapper)
        self.assertIn(
            "build/presentation/toolbox-${cpu}-release/LokaScrapbookStandaloneFlow${suffix}.bin",
            wrapper,
        )
        self.assertIn("build/presentation/toolbox-${cpu}-release/ASSETS.LRP", wrapper)
        self.assertEqual(
            tasks["Standalone: Toolbox PPC Build / Stage / Release"]["args"],
            [
                "scripts/toolbox-standalone-flow.sh",
                "${input:toolboxStandaloneReleaseAction}",
                "ppc",
            ],
        )


if __name__ == "__main__":
    unittest.main()
