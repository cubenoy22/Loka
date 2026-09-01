#ifndef LOKA_WIN32_DISPLAY_SCALE_HPP
#define LOKA_WIN32_DISPLAY_SCALE_HPP

#include <windows.h>
#include "core/Frame.hpp"

namespace loka
{
  namespace win32
  {
    /** Immutable conversion between Loka logical display units and one
        Win32 window's device-pixel coordinate space. */
    class Win32DisplayScale
    {
    public:
      explicit Win32DisplayScale(UINT dpi = 96);

      static bool queryForWindow(HWND hwnd, Win32DisplayScale &out);
      static Win32DisplayScale forWindow(HWND hwnd);
      static Win32DisplayScale forSystem();

      UINT dpi() const
      {
        return this->dpi_;
      }
      int percent() const;
      int projectEdge(int logicalCoordinate) const;
      int unprojectEdge(int nativeCoordinate) const;
      int projectLength(int logicalLength) const;
      int unprojectLength(int nativeLength) const;
      void projectFrame(const loka::core::Frame &logicalFrame, RECT &nativeRect) const;
      loka::core::Frame unprojectContentFrame(const RECT &nativeWindowRect,
                                              int nativeClientWidth,
                                              int nativeClientHeight) const;
      bool adjustWindowRect(RECT &nativeClientRect,
                            DWORD style,
                            BOOL hasMenu,
                            DWORD exStyle) const;

      bool operator==(const Win32DisplayScale &rhs) const
      {
        return this->dpi_ == rhs.dpi_;
      }
      bool operator!=(const Win32DisplayScale &rhs) const
      {
        return !(*this == rhs);
      }

    private:
      UINT dpi_;
    };
  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_DISPLAY_SCALE_HPP
