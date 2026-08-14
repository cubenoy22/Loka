#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG_SCRIPTS = ROOT / "scripts" / "rig"
sys.dont_write_bytecode = True
sys.path.insert(0, str(RIG_SCRIPTS))

import loka_rig_common as common
from toolbox import loka_toolbox_rig as toolbox


def load_cli():
    path = RIG_SCRIPTS / "loka-rig.py"
    spec = importlib.util.spec_from_file_location("loka_rig_cli_test", path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class FakeAdapter:
    def __init__(self, root, fail_at=None):
        self.progress = common.RunProgress()
        self.archive = pathlib.Path(root) / "archive"
        self.archive.mkdir()
        self.fail_at = fail_at
        self.events = []

    def _event(self, name):
        self.events.append(name)
        if self.fail_at == name:
            raise common.RigError(name, f"{name} refused")

    def prepare(self):
        self._event("prepare")

    def build(self):
        self._event("build")

    def run_runtime(self):
        self._event("runtime")

    def collect(self):
        self._event("collect")

    def best_effort_collect(self):
        self.events.append("best-effort-collect")

    def note_failure(self):
        self.events.append("note-failure")

    def finalize_manifest(self, result):
        self._event(f"manifest-{result}")

    def cleanup_success(self):
        self._event("cleanup")


class LokaRigCommonTest(unittest.TestCase):
    def test_common_module_owns_the_rig_repository_root(self):
        self.assertEqual(common.REPOSITORY_ROOT, ROOT)

    def test_common_executor_owns_the_success_sequence(self):
        with tempfile.TemporaryDirectory() as directory:
            adapter = FakeAdapter(directory)
            archive = common.execute_adapter(adapter)
            self.assertEqual(archive, adapter.archive)
            self.assertEqual(
                adapter.events,
                ["prepare", "build", "runtime", "collect", "manifest-passed", "cleanup", "manifest-passed"],
            )
            self.assertTrue(adapter.progress.build_passed)
            self.assertTrue(adapter.progress.runtime_passed)
            self.assertTrue(adapter.progress.presentation_collected)
            self.assertTrue(adapter.progress.manifest_finalized)

    def test_runtime_pass_survives_a_later_presentation_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            adapter = FakeAdapter(directory, fail_at="collect")
            with self.assertRaises(common.RigError) as caught:
                common.execute_adapter(adapter)
            self.assertEqual(caught.exception.stage, "collect")
            self.assertEqual(
                adapter.events,
                [
                    "prepare",
                    "build",
                    "runtime",
                    "collect",
                    "note-failure",
                    "best-effort-collect",
                    "manifest-failed",
                ],
            )
            result = common.RunResult.from_progress(
                adapter="fake",
                rig_id="fake-rig",
                requested_ref="topic",
                commit_sha="a" * 40,
                mode="flow",
                result="failed",
                progress=adapter.progress,
                recording_status="not-requested",
                target_retained="1",
                target_workdir="/tmp/fake-run",
                next_diagnostic_command="inspect fake run",
            )
            fields = dict(result.manifest_fields())
            self.assertEqual(fields["result"], "failed")
            self.assertEqual(fields["runtime_verification"], "passed")
            self.assertEqual(fields["machine_verdict"], "passed")
            self.assertEqual(fields["presentation_status"], "failed-or-not-reached")

    def test_cleanup_failure_rewrites_result_without_erasing_completed_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            adapter = FakeAdapter(directory, fail_at="cleanup")
            with self.assertRaises(common.RigError) as caught:
                common.execute_adapter(adapter)
            self.assertEqual(caught.exception.stage, "cleanup")
            self.assertEqual(
                adapter.events,
                [
                    "prepare",
                    "build",
                    "runtime",
                    "collect",
                    "manifest-passed",
                    "cleanup",
                    "note-failure",
                    "manifest-failed",
                ],
            )
            result = common.RunResult.from_progress(
                adapter="fake",
                rig_id="fake-rig",
                requested_ref="topic",
                commit_sha="a" * 40,
                mode="flow",
                result="failed",
                progress=adapter.progress,
                recording_status="manual",
                target_retained="1",
                target_workdir="/tmp/fake-run",
                next_diagnostic_command="inspect fake run",
            )
            fields = dict(result.manifest_fields())
            self.assertEqual(fields["result"], "failed")
            self.assertEqual(fields["machine_verdict"], "passed")
            self.assertEqual(fields["presentation_status"], "collected")

    def test_manifest_failure_prevents_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            adapter = FakeAdapter(directory, fail_at="manifest-passed")
            with self.assertRaises(common.RigError) as caught:
                common.execute_adapter(adapter)
            self.assertEqual(caught.exception.stage, "manifest")
            self.assertEqual(
                adapter.events,
                [
                    "prepare",
                    "build",
                    "runtime",
                    "collect",
                    "manifest-passed",
                    "note-failure",
                ],
            )
            self.assertNotIn("cleanup", adapter.events)

    def test_manifest_hashes_exclude_both_committed_and_temporary_manifests(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "capture.png").write_bytes(b"pixels")
            (root / "run-manifest.txt").write_text("old", encoding="utf-8")
            (root / "run-manifest.txt.tmp").write_text("partial", encoding="utf-8")
            self.assertEqual([path for path, _ in common.artifact_hashes(root)], ["capture.png"])

    def test_manifest_refuses_adapter_field_overlap_and_symlink_artifacts(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            progress = common.RunProgress()
            progress.finish()
            result = common.RunResult.from_progress(
                adapter="fake",
                rig_id="fake-rig",
                requested_ref="HEAD",
                commit_sha="a" * 40,
                mode="flow",
                result="failed",
                progress=progress,
                recording_status="not-requested",
                target_retained="not-created",
                target_workdir="not-created",
                next_diagnostic_command="inspect",
            )
            with self.assertRaises(common.RigError) as duplicate:
                common.write_manifest(root, result, (("rig_id", "other"),))
            self.assertEqual(duplicate.exception.stage, "manifest")
            outside = root / "ordinary.log"
            outside.write_bytes(b"outside")
            (root / "linked.log").symlink_to(outside)
            with self.assertRaises(common.RigError) as symlink:
                common.write_manifest(root, result, ())
            self.assertEqual(symlink.exception.stage, "manifest")


class LokaRigCliTest(unittest.TestCase):
    def test_dispatcher_finds_exactly_one_adapter_descriptor(self):
        cli = load_cli()
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            macos = root / "scripts" / "rig" / "macos" / "rigs"
            toolbox = root / "scripts" / "rig" / "toolbox" / "rigs"
            macos.mkdir(parents=True)
            toolbox.mkdir(parents=True)
            (toolbox / "toolbox-maciix.ini").write_text("[rig]\n", encoding="utf-8")
            self.assertEqual(cli.find_adapter(root, "toolbox-maciix").name, "toolbox")
            with self.assertRaises(common.RigError):
                cli.find_adapter(root, "missing")
            (macos / "toolbox-maciix.ini").write_text("[rig]\n", encoding="utf-8")
            with self.assertRaises(common.RigError):
                cli.find_adapter(root, "toolbox-maciix")

    def test_public_parser_contains_only_common_run_arguments(self):
        cli = load_cli()
        args = cli.parse_args(
            ("run", "toolbox-maciix", "--ref", "HEAD", "--mode", "flow")
        )
        self.assertEqual(args.rig_id, "toolbox-maciix")
        self.assertEqual(args.requested_ref, "HEAD")
        self.assertEqual(args.mode, "flow")
        self.assertIsNone(args.local_config)


class ToolboxRigAdapterTest(unittest.TestCase):
    def test_tracked_descriptor_and_local_mapping_stay_separate(self):
        descriptor = toolbox.load_descriptor(
            ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
        )
        self.assertEqual(descriptor.rig_id, "toolbox-maciix")
        self.assertEqual(descriptor.supported_modes, frozenset(("flow",)))
        self.assertTrue(descriptor.disposable_for_input)
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            config = root / "local.ini"
            config.write_text(
                "[local]\n"
                f"archive_root = {root}/archive\n"
                f"mame_env_file = {root}/mame.env\n"
                f"golden_root = {root}/golden\n",
                encoding="utf-8",
            )
            mapping = toolbox.load_local_mapping(config)
            self.assertEqual(mapping.archive_root, root / "archive")
            self.assertEqual(mapping.mame_env_file, root / "mame.env")
            self.assertEqual(mapping.golden_root, root / "golden")

    def test_missing_mame_machine_uses_the_presentation_rail_default(self):
        descriptor = toolbox.load_descriptor(
            ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
        )
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            environment = root / "mame.env"
            environment.write_text("MAME_EXECUTABLE=/bin/false\n", encoding="utf-8")
            mapping = toolbox.LocalMapping(root / "archive", environment, root / "golden")
            run = toolbox.ToolboxRigRun(ROOT, descriptor, mapping, "HEAD", "flow")

            self.assertEqual(run._configured_machine(), "maciix")

    def test_golden_staging_uses_the_checkout_registry_and_fails_on_absence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            checkout = root / "checkout"
            registry = checkout / "tests" / "toolbox" / "scenarios.txt"
            registry.parent.mkdir(parents=True)
            registry.write_text("first alpha\nsecond beta\n", encoding="utf-8")
            golden = root / "golden"
            (golden / "first").mkdir(parents=True)
            (golden / "second").mkdir()
            (golden / "first" / "alpha.png").write_bytes(b"alpha")
            with self.assertRaises(common.RigError) as caught:
                toolbox.stage_goldens(checkout, golden)
            self.assertEqual(caught.exception.stage, "golden-preflight")
            (golden / "second" / "beta.png").write_bytes(b"beta")
            staged = toolbox.stage_goldens(checkout, golden)
            self.assertEqual(staged, (("first", "alpha"), ("second", "beta")))
            self.assertEqual(
                (
                    checkout
                    / "build"
                    / "mame-scenario"
                    / "golden"
                    / "second"
                    / "beta.png"
                ).read_bytes(),
                b"beta",
            )


if __name__ == "__main__":
    unittest.main()
