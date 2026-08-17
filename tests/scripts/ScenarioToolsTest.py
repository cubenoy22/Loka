#!/usr/bin/env python3
"""Characterization tests for the OS-neutral scenario runner tools."""

import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCENARIO_DIR = os.path.join(PROJECT_DIR, "tests", "scenarios")
PNG_TOOL = os.path.join(SCENARIO_DIR, "pngtool.py")


def run_tool(*arguments):
    return subprocess.run(
        [sys.executable] + list(arguments),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )


def png_chunk(kind, payload):
    checksum = zlib.crc32(kind)
    checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_rgb_png(path, width, height, pixels):
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(pixels[y * width + x])
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(rows)))
        + png_chunk(b"IEND", b"")
    )
    with open(path, "wb") as handle:
        handle.write(payload)


class ExpectedAuditPinsTest(unittest.TestCase):
    def test_win32_vehicle_map_covers_the_shared_registry_examples(self):
        runner_path = os.path.join(PROJECT_DIR, "tests", "win32", "run-scenario.ps1")
        with open(runner_path, "r", encoding="utf-8") as handle:
            runner = handle.read()
        cmake_path = os.path.join(PROJECT_DIR, "win32", "CMakeLists.txt")
        with open(cmake_path, "r", encoding="utf-8") as handle:
            cmake = handle.read()
        with open(os.path.join(PROJECT_DIR, "CMakePresets.json"), "r", encoding="utf-8") as handle:
            presets = json.load(handle)
        win32_tests = next(
            preset
            for preset in presets["buildPresets"]
            if preset["name"] == "win32-tests"
        )

        vehicles = {
            "scrapbook": ("LokaScrapbookScenarioWin32", "ScrapbookUI"),
            "helloworld": ("LokaHelloWorldScenarioWin32", "HelloWorld"),
            "tutorial": ("LokaTutorialScenarioWin32", "Tutorial"),
            "minesweeper": ("LokaMineSweeperScenarioWin32", "MineSweeper"),
            "floppybird": ("LokaFloppyBirdScenarioWin32", "FloppyBird"),
        }
        for example, (target, output_directory) in vehicles.items():
            self.assertIn('"{}" = @{{'.format(example), runner)
            self.assertIn('Executable = "{}.exe"'.format(target), runner)
            self.assertIn('OutputDirectory = "{}"'.format(output_directory), runner)
            self.assertIn(target, cmake)
            self.assertIn(target, win32_tests["targets"])

        self.assertNotIn("currently supports only ScrapbookUI", runner)
        hello_driver_path = os.path.join(
            PROJECT_DIR, "tests", "win32", "HelloWorldScenarioDriver.cpp"
        )
        with open(hello_driver_path, "r", encoding="utf-8") as handle:
            hello_driver = handle.read()
        self.assertIn(
            "SceneScenarioDriver<scenario_tests::HelloWorldScenario>", hello_driver
        )

    def test_scrapbook_refusal_fixtures_have_one_neutral_mapping(self):
        registry = os.path.join(SCENARIO_DIR, "scrapbook-package-fixtures.txt")
        with open(registry, "r", encoding="ascii") as handle:
            entries = [line.split() for line in handle.read().splitlines()]
        self.assertEqual(
            entries,
            [
                ["open-first-page-refused", "corrupt-bag=1"],
                ["refused-flip-keeps-page", "corrupt-bag=3"],
                ["open-text-page-refused", "corrupt-bag=5"],
            ],
        )
        registered = os.path.join(SCENARIO_DIR, "scenarios.txt")
        with open(registered, "r", encoding="ascii") as handle:
            scenarios = set(handle.read().splitlines())
        for scenario, fixture in entries:
            self.assertIn("scrapbook " + scenario, scenarios)
            self.assertRegex(fixture, r"^corrupt-bag=[0-9]+$")

    def test_expected_audits_cover_registry_and_pin_app_identity(self):
        registry = os.path.join(PROJECT_DIR, "tests", "scenarios", "scenarios.txt")
        with open(registry, "r", encoding="utf-8") as handle:
            entries = [line.split() for line in handle.read().splitlines()]
        self.assertEqual(len(entries), 17)
        self.assertEqual(len(entries), len({tuple(entry) for entry in entries}))
        for entry in entries:
            self.assertEqual(len(entry), 2)
            example, scenario = entry
            audit_path = os.path.join(SCENARIO_DIR, "expected", example, scenario + ".audit")
            with open(audit_path, "rb") as handle:
                audit = handle.read()

            self.assertNotIn(b"\r", audit)
            lines = audit.splitlines(keepends=True)
            self.assertEqual(
                lines[0],
                "loka_scenario_audit version=1 scenario={}\n".format(scenario).encode("ascii"),
            )
            self.assertEqual(lines[-1], b"terminal status=succeeded\n")
            identities = {
                "helloworld": b"HelloWorld",
                "scrapbook": b"ScrapbookUI",
                "tutorial": b"Tutorial",
                "minesweeper": b"MineSweeper",
                "floppybird": b"FloppyBird",
            }
            identity = identities[example]
            self.assertIn(b"test\t" + identity + b"\n", audit)
            self.assertIn(b"step\t" + scenario.encode("ascii") + b"\n", audit)
            self.assertIn(b"status\tok\n", audit)
            self.assertNotIn(b"/home/", audit)
            self.assertNotIn(b"/mnt/", audit)
            self.assertNotIn(b"build/", audit)

    def test_byte_compare_rejects_observed_string_mutation(self):
        expected = os.path.join(SCENARIO_DIR, "expected", "scrapbook", "open-text-page.audit")
        with tempfile.TemporaryDirectory(prefix="scenario-audit-") as directory:
            actual = os.path.join(directory, "actual.audit")
            with open(expected, "rb") as handle:
                contents = handle.read()
            observed = b"text_matches_package_asset\ttrue\n"
            self.assertIn(observed, contents)
            with open(actual, "wb") as handle:
                handle.write(contents)
            same = subprocess.run(["cmp", expected, actual], check=False)
            self.assertEqual(same.returncode, 0)

            with open(actual, "wb") as handle:
                handle.write(contents.replace(observed, b"text_matches_package_asset\tfalse\n", 1))
            changed = subprocess.run(
                ["cmp", expected, actual],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                check=False,
            )
            self.assertNotEqual(changed.returncode, 0)

    def test_startup_audits_reject_per_example_observation_mutations(self):
        mutations = {
            "helloworld": (b"text.value\tLoka Sample\n", b"text.value\tChanged\n"),
            "tutorial": (b"text.value\tLoka Tutorial\n", b"text.value\tChanged\n"),
            "minesweeper": (b"button.text\tNew Game\n", b"button.text\tChanged\n"),
            "floppybird": (b"surface.rects\t72,114,18,14\n", b"surface.rects\t0,0,1,1\n"),
        }
        with tempfile.TemporaryDirectory(prefix="startup-audit-") as directory:
            for example, (observation, mutation) in mutations.items():
                expected = os.path.join(
                    SCENARIO_DIR, "expected", example, "startup.audit"
                )
                actual = os.path.join(directory, example + ".audit")
                with open(expected, "rb") as handle:
                    contents = handle.read()
                self.assertIn(observation, contents)
                with open(actual, "wb") as handle:
                    handle.write(contents.replace(observation, mutation, 1))
                changed = subprocess.run(
                    ["cmp", expected, actual],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
                self.assertNotEqual(changed.returncode, 0, example)


class PngToolTest(unittest.TestCase):
    def test_exact_compare_and_diff_pin_one_pixel_change(self):
        with tempfile.TemporaryDirectory(prefix="scenario-png-") as directory:
            expected = os.path.join(directory, "expected.png")
            actual = os.path.join(directory, "actual.png")
            difference = os.path.join(directory, "diff.png")
            write_rgb_png(expected, 2, 2, [(1, 2, 3)] * 4)
            write_rgb_png(actual, 2, 2, [(1, 2, 3), (9, 8, 7), (1, 2, 3), (1, 2, 3)])

            same = run_tool(PNG_TOOL, "compare", expected, expected)
            changed = run_tool(PNG_TOOL, "compare", expected, actual)
            diffed = run_tool(PNG_TOOL, "diff", expected, actual, difference)
            self.assertEqual(same.returncode, 0, same.stderr)
            self.assertNotEqual(changed.returncode, 0)
            self.assertIn("differing pixels: 1", changed.stdout)
            self.assertEqual(diffed.returncode, 1)
            self.assertTrue(os.path.isfile(difference))
            self.assertGreater(os.path.getsize(difference), 0)


if __name__ == "__main__":
    unittest.main()
