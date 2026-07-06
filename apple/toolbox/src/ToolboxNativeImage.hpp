#ifndef LOKA_TOOLBOX_NATIVE_IMAGE_HPP
#define LOKA_TOOLBOX_NATIVE_IMAGE_HPP

#include <Quickdraw.h>
#include <vector>
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"

namespace loka
{
  namespace toolbox
  {
    enum ToolboxNativeImageKind
    {
      TOOLBOX_NATIVE_IMAGE_KIND_UNKNOWN = 0,
      TOOLBOX_NATIVE_IMAGE_KIND_PICT = 1,
      TOOLBOX_NATIVE_IMAGE_KIND_PICT_BYTES = 2
    };

    struct ToolboxPictBytesPayload
    {
      // Shares the source Blob's refcounted buffer instead of copying the PICT
      // bytes a second time; the streaming getPicProc reads straight from it.
      loka::core::resource::Blob blob;
      std::size_t pictureOffset;
      ToolboxPictBytesPayload()
          : blob(),
            pictureOffset(0)
      {
      }
    };

    struct ToolboxNativeImage
    {
      unsigned long magic;
      short kind;
      void *payload;
      unsigned char ownsPayload;
    };

    loka::core::resource::Image MakeImageFromPicHandle(PicHandle picture, int width, int height, bool takeOwnership);
    loka::core::resource::Image
    MakeImageFromPictBlob(const loka::core::resource::Blob &blob, std::size_t pictureOffset, int width, int height);

    const ToolboxNativeImage *TryGetToolboxNativeImage(const loka::core::resource::Image &image);

    const unsigned long kToolboxNativeImageMagic = 0x4C4F4B41UL; // 'LOKA'
  } // namespace toolbox
} // namespace loka

#endif // LOKA_TOOLBOX_NATIVE_IMAGE_HPP
