"""Exercise native picker command routing without configuring a build tree."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PRESETS = json.loads((ROOT / "CMakePresets.json").read_text())


class NativeBuildPickerTest(unittest.TestCase):
    def check_calls(self, calls, platform, key):
        configure, build = calls
        configure_name = configure[1]
        build_name = build[2]
        self.assertIn(configure_name, {p["name"] for p in PRESETS["configurePresets"]})
        preset = next(p for p in PRESETS["buildPresets"] if p["name"] == build_name)
        self.assertEqual(preset["configurePreset"], configure_name)
        if key == "Tests":
            self.assertEqual(configure_name, platform + "-debug")
            self.assertEqual(build_name, platform + "-tests")
        self.assertEqual("-DLOKA_BUILD_SMIRKYCARD=ON" in configure, key == "SmirkyCard")

    def test_macos_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stub = root / "cmake"
            stub.write_text('#!/bin/sh\nprintf "%s\\n" "$*" >> "$PICKER_LOG"\n')
            stub.chmod(0o755)
            for key in ("Tests", "HelloWorld", "All", "SmirkyCard", "HelloWorldStandaloneLoop"):
                with self.subTest(key=key):
                    log = root / "calls"
                    log.write_text("")
                    subprocess.run([os.environ.get("PICKER_BASH", "bash"),
                                    str(ROOT / "scripts/vscode-build-target.sh"), "macos", key],
                                   env=dict(os.environ, PATH=str(root) + os.pathsep + os.environ["PATH"],
                                            PICKER_LOG=str(log)), check=True)
                    self.check_calls([line.split() for line in log.read_text().splitlines()], "macos", key)

    def test_windows_powershell_commands(self):
        shell = shutil.which("powershell.exe")
        if not shell:
            candidate = Path("/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe")
            shell = str(candidate) if candidate.exists() else None
        if not shell:
            self.skipTest("[skip] Windows PowerShell not available (pwsh alone is insufficient)")
        script = str(ROOT / "scripts/vscode-build-target.ps1")
        if os.name != "nt":
            script = subprocess.check_output(["wslpath", "-w", script], text=True).strip()
        for key in ("Tests", "HelloWorld", "All", "SmirkyCard", "HelloWorldStandaloneLoop"):
            with self.subTest(key=key):
                command = ("function global:cmake { 'CMAKE:' + (ConvertTo-Json -Compress -InputObject @($args)); "
                           "$global:LASTEXITCODE = 0 }; & '" + script.replace("'", "''") + "' win32 " + key)
                result = subprocess.run([shell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", command],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                calls = [json.loads(line[6:]) for line in result.stdout.splitlines() if line.startswith("CMAKE:")]
                self.check_calls(calls, "win32", key)

    def test_bash32_empty_array_expansion_is_guarded(self):
        source = (ROOT / "scripts/vscode-build-target.sh").read_text()
        self.assertIn('${configure_args[@]+"${configure_args[@]}"}', source)

    def test_documented_picker_keys_are_reachable(self):
        tasks = json.loads((ROOT / ".vscode/tasks.json").read_text())
        inputs = {item["id"]: item for item in tasks["inputs"]}
        for name in ("MAME_DEVELOPMENT.md", "TOOLBOX_STANDALONE_FLOW.md", "environments.md"):
            prose = re.sub(r"\s+", " ", (ROOT / "docs" / name).read_text())
            for task, following in re.findall(r"\*\*(Build & Start(?: Loop)? in MAME via SCSI)\*\* and pick (.*?)(?:from the prompt)", prose):
                picker = inputs["lokaScsiLoop" if "Start Loop" in task else "lokaScsiApp"]
                options = {o["value"] if isinstance(o, dict) else o for o in picker["options"]}
                for key in re.findall(r"`(\w+)`", following):
                    self.assertIn(key, options, name)
    def test_native_reel_documentation_uses_visible_tasks(self):
        tasks = json.loads((ROOT / ".vscode/tasks.json").read_text())
        prose = re.sub(r"\s+", " ", (ROOT / "docs/environments.md").read_text())
        for label in re.findall(r"\*\*(Build: .*?)\*\*", prose):
            task = next(t for t in tasks["tasks"] if t["label"] == label)
            self.assertFalse(task.get("hide", False), label)


if __name__ == "__main__":
    unittest.main()
