#ifndef LOKA_WIN32_EDIT_TEXT_BRIDGE_TESTS_HPP
#define LOKA_WIN32_EDIT_TEXT_BRIDGE_TESTS_HPP

void testWin32EditTextBridgeRoundTripsUtf16();

void testWin32TextEditorConversion();
void testWin32TextEditorActionsUseLineQueries();
void testWin32TextEditorRefusalRestoresAndClearsUndo();
void testWin32TextEditorNestedInput();
void testWin32TextEditorFailedReplacementRetries();
void testWin32TextEditorLayoutDpiAndRetirement();

#endif // LOKA_WIN32_EDIT_TEXT_BRIDGE_TESTS_HPP
