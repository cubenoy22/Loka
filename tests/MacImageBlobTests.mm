#include "MacImageBlobTests.hpp"
#include "MacPlatformContext.hpp"
#include "support/NativeImageBlobPin.hpp"
#include <AppKit/AppKit.h>

void testMacImageDecodeRetainsOutputOwnedBlob()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  MacPlatformContext platform;
  {
    loka::core::resource::Image image;
    native_image_blob_pin::verifyAlias(platform, image);
    NSImage *native = static_cast<NSImage *>(image.nativeHandle());
    LOKA_VERIFY([native isValid]);
    NSBitmapImageRep *bitmap = [NSBitmapImageRep imageRepWithData:[native TIFFRepresentation]];
    LOKA_VERIFY(bitmap != nil && [bitmap pixelsWide] == 2 && [bitmap pixelsHigh] == 3);
  }
  native_image_blob_pin::verifyRefusals(platform);
  [pool drain];
}
