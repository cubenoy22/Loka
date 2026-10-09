#include "Win32BitmapCapture.hpp"
#include "app/internal/NativeResourceReservation.hpp"

namespace
{
  void ReleaseCapturedBitmap(void *handle)
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
    bool CaptureWindowClientBitmap(PlatformContext *context, HWND hwnd, loka::core::resource::Image &out)
    {
      out = loka::core::resource::Image::Empty();
      if (!context || !hwnd)
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

      loka::app::internal::Reservation reservation(*context, &ReleaseCapturedBitmap);
      if (!reservation.isValid())
        return false;

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

      return reservation.publishImage(bitmap, width, height, out);
    }
  } // namespace win32
} // namespace loka
