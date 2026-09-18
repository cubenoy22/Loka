#!/usr/bin/env python3
"""Positive and negative controls for the literal Win32 geometry wall."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("checker", ROOT / "tools/ci/check_native_geometry.py")
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class NativeGeometryCheckerTest(unittest.TestCase):
    def test_each_api_is_refused_in_unreviewed_files(self):
        for api in ("MoveWindow", "SetWindowPos", "CreateWindowExW", "InvalidateRect", "MulDiv"):
            with self.subTest(api=api):
                self.assertEqual(list(checker.violations("win32/src/context/New.cpp",
                    f"#if 0\n::{api} /* gap */ (hwnd, &rect, 1);\n#endif")), [(2, api)])

    def test_permissions_are_api_specific_and_exact_paths(self):
        for path, apis in checker.ALLOWLIST.items():
            for api in apis:
                self.assertEqual(list(checker.violations(path, f"{api}(x);")), [])
                self.assertTrue(list(checker.violations(path + ".copy.cpp", f"{api}(x);")))
        self.assertTrue(list(checker.violations("win32/src/Win32Window.cpp", "MulDiv(1, 2, 3);")))

    def test_null_invalidation_and_noncode(self):
        source = '''// MoveWindow(x);
/* SetWindowPos(x); */
const char *s = "CreateWindowExW(x)";
InvalidateRect(hwnd, NULL, TRUE);
InvalidateRect(hwnd, 0, TRUE);
InvalidateRect(hwnd, &r, TRUE);
'''
        self.assertEqual(list(checker.violations("win32/src/context/New.cpp", source)), [(6, "InvalidateRect")])

    def test_header_multiline_and_spliced_calls(self):
        self.assertEqual(list(checker.violations("win32/src/New.hpp", "Move\\\nWindow\n(hwnd);")), [(1, "MoveWindow")])

    def test_tree_scope(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("win32/src/New.hpp", "win32/src/nested/New.cpp", "tests/Win32Fixture.cpp"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("MoveWindow(hwnd, 0, 0, 1, 1, TRUE);", encoding="utf-8")
            files, findings = checker.check_tree(root)
            self.assertEqual(len(files), 2)
            self.assertEqual(len(findings), 2)


if __name__ == "__main__":
    unittest.main()
