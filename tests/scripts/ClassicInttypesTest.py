#!/usr/bin/env python3
"""Pin the Classic inttypes shim without requiring a Retro68 installation."""
import argparse
import json
import os
import re
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHIM = ROOT / "apple/toolbox/compat/newlib"
BUILD_DIR = None


class ClassicInttypesTest(unittest.TestCase):
    def compile_case(self, header, body):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "inttypes.h").write_text(header)
            source = '#include <inttypes.h>\n#include <inttypes.h>\n' + body
            for language, variable, default, dialect in (
                ("c", "CC", "cc", "gnu11"),
                ("c++", "CXX", "c++", "gnu++98"),
            ):
                with self.subTest(language=language):
                    command = shlex.split(os.environ.get(variable, default)) + [
                        "-x", language, "-std=" + dialect, "-fsyntax-only",
                        "-Werror", "-Wformat=2", "-I" + str(SHIM),
                        "-I" + directory, "-",
                    ]
                    result = subprocess.run(command, input=source, text=True,
                                            capture_output=True)
                    self.assertEqual(result.returncode, 0, result.stderr)

    def check_format(self, prefix, integer_type, existing=False):
        header = ('#pragma once\n#define __NEWLIB__ 4\n'
                  '#define __INT64_TYPE__ long long int\n'
                  '#define __PRI64(x) "' + prefix + '" #x\n')
        # Remove the compiler builtin before the fixture declares its own value.
        header = '#undef __INT64_TYPE__\n' + header
        if existing:
            header += '#define PRId64 "d"\n'
        self.compile_case(header,
            'extern void output(const char *, ...) '
            '__attribute__((format(printf, 1, 2)));\n'
            'void probe(void) { output("%" PRId64, (' + integer_type + ')0); }\n')

    def test_missing_format_uses_vendor_type(self):
        for prefix, integer_type in (("ll", "long long"), ("l", "long")):
            with self.subTest(prefix=prefix):
                self.check_format(prefix, integer_type)

    def test_existing_format_is_preserved(self):
        self.check_format("ll", "int", existing=True)

    def test_incomplete_or_non_newlib_headers_are_untouched(self):
        for definitions in (
            '#define __PRI64(x) "ll" #x\n',
            '#define __NEWLIB__ 4\n',
            '#define __NEWLIB__ 4\n#define __PRI64(x) "ll" #x\n'
            '#undef __INT64_TYPE__\n',
        ):
            with self.subTest(definitions=definitions):
                self.compile_case('#pragma once\n' + definitions,
                    '#ifdef PRId64\n#error Unexpected fallback\n#endif\n')


class ClassicWorkflowTest(unittest.TestCase):
    def test_toolbox_builds_and_checks_both_consumers(self):
        workflow = (ROOT / ".github/workflows/toolbox.yml").read_text()
        # Every container body in the workflow: the 68K and PPC jobs each own one.
        scripts = re.findall(r"/bin/bash -ceu '(.*?)'", workflow, re.S)
        self.assertGreaterEqual(len(scripts), 2)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            for tool in ("cmake", "python3"):
                stub = path / tool
                stub.write_text('#!/bin/sh\nprintf "%s" "' + tool + '" >> "$CALL_LOG"\n'
                                'printf " <%s>" "$@" >> "$CALL_LOG"\n'
                                'printf "\\n" >> "$CALL_LOG"\n')
                stub.chmod(0o755)
            log = path / "calls"
            env = dict(os.environ, PATH=directory + os.pathsep + os.environ["PATH"],
                       CALL_LOG=str(log))
            for script in scripts:
                subprocess.run(["bash", "-ceu", script], env=env, check=True)
            calls = log.read_text().splitlines()
            for cpu, suffix in (("68k", "68K"), ("ppc", "PPC")):
                build = "build/retro68/" + cpu + "/SmirkyCardCI"
                configure = next((line for line in calls
                                  if line.startswith("cmake <--preset> <retro68-" + cpu)
                                  and " <-B> <" + build + ">" in line), "")
                self.assertIn(" <-DLOKA_BUILD_SMIRKYCARD=ON>", configure)
                self.assertIn(" <-DLOKA_TOOLBOX_MULTIVERSAL_INTERFACES=ON>", configure)
                self.assertIn("cmake <--build> <" + build + "> <--target> <LokaSmirkyCard"
                              + suffix + "_APPL>", calls)
                self.assertIn("python3 <tests/scripts/ClassicInttypesTest.py> <--build-dir> <"
                              + build + "> <ClassicQuickjsHeadersTest>", calls)
                self.assertIn("python3 <tests/scripts/ClassicQuickjsStackTest.py> <--build-dir> <"
                              + build + ">", calls)


class ClassicQuickjsHeadersTest(unittest.TestCase):
    def test_real_consumer_headers(self):
        if BUILD_DIR is None:
            self.skipTest("pass --build-dir for actual Classic consumer flags")
        entries = json.loads((BUILD_DIR / "compile_commands.json").read_text())
        entry = next(e for e in entries if e["file"].endswith("/SmirkyCard/src/ScriptRuntime.cpp"))
        original = entry.get("arguments") or shlex.split(entry["command"])
        command = []
        skip = False
        for argument in original:
            if skip:
                skip = False
            elif argument in ("-o", "-c"):
                skip = True
            else:
                command.append(argument)
        source = ('#include <quickjs.h>\n#include <inttypes.h>\n'
                  '#include <stdio.h>\n'
                  'typedef char limits_available[(INT32_MIN < 0 && INT32_MAX > 0) ? 1 : -1];\n'
                  'void probe(int64_t value) { printf("%" PRId64, value); }\n')
        result = subprocess.run(command + ["-fsyntax-only", "-Werror=format", "-Wformat=2", "-x", "c++", "-"],
                                input=source, text=True, capture_output=True,
                                cwd=entry["directory"])
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path)
    arguments, remaining = parser.parse_known_args()
    BUILD_DIR = arguments.build_dir.resolve() if arguments.build_dir else None
    unittest.main(argv=[__file__] + remaining)
