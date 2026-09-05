#ifndef LOKA_WIN32_BITMAP_CAPTURE_HPP
#define LOKA_WIN32_BITMAP_CAPTURE_HPP

#include <windows.h>
#include "core/resource/Image.hpp"

namespace loka
{
  namespace win32
  {
    /** Reads existing client pixels without requesting a repaint. On success,
        out owns the captured HBITMAP; on failure, out is empty. */
    bool CaptureWindowClientBitmap(HWND hwnd, loka::core::resource::Image &out);
  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_BITMAP_CAPTURE_HPP
