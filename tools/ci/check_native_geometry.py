#!/usr/bin/env python3
"""Keep Win32 geometry API calls in reviewed policy/typed-wrapper files (#818).

This is a literal-call guard, not a C++ type checker or macro resolver. The
allowlist grants file-level trust; review must verify typed wrapper arguments.
Comments/strings are ignored; inactive preprocessor branches are checked.
"""
from pathlib import Path
import re
import sys

# API-specific permissions keep the file-level allowlist from licensing MulDiv
# in a controller, or native windows inside the arithmetic policy.
ALLOWLIST = {
    "win32/src/platform/Win32DisplayScale.cpp": {"MulDiv"},
    # Typed child/layout wrappers and callbacks carrying already-native damage.
    "win32/src/Win32ScenePlatformController.cpp": {
        "MoveWindow", "SetWindowPos", "CreateWindowExW", "InvalidateRect"},
    # Typed top-level wrappers; chrome, desktop origin and #712 clamp stay native.
    "win32/src/Win32Window.cpp": {"MoveWindow", "SetWindowPos", "CreateWindowExW"},
}
# The sprite path calls projectDeviceOnly; it needs no raw-API exception.
CALL = re.compile(r"\b(MoveWindow|SetWindowPos|CreateWindowExW|InvalidateRect|MulDiv)\s*\(")
NONCODE = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)
NULL_DAMAGE = re.compile(r"\s*[^,()]+,\s*(?:NULL|0|nullptr)\s*,")
SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".inc", ".inl", ".cc", ".cxx"}


def violations(path, source):
    source = re.sub(r"\\\r?\n", "", source)
    source = NONCODE.sub(lambda m: " " * (len(m[0]) - m[0].count("\n"))
                        + "\n" * m[0].count("\n"), source)
    for match in CALL.finditer(source):
        api = match[1]
        # Whole-window invalidation constructs no geometry.
        if api == "InvalidateRect" and NULL_DAMAGE.match(source, match.end()):
            continue
        if api not in ALLOWLIST.get(path, set()):
            yield source.count("\n", 0, match.start()) + 1, api


def check_tree(root):
    files = sorted(p for p in (root / "win32/src").rglob("*")
                   if p.is_file() and p.suffix.lower() in SUFFIXES)
    findings = []
    for path in files:
        relative = path.relative_to(root).as_posix()
        findings.extend(f"{relative}:{line}: native geometry call {api} outside typed wrappers"
                        for line, api in violations(relative, path.read_text(encoding="utf-8")))
    return files, findings


def main():
    files, findings = check_tree(Path(__file__).resolve().parents[2])
    for finding in findings:
        print(finding)
    print(f"check_native_geometry: {len(files)} files, {len(findings)} findings")
    return int(bool(findings))


if __name__ == "__main__":
    sys.exit(main())
