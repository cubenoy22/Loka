#include "MacBitmapCapture.hpp"
#include "app/internal/NativeResourceReservation.hpp"
#include <AppKit/AppKit.h>

namespace
{
  void ReleaseCapturedBitmap(void *handle)
  {
    if (handle)
      [(NSBitmapImageRep *)handle release];
  }
}

namespace loka
{
  namespace macos
  {
    // Deliberate rail twin of Win32 CaptureWindowClientBitmap: reserve before
    // acquisition, finish native construction, then consume at publication.
    bool CaptureViewBitmap(PlatformContext *context, void *nativeView, loka::core::resource::Image &out)
    {
      out = loka::core::resource::Image::Empty();
      NSView *view = (NSView *)nativeView;
      if (!context || !view)
        return false;
      NSRect bounds = [view bounds];
      if (bounds.size.width <= 0 || bounds.size.height <= 0)
        return false;
      loka::app::internal::Reservation reservation(*context, &ReleaseCapturedBitmap);
      if (!reservation.isValid())
        return false;
      NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:bounds];
      if (!bitmap)
        return false;
      [bitmap retain];
      [view cacheDisplayInRect:bounds toBitmapImageRep:bitmap];
      return reservation.publishImage(bitmap, (int)bounds.size.width, (int)bounds.size.height, out);
    }
  }
}
