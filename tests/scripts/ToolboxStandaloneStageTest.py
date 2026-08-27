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

    def _run_release(self, cpu=None):
        environment = os.environ.copy()
        environment["PATH"] = str(self.fixture / "tools") + os.pathsep + environment["PATH"]
        environment["RETRO68_TOOLCHAIN_BIN"] = str(self.fixture / "tools")
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

    def test_release_contains_five_loops_and_interactive_simpleviewer(self):
        result = self._run_release()
        self.assertEqual(result.returncode, 0, result.stderr)

        release = self.fixture / "build" / "release" / "toolbox-68k"
        expected = {"ASSETS.LRP", "README.md"}
        for name in (
            "LokaScrapbookStandaloneLoop68K",
            "LokaHelloStandaloneLoop68K",
            "LokaTutorialStandaloneLoop68K",
            "LokaMineStandaloneLoop68K",
            "LokaFloppyStandaloneLoop68K",
            "LokaSimpleViewer68K",
        ):
            expected.add(f"{name}.bin")
            expected.add(f"{name}.dsk")
        self.assertEqual({path.name for path in release.iterdir()}, expected)
        self.assertIn(
            b"ASSETS.LRP\n",
            (release / "LokaScrapbookStandaloneLoop68K.dsk").read_bytes(),
        )

    def test_ppc_release_contains_native_ppc_artifacts(self):
        result = self._run_release("ppc")
        self.assertEqual(result.returncode, 0, result.stderr)

        release = self.fixture / "build" / "release" / "toolbox-ppc"
        expected = {"ASSETS.LRP", "README.md"}
        for name in (
            "LokaScrapbookStandaloneLoopPPC",
            "LokaHelloStandaloneLoopPPC",
            "LokaTutorialStandaloneLoopPPC",
            "LokaMineStandaloneLoopPPC",
            "LokaFloppyStandaloneLoopPPC",
            "LokaSimpleViewerPPC",
        ):
            expected.add(f"{name}.bin")
            expected.add(f"{name}.dsk")
        self.assertEqual({path.name for path in release.iterdir()}, expected)
        self.assertIn(
            b"ASSETS.LRP\n",
            (release / "LokaScrapbookStandaloneLoopPPC.dsk").read_bytes(),
        )

    def test_vscode_tasks_use_the_completed_stage_for_floppy_and_scsi(self):
        tasks_document = json.loads((PROJECT_DIR / ".vscode" / "tasks.json").read_text())
        tasks = {task["label"]: task for task in tasks_document["tasks"]}
        stage_root = "build/presentation/toolbox-68k-release"

        self.assertEqual(
            tasks["Stage & Mount in Running MAME: Scrapbook Standalone Flow"]["dependsOn"],
            [
                "MAME: Eject Floppy",
                "Stage: Toolbox 68K Standalone Flow Release",
                "MAME: Mount Scrapbook Standalone Flow Stage",
            ],
        )
        self.assertIn(
            f"${{workspaceFolder}}/{stage_root}/LokaScrapbookStandaloneFlow68K.dsk",
            tasks["MAME: Mount Scrapbook Standalone Flow Stage"]["args"],
        )
        self.assertEqual(
            tasks["Stage & Start in MAME via SCSI: Scrapbook Standalone Flow"][
                "dependsOn"
            ],
            [
                "Stage: Toolbox 68K Standalone Flow Release",
                "Prepare SCSI Dev Disk: Scrapbook Standalone Flow",
                "MAME: Start",
            ],
        )
        self.assertEqual(
            tasks["Prepare SCSI Dev Disk: Scrapbook Standalone Flow"]["args"],
            [
                f"${{workspaceFolder}}/{stage_root}/LokaScrapbookStandaloneFlow68K.bin",
                f"${{workspaceFolder}}/{stage_root}/ASSETS.LRP",
            ],
        )
        self.assertEqual(
            tasks["Standalone: Toolbox PPC Release Action"]["args"],
            [
                "scripts/toolbox-standalone-flow.sh",
                "${input:toolboxStandaloneReleaseAction}",
                "ppc",
            ],
        )
        picker = next(
            item for item in tasks_document["inputs"] if item["id"] == "retro68DskPath"
        )
        self.assertIn(
            f"{stage_root}/LokaScrapbookStandaloneFlow68K.dsk",
            [option["value"] for option in picker["options"]],
        )


if __name__ == "__main__":
    unittest.main()
