"""Prepare a build-local copy of the pinned engine for the Classic probe.

No shared dependency cache is mutated. This is a bounded compatibility
experiment, not a general QuickJS port or a feature-stripped engine.
"""
from pathlib import Path
import shutil
import sys


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise SystemExit("Pinned source differs at: " + before)
    return text.replace(before, after)


source = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
repository = Path(__file__).resolve().parents[3]
if not output.is_relative_to(repository / "build"):
    raise SystemExit("Patched dependency output must remain under repository build/")
if source == output or output.is_relative_to(source):
    raise SystemExit("Source and output must be separate directories")
shutil.copytree(source, output, dirs_exist_ok=True)

path = output / "cutils.h"
text = path.read_text()
for before, after in (
    ("&& !defined(__DJGPP)",
     "&& !defined(__DJGPP) && !defined(LOKA_QUICKJS_68K_PROBE)"),
    ("|| defined(__DJGPP)",
     "|| defined(__DJGPP) || defined(LOKA_QUICKJS_68K_PROBE)"),
    ("#ifdef __DJGPP\n  struct timeval tv;",
     "#if defined(LOKA_QUICKJS_68K_PROBE)\n"
     "  extern uint64_t loka_probe_hrtime_ns(void);\n"
     "  return loka_probe_hrtime_ns();\n"
     "#elif defined(__DJGPP)\n  struct timeval tv;"),
):
    text = replace_once(text, before, after)
path.write_text(text)

path = output / "quickjs.c"
text = path.read_text()
for before, after in (
    ("int new_line_num, new_col_num, line_num, col_num, pc, v, ret;",
     "int new_line_num, new_col_num, line_num, col_num, pc, ret;\n    int32_t v;"),
    ("int radix, flags;", "int flags;\n    int32_t radix;"),
    ("int remainingElementsCount;", "int32_t remainingElementsCount;"),
    ("int is_zero, index;", "int is_zero;\n    int32_t index;"),
):
    text = replace_once(text, before, after)
path.write_text(text)
