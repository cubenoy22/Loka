#ifndef LOKA_WIN32_BITMAP_CAPTURE_HPP
#define LOKA_WIN32_BITMAP_CAPTURE_HPP

#include <windows.h>
#include "core/resource/Image.hpp"

class PlatformContext;

namespace loka
{
  namespace win32
  {
    /** Reads existing client pixels without requesting a repaint. On success,
        out holds the captured HBITMAP until context retirement. The borrowed
        context must outlive the Image; absent context refuses before DC acquisition.
        On failure, out is empty. */
    bool CaptureWindowClientBitmap(PlatformContext *context, HWND hwnd, loka::core::resource::Image &out);
  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_BITMAP_CAPTURE_HPP
