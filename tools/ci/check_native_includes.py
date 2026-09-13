#!/usr/bin/env python3
"""Keep native toolkit includes out of portable public headers (#669)."""

from pathlib import Path
import re
import sys


PORTABLE_DIRS = ("common/core", "common/app", "common/dsl")
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inc", ".inl"}

# Explicit native SDK policy, case-insensitive: Windows entry headers, Classic
# Toolbox/Universal Interfaces (including Retro68's umbrella), and Apple GUI
# framework directories. Extend this list when another native header is found;
# this is a literal-include guard, not a transitive include or macro resolver.
NATIVE_HEADERS = {
    # Windows SDK entry points.
    "windows.h", "windowsx.h", "winuser.h", "windef.h", "winbase.h",
    "wingdi.h", "commctrl.h", "commdlg.h", "shellapi.h", "shlobj.h",
    # Classic Toolbox / Universal Interfaces. Only names that cannot be
    # mistaken for a portable header are listed: the Toolbox also ships
    # `Types.h`, `Memory.h`, `Events.h`, `Files.h`, `Strings.h` and similar
    # generic basenames, and a portable Loka header may legitimately carry one
    # of those; the generic names are caught by their umbrella includes here.
    "carbon.h", "cocoa.h", "appkit.h", "mactypes.h", "quickdraw.h",
    "quickdrawtext.h", "qdoffscreen.h", "macwindows.h", "macmemory.h",
    "mactexteditor.h", "macerrors.h", "osutils.h", "toolutils.h",
    "controlmanager.h", "dialogmanager.h", "eventmanager.h", "macevents.h",
    "menumanager.h", "filemanager.h", "fontmanager.h", "textedit.h",
    "textutils.h", "resourcemanager.h", "scrapmanager.h", "soundmanager.h",
    "standardfile.h", "navigation.h", "appearance.h", "balloons.h",
    "lowmem.h", "processes.h", "gestalt.h", "gestaltequ.h", "mixedmode.h",
    "multiverse.h", "pictutils.h", "fixmath.h", "controlstrip.h",
    "appleevents.h", "aeregistry.h", "aepackobject.h", "internetconfig.h",
    "numberformatting.h", "datetimeutils.h", "deviceserial.h",
    "carbonevents.h", "hiview.h", "hitoolbox.h", "atsui.h",
}
NATIVE_FRAMEWORKS = (
    "carbon/", "cocoa/", "appkit/", "hitoolbox/", "applicationservices/",
    "coreservices/", "quickdraw/",
)
# Bare NS*.h GUI headers can also be included without their AppKit directory.
APPKIT_HEADER = re.compile(r"ns[a-z0-9_]+\.h$")
INCLUDE = re.compile(r'^\s*#\s*(?:include|import)\s*[<"]([^>"\n]+)[>"]', re.MULTILINE)
COMMENTS = re.compile(r'/\*.*?\*/|//[^\n]*', re.DOTALL)


def native_includes(source):
    # Match preprocessing order for continued directives and preserve line
    # numbers through comments. Inspect inactive branches on every CI host.
    source = re.sub(r"\\\r?\n", "", source)
    source = COMMENTS.sub(lambda match: " " + "\n" * match[0].count("\n"), source)
    for match in INCLUDE.finditer(source):
        name = match[1].replace("\\", "/").lower()
        basename = name.rsplit("/", 1)[-1]
        if (basename in NATIVE_HEADERS or name.startswith(NATIVE_FRAMEWORKS)
                or APPKIT_HEADER.fullmatch(basename)):
            yield source.count("\n", 0, match.start(1)) + 1, match[1]


def main():
    root = Path(__file__).resolve().parents[2]
    findings = []
    files = sorted(path for directory in PORTABLE_DIRS
                   for path in (root / directory).rglob("*")
                   if path.is_file() and path.suffix.lower() in HEADER_SUFFIXES)
    for path in files:
        for line, name in native_includes(path.read_text(encoding="utf-8")):
            findings.append(f"{path.relative_to(root)}:{line}: native include {name}")
    for finding in findings:
        print(finding)
    print(f"check_native_includes: {len(files)} headers, {len(findings)} findings")
    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main())
