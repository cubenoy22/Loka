#!/usr/bin/env python3

import importlib.util
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import types
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
RIG_SCRIPTS = ROOT / "scripts" / "rig"
sys.dont_write_bytecode = True
sys.path.insert(0, str(RIG_SCRIPTS))

import loka_rig_common as common
import capture_profile_guard
import golden_identity_guard
import package_fixture_guard
from macos import loka_macos_rig as macos
from toolbox import loka_toolbox_rig as toolbox
from toolbox import classic_golden_identity as golden_identity


# Every field tests/macos/ScenarioDriverSupport.mm can put in a capture
# profile. A descriptor may declare any subset; it may not declare a field the
# capture never carries, which the guard would refuse on every run.
MACOS_PROFILE_FIELDS = {
    "profile_version",
    "os_build",
    "arch",
    "scale_percent_available",
    "scale_percent",
    "depth_available",
    "depth",
    "appearance_available",
    "appearance",
    "capture_api",
    "pixel_width",
    "pixel_height",
}


def make_toolbox_golden_bundle(root, checkout, scenarios):
    registry = checkout / "tests" / "scenarios" / "scenarios.txt"
    registry.parent.mkdir(parents=True, exist_ok=True)
    registry.write_text(
        "".join(f"{example} {scenario}\n" for example, scenario in scenarios),
        encoding="utf-8",
    )
    declarations = checkout / "tests" / "scenarios" / "startup-golden-identities.txt"
    declarations.write_text("", encoding="utf-8")
    descriptor = checkout / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
    descriptor.parent.mkdir(parents=True, exist_ok=True)
    descriptor.write_text(
        (ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini").read_text(
            encoding="utf-8"
        ),
        encoding="utf-8",
    )
    identity = {
        "gcc_version": "fake-gcc",
        "universal_interfaces_version": "0x0340",
        "retro68_identity_kind": "toolchain-content-sha256",
        "retro68_identity": "a" * 64,
        "mame_executable_sha256": "b" * 64,
        "mame_rom_identity_kind": "verified-rom-inventory-sha256",
        "mame_rom_identity": "c" * 64,
        "ram_size": "8M",
        "machine": "maciix",
        "capture_adapter": "mame-screen-snapshot.v1",
        "boot_hd_sha256": "d" * 64,
    }
    current = root / "current-identity.txt"
    current.write_text(
        "identity_version=1\n"
        + "".join(f"{key}={identity[key]}\n" for key in golden_identity.IDENTITY_FIELDS)
        + f"identity_sha256={golden_identity.identity_sha256(identity)}\n",
        encoding="utf-8",
    )
    bundle = root / "golden"
    capture = root / "capture.png"
    capture.write_bytes(b"pixels")
    application = root / "application.bin"
    application.write_bytes(b"application")
    for example, scenario in scenarios:
        golden_identity.stage_capture(
            types.SimpleNamespace(
                bundle=bundle,
                registry=registry,
                declarations=declarations,
                descriptor=descriptor,
                current_identity=current,
                capture=capture,
                application=application,
                source_tree=ROOT,
                example=example,
                scenario=scenario,
            )
        )
    approved = golden_identity.identity_sha256(identity)
    # Rewrite the line rather than a known spelling of its value: the tracked
    # descriptor now carries a real digest, and a substring replace against
    # "unapproved" would silently no-op and leave the fixture pointing at the
    # production identity.
    replaced, count = re.subn(
        r"^reference_identity_sha256 = .*$",
        f"reference_identity_sha256 = {approved}",
        descriptor.read_text(encoding="utf-8"),
        flags=re.MULTILINE,
    )
    assert count == 1, f"expected one reference_identity_sha256 line, found {count}"
    descriptor.write_text(replaced, encoding="utf-8")
    return bundle


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
        # Pinned to the exact reviewed value, not merely "looks like a digest":
        # this authority is what makes a bundle verdict-eligible, so changing it
        # has to change a test and surface in review.
        self.assertEqual(
            descriptor.reference_identity_sha256,
            "080c51416bcac65bb9e621709e8666dbf06fef4acf9930b94ba4caf4e929b15d",
        )
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

    def test_toolbox_manifest_preserves_reference_refusal(self):
        descriptor = toolbox.load_descriptor(
            ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
        )
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            environment = root / "mame.env"
            environment.write_text("MAME_EXECUTABLE=/bin/false\n", encoding="utf-8")
            mapping = toolbox.LocalMapping(root / "archive-root", environment, root / "golden")
            run = toolbox.ToolboxRigRun(ROOT, descriptor, mapping, "HEAD", "flow")
            run.archive = root / "archive"
            run.archive.mkdir()
            run.checkout = root / "checkout"
            refusal = run.archive / "presentation.incomplete" / "machine-verdict.txt"
            refusal.parent.mkdir()
            refusal.write_text(
                "machine_verdict=refused\n"
                "runtime_verification=passed\n"
                "refusal_reason=identity mismatch\n",
                encoding="utf-8",
            )
            run.progress.fail("runtime", "presentation rail refused the reference")
            run.progress.finish()
            run.finalize_manifest("failed")
            manifest = (run.archive / "run-manifest.txt").read_text(encoding="utf-8")
            self.assertIn("machine_verdict=refused\n", manifest)
            self.assertIn("runtime_verification=passed\n", manifest)

    def test_toolbox_manifest_does_not_promote_preflight_marker_to_refused(self):
        descriptor = toolbox.load_descriptor(
            ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
        )
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            environment = root / "mame.env"
            environment.write_text("MAME_EXECUTABLE=/missing\n", encoding="utf-8")
            mapping = toolbox.LocalMapping(root / "archive-root", environment, root / "golden")
            run = toolbox.ToolboxRigRun(ROOT, descriptor, mapping, "HEAD", "flow")
            run.archive = root / "archive"
            run.archive.mkdir()
            run.checkout = root / "checkout"
            verdict = run.archive / "presentation.incomplete" / "machine-verdict.txt"
            verdict.parent.mkdir()
            verdict.write_text(
                # Pin the adapter's defensive read of an old/malformed marker:
                # refused is invalid unless runtime verification passed.
                "machine_verdict=refused\n"
                "runtime_verification=failed-or-not-reached\n"
                "refusal_reason=missing provenance\n",
                encoding="utf-8",
            )
            run.progress.fail("runtime", "presentation rail failed before launch")
            run.progress.finish()
            run.finalize_manifest("failed")
            manifest = (run.archive / "run-manifest.txt").read_text(encoding="utf-8")
            self.assertIn("machine_verdict=failed-or-not-reached\n", manifest)
            self.assertIn("runtime_verification=failed-or-not-reached\n", manifest)
            self.assertNotIn("machine_verdict=refused\n", manifest)

    def test_golden_staging_uses_the_checkout_registry_and_fails_on_absence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            checkout = root / "checkout"
            golden = make_toolbox_golden_bundle(
                root, checkout, (("first", "startup"), ("second", "startup"))
            )
            missing = golden / "second" / "startup.png"
            missing.unlink()
            with self.assertRaises(common.RigError) as caught:
                toolbox.stage_goldens(checkout, golden)
            self.assertEqual(caught.exception.stage, "golden-preflight")
            missing.write_bytes(b"pixels")
            staged = toolbox.stage_goldens(checkout, golden)
            self.assertEqual(staged, (("first", "startup"), ("second", "startup")))
            self.assertEqual(
                (
                    checkout
                    / "build"
                    / "mame-scenario"
                    / "golden"
                    / "second"
                    / "startup.png"
                ).read_bytes(),
                b"pixels",
            )

    def test_golden_staging_refuses_legacy_sidecars_and_copies_one_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            checkout = root / "checkout"
            registry = checkout / "tests" / "scenarios" / "scenarios.txt"
            registry.parent.mkdir(parents=True)
            registry.write_text("first alpha\n", encoding="utf-8")
            descriptor = checkout / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
            descriptor.parent.mkdir(parents=True)
            shutil.copy2(
                ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini",
                descriptor,
            )
            golden = root / "golden" / "first" / "alpha.png"
            golden.parent.mkdir(parents=True)
            golden.write_bytes(b"alpha")
            golden.with_name("alpha.png.mame-machine").write_text("maciix\n", encoding="utf-8")

            with self.assertRaises(common.RigError) as caught:
                toolbox.stage_goldens(checkout, root / "golden")
            self.assertEqual(caught.exception.stage, "golden-preflight")
            self.assertIn("legacy PNG/.mame-machine baseline", str(caught.exception))
            self.assertIn("re-bake", str(caught.exception))

            shutil.rmtree(root / "golden")
            bundle = make_toolbox_golden_bundle(root, checkout, (("first", "startup"),))
            toolbox.stage_goldens(checkout, bundle)
            staged_root = checkout / "build" / "mame-scenario" / "golden"
            self.assertTrue((staged_root / "manifest.txt").is_file())
            self.assertEqual(
                len(list(staged_root.rglob("manifest.txt"))),
                1,
            )


class PackageFixtureGuardTest(unittest.TestCase):
    def test_missing_registry_refuses_as_package_fixture_error(self):
        registry = pathlib.Path("/definitely/missing/package-fixtures.txt")
        with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
            package_fixture_guard.read_fixtures(registry)
        self.assertIn("cannot read package fixture registry", str(caught.exception))
        self.assertIn(str(registry), str(caught.exception))

    def test_malformed_line_refuses_and_names_the_line(self):
        with tempfile.TemporaryDirectory() as directory:
            registry = pathlib.Path(directory) / "fixtures.txt"
            registry.write_text(
                "first corrupt-bag=1\nmalformed fixture row\n", encoding="utf-8"
            )
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.read_fixtures(registry)
            self.assertIn(f"{registry}:2:", str(caught.exception))
            self.assertIn("invalid package fixture registry entry", str(caught.exception))

    def test_duplicate_scenario_refuses_and_names_the_line(self):
        with tempfile.TemporaryDirectory() as directory:
            registry = pathlib.Path(directory) / "fixtures.txt"
            registry.write_text(
                "first corrupt-bag=1\nfirst corrupt-bag=2\n", encoding="utf-8"
            )
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.read_fixtures(registry)
            self.assertIn(f"{registry}:2:", str(caught.exception))
            self.assertIn("duplicate package fixture scenario 'first'", str(caught.exception))

    def test_unregistered_scenario_has_no_corrupt_bag(self):
        fixtures = (("first", 1), ("second", 2))
        self.assertIsNone(
            package_fixture_guard.corrupt_bag_for(fixtures, "unregistered")
        )

    def test_missing_staged_fixture_refuses_with_staging_diagnosis(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.verify_staged(
                    source, staged, 3, "refused-cell"
                )
            self.assertEqual(
                str(caught.exception),
                f"scenario 'refused-cell' declares corrupt-bag=3 but {staged} does not exist; "
                "this rail did not stage the fixture",
            )

    def test_missing_staged_package_refuses_with_staging_diagnosis(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.verify_staged(
                    source, staged, None, "ordinary-cell"
                )
            self.assertEqual(
                str(caught.exception),
                f"scenario 'ordinary-cell' declares no package fixture but {staged} does not exist; "
                "this rail did not stage the package",
            )

    def test_declared_corruption_refuses_byte_identical_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            staged.write_bytes(b"package")
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.verify_staged(
                    source, staged, 5, "refused-cell"
                )
            self.assertEqual(
                str(caught.exception),
                f"scenario 'refused-cell' declares corrupt-bag=5 but {staged} is byte-identical "
                f"to {source}; this rail did not stage the fixture",
            )

    def test_unreadable_source_refuses_as_package_fixture_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "missing-source.lrp"
            staged = root / "staged.lrp"
            staged.write_bytes(b"package")
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.verify_staged(source, staged, None)
            self.assertIn("cannot compare staged package", str(caught.exception))
            self.assertIn(str(source), str(caught.exception))

    def test_undeclared_corruption_refuses_different_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            staged.write_bytes(b"changed package")
            with self.assertRaises(package_fixture_guard.PackageFixtureError) as caught:
                package_fixture_guard.verify_staged(
                    source, staged, None, "ordinary-cell"
                )
            self.assertEqual(
                str(caught.exception),
                f"scenario 'ordinary-cell' declares no package fixture but {staged} differs from {source}",
            )

    def test_declared_corruption_accepts_different_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            staged.write_bytes(b"changed package")
            package_fixture_guard.verify_staged(source, staged, 1)

    def test_no_fixture_accepts_byte_identical_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / "source.lrp"
            staged = root / "staged.lrp"
            source.write_bytes(b"package")
            staged.write_bytes(b"package")
            package_fixture_guard.verify_staged(source, staged, None)

    def test_win32_rail_verifies_the_staged_package_after_lrpc(self):
        runner = (ROOT / "tests" / "win32" / "run-scenario.ps1").read_text(
            encoding="utf-8"
        )
        plan_call = runner.index("$corruptBag = ((Invoke-PackageFixturePlan)")
        lrpc_call = runner.index("& $Lrpc @stageArguments")
        verify_call = runner.index("Invoke-PackageFixtureVerify $StagedAssets")
        self.assertLess(plan_call, lrpc_call)
        self.assertLess(lrpc_call, verify_call)
        # Ordering alone would still hold with both refusals deleted, which is
        # the whole behaviour. Pin that each answer is acted on, and that the
        # plan answer's shape is checked before it becomes an lrpc argument.
        plan_refusal = runner.index(
            'Fail-Stage "stage" $corruptBag'
        )
        shape_check = runner.index("$corruptBag -notmatch '^[0-9]+$'")
        verify_refusal = runner.index(
            'Fail-Stage "stage" $fixtureMessage'
        )
        self.assertLess(plan_call, plan_refusal)
        self.assertLess(plan_refusal, shape_check)
        self.assertLess(shape_check, lrpc_call)
        self.assertLess(verify_call, verify_refusal)
        self.assertIn(
            'Convert-ToWslPath $PackageFixtureGuard "stage"', runner
        )
        self.assertIn('Convert-ToWslPath $FixtureRegistry "stage"', runner)
        self.assertIn('Convert-ToWslPath $SourceAssets "stage"', runner)
        self.assertIn('Convert-ToWslPath $StagedAssets "stage"', runner)


class GoldenIdentityGuardTest(unittest.TestCase):
    def make_contract(self, root):
        registry = root / "scenarios.txt"
        registry.write_text(
            "example startup\nexample interaction\n", encoding="utf-8"
        )
        declarations = root / "startup-golden-identities.txt"
        declarations.write_text("", encoding="utf-8")
        goldens = root / "golden"
        startup = goldens / "example" / "startup.png"
        startup.parent.mkdir(parents=True)
        startup.write_bytes(b"settled startup bytes")
        return registry, declarations, goldens

    def test_undeclared_identity_refuses_at_record_time(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            capture = root / "capture.png"
            capture.write_bytes(b"settled startup bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("byte-identical", str(caught.exception))
            self.assertIn("not declared", str(caught.exception))

    def test_declared_identity_passes_at_record_time(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            declarations.write_text(
                "example interaction the interaction settles where startup does\n",
                encoding="utf-8",
            )
            capture = root / "capture.png"
            capture.write_bytes(b"settled startup bytes")
            golden_identity_guard.verify_candidate_recording(
                registry,
                declarations,
                goldens,
                capture,
                "example",
                "interaction",
            )

    def test_stale_declaration_refuses_at_record_time(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            declarations.write_text(
                "example interaction the interaction settles where startup does\n",
                encoding="utf-8",
            )
            capture = root / "capture.png"
            capture.write_bytes(b"different settled bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("stale startup-identity declaration", str(caught.exception))

    def test_missing_startup_refuses_instead_of_passing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            (goldens / "example" / "startup.png").unlink()
            capture = root / "capture.png"
            capture.write_bytes(b"settled bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("record example startup first", str(caught.exception))

    def test_re_recording_startup_checks_existing_declared_siblings(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            declarations.write_text(
                "example interaction the interaction settles where startup does\n",
                encoding="utf-8",
            )
            sibling = goldens / "example" / "interaction.png"
            sibling.write_bytes(b"settled startup bytes")
            replacement_startup = root / "replacement-startup.png"
            replacement_startup.write_bytes(b"changed startup bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    replacement_startup,
                    "example",
                    "startup",
                )
            self.assertIn("stale startup-identity declaration", str(caught.exception))

    def test_declaration_without_a_reason_refuses_the_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            declarations.write_text("example interaction\n", encoding="utf-8")
            capture = root / "capture.png"
            capture.write_bytes(b"settled startup bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("states no reason", str(caught.exception))

    def test_declaration_reason_is_read_back_with_its_cell(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            _, declarations, _ = self.make_contract(root)
            declarations.write_text(
                "example interaction  the interaction settles where startup does  \n",
                encoding="utf-8",
            )
            self.assertEqual(
                golden_identity_guard.read_declarations(declarations),
                ((("example", "interaction"), "the interaction settles where startup does"),),
            )

    def test_registry_still_refuses_a_trailing_reason(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            registry.write_text(
                "example startup\nexample interaction because we felt like it\n",
                encoding="utf-8",
            )
            capture = root / "capture.png"
            capture.write_bytes(b"settled startup bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("invalid scenario registry entry", str(caught.exception))

    def test_unregistered_declaration_refuses_the_contract(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            registry, declarations, goldens = self.make_contract(root)
            declarations.write_text(
                "example absent this cell is not in the registry\n",
                encoding="utf-8",
            )
            capture = root / "capture.png"
            capture.write_bytes(b"different settled bytes")
            with self.assertRaises(golden_identity_guard.GoldenIdentityError) as caught:
                golden_identity_guard.verify_candidate_recording(
                    registry,
                    declarations,
                    goldens,
                    capture,
                    "example",
                    "interaction",
                )
            self.assertIn("declaration is not registered", str(caught.exception))



class CaptureProfileGuardTest(unittest.TestCase):
    DECLARED = (
        "[rig]\n"
        "rig_id = fake\n"
        "\n"
        "[capture]\n"
        "appearance = light\n"
        "scale_percent = 100\n"
    )
    REPORTED = (
        "profile_version=2\n"
        "appearance=light\n"
        "scale_percent=100\n"
        "depth=32\n"
    )

    def make(self, root, declared=None, reported=None):
        descriptor = root / "fake-rig.ini"
        descriptor.write_text(self.DECLARED if declared is None else declared, encoding="utf-8")
        profile = root / "actual.profile"
        profile.write_text(self.REPORTED if reported is None else reported, encoding="utf-8")
        return descriptor, profile

    def test_module_is_loaded_from_the_shared_rig_directory(self):
        self.assertEqual(
            pathlib.Path(capture_profile_guard.__file__).resolve(),
            (RIG_SCRIPTS / "capture_profile_guard.py").resolve(),
        )

    def test_shared_cli_refusal_uses_rail_neutral_wording(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(
                pathlib.Path(directory),
                reported=self.REPORTED.replace("appearance=light", "appearance=dark"),
            )
            completed = subprocess.run(
                [
                    sys.executable,
                    str(RIG_SCRIPTS / "capture_profile_guard.py"),
                    "--descriptor",
                    str(descriptor),
                    "--profile",
                    str(profile),
                ],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 4)
            self.assertIn("Capture profile refused: ", completed.stderr)
            self.assertNotIn("Win32 capture profile refused", completed.stderr)

    def test_matching_environment_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(pathlib.Path(directory))
            capture_profile_guard.verify_capture_profile(descriptor, profile)

    def test_moved_field_refuses_and_names_both_values(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(
                pathlib.Path(directory),
                reported=self.REPORTED.replace("appearance=light", "appearance=dark"),
            )
            with self.assertRaises(capture_profile_guard.CaptureProfileError) as caught:
                capture_profile_guard.verify_capture_profile(descriptor, profile)
            self.assertIn("declares appearance=light", str(caught.exception))
            self.assertIn("reports appearance=dark", str(caught.exception))

    def test_absent_field_refuses_rather_than_passing(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(
                pathlib.Path(directory),
                reported="profile_version=2\nscale_percent=100\n",
            )
            with self.assertRaises(capture_profile_guard.CaptureProfileError) as caught:
                capture_profile_guard.verify_capture_profile(descriptor, profile)
            self.assertIn("reports no such field", str(caught.exception))

    def test_field_the_descriptor_omits_is_not_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(
                pathlib.Path(directory),
                reported=self.REPORTED.replace("depth=32", "depth=16"),
            )
            capture_profile_guard.verify_capture_profile(descriptor, profile)

    def test_descriptor_without_a_capture_section_refuses(self):
        with tempfile.TemporaryDirectory() as directory:
            descriptor, profile = self.make(
                pathlib.Path(directory), declared="[rig]\nrig_id = fake\n"
            )
            with self.assertRaises(capture_profile_guard.CaptureProfileError) as caught:
                capture_profile_guard.verify_capture_profile(descriptor, profile)
            self.assertIn("declares no [capture] section", str(caught.exception))

    def test_win32_example_descriptor_shows_the_shape_the_guard_reads(self):
        # No Win32 machine's descriptor is tracked: a rig is a fact about one
        # machine, the same as the golden it pins, and both now live beside the
        # operator. What ships is the example the refusal tells people to copy,
        # so the guard has to be able to read it -- a template the guard rejects
        # would send everyone who followed the refusal into a second refusal.
        descriptor = ROOT / "scripts" / "rig" / "win32" / "rigs" / "local.example.ini"
        declared = capture_profile_guard.read_declared_capture(descriptor)
        self.assertEqual(
            sorted(declared),
            ["appearance", "arch", "depth", "os_build", "scale_percent"],
        )

    def test_tahoe_declares_only_fields_the_macos_profile_carries(self):
        descriptor = ROOT / "scripts" / "rig" / "macos" / "rigs" / "tahoe.ini"
        declared = capture_profile_guard.read_declared_capture(descriptor)
        self.assertEqual(
            declared,
            {
                "profile_version": "2",
                "os_build": "25G76",
                "arch": "x86_64",
                "scale_percent": "200",
                "depth": "24",
                "appearance": "light",
                "capture_api": "NSView.cacheDisplayInRect.v1",
            },
        )
        self.assertLessEqual(set(declared), MACOS_PROFILE_FIELDS)

    def test_mavericks_declares_the_environment_its_rig_reported(self):
        """The Mavericks rig captured on 2026-08-22; these are its numbers.

        Until then this descriptor carried no [capture] section on purpose:
        any value would have been invented, and the guard would have passed a
        bake against a number nobody measured. The section replaces that
        absence with what the rig actually reported, so the values are a
        tracked edit rather than whatever the desktop happened to be.
        """
        descriptor = ROOT / "scripts" / "rig" / "macos" / "rigs" / "mavericks-10.9.ini"
        declared = capture_profile_guard.read_declared_capture(descriptor)
        self.assertEqual(
            declared,
            {
                "profile_version": "2",
                "os_build": "13F1911",
                "arch": "x86_64",
                "scale_percent": "100",
                "depth": "24",
                "appearance_available": "0",
                "capture_api": "NSView.cacheDisplayInRect.v1",
            },
        )
        self.assertLessEqual(set(declared), MACOS_PROFILE_FIELDS)

    def test_mavericks_declares_no_appearance_value(self):
        """10.9 predates the appearance API, so there is no value to pin.

        The vehicle reports appearance_available=0 and emits no appearance
        field at all, so declaring one would name something the capture never
        carries and the guard would refuse every run. The absence is what is
        declared instead, which is why a build that later gains appearance
        reporting is refused rather than silently compared against goldens
        baked before the desktop theme could reach the capture.
        """
        descriptor = ROOT / "scripts" / "rig" / "macos" / "rigs" / "mavericks-10.9.ini"
        declared = capture_profile_guard.read_declared_capture(descriptor)
        self.assertNotIn("appearance", declared)
        self.assertEqual(declared["appearance_available"], "0")


class RunnerRigScriptReferenceTest(unittest.TestCase):
    """Each rail names its guards by path, and no rail runner is executed here.

    The PowerShell runner cannot run on the headless suite's hosts at all, and
    the shell runners need their platform, so a path that stopped resolving
    would surface only on the rig it belongs to -- after someone travelled to
    it. Pinning the set also makes a rail acquiring or losing a guard a
    reviewable edit rather than a silent one.
    """

    REFERENCE_PATTERN = re.compile(r"scripts/rig/[A-Za-z0-9_./-]+\.py")
    EXPECTED = {
        "tests/win32/run-scenario.ps1": {
            "scripts/rig/capture_profile_guard.py",
            "scripts/rig/golden_identity_guard.py",
            "scripts/rig/package_fixture_guard.py",
        },
        "tests/macos/run-scenario.sh": {
            "scripts/rig/capture_profile_guard.py",
            "scripts/rig/golden_identity_guard.py",
            "scripts/rig/package_fixture_guard.py",
        },
        # The Toolbox rail declares no capture environment: a Classic capture
        # comes from an emulator or real hardware, and that environment has
        # never been recorded as a profile. It is the one rail this wall does
        # not yet cover.
        "tests/toolbox/run-scenario.sh": {
            "scripts/rig/package_fixture_guard.py",
            "scripts/rig/toolbox/classic_golden_identity.py",
        },
    }

    def test_every_runner_references_exactly_its_declared_rig_scripts(self):
        for runner, expected in self.EXPECTED.items():
            with self.subTest(runner=runner):
                text = (ROOT / runner).read_text(encoding="utf-8")
                self.assertEqual(set(self.REFERENCE_PATTERN.findall(text)), expected)

    def test_every_referenced_rig_script_exists(self):
        for runner, expected in self.EXPECTED.items():
            for relative in sorted(expected):
                with self.subTest(runner=runner, script=relative):
                    self.assertTrue(
                        (ROOT / relative).is_file(),
                        f"{runner} names {relative}, which is not in the tree",
                    )


def drive_macos_adapter_build(root):
    """Record the commands the macOS adapter's build would issue on its rig."""
    descriptor = macos.load_descriptor(
        ROOT / "scripts" / "rig" / "macos" / "rigs" / "mavericks-10.9.ini"
    )
    mapping = macos.LocalMapping(
        vm_host_ssh="vm-host",
        vm_name="VM",
        target_host="auto",
        target_user="user",
        target_identity_file=root / "key",
        target_proxy_ssh="vm-host",
        target_legacy_rsa=True,
        target_root=pathlib.PurePosixPath("/target"),
        archive_root=root / "archive",
        vm_snapshot="",
        target_python="/usr/bin/python3",
    )
    run = macos.MacOSRigRun(ROOT, descriptor, mapping, "HEAD", "flow", "startup", False)
    run.target_source = pathlib.PurePosixPath("/target/run/source")
    recorded = []
    run._run_logged_ssh = lambda command, log_name, stage: recorded.append(shlex.split(command))
    run._build()
    return recorded


def drive_toolbox_adapter_build(root):
    """Record the commands the Toolbox adapter's build would issue on its rig."""
    descriptor = toolbox.load_descriptor(
        ROOT / "scripts" / "rig" / "toolbox" / "rigs" / "toolbox-maciix.ini"
    )
    mapping = toolbox.LocalMapping(root / "archive", root / "mame.env", root / "golden")
    run = toolbox.ToolboxRigRun(ROOT, descriptor, mapping, "HEAD", "flow")
    run.checkout = root / "checkout"
    recorded = []
    run._logged_run = lambda arguments, log_name, stage, **keywords: recorded.append(
        [str(argument) for argument in arguments]
    )
    run.build()
    return recorded


class AdapterHostToolBuildTest(unittest.TestCase):
    """Every rig adapter builds the host lrpc its scenario runner stages through.

    The runners refuse without build/host/lrpc, and an adapter's build runs
    only on its own rig -- over SSH to a VM, or against an emulator -- so no
    suite ever executes one for real. An adapter that forgets the step is
    green everywhere a test can reach and dead on arrival at the rig, which is
    how the macOS adapter went out unable to run any cell: #467 gave the
    runner a staging step that needs lrpc, and only the Toolbox adapter
    already built it.

    Each adapter is driven here with its command seam replaced by a recorder,
    so what is checked is the command the adapter would issue rather than the
    text of its source. A source grep would pass an adapter that only
    mentions lrpc in a comment or in dead code.

    Commands are compared as argument tokens, not as text: a substring test
    accepts an adapter that builds into build/host/lrpc2, which the runner
    never reads.

    DRIVERS is spelled out rather than inferred: a newly registered adapter
    fails test_registered_adapters_are_the_ones_this_wall_covers until someone
    writes its driver, and an adapter that legitimately runs no packaged cell
    belongs here with its reason rather than silently outside the wall.
    """

    DRIVERS = {"macos": drive_macos_adapter_build, "toolbox": drive_toolbox_adapter_build}

    def build_commands(self, name):
        with tempfile.TemporaryDirectory() as directory:
            return self.DRIVERS[name](pathlib.Path(directory))

    def test_registered_adapters_are_the_ones_this_wall_covers(self):
        cli = load_cli()
        self.assertEqual({adapter.name for adapter in cli.ADAPTERS}, set(self.DRIVERS))

    def test_every_adapter_configures_and_builds_the_host_lrpc(self):
        for name in sorted(self.DRIVERS):
            with self.subTest(adapter=name):
                commands = self.build_commands(name)
                self.assertTrue(
                    any("tools/lrpc" in c and "build/host/lrpc" in c for c in commands),
                    f"{name} adapter issues no command configuring tools/lrpc "
                    f"into build/host/lrpc; it ran {commands}",
                )
                self.assertTrue(
                    any("--build" in c and "build/host/lrpc" in c for c in commands),
                    f"{name} adapter issues no command building build/host/lrpc, "
                    f"which its scenario runner refuses without; it ran {commands}",
                )

    def test_the_runners_still_require_what_the_wall_pins(self):
        runners = (
            "tests/macos/run-scenario.sh",
            "tests/win32/run-scenario.ps1",
            "tests/toolbox/run-scenario.sh",
        )
        for runner in runners:
            with self.subTest(runner=runner):
                text = (ROOT / runner).read_text(encoding="utf-8")
                self.assertTrue(
                    "build/host/lrpc" in text,
                    f"{runner} no longer stages through build/host/lrpc; "
                    "this wall is pinning a requirement that moved",
                )


if __name__ == "__main__":
    unittest.main()
