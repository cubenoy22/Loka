#ifndef LOKA_WIN32_EDIT_TEXT_BRIDGE_HPP
#define LOKA_WIN32_EDIT_TEXT_BRIDGE_HPP

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>

#include "core/String.hpp"
#include "platform/Win32String.hpp"

namespace loka
{
  namespace win32
  {
    /** The EditText native pair must stay UTF-16 end to end: an ANSI EDIT
        control stores IME input in the system codepage, and reading those
        bytes back as UTF-8 destroys any out-of-ASCII text (#160). Creation,
        write, and readback all live here so the whole contract is pinned by
        one committed LokaTestsWin32 test. */
    inline HWND CreateEditTextControl(HWND parent, int x, int y, int width, int height)
    {
      DWORD style = WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL;
      return CreateWindowExW(
          WS_EX_CLIENTEDGE, L"EDIT", L"", style, x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
    }

    inline void ReadEditTextWide(HWND hwnd, std::wstring &out)
    {
      out.clear();
      if (!hwnd)
      {
        return;
      }
      int length = GetWindowTextLengthW(hwnd);
      if (length <= 0)
      {
        return;
      }
      std::vector<wchar_t> buffer(length + 1, L'\0');
      GetWindowTextW(hwnd, &buffer[0], length + 1);
      out.assign(&buffer[0]);
    }

    inline loka::core::String ReadEditTextString(HWND hwnd)
    {
      std::wstring wide;
      ReadEditTextWide(hwnd, wide);
      return loka::core::String(CreateWin32StringFromUtf16(wide.c_str(), wide.size()));
    }

    inline void WriteEditTextString(HWND hwnd, const loka::core::String &value)
    {
      std::wstring wide;
      if (!MaterializeWideString(value, wide))
      {
        wide.clear();
      }
      SetWindowTextW(hwnd, wide.c_str());
    }

  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_EDIT_TEXT_BRIDGE_HPP
