#ifndef LOKA_MAC_BITMAP_CAPTURE_HPP
#define LOKA_MAC_BITMAP_CAPTURE_HPP

#include "core/resource/Image.hpp"
class PlatformContext;

namespace loka
{
  namespace macos
  {
    /** Captures existing view pixels. The borrowed context must outlive the
        Image; absent context refuses before native bitmap acquisition.
        Publication consumes the retained bitmap, including allocation refusal.
        On failure out is empty. */
    bool CaptureViewBitmap(PlatformContext *context, void *view, loka::core::resource::Image &out);
  }
}
#endif
