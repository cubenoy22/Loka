#include "Win32EditTextBridgeTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include "context/Win32EditTextBridge.hpp"
#include "platform/StringUTF8.hpp"

namespace
{
  // こんにちは + U+1F34E (surrogate pair) — the IME-shaped payload the ANSI
  // path used to destroy (#160).
  const wchar_t kJapanesePayload[] = {0x3053, 0x3093, 0x306B, 0x3061, 0x306F, 0xD83C, 0xDF4E, 0};
  const char kJapanesePayloadUtf8[] = "\xE3\x81\x93\xE3\x82\x93\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\xAF\xF0\x9F\x8D\x8E";

  LRESULT CALLBACK AnsiTextNegativeControlProc(HWND hwnd,
                                               UINT message,
                                               WPARAM wParam,
                                               LPARAM lParam)
  {
    return DefWindowProcA(hwnd, message, wParam, lParam);
  }

  HWND CreateAnsiTextNegativeControl(HWND parent)
  {
    static const char kClassName[] = "LokaAnsiTextNegativeControl";
    WNDCLASSA windowClass = {0};
    windowClass.lpfnWndProc = &AnsiTextNegativeControlProc;
    windowClass.hInstance = GetModuleHandleW(NULL);
    windowClass.lpszClassName = kClassName;
    const ATOM registered = RegisterClassA(&windowClass);
    LOKA_VERIFY(registered || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);

    // Deliberate test-only A-family window: it preserves the lossy code-page
    // boundary that the production UTF-16 EDIT bridge must avoid (#160).
    return CreateWindowExA(WS_EX_CLIENTEDGE,
                           kClassName,
                           "",
                           WS_CHILD | WS_BORDER,
                           0,
                           30,
                           180,
                           24,
                           parent,
                           NULL,
                           GetModuleHandleW(NULL),
                           NULL);
  }
} // namespace

void testWin32EditTextBridgeRoundTripsUtf16()
{
  printf("\n==== [testWin32EditTextBridgeRoundTripsUtf16] start ====\n");
  HWND parent = CreateWindowExW(
      0, L"STATIC", L"edit-bridge-host", WS_OVERLAPPED, 0, 0, 200, 100, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(parent);

  HWND edit = loka::win32::CreateEditTextControl(parent, 0, 0, 180, 24);
  assert(edit);
  LOKA_VERIFY(IsWindowUnicode(edit) &&
              "the EDIT control must be a Unicode window: an ANSI EDIT stores IME input in the system codepage");

  // Readback direction: what an IME commit leaves in the control must reach
  // the logical String losslessly.
  SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(kJapanesePayload));
  loka::core::String read = loka::win32::ReadEditTextString(edit);
  std::string readUtf8;
  LOKA_VERIFY(loka::platform::CollectUtf8(read, readUtf8));
  assert(readUtf8 == kJapanesePayloadUtf8 &&
         "readback must not reinterpret native UTF-16 through the ANSI codepage or as UTF-8 bytes");

  // Write direction: a logical String must land in the control as UTF-16.
  SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
  loka::win32::WriteEditTextString(edit, loka::core::String(std::string(kJapanesePayloadUtf8)));
  std::wstring wrote;
  loka::win32::ReadEditTextWide(edit, wrote);
  assert(wrote == std::wstring(kJapanesePayload) &&
         "write must materialize the logical String as UTF-16, not ANSI bytes");

  // Documented pre-fix failure shape: the same payload through an ANSI window
  // and a bytes-as-UTF-8 readback does NOT survive. This contrast is the
  // reason the bridge exists; it must keep failing if someone "simplifies"
  // the bridge back to the A path.
  HWND ansiEdit = CreateAnsiTextNegativeControl(parent);
  LOKA_VERIFY(ansiEdit && !IsWindowUnicode(ansiEdit));
  SendMessageW(ansiEdit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(kJapanesePayload));
  int ansiLen = GetWindowTextLengthA(ansiEdit);
  std::string ansiBytes(static_cast<std::size_t>(ansiLen > 0 ? ansiLen : 0), '\0');
  if (ansiLen > 0)
  {
    GetWindowTextA(ansiEdit, &ansiBytes[0], ansiLen + 1);
  }
  assert(ansiBytes != kJapanesePayloadUtf8 &&
         "the ANSI readback path cannot represent the payload; if this ever matches, the contrast pin is stale");

  DestroyWindow(ansiEdit);
  DestroyWindow(edit);
  DestroyWindow(parent);
  printf("==== [testWin32EditTextBridgeRoundTripsUtf16] end ====\n");
}
