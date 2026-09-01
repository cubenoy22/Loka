#include "Win32DisplayScale.hpp"

namespace
{
  const UINT kDefaultDpi = 96;

  typedef UINT(WINAPI *GetDpiForWindowFn)(HWND);
  typedef UINT(WINAPI *GetDpiForSystemFn)();
  typedef BOOL(WINAPI *AdjustWindowRectExForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);

  FARPROC ResolveUser32Procedure(const char *name)
  {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    return user32 ? GetProcAddress(user32, name) : 0;
  }

  UINT ReadSystemDpiFromDeviceContext()
  {
    HDC dc = GetDC(NULL);
    if (!dc)
    {
      return kDefaultDpi;
    }
    const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? static_cast<UINT>(dpi) : kDefaultDpi;
  }
} // namespace

namespace loka
{
  namespace win32
  {
    Win32DisplayScale::Win32DisplayScale(UINT dpi)
        : dpi_(dpi > 0 ? dpi : kDefaultDpi)
    {
    }

    bool Win32DisplayScale::queryForWindow(HWND hwnd, Win32DisplayScale &out)
    {
      static GetDpiForWindowFn getDpiForWindow =
          reinterpret_cast<GetDpiForWindowFn>(ResolveUser32Procedure("GetDpiForWindow"));
      if (hwnd && getDpiForWindow)
      {
        const UINT dpi = getDpiForWindow(hwnd);
        if (dpi > 0)
        {
          out = Win32DisplayScale(dpi);
          return true;
        }
      }
      if (hwnd)
      {
        HDC dc = GetDC(hwnd);
        if (dc)
        {
          const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
          ReleaseDC(hwnd, dc);
          if (dpi > 0)
          {
            out = Win32DisplayScale(static_cast<UINT>(dpi));
            return true;
          }
        }
      }
      return false;
    }

    Win32DisplayScale Win32DisplayScale::forWindow(HWND hwnd)
    {
      Win32DisplayScale result;
      return Win32DisplayScale::queryForWindow(hwnd, result)
                 ? result
                 : Win32DisplayScale::forSystem();
    }

    Win32DisplayScale Win32DisplayScale::forSystem()
    {
      static GetDpiForSystemFn getDpiForSystem =
          reinterpret_cast<GetDpiForSystemFn>(ResolveUser32Procedure("GetDpiForSystem"));
      if (getDpiForSystem)
      {
        const UINT dpi = getDpiForSystem();
        if (dpi > 0)
        {
          return Win32DisplayScale(dpi);
        }
      }
      return Win32DisplayScale(ReadSystemDpiFromDeviceContext());
    }

    int Win32DisplayScale::percent() const
    {
      return MulDiv(static_cast<int>(this->dpi_), 100, static_cast<int>(kDefaultDpi));
    }

    int Win32DisplayScale::projectEdge(int logicalCoordinate) const
    {
      return MulDiv(logicalCoordinate,
                    static_cast<int>(this->dpi_),
                    static_cast<int>(kDefaultDpi));
    }

    int Win32DisplayScale::unprojectEdge(int nativeCoordinate) const
    {
      return MulDiv(nativeCoordinate,
                    static_cast<int>(kDefaultDpi),
                    static_cast<int>(this->dpi_));
    }

    int Win32DisplayScale::projectLength(int logicalLength) const
    {
      return this->projectEdge(logicalLength);
    }

    int Win32DisplayScale::unprojectLength(int nativeLength) const
    {
      return this->unprojectEdge(nativeLength);
    }

    void Win32DisplayScale::projectFrame(const loka::core::Frame &logicalFrame,
                                         RECT &nativeRect) const
    {
      nativeRect.left = this->projectEdge(logicalFrame.x);
      nativeRect.top = this->projectEdge(logicalFrame.y);
      nativeRect.right = this->projectEdge(logicalFrame.x + logicalFrame.width);
      nativeRect.bottom = this->projectEdge(logicalFrame.y + logicalFrame.height);
    }

    loka::core::Frame Win32DisplayScale::unprojectContentFrame(
        const RECT &nativeWindowRect,
        int nativeClientWidth,
        int nativeClientHeight) const
    {
      return loka::core::Frame(this->unprojectEdge(nativeWindowRect.left),
                               this->unprojectEdge(nativeWindowRect.top),
                               this->unprojectLength(nativeClientWidth),
                               this->unprojectLength(nativeClientHeight));
    }

    bool Win32DisplayScale::adjustWindowRect(RECT &nativeClientRect,
                                             DWORD style,
                                             BOOL hasMenu,
                                             DWORD exStyle) const
    {
      static AdjustWindowRectExForDpiFn adjustWindowRectExForDpi =
          reinterpret_cast<AdjustWindowRectExForDpiFn>(
              ResolveUser32Procedure("AdjustWindowRectExForDpi"));
      if (adjustWindowRectExForDpi)
      {
        return adjustWindowRectExForDpi(
                   &nativeClientRect, style, hasMenu, exStyle, this->dpi_)
               != FALSE;
      }
      return AdjustWindowRectEx(&nativeClientRect, style, hasMenu, exStyle) != FALSE;
    }
  } // namespace win32
} // namespace loka
