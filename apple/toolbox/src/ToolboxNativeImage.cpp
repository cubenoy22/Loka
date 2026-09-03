#include "ToolboxNativeImage.hpp"
#include <cstring>
#include <vector>

namespace
{
  static const loka::toolbox::ToolboxPictBytesPayload *gMaskPictBytes = 0;
  static std::size_t gMaskPictReadPos = 0;
  static QDGetPicUPP gMaskGetPicUPP = 0;
  static QDProcs gMaskPictProcs;

  static pascal void ReadMaskPictBytes(void *dataPtr, short byteCount)
  {
    if (!gMaskPictBytes || !dataPtr || byteCount <= 0)
    {
      return;
    }
    const std::vector<unsigned char> &bytes = gMaskPictBytes->blob.bytes();
    std::size_t end = gMaskPictBytes->pictureEnd;
    if (end > bytes.size())
    {
      end = bytes.size();
    }
    unsigned char *destination = static_cast<unsigned char *>(dataPtr);
    long remaining = byteCount;
    while (remaining > 0)
    {
      if (gMaskPictReadPos >= end)
      {
        std::memset(destination, 0, static_cast<std::size_t>(remaining));
        return;
      }
      std::size_t available = end - gMaskPictReadPos;
      std::size_t chunk = static_cast<std::size_t>(remaining);
      if (chunk > available)
      {
        chunk = available;
      }
      std::memcpy(destination, &bytes[gMaskPictReadPos], chunk);
      destination += chunk;
      gMaskPictReadPos += chunk;
      remaining -= static_cast<long>(chunk);
    }
  }

  bool DrawNativePict(const loka::toolbox::ToolboxNativeImage *native, const Rect &destinationRect)
  {
    if (!native || !native->payload)
    {
      return false;
    }
    if (native->kind == loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT)
    {
      DrawPicture(static_cast<PicHandle>(native->payload), &destinationRect);
      return true;
    }
    if (native->kind != loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT_BYTES)
    {
      return false;
    }

    const loka::toolbox::ToolboxPictBytesPayload *payload =
        static_cast<const loka::toolbox::ToolboxPictBytesPayload *>(native->payload);
    const std::vector<unsigned char> &bytes = payload->blob.bytes();
    const std::size_t headerSize = sizeof(Picture) + sizeof(long) * 8;
    if (payload->pictureEnd > bytes.size() || payload->pictureOffset + headerSize > payload->pictureEnd)
    {
      return false;
    }
    PicHandle picture = (PicHandle)NewHandle(static_cast<Size>(headerSize));
    if (!picture || !*picture)
    {
      return false;
    }
    HLock((Handle)picture);
    std::memcpy(*picture, &bytes[payload->pictureOffset], headerSize);
    HUnlock((Handle)picture);
    if (!gMaskGetPicUPP)
    {
      gMaskGetPicUPP = NewQDGetPicUPP(ReadMaskPictBytes);
    }
    if (!gMaskGetPicUPP)
    {
      // CFM builds allocate the routine descriptor, so under heap pressure it
      // can be null; DrawPicture would then stream through a null callback.
      // Report the allocation failure so the caller takes its retry path
      // instead of caching an incomplete mask as a success.
      KillPicture(picture);
      return false;
    }
    GrafPtr port = 0;
    GetPort(&port);
    QDProcsPtr oldProcs = port ? port->grafProcs : 0;
    if (oldProcs)
    {
      gMaskPictProcs = *oldProcs;
    }
    else
    {
      SetStdProcs(&gMaskPictProcs);
    }
    gMaskPictProcs.getPicProc = gMaskGetPicUPP;
    if (port)
    {
      port->grafProcs = &gMaskPictProcs;
    }
    gMaskPictBytes = payload;
    gMaskPictReadPos = payload->pictureOffset + sizeof(Picture);
    DrawPicture(picture, &destinationRect);
    gMaskPictBytes = 0;
    gMaskPictReadPos = 0;
    if (port)
    {
      port->grafProcs = oldProcs;
    }
    KillPicture(picture);
    return true;
  }

  bool BuildBinaryMask(loka::toolbox::ToolboxNativeImage *native, int width, int height)
  {
    // Classic PICT has no alpha channel. On first sprite use, render it into a
    // one-bit port: QuickDraw white (zero bits) is the transparent color key
    // and black (one bits) is both the CopyMask source and mask.
    if (!native || width <= 0 || width > 32767 || height <= 0 || height > 32767)
    {
      return false;
    }
    const short maskWidth = static_cast<short>(width);
    const short maskHeight = static_cast<short>(height);
    const short rowBytes = static_cast<short>(((maskWidth + 15) / 16) * 2);
    Ptr pixels = NewPtrClear(static_cast<long>(rowBytes) * maskHeight);
    if (!pixels)
    {
      return false;
    }
    native->binaryMask.baseAddr = pixels;
    native->binaryMask.rowBytes = rowBytes;
    SetRect(&native->binaryMask.bounds, 0, 0, maskWidth, maskHeight);

    GrafPtr previousPort = 0;
    GetPort(&previousPort);
    GrafPort maskPort;
    OpenPort(&maskPort);
    SetPort(&maskPort);
    SetPortBits(&native->binaryMask);
    PortSize(maskWidth, maskHeight);
    Rect sourceRect;
    SetRect(&sourceRect, 0, 0, maskWidth, maskHeight);
    BackColor(whiteColor);
    ForeColor(blackColor);
    EraseRect(&sourceRect);
    const bool drewPicture = DrawNativePict(native, sourceRect);
    SetPort(previousPort);
    ClosePort(&maskPort);
    if (!drewPicture)
    {
      DisposePtr(native->binaryMask.baseAddr);
      native->binaryMask.baseAddr = 0;
      return false;
    }
    return true;
  }

  void ReleaseToolboxNativeImage(void *handle, void *)
  {
    loka::toolbox::ToolboxNativeImage *native = static_cast<loka::toolbox::ToolboxNativeImage *>(handle);
    if (!native)
    {
      return;
    }

    if (native->binaryMask.baseAddr)
    {
      DisposePtr(native->binaryMask.baseAddr);
      native->binaryMask.baseAddr = 0;
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

    const BitMap *PrepareToolboxBinaryMask(const loka::core::resource::Image &image)
    {
      ToolboxNativeImage *native = static_cast<ToolboxNativeImage *>(image.nativeHandle());
      if (!image.isValid() || !native || native->magic != kToolboxNativeImageMagic)
      {
        return 0;
      }
      if (!native->binaryMask.baseAddr && !BuildBinaryMask(native, image.width(), image.height()))
      {
        return 0;
      }
      return &native->binaryMask;
    }
  } // namespace toolbox
} // namespace loka
