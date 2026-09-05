#include "Win32BitmapCapture.hpp"

namespace
{
  void ReleaseCapturedBitmap(void *handle, void *)
  {
    if (handle)
    {
      DeleteObject(static_cast<HBITMAP>(handle));
    }
  }

} // namespace

namespace loka
{
  namespace win32
  {
    bool CaptureWindowClientBitmap(HWND hwnd, loka::core::resource::Image &out)
    {
      out = loka::core::resource::Image::Empty();
      if (!hwnd)
      {
        return false;
      }

      RECT rc;
      if (!GetClientRect(hwnd, &rc))
      {
        return false;
      }
      const int width = rc.right - rc.left;
      const int height = rc.bottom - rc.top;
      if (width <= 0 || height <= 0)
      {
        return false;
      }

      // GetWindowDC includes the non-client frame; the bitmap is client-only.
      POINT clientOrigin = {0, 0};
      RECT windowRect;
      if (!ClientToScreen(hwnd, &clientOrigin) || !GetWindowRect(hwnd, &windowRect))
      {
        return false;
      }

      HDC windowDC = GetWindowDC(hwnd);
      if (!windowDC)
      {
        return false;
      }

      HDC memDC = CreateCompatibleDC(windowDC);
      if (!memDC)
      {
        ReleaseDC(hwnd, windowDC);
        return false;
      }

      HBITMAP bitmap = CreateCompatibleBitmap(windowDC, width, height);
      if (!bitmap)
      {
        DeleteDC(memDC);
        ReleaseDC(hwnd, windowDC);
        return false;
      }

      HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);
      if (!oldBitmap || oldBitmap == HGDI_ERROR)
      {
        DeleteObject(bitmap);
        DeleteDC(memDC);
        ReleaseDC(hwnd, windowDC);
        return false;
      }
      const BOOL copied = BitBlt(memDC, 0, 0, width, height, windowDC,
                                 clientOrigin.x - windowRect.left,
                                 clientOrigin.y - windowRect.top, SRCCOPY);
      SelectObject(memDC, oldBitmap);
      DeleteDC(memDC);
      ReleaseDC(hwnd, windowDC);
      if (!copied)
      {
        DeleteObject(bitmap);
        return false;
      }

      out = loka::core::resource::Image::FromNative(bitmap, width, height, &ReleaseCapturedBitmap, 0);
      return out.isValid();
    }
  } // namespace win32
} // namespace loka
