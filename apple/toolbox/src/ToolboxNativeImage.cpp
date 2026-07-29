#include "ToolboxNativeImage.hpp"

namespace
{
  void ReleaseToolboxNativeImage(void *handle, void *)
  {
    loka::toolbox::ToolboxNativeImage *native = static_cast<loka::toolbox::ToolboxNativeImage *>(handle);
    if (!native)
    {
      return;
    }

    if (native->ownsPayload && native->payload)
    {
      if (native->kind == loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT)
      {
        KillPicture(static_cast<PicHandle>(native->payload));
      }
      else if (native->kind == loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT_BYTES)
      {
        delete static_cast<loka::toolbox::ToolboxPictBytesPayload *>(native->payload);
      }
    }

    delete native;
  }
} // namespace

namespace loka
{
  namespace toolbox
  {
    loka::core::resource::Image MakeImageFromPicHandle(PicHandle picture, int width, int height, bool takeOwnership)
    {
      if (!picture)
      {
        return loka::core::resource::Image::Empty();
      }

      ToolboxNativeImage *native = new ToolboxNativeImage();
      native->magic = kToolboxNativeImageMagic;
      native->kind = TOOLBOX_NATIVE_IMAGE_KIND_PICT;
      native->payload = picture;
      native->ownsPayload = takeOwnership ? 1 : 0;

      return loka::core::resource::Image::FromNative(native, width, height, &ReleaseToolboxNativeImage, 0);
    }

    loka::core::resource::Image
    MakeImageFromPictBlob(const loka::core::resource::Blob &blob,
                          std::size_t pictureOffset,
                          std::size_t pictureEnd,
                          int width,
                          int height)
    {
      if (pictureOffset >= pictureEnd || pictureEnd > blob.bytes().size() || width <= 0 || height <= 0)
      {
        return loka::core::resource::Image::Empty();
      }

      ToolboxPictBytesPayload *payload = new ToolboxPictBytesPayload();
      // Share the source buffer only when the Blob is a stable snapshot
      // (completed and immutable); the streamed bytes must stay consistent
      // with the width/height parsed here. For a mutable or still-loading
      // Blob, take an owned copy so later setBytes()/mutableBytes() can't
      // desync the rendered picture from its reported size — matching the
      // snapshot behavior of the macOS/Win32 decoders.
      if (blob.isCompleted() && !blob.isMutable())
      {
        payload->blob = blob;
        payload->pictureOffset = pictureOffset;
        payload->pictureEnd = pictureEnd;
      }
      else
      {
        // Only the picture's range. The API's contract is that no
        // implementation reads outside the supplied range, and a bag-sized
        // mutable blob copied whole per image would be the doubling this
        // seam exists to avoid. The snapshot is its own coordinate system --
        // rebasing to zero here is not the cross-boundary double-count the
        // design guards against, because the payload stores blob and offsets
        // as one consistent pair.
        const std::vector<unsigned char> &source = blob.bytes();
        loka::core::resource::Blob snapshot = loka::core::resource::Blob::Create();
        snapshot.setBytes(std::vector<unsigned char>(source.begin() + pictureOffset,
                                                     source.begin() + pictureEnd));
        snapshot.setCompleted(true);
        payload->blob = snapshot;
        payload->pictureOffset = 0;
        payload->pictureEnd = pictureEnd - pictureOffset;
      }

      ToolboxNativeImage *native = new ToolboxNativeImage();
      native->magic = kToolboxNativeImageMagic;
      native->kind = TOOLBOX_NATIVE_IMAGE_KIND_PICT_BYTES;
      native->payload = payload;
      native->ownsPayload = 1;

      return loka::core::resource::Image::FromNative(native, width, height, &ReleaseToolboxNativeImage, 0);
    }

    const ToolboxNativeImage *TryGetToolboxNativeImage(const loka::core::resource::Image &image)
    {
      if (!image.isValid())
      {
        return 0;
      }
      const ToolboxNativeImage *native = static_cast<const ToolboxNativeImage *>(image.nativeHandle());
      if (!native || native->magic != kToolboxNativeImageMagic)
      {
        return 0;
      }
      return native;
    }
  } // namespace toolbox
} // namespace loka
