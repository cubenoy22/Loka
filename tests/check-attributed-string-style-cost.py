#!/usr/bin/env python3
"""Optional GCC/gcov cost pin for #848; never a target build prerequisite.

Run from any directory: python3 tests/check-attributed-string-style-cost.py
The semantic fixture compares four equal bytes with segment entries at 0, 1, 2.
Restoring per-byte style comparison must fail this pin (four calls, not three).
"""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
build = root / "build/Testing-Coverage"
subprocess.run(["cmake", "--preset", "testing-coverage"], cwd=root, check=True)
subprocess.run(["cmake", "--build", "--preset", "testing-coverage", "-j", "4",
                "--target", "LokaContractTests"], cwd=root, check=True)
coverage = build / "CMakeFiles/LokaCommonTesting.dir/common/app/style/AttributedString.cpp.gcda"
coverage.unlink(missing_ok=True)
subprocess.run([str(build / "LokaContractTests"), "testAttributedStringStyleBoundaries"],
               cwd=build, check=True)
result = subprocess.run(["gcov", "--stdout", "--object-file", str(coverage.with_suffix(".gcno")),
                         str(root / "common/app/style/AttributedString.cpp")],
                        cwd=build, check=True, text=True, stdout=subprocess.PIPE)
(build / "attributed-string-style-cost.gcov").write_text(result.stdout)
counts = [line.split(":", 2)[0].strip() for line in result.stdout.splitlines()
          if "if (left.segment().style != right.segment().style)" in line]
print("Style boundary cost pin: comparisons=" + repr(counts) + " expected=['3']", flush=True)
if counts != ["3"]:
    raise SystemExit(1)
