#include "Win32ImageBlobTests.hpp"
#include "Win32PlatformContext.hpp"
#include "support/NativeImageBlobPin.hpp"
#include <windows.h>

void testWin32ImageDecodeRetainsOutputOwnedBlob()
{
  Win32PlatformContext platform;
  {
    loka::core::resource::Image image;
    native_image_blob_pin::verifyAlias(platform, image);
    BITMAP bitmap = {};
    LOKA_VERIFY(GetObjectW(static_cast<HBITMAP>(image.nativeHandle()),
                           static_cast<int>(sizeof(bitmap)), &bitmap) == static_cast<int>(sizeof(bitmap)));
    LOKA_VERIFY(bitmap.bmWidth == 2 && bitmap.bmHeight == 3);
  }
  native_image_blob_pin::verifyRefusals(platform);
}
