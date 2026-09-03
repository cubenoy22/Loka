#include "MacRectSurfacePaintTests.hpp"
#include "support/TestVerify.hpp"
#include "MacObjCCompat.hpp"
#include "context/MacRectSurfaceContext.hpp"
#include "core/resource/Image.hpp"
#include <AppKit/AppKit.h>

namespace
{
  void ReleaseRectSurfaceTestImage(void *handle, void *)
  {
    [(NSImage *)handle release];
  }

  void ReleaseRectSurfaceTestRep(void *handle, void *)
  {
    [(NSBitmapImageRep *)handle release];
  }

  // Paints the prepared image over a 2x1 red target and verifies both pixels
  // came out as the opaque green the source carried.
  void VerifyPreparedRectSurfaceImagePaintsGreen(NSImage *preparedImage)
  {
    NSBitmapImageRep *targetRep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:0
                      pixelsWide:2
                      pixelsHigh:1
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                     bytesPerRow:8
                    bitsPerPixel:32];
    LOKA_VERIFY(targetRep != nil);
    unsigned char *targetPixels = [targetRep bitmapData];
    LOKA_VERIFY(targetPixels != 0);
    for (int x = 0; x < 2; ++x)
    {
      targetPixels[x * 4 + 0] = 255;
      targetPixels[x * 4 + 1] = 0;
      targetPixels[x * 4 + 2] = 0;
      targetPixels[x * 4 + 3] = 255;
    }

    NSGraphicsContext *targetContext = [NSGraphicsContext graphicsContextWithBitmapImageRep:targetRep];
    LOKA_VERIFY(targetContext != nil);
    NSGraphicsContext *previousContext = [NSGraphicsContext currentContext];
    [NSGraphicsContext setCurrentContext:targetContext];
    [preparedImage drawInRect:NSMakeRect(0, 0, 2, 1)
                     fromRect:NSZeroRect
                    operation:LOKA_MAC_COMPOSITING_SOURCE_OVER
                     fraction:1.0];
    [targetContext flushGraphics];
    [NSGraphicsContext setCurrentContext:previousContext];

    for (int x = 0; x < 2; ++x)
    {
      const unsigned char *pixel = targetPixels + x * 4;
      LOKA_VERIFY(pixel[0] == 0 && pixel[1] == 255 && pixel[2] == 0 && pixel[3] == 255);
    }

    [targetRep release];
  }
}

void testMacRectSurfaceImagePaintsNoAlphaSourceOpaque()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSBitmapImageRep *sourceRep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:0
                    pixelsWide:2
                    pixelsHigh:1
                 bitsPerSample:8
               samplesPerPixel:3
                      hasAlpha:NO
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:6
                  bitsPerPixel:24];
  LOKA_VERIFY(sourceRep != nil);
  unsigned char *sourcePixels = [sourceRep bitmapData];
  LOKA_VERIFY(sourcePixels != 0);
  for (int x = 0; x < 2; ++x)
  {
    sourcePixels[x * 3 + 0] = 0;
    sourcePixels[x * 3 + 1] = 255;
    sourcePixels[x * 3 + 2] = 0;
  }
  NSImage *sourceImage = [[NSImage alloc] initWithSize:NSMakeSize(2, 1)];
  LOKA_VERIFY(sourceImage != nil);
  [sourceImage addRepresentation:sourceRep];
  [sourceRep release];

  loka::core::resource::Image image = loka::core::resource::Image::FromNative(
      sourceImage, 2, 1, &ReleaseRectSurfaceTestImage, 0);
  LOKA_VERIFY(image.isValid());
  MacRectSurfacePreparedImage preparedSlot;
  NSImage *preparedImage = (NSImage *)preparedSlot.prepare(image);
  LOKA_VERIFY(preparedImage != nil);
  VerifyPreparedRectSurfaceImagePaintsGreen(preparedImage);

  preparedSlot.clear();
  image = loka::core::resource::Image::Empty();
  [pool drain];
}

// MacButtonContext / MacTextContext captureBitmap hand over the
// NSBitmapImageRep itself as the Image's native handle, not an NSImage. The
// prepare door must accept that rep (wrapping it) instead of sending it an
// NSImage-only selector.
void testMacRectSurfaceImagePaintsCapturedRepSource()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSBitmapImageRep *sourceRep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:0
                    pixelsWide:2
                    pixelsHigh:1
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSDeviceRGBColorSpace
                   bytesPerRow:8
                  bitsPerPixel:32];
  LOKA_VERIFY(sourceRep != nil);
  unsigned char *sourcePixels = [sourceRep bitmapData];
  LOKA_VERIFY(sourcePixels != 0);
  for (int x = 0; x < 2; ++x)
  {
    sourcePixels[x * 4 + 0] = 0;
    sourcePixels[x * 4 + 1] = 255;
    sourcePixels[x * 4 + 2] = 0;
    sourcePixels[x * 4 + 3] = 255;
  }

  loka::core::resource::Image image = loka::core::resource::Image::FromNative(
      sourceRep, 2, 1, &ReleaseRectSurfaceTestRep, 0);
  LOKA_VERIFY(image.isValid());
  MacRectSurfacePreparedImage preparedSlot;
  NSImage *preparedImage = (NSImage *)preparedSlot.prepare(image);
  LOKA_VERIFY(preparedImage != nil);
  VerifyPreparedRectSurfaceImagePaintsGreen(preparedImage);

  preparedSlot.clear();
  image = loka::core::resource::Image::Empty();
  [pool drain];
}
