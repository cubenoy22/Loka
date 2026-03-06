#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "loka/platform/file/FileHandle.hpp"
#include "app/AppConfigurable.hpp"
#include "app/scene/NativeNodeContext.hpp"
#include "app/scene/Node.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "ToolboxNativeImage.hpp"
#include <cstring>

namespace
{
  static unsigned short ReadU16BE(const unsigned char *p)
  {
    return static_cast<unsigned short>(
        (static_cast<unsigned short>(p[0]) << 8) |
        static_cast<unsigned short>(p[1]));
  }

  static short ReadS16BE(const unsigned char *p)
  {
    return static_cast<short>(ReadU16BE(p));
  }

  static bool TryParsePictAt(const std::vector<unsigned char> &bytes,
                             std::size_t offset,
                             std::size_t &pictureOffsetOut,
                             std::size_t &pictureSizeOut,
                             int &widthOut,
                             int &heightOut)
  {
    if (offset + 10 > bytes.size())
    {
      return false;
    }

    const unsigned char *base = &bytes[offset];
    const unsigned short pictSize = ReadU16BE(base);
    if (pictSize < 10)
    {
      return false;
    }

    const std::size_t pictureSize = static_cast<std::size_t>(pictSize);
    if (offset + pictureSize > bytes.size())
    {
      return false;
    }

    const short top = ReadS16BE(base + 2);
    const short left = ReadS16BE(base + 4);
    const short bottom = ReadS16BE(base + 6);
    const short right = ReadS16BE(base + 8);

    int width = static_cast<int>(right) - static_cast<int>(left);
    int height = static_cast<int>(bottom) - static_cast<int>(top);
    if (width < 0)
    {
      width = -width;
    }
    if (height < 0)
    {
      height = -height;
    }

    pictureOffsetOut = offset;
    pictureSizeOut = pictureSize;
    widthOut = width;
    heightOut = height;
    return true;
  }

  static bool ParsePict(const std::vector<unsigned char> &bytes,
                        std::size_t &pictureOffsetOut,
                        std::size_t &pictureSizeOut,
                        int &widthOut,
                        int &heightOut)
  {
    // 1) Raw PICT stream
    if (TryParsePictAt(bytes, 0, pictureOffsetOut, pictureSizeOut, widthOut, heightOut))
    {
      return true;
    }
    // 2) Classic file format with 512-byte header
    if (bytes.size() > 522 &&
        TryParsePictAt(bytes, 512, pictureOffsetOut, pictureSizeOut, widthOut, heightOut))
    {
      return true;
    }
    return false;
  }
}

ToolboxPlatformContext::ToolboxPlatformContext() {}
ToolboxPlatformContext::~ToolboxPlatformContext() {}

App *ToolboxPlatformContext::createApp(AppConfigurable *config, HINSTANCE, int) const
{
  return new ToolboxApp(config);
}

Window *ToolboxPlatformContext::createWindow(const WindowProps &props)
{
  return new ToolboxWindow(this, props);
}

loka::app::scene::NodeContext *ToolboxPlatformContext::createNodeContext(loka::app::scene::Node *node) const
{
  loka::app::scene::NativeNodeContext *context = new loka::app::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}

bool ToolboxPlatformContext::openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
{
  out.displayPath = item.toString();
  out.kind = item.kind();
#if defined(LOKA_RETRO68)
  out.hasSpec = false;
#endif
  return !out.displayPath.empty();
}

bool ToolboxPlatformContext::createImageFromBlob(const loka::core::resource::Blob &blob,
                                                 loka::core::resource::Image &out) const
{
  const std::vector<unsigned char> &bytes = blob.bytes();
  out = loka::core::resource::Image::Empty();
  if (bytes.empty())
  {
    return false;
  }

  std::size_t pictureOffset = 0;
  std::size_t pictureSize = 0;
  int width = 0;
  int height = 0;
  if (!ParsePict(bytes, pictureOffset, pictureSize, width, height))
  {
    return false;
  }

  PicHandle picture = (PicHandle)NewHandleClear((Size)pictureSize);
  if (!picture || !*picture)
  {
    return false;
  }

  HLock((Handle)picture);
  std::memcpy(*picture, &bytes[pictureOffset], pictureSize);
  HUnlock((Handle)picture);

  out = loka::toolbox::MakeImageFromPicHandle(picture, width, height, true);
  if (!out.isValid())
  {
    KillPicture(picture);
    return false;
  }
  return true;
}
