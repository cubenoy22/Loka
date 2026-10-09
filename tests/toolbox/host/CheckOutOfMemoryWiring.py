"""Structural pin for initial-mount wiring; the host does not run ToolboxApp::run.

The real event-loop/bootstrap fixture is absent. Runtime path acceptance remains
on MAME; this check only detects removal/movement of the fatal calls.
"""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
app = (root / "apple/toolbox/src/ToolboxApp.cpp").read_text()
checks = [
    re.search(r"InitDialogs\(0\);\s*loka::toolbox::ArmOutOfMemoryReserve\(\);", app),
    re.search(r"toolboxWindow->ensureSceneMounted\(\);\s*"
              r"if \(toolboxWindow->scene\(\) && toolboxWindow->scene\(\)->composeRefusedForMemory\(\)\)\s*"
              r"\{\s*loka::toolbox::QuitForOutOfMemory\(\);", app),
]
operators = (root / "apple/toolbox/src/ToolboxOperatorNew.cpp").read_text()
for signature in [r"void \*operator new\(std::size_t size\)",
                  r"void __throw_bad_alloc\(\)",
                  r"void __throw_bad_array_new_length\(\)"]:
    checks.append(re.search(signature + r"\s*\{[^}]*loka::toolbox::QuitForOutOfMemory\(\);", operators))
if not all(checks):
    sys.exit("OOM initial-mount/reserve/operator wiring missing")
print("OOM structural wiring checks passed (not a ToolboxApp runtime test)")
