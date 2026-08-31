#!/usr/bin/env python3
"""Tests for the final Retro68 MacBinary size report and regression gate."""

import importlib.util
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest


PROJECT_DIR = pathlib.Path(__file__).resolve().parents[2]
REPORT_TOOL = PROJECT_DIR / "tools" / "ci" / "retro68_size_report.py"
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
