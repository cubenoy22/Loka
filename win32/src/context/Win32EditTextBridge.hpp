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
#include "platform/Win32DisplayScale.hpp"

namespace loka
{
  namespace win32
  {
    inline DWORD EditTextControlExStyle()
    {
      return WS_EX_CLIENTEDGE;
    }

    inline DWORD EditTextControlStyle()
    {
      return WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL;
    }

    /** The EditText native pair must stay UTF-16 end to end: an ANSI EDIT
        control stores IME input in the system codepage, and reading those
        bytes back as UTF-8 destroys any out-of-ASCII text (#160). Creation,
        write, and readback all live here so the whole contract is pinned by
        one committed LokaTestsWin32 test. */
    HWND CreateEditTextControl(HWND parent, const NativeRect &geometry);

    /** Legacy test caller coordinates are already device pixels. */
    inline HWND CreateEditTextControl(HWND parent, int x, int y, int width, int height)
    {
      const RECT pixels = {x, y, x + width, y + height};
      return CreateEditTextControl(parent, Win32DisplayScale::fromDevicePixels(pixels));
    }

    /** Deliberate multiline twin of the single-line helpers below. Stage 1
        accepts ASCII only; CRLF and lone CR/LF each represent one logical CR.
        Callers reserve bounded cancellation storage before accepting input. */
    inline bool TextEditorToWide(const std::string &logical, std::wstring &wide)
    {
      wide.clear();
      for (std::size_t i = 0; i < logical.size(); ++i)
      {
        const unsigned char value = static_cast<unsigned char>(logical[i]);
        if (!value || value > 127 || value == '\n')
        {
          wide.clear();
          return false;
        }
        wide += static_cast<wchar_t>(value);
        if (value == '\r')
          wide += L'\n';
      }
      return true;
    }

    inline bool TextEditorFromWide(const wchar_t *wide, std::size_t length, std::string &logical)
    {
      logical.clear();
      for (std::size_t i = 0; i < length; ++i)
      {
        const wchar_t value = wide[i];
        if (!value || value > 127)
        {
          logical.clear();
          return false;
        }
        logical += value == L'\n' ? '\r' : static_cast<char>(value);
        if (value == L'\r' && i + 1 < length && wide[i + 1] == L'\n')
          ++i;
      }
      return true;
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
