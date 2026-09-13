#!/usr/bin/env python3
"""Tests for the final Retro68 MacBinary size report and regression gate."""

import importlib.util
import json
import os
import textwrap
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


PROJECT_DIR = pathlib.Path(__file__).resolve().parents[2]
REPORT_TOOL = PROJECT_DIR / "tools" / "ci" / "retro68_size_report.py"
TOOLBOX_WORKFLOW = PROJECT_DIR / ".github" / "workflows" / "toolbox.yml"
SIZE_GATE_INVOCATION = (
    'python3 tools/ci/retro68_size_report.py '
    'build/retro68/68k/Release "${args[@]}"'
)
sys.dont_write_bytecode = True


def load_report_tool():
    spec = importlib.util.spec_from_file_location(
        "retro68_size_report_test", REPORT_TOOL
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def pad_128(payload):
    return payload + bytes((-len(payload)) % 128)


def make_macbinary(resources):
    grouped = {}
    data_section = bytearray()
    offsets = []
    for kind, payload in resources:
        offset = len(data_section)
        data_section.extend(struct.pack(">I", len(payload)))
        data_section.extend(payload)
        grouped.setdefault(kind, []).append(len(offsets))
        offsets.append(offset)

    type_entries = bytearray()
    references = bytearray()
    reference_base = 2 + 8 * len(grouped)
    for kind, indices in grouped.items():
        type_entries.extend(kind.encode("ascii"))
        type_entries.extend(struct.pack(">H", len(indices) - 1))
        type_entries.extend(struct.pack(">H", reference_base + len(references)))
        for resource_id, index in enumerate(indices):
            data_offset = offsets[index]
            references.extend(struct.pack(">hH", resource_id, 0xFFFF))
            references.append(0)
            references.extend(data_offset.to_bytes(3, "big"))
            references.extend(bytes(4))

    type_list = struct.pack(">H", len(grouped) - 1) + type_entries + references
    data_offset = 256
    map_offset = data_offset + len(data_section)
    map_bytes = bytearray(28 + len(type_list))
    struct.pack_into(">H", map_bytes, 24, 28)
    struct.pack_into(">H", map_bytes, 26, 28 + len(type_list))
    map_bytes[28:] = type_list
    header = struct.pack(
        ">IIII", data_offset, map_offset, len(data_section), len(map_bytes)
    )
    map_bytes[:16] = header
    resource_fork = header + bytes(data_offset - 16) + data_section + map_bytes

    macbinary = bytearray(128)
    macbinary[1] = 7
    macbinary[2:9] = b"Fixture"
    macbinary[65:69] = b"APPL"
    struct.pack_into(">I", macbinary, 87, len(resource_fork))
    return bytes(macbinary) + pad_128(resource_fork)


def baseline_for(path, total, code, data, rela, allowance=4096):
    return {
        "schema_version": 1,
        "identity": {"fixture": "test"},
        "material_growth_bytes": allowance,
        "artifacts": [
            {
                "name": "Fixture68K",
                "path": path,
                "baseline": {
                    "total": total,
                    "CODE": code,
                    "DATA": data,
                    "RELA": rela,
                },
            }
        ],
    }


class Retro68SizeReportTest(unittest.TestCase):
    def test_toolbox_workflow_runs_the_release_size_gate(self):
        workflow = TOOLBOX_WORKFLOW.read_text(encoding="utf-8")
        self.assertEqual(workflow.count(SIZE_GATE_INVOCATION), 1)

        mutations = {
            "invocation removed": workflow.replace(SIZE_GATE_INVOCATION, "", 1),
            "build root drifted": workflow.replace(
                SIZE_GATE_INVOCATION,
                SIZE_GATE_INVOCATION.replace("Release", "Unexpected"),
                1,
            ),
        }
        for name, mutated in mutations.items():
            with self.subTest(name=name):
                self.assertEqual(mutated.count(SIZE_GATE_INVOCATION), 0)

    def workflow_script(self, name):
        step = TOOLBOX_WORKFLOW.read_text().split("      - name: " + name + "\n", 1)[1]
        step = step.split("\n      - name:", 1)[0]
        return textwrap.dedent(step.split("        run: |\n", 1)[1])

    def test_workflow_gate_selects_pr_base_and_refuses_absent_identity(self):
        script = self.workflow_script("Report and gate Toolbox 68K binary sizes")
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            probe = root / "python3"
            probe.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n')
            probe.chmod(0o755)
            for is_pr, ref, expected in (("false", "", "--report-only"),
                                         ("true", "commit", "--compare-build-root"),
                                         ("true", "", None)):
                with self.subTest(is_pr=is_pr, ref=ref):
                    env = dict(os.environ, PATH=str(root) + os.pathsep + os.environ["PATH"],
                               IS_PR=is_pr, COMPARISON_REF=ref, RUNNER_TEMP=str(root))
                    result = subprocess.run(["bash", "-eu", "-c", script], env=env,
                                            capture_output=True, text=True)
                    if expected is None:
                        self.assertNotEqual(result.returncode, 0)
                        self.assertEqual(result.stdout, "")
                    else:
                        self.assertEqual(result.returncode, 0, result.stderr)
                        self.assertIn(expected, result.stdout.splitlines())
                        if is_pr == "true":
                            self.assertIn("commit", result.stdout.splitlines())
                            self.assertNotIn("--report-only", result.stdout)

    def test_workflow_builds_the_immutable_target_of_the_ci_merge(self):
        script = self.workflow_script("Build the PR comparison commit")
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            repo = root / "repo"
            repo.mkdir()
            env = dict(os.environ, GIT_AUTHOR_NAME="Fixture", GIT_AUTHOR_EMAIL="fixture@example.invalid",
                       GIT_COMMITTER_NAME="Fixture", GIT_COMMITTER_EMAIL="fixture@example.invalid",
                       GIT_CONFIG_NOSYSTEM="1", GIT_CONFIG_GLOBAL=os.devnull)
            def git(*args):
                return subprocess.check_output(["git", *args], cwd=repo, env=env,
                                               stderr=subprocess.DEVNULL, text=True).strip()
            git("init", "-b", "target")
            git("commit", "--allow-empty", "-m", "root")
            git("switch", "-c", "topic")
            git("commit", "--allow-empty", "-m", "PR")
            git("switch", "target")
            git("commit", "--allow-empty", "-m", "target advanced")
            target = git("rev-parse", "HEAD")
            git("merge", "--no-ff", "topic", "-m", "CI merge")
            bin_dir = root / "bin"
            bin_dir.mkdir()
            docker = bin_dir / "docker"
            docker.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$RUNNER_TEMP/docker-args"\n')
            docker.chmod(0o755)
            env.update(PATH=str(bin_dir) + os.pathsep + os.environ["PATH"],
                       RUNNER_TEMP=str(root), GITHUB_OUTPUT=str(root / "output"),
                       PR_BASE_SHA=target, RETRO68_IMAGE="fixture-pinned-image")
            result = subprocess.run(["bash", "-eu", "-c", script], cwd=repo, env=env,
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((root / "output").read_text().strip(), "commit=" + target)
            self.assertEqual(git("-C", str(root / "retro68-size-base"), "rev-parse", "HEAD"), target)
            docker_args = (root / "docker-args").read_text()
            self.assertIn("fixture-pinned-image", docker_args)
            self.assertIn("INTERFACES=multiversal", docker_args)
            self.assertIn("retro68-68k-release", docker_args)
            self.assertNotIn("retro68-ppc-release", docker_args)

    def comparison_fixture(self, root, base_code=100, head_code=228, allowance=128):
        relative = "example/Fixture68K.bin"
        for folder, count in (("base", base_code), ("head", head_code)):
            artifact = root / folder / relative
            artifact.parent.mkdir(parents=True)
            artifact.write_bytes(make_macbinary(
                [("CODE", b"x" * count), ("DATA", b"data"), ("RELA", b"rela")]
            ))
        # The bank is far behind both builds: the old cumulative gate fails.
        bank = root / "bank.json"
        bank.write_text(json.dumps(baseline_for(relative, 1, 1, 1, 1, allowance)))
        return [sys.executable, str(REPORT_TOOL), str(root / "head"),
                "--baseline", str(bank)]

    def run_cli(self, args):
        return subprocess.run(args, capture_output=True, text=True, check=False)

    def test_stacked_growth_is_charged_only_to_the_measured_base(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            args = self.comparison_fixture(root)
            self.assertEqual(self.run_cli(args).returncode, 1)
            result = self.run_cli(args + ["--compare-build-root", str(root / "base"),
                                          "--comparison-ref", "fixture-base-commit"])
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("fixture-base-commit", result.stdout)
            self.assertIn("+128", result.stdout)
            self.assertNotIn("REGRESSION", result.stdout)

    def test_material_growth_against_base_still_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            args = self.comparison_fixture(root, head_code=356)
            result = self.run_cli(args + ["--compare-build-root", str(root / "base"),
                                          "--comparison-ref", "base"])
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn("Fixture68K: +256 bytes", result.stderr)

    def test_missing_or_corrupt_base_never_falls_back_to_the_bank(self):
        for corrupt in (False, True):
            with self.subTest(corrupt=corrupt), tempfile.TemporaryDirectory() as directory:
                root = pathlib.Path(directory)
                args = self.comparison_fixture(root)
                artifact = root / "base/example/Fixture68K.bin"
                if corrupt:
                    artifact.write_bytes(b"broken")
                else:
                    artifact.unlink()
                result = self.run_cli(args + ["--compare-build-root", str(root / "base"),
                                              "--comparison-ref", "base"])
                self.assertEqual(result.returncode, 2)
                self.assertIn("base/example/Fixture68K.bin", result.stderr)

    def test_report_only_ignores_growth_but_not_missing_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            args = self.comparison_fixture(root) + ["--report-only"]
            result = self.run_cli(args)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("Informational bank comparison", result.stdout)
            (root / "head/example/Fixture68K.bin").unlink()
            self.assertEqual(self.run_cli(args).returncode, 2)

    def test_comparison_requires_identity_and_cannot_be_report_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            args = self.comparison_fixture(root)
            for options in (["--compare-build-root", str(root / "base")],
                            ["--comparison-ref", "base"],
                            ["--compare-build-root", str(root / "base"),
                             "--comparison-ref", "base", "--report-only"]):
                with self.subTest(options=options):
                    self.assertEqual(self.run_cli(args + options).returncode, 2)

    def test_sums_multiple_resources_of_the_same_type(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            artifact = pathlib.Path(directory) / "Fixture68K.bin"
            artifact.write_bytes(
                make_macbinary(
                    [
                        ("CODE", b"abc"),
                        ("DATA", b"data"),
                        ("CODE", b"defgh"),
                        ("RELA", b"rr"),
                        ("SIZE", b"ignored"),
                    ]
                )
            )
            self.assertEqual(
                tool.resource_payload_sizes(artifact),
                {"CODE": 8, "DATA": 4, "RELA": 2},
            )

    def test_missing_required_resource_type_is_refused(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            artifact = pathlib.Path(directory) / "Fixture68K.bin"
            artifact.write_bytes(
                make_macbinary([("CODE", b"code"), ("DATA", b"data")])
            )
            with self.assertRaises(tool.SizeReportError) as caught:
                tool.resource_payload_sizes(artifact)
            self.assertIn(
                "required resource type(s) missing: RELA", str(caught.exception)
            )

    def test_truncated_resource_payload_is_refused(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            artifact = pathlib.Path(directory) / "Fixture68K.bin"
            encoded = make_macbinary(
                [("CODE", b"code"), ("DATA", b"data"), ("RELA", b"rela")]
            )
            artifact.write_bytes(encoded[:-200])
            with self.assertRaises(tool.SizeReportError) as caught:
                tool.resource_payload_sizes(artifact)
            self.assertIn(
                "resource fork is missing or truncated", str(caught.exception)
            )

    def test_cli_reports_component_deltas_and_fails_only_material_total_growth(self):
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            root = pathlib.Path(directory)
            relative = "example/Fixture68K.bin"
            artifact = root / relative
            artifact.parent.mkdir(parents=True)
            artifact.write_bytes(
                make_macbinary(
                    [("CODE", b"code"), ("DATA", b"data"), ("RELA", b"rela")]
                )
            )
            total = artifact.stat().st_size
            baseline_path = root / "baseline.json"
            baseline_path.write_text(
                json.dumps(baseline_for(relative, total - 128, 3, 5, 4, allowance=128)),
                encoding="utf-8",
            )
            accepted = subprocess.run(
                [
                    sys.executable,
                    str(REPORT_TOOL),
                    "--baseline",
                    str(baseline_path),
                    str(root),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(accepted.returncode, 0, accepted.stderr)
            self.assertIn("+128", accepted.stdout)
            self.assertIn("+1", accepted.stdout)
            self.assertIn("ok", accepted.stdout)

            baseline_path.write_text(
                json.dumps(baseline_for(relative, total - 128, 3, 5, 4, allowance=127)),
                encoding="utf-8",
            )
            rejected = subprocess.run(
                [
                    sys.executable,
                    str(REPORT_TOOL),
                    "--baseline",
                    str(baseline_path),
                    str(root),
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )
            self.assertEqual(rejected.returncode, 1)
            self.assertIn("REGRESSION", rejected.stdout)
            self.assertIn("Fixture68K: +128 bytes", rejected.stderr)

    def test_baseline_path_cannot_escape_the_build_root(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            baseline_path = pathlib.Path(directory) / "baseline.json"
            baseline_path.write_text(
                json.dumps(baseline_for("../outside.bin", 1, 1, 1, 1)),
                encoding="utf-8",
            )
            with self.assertRaises(tool.SizeReportError) as caught:
                tool.load_baseline(baseline_path)
            self.assertIn("must stay under the build root", str(caught.exception))

    def test_non_object_baseline_is_refused_cleanly(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            baseline_path = pathlib.Path(directory) / "baseline.json"
            baseline_path.write_text("[]", encoding="utf-8")
            with self.assertRaises(tool.SizeReportError) as caught:
                tool.load_baseline(baseline_path)
            self.assertIn("baseline must be an object", str(caught.exception))

    def test_unbaselined_final_artifact_is_refused(self):
        tool = load_report_tool()
        with tempfile.TemporaryDirectory(prefix="retro68-size-") as directory:
            root = pathlib.Path(directory)
            relative = "example/Fixture/Fixture68K.bin"
            artifact = root / relative
            artifact.parent.mkdir(parents=True)
            encoded = make_macbinary(
                [("CODE", b"code"), ("DATA", b"data"), ("RELA", b"rela")]
            )
            artifact.write_bytes(encoded)
            extra = root / "example" / "NewExample" / "NewExample68K.bin"
            extra.parent.mkdir(parents=True)
            extra.write_bytes(encoded)
            baseline = baseline_for(relative, artifact.stat().st_size, 4, 4, 4)
            with self.assertRaises(tool.SizeReportError) as caught:
                tool.report(root, baseline)
            self.assertIn(
                "unbaselined final 68K artifact(s): "
                "example/NewExample/NewExample68K.bin",
                str(caught.exception),
            )


if __name__ == "__main__":
    unittest.main()
