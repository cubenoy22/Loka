"""Prepare a build-local QuickJS-ng copy for the Retro68 SmirkyCard target."""
from pathlib import Path
import shutil
import sys


def replace_once(text, before, after):
    if text.count(before) != 1:
        raise SystemExit("Pinned source differs at: " + before)
    return text.replace(before, after)


if len(sys.argv) != 3:
    raise SystemExit("usage: prepare_quickjs68k.py SOURCE OUTPUT")

source = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
if source == output or output.is_relative_to(source):
    raise SystemExit("Source and output must be separate directories")
shutil.copytree(source, output, dirs_exist_ok=True)

path = output / "cutils.h"
text = path.read_text()
for before, after in (
    ("&& !defined(__DJGPP)",
     "&& !defined(__DJGPP) && !defined(LOKA_SMIRKYCARD_QUICKJS_68K)"),
    ("|| defined(__DJGPP)",
     "|| defined(__DJGPP) || defined(LOKA_SMIRKYCARD_QUICKJS_68K)"),
    ("#ifdef __DJGPP\n  struct timeval tv;",
     "#if defined(LOKA_SMIRKYCARD_QUICKJS_68K)\n"
     "  extern uint64_t smirkycard_hrtime_ns(void);\n"
     "  return smirkycard_hrtime_ns();\n"
     "#elif defined(__DJGPP)\n  struct timeval tv;"),
):
    text = replace_once(text, before, after)
path.write_text(text)

path = output / "quickjs.c"
text = path.read_text()
text = replace_once(text, '#include "dtoa.h"',
                    '#include "dtoa.h"\n#include "ClassicQuickjsStack.h"')
# Every current alloca holds JSValues, with pointer refs after the interpreter
# frame. Refuse source drift so a new allocation's alignment is reviewed.
if text.count("alloca(") != 5:
    raise SystemExit("Pinned QuickJS stack allocation sites differ")
text = text.replace("alloca(", "SMIRKYCARD_JS_ALLOCA(")
text = replace_once(text, "sp = js_get_stack_pointer() - alloca_size;",
                    "sp = js_get_stack_pointer() - SMIRKYCARD_JS_STACK_SIZE(alloca_size);")
for before, after in (
    ("int new_line_num, new_col_num, line_num, col_num, pc, v, ret;",
     "int new_line_num, new_col_num, line_num, col_num, pc, ret;\n    int32_t v;"),
    ("int radix, flags;", "int flags;\n    int32_t radix;"),
    ("int remainingElementsCount;", "int32_t remainingElementsCount;"),
    ("int is_zero, index;", "int is_zero;\n    int32_t index;"),
):
    text = replace_once(text, before, after)
path.write_text(text)
