#!/usr/bin/env python3
"""Characterization tests for the OS-neutral scenario runner tools."""

import json
import os
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCENARIO_DIR = os.path.join(PROJECT_DIR, "tests", "scenarios")
PNG_TOOL = os.path.join(SCENARIO_DIR, "pngtool.py")

sys.path.insert(0, os.path.join(PROJECT_DIR, "scripts", "rig"))
import golden_identity_guard  # noqa: E402


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
    def test_startup_identity_declarations_are_tracked_and_win32_guards_before_copy(self):
        declarations = os.path.join(
            SCENARIO_DIR, "startup-golden-identities.txt"
        )
        entries = golden_identity_guard.read_declarations(pathlib.Path(declarations))
        self.assertEqual(
            [cell for cell, _ in entries],
            [
                ("minesweeper", "new-game-twice"),
                ("scrapbook", "flip-forward-back"),
            ],
        )

        runner_path = os.path.join(PROJECT_DIR, "tests", "win32", "run-scenario.ps1")
        with open(runner_path, "r", encoding="utf-8") as handle:
            runner = handle.read()
        guard_call = runner.index("Invoke-GoldenIdentityGuard $Actual")
        golden_copy = runner.index("Copy-Item -LiteralPath $Actual -Destination $Golden")
        self.assertLess(guard_call, golden_copy)
        self.assertIn("--declarations $StartupIdentityDeclarations", runner)

        # The rig's declared capture environment is checked before the golden is
        # written *and* before it is compared, so a bake on a machine that does
        # not match refuses instead of pinning whatever the desktop was.
        resolve_call = runner.index("$RigDescriptor = Resolve-RigDescriptor $ActualProfile")
        profile_guard = runner.index(
            "Invoke-CaptureProfileGuard $RigDescriptor $ActualProfile"
        )
        update_branch = runner.index("if ($UpdateGolden) {")
        compare_profile = runner.index("$expectedProfile = Read-ScenarioProfile $GoldenProfile")
        self.assertLess(resolve_call, profile_guard)
        self.assertLess(profile_guard, update_branch)
        self.assertLess(profile_guard, golden_copy)
        self.assertLess(profile_guard, compare_profile)
        self.assertIn("--descriptor $Descriptor", runner)

        # The descriptor is chosen from the architecture the vehicle reports.
        # Fixing it to one architecture would refuse every run on the others --
        # this rail is supported on ARM64 Windows as well as x64.
        self.assertIn('Join-Path $RigDescriptorDirectory "win32-$arch.ini"', runner)
        self.assertNotIn("rigs/win32-x64.ini", runner)
        rigs = os.path.join(PROJECT_DIR, "scripts", "rig", "win32", "rigs")
        self.assertTrue(os.path.isfile(os.path.join(rigs, "win32-x64.ini")))

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
        self.assertEqual(len(entries), 16)
        self.assertEqual(len(entries), len({tuple(entry) for entry in entries}))
        registered_audits = set()
        for entry in entries:
            self.assertEqual(len(entry), 2)
            example, scenario = entry
            audit_path = os.path.join(SCENARIO_DIR, "expected", example, scenario + ".audit")
            registered_audits.add(os.path.relpath(audit_path, SCENARIO_DIR))
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

        tracked_audits = {
            os.path.relpath(path, SCENARIO_DIR)
            for path in pathlib.Path(SCENARIO_DIR, "expected").glob("*/*.audit")
            if path.name != "standalone-tour.audit"
        }
        self.assertEqual(tracked_audits, registered_audits)

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


class PresetFlagSeedingTest(unittest.TestCase):
    """Pin the MSVC flag-seeding contract for the win32 preset family.

    Assigning the completed CMAKE_<LANG>_FLAGS / CMAKE_EXE_LINKER_FLAGS cache
    entries from a preset replaces the platform defaults CMake would otherwise
    compose (for MSVC that silently drops /EHsc, which /WX then turns into a
    build break). Presets must seed platform-specific inputs through the
    *_INIT variables instead. The Linux instrumentation presets
    (testing-coverage, testing-asan) intentionally own their complete flag
    sets and are outside this rule.
    """

    COMPLETED_FORMS = ("CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS")

    def load_configure_presets(self):
        with open(os.path.join(PROJECT_DIR, "CMakePresets.json"), "r", encoding="utf-8") as handle:
            return json.load(handle)["configurePresets"]

    def test_win32_presets_never_replace_completed_flag_sets(self):
        checked = 0
        for preset in self.load_configure_presets():
            if not preset["name"].startswith("win32"):
                continue
            checked += 1
            cache = preset.get("cacheVariables", {})
            for completed in self.COMPLETED_FORMS:
                self.assertNotIn(
                    completed,
                    cache,
                    "{} assigns {}; seed it through {}_INIT so MSVC defaults survive".format(
                        preset["name"], completed, completed
                    ),
                )
        self.assertGreater(checked, 0)

    def test_winxp_presets_seed_the_xp_target_through_init(self):
        presets = {p["name"]: p for p in self.load_configure_presets()}
        for name in ("win32-xp-debug", "win32-xp-release"):
            cache = presets[name].get("cacheVariables", {})
            for lang_flags in ("CMAKE_C_FLAGS_INIT", "CMAKE_CXX_FLAGS_INIT"):
                self.assertIn("/D_WIN32_WINNT=0x0501", cache.get(lang_flags, ""), name)
                self.assertIn("/DWINVER=0x0501", cache.get(lang_flags, ""), name)
            self.assertIn(
                "/SUBSYSTEM:WINDOWS,5.01",
                cache.get("CMAKE_EXE_LINKER_FLAGS_INIT", ""),
                name,
            )


if __name__ == "__main__":
    unittest.main()
