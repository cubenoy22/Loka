#!/usr/bin/env python3

import hashlib
import importlib.util
import pathlib
import sys
import tempfile
import unittest


sys.dont_write_bytecode = True
ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts" / "macos" / "loka_macos_rig.py"
SPEC = importlib.util.spec_from_file_location("loka_macos_rig", MODULE_PATH)
assert SPEC and SPEC.loader
rig = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = rig
SPEC.loader.exec_module(rig)


class MacOSRigProtocolTest(unittest.TestCase):
    def test_tracked_descriptor_separates_machine_local_mapping(self):
        descriptor = rig.load_descriptor(ROOT / "scripts" / "macos" / "rigs" / "mavericks-10.9.ini")
        self.assertEqual(descriptor.rig_id, "mavericks-10.9")
        self.assertEqual(descriptor.supported_modes, frozenset(("flow", "inspect")))
        self.assertFalse(descriptor.disposable_for_input)

        with tempfile.TemporaryDirectory() as directory:
            local_path = pathlib.Path(directory) / "local.ini"
            local_path.write_text(
                "[local]\n"
                "vm_host_ssh = host-alias\n"
                "vm_name = VM Name\n"
                "target_host = auto\n"
                "target_user = test-user\n"
                f"target_identity_file = {directory}/id_rsa\n"
                "target_proxy_ssh = host-alias\n"
                "target_legacy_rsa = true\n"
                "target_root = /Users/test/loka-rig\n"
                f"archive_root = {directory}/archive\n",
                encoding="utf-8",
            )
            mapping = rig.load_local_mapping(local_path)
            self.assertEqual(mapping.vm_name, "VM Name")
            self.assertEqual(str(mapping.target_root), "/Users/test/loka-rig")

    def test_unknown_descriptor_vocabulary_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "rig.ini"
            text = (ROOT / "scripts" / "macos" / "rigs" / "mavericks-10.9.ini").read_text(encoding="utf-8")
            path.write_text(text + "unexpected = value\n", encoding="utf-8")
            with self.assertRaises(rig.RigError) as caught:
                rig.load_descriptor(path)
            self.assertEqual(caught.exception.stage, "configuration")

    def test_vm_cleanup_restores_only_a_state_the_orchestrator_changed(self):
        self.assertEqual(rig.VmLease("running").success_action(), "leave-running")
        self.assertEqual(rig.VmLease("stopped").success_action(), "stop")
        self.assertEqual(rig.VmLease("suspended").success_action(), "suspend")

    def test_cleanup_is_a_success_commit_point(self):
        self.assertFalse(rig.cleanup_allowed("failed", True, True))
        self.assertFalse(rig.cleanup_allowed("passed", False, True))
        self.assertFalse(rig.cleanup_allowed("passed", True, False))
        self.assertTrue(rig.cleanup_allowed("passed", True, True))

    def test_parallels_address_discovery_declines_missing_fact(self):
        self.assertEqual(rig.parse_parallels_ipv4("IP Addresses: 192.0.2.17\n"), "192.0.2.17")
        self.assertIsNone(rig.parse_parallels_ipv4("IP Addresses: -\n"))

    def test_manifest_hashes_finalized_artifacts_in_stable_order(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            a_bytes = b"a\n"
            z_bytes = b"z\n"
            (root / "z.log").write_bytes(z_bytes)
            (root / "a.snap").write_bytes(a_bytes)
            hashes = rig.artifact_hashes(root)
            manifest = rig.render_manifest(
                (("rig_id", "mavericks-10.9"), ("result", "passed")),
                hashes,
            )
            expected = (
                "manifest_version=1\n"
                "rig_id=mavericks-10.9\n"
                "result=passed\n"
                f"artifact_sha256={hashlib.sha256(a_bytes).hexdigest()}  a.snap\n"
                f"artifact_sha256={hashlib.sha256(z_bytes).hexdigest()}  z.log\n"
            )
            self.assertEqual(manifest, expected)

    def test_remote_environment_precedes_exec(self):
        command = rig.remote_in_directory(
            pathlib.PurePosixPath("/tmp/run path"),
            ("/bin/echo", "hello world"),
            {"MODE": "inspect"},
        )
        self.assertEqual(command, "cd '/tmp/run path' && MODE=inspect exec /bin/echo 'hello world'")


if __name__ == "__main__":
    unittest.main()
