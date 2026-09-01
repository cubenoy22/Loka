#include "Win32DisplayFont.hpp"
#include <cassert>

namespace
{
  typedef BOOL(WINAPI *SystemParametersInfoForDpiFn)(UINT, UINT, PVOID, UINT, UINT);

  SystemParametersInfoForDpiFn ResolveSystemParametersInfoForDpi()
  {
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    return user32
               ? reinterpret_cast<SystemParametersInfoForDpiFn>(
                     GetProcAddress(user32, "SystemParametersInfoForDpi"))
               : 0;
  }

  bool ReadMessageFont(const loka::win32::Win32DisplayScale &scale,
                       LOGFONTW &out)
  {
    NONCLIENTMETRICSW metrics;
    ZeroMemory(&metrics, sizeof(metrics));
    metrics.cbSize = sizeof(metrics);
    static SystemParametersInfoForDpiFn systemParametersInfoForDpi =
        ResolveSystemParametersInfoForDpi();
    if (systemParametersInfoForDpi
        && systemParametersInfoForDpi(
            SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, scale.dpi()))
    {
      out = metrics.lfMessageFont;
      return true;
    }
    if (!SystemParametersInfoW(
            SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0))
    {
      // XP's NONCLIENTMETRICS predates iPaddedBorderWidth. A binary built
      // against a modern SDK must retry with the older structure size.
      metrics.cbSize = sizeof(metrics) - sizeof(int);
      if (!SystemParametersInfoW(
              SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0))
      {
        return false;
      }
    }
    const loka::win32::Win32DisplayScale systemScale =
        loka::win32::Win32DisplayScale::forSystem();
    out = metrics.lfMessageFont;
    out.lfHeight = MulDiv(out.lfHeight,
                          static_cast<int>(scale.dpi()),
                          static_cast<int>(systemScale.dpi()));
    out.lfWidth = MulDiv(out.lfWidth,
                         static_cast<int>(scale.dpi()),
                         static_cast<int>(systemScale.dpi()));
    return true;
  }
} // namespace

namespace loka
{
  namespace win32
  {
    Win32DisplayFont::Win32DisplayFont()
        : font_(0)
    {
    }

    Win32DisplayFont::~Win32DisplayFont()
    {
      if (this->font_)
      {
        DeleteObject(this->font_);
        this->font_ = 0;
      }
    }

    bool Win32DisplayFont::create(const Win32DisplayScale &scale)
    {
      assert(!this->font_ && "a display font must be created into an empty owner");
      if (this->font_)
      {
        return false;
      }
      LOGFONTW logicalFont;
      ZeroMemory(&logicalFont, sizeof(logicalFont));
      if (!ReadMessageFont(scale, logicalFont))
      {
        return false;
      }
      HFONT replacement = CreateFontIndirectW(&logicalFont);
      if (!replacement)
      {
        return false;
      }
      this->font_ = replacement;
      return true;
    }

    void Win32DisplayFont::swap(Win32DisplayFont &other)
    {
      HFONT temporary = this->font_;
      this->font_ = other.font_;
      other.font_ = temporary;
    }
  } // namespace win32
} // namespace loka
