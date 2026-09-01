#include "Win32DisplayScale.hpp"

namespace
{
  const UINT kDefaultDpi = 96;

  typedef UINT(WINAPI *GetDpiForWindowFn)(HWND);
  typedef UINT(WINAPI *GetDpiForSystemFn)();
  typedef HRESULT(WINAPI *GetDpiForMonitorFn)(HMONITOR, int, UINT *, UINT *);
  typedef BOOL(WINAPI *AdjustWindowRectExForDpiFn)(LPRECT, DWORD, BOOL, DWORD, UINT);

  /** Process-lifetime capability box that keeps Shcore loaded for as long as
      its resolved GetDpiForMonitor entry point can be called. */
  class MonitorDpiQuery
  {
  public:
    MonitorDpiQuery()
        : module_(0),
          function_(0)
    {
      // XP rejects this flag and follows the DC fallback. Windows 8.1 can
      // load Shcore only from System32 without opening a DLL search path.
      const DWORD kLoadLibrarySearchSystem32 = 0x00000800;
      this->module_ = LoadLibraryExW(
          L"shcore.dll", NULL, kLoadLibrarySearchSystem32);
      if (this->module_)
      {
        this->function_ = reinterpret_cast<GetDpiForMonitorFn>(
            GetProcAddress(this->module_, "GetDpiForMonitor"));
      }
    }

    ~MonitorDpiQuery()
    {
      if (this->module_)
      {
        FreeLibrary(this->module_);
        this->module_ = 0;
        this->function_ = 0;
      }
    }

    bool read(HWND hwnd, UINT &out) const
    {
      const HMONITOR monitor = MonitorFromWindow(
          hwnd, MONITOR_DEFAULTTONEAREST);
      UINT dpiX = 0;
      UINT dpiY = 0;
      if (!this->function_ || !monitor
          || FAILED(this->function_(monitor, 0, &dpiX, &dpiY))
          || dpiX == 0)
      {
        return false;
      }
      out = dpiX;
      return true;
    }

  private:
    HMODULE module_;
    GetDpiForMonitorFn function_;

    MonitorDpiQuery(const MonitorDpiQuery &);
    MonitorDpiQuery &operator=(const MonitorDpiQuery &);
  };

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

  bool ReadWindowMonitorDpi(HWND hwnd, UINT &out)
  {
    static MonitorDpiQuery query;
    return query.read(hwnd, out);
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
      UINT monitorDpi = 0;
      if (hwnd && ReadWindowMonitorDpi(hwnd, monitorDpi))
      {
        out = Win32DisplayScale(monitorDpi);
        return true;
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

    int Win32DisplayScale::scaleLengthFrom(
        const Win32DisplayScale &sourceScale,
        int sourceLength) const
    {
      return MulDiv(sourceLength,
                    static_cast<int>(this->dpi_),
                    static_cast<int>(sourceScale.dpi()));
    }

    void Win32DisplayScale::projectFrame(const loka::core::Frame &logicalFrame,
                                         RECT &nativeRect) const
    {
      nativeRect.left = this->projectEdge(logicalFrame.x);
      nativeRect.top = this->projectEdge(logicalFrame.y);
      nativeRect.right = this->projectEdge(logicalFrame.x + logicalFrame.width);
      nativeRect.bottom = this->projectEdge(logicalFrame.y + logicalFrame.height);
    }

    loka::core::Frame Win32DisplayScale::windowContentFrameFromNative(
        const RECT &nativeWindowRect,
        int nativeClientWidth,
        int nativeClientHeight) const
    {
      return loka::core::Frame(nativeWindowRect.left,
                               nativeWindowRect.top,
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
      RECT systemAdjusted = nativeClientRect;
      if (!AdjustWindowRectEx(&systemAdjusted, style, hasMenu, exStyle))
      {
        return false;
      }
      // The legacy API returns non-client edge deltas at system DPI. Preserve
      // the already projected client rect and move each edge by the same
      // metric expressed at the target monitor DPI.
      const Win32DisplayScale systemScale = Win32DisplayScale::forSystem();
      nativeClientRect.left += this->scaleLengthFrom(
          systemScale,
          static_cast<int>(systemAdjusted.left - nativeClientRect.left));
      nativeClientRect.top += this->scaleLengthFrom(
          systemScale,
          static_cast<int>(systemAdjusted.top - nativeClientRect.top));
      nativeClientRect.right += this->scaleLengthFrom(
          systemScale,
          static_cast<int>(systemAdjusted.right - nativeClientRect.right));
      nativeClientRect.bottom += this->scaleLengthFrom(
          systemScale,
          static_cast<int>(systemAdjusted.bottom - nativeClientRect.bottom));
      return true;
    }
  } // namespace win32
} // namespace loka
