#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "platform/file/FileHandle.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/scene/Node.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "ToolboxNativeImage.hpp"
#include <cstring>
#include <vector>

namespace
{
  struct SpecBinding
  {
    loka::core::String displayPath;
    FSSpec spec;
  };

  static std::vector<SpecBinding> gChosenSpecs;

  static bool FindChosenSpec(const loka::core::String &displayPath, FSSpec &specOut)
  {
    for (std::size_t i = 0; i < gChosenSpecs.size(); ++i)
    {
      if (gChosenSpecs[i].displayPath.equals(displayPath))
      {
        specOut = gChosenSpecs[i].spec;
        return true;
      }
    }
    return false;
  }
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

  static std::size_t FindPictSizeByTerminator(const std::vector<unsigned char> &bytes,
                                              std::size_t offset)
  {
    // PICT end opcode is 0x00FF on word boundary.
    if (offset + 12 > bytes.size())
    {
      return 0;
    }
    std::size_t last = 0;
    for (std::size_t pos = offset + 10; pos + 1 < bytes.size(); pos += 2)
    {
      if (bytes[pos] == 0x00 && bytes[pos + 1] == 0xFF)
      {
        last = (pos + 2) - offset;
      }
    }
    return last;
  }

  static bool TryParsePictAt(const std::vector<unsigned char> &bytes,
                             std::size_t offset,
                             std::size_t &pictureOffsetOut,
                             std::size_t &pictureSizeOut,
                             int &widthOut,
                             int &heightOut,
                             bool &sizeFieldFallbackOut,
                             bool &zeroSizeFieldOut)
  {
    if (offset + 10 > bytes.size())
    {
      return false;
    }
    sizeFieldFallbackOut = false;
    zeroSizeFieldOut = false;

    const unsigned char *base = &bytes[offset];
    const unsigned short pictSize = ReadU16BE(base);
    zeroSizeFieldOut = (pictSize == 0);
    std::size_t pictureSize = static_cast<std::size_t>(pictSize);

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
    if (width == 0 || height == 0)
    {
      return false;
    }

    if (!(pictureSize >= 10 && offset + pictureSize <= bytes.size()))
    {
      pictureSize = FindPictSizeByTerminator(bytes, offset);
      if (pictureSize >= 10 && offset + pictureSize <= bytes.size())
      {
        sizeFieldFallbackOut = true;
      }
      else
      {
        // Keep stream path permissive: if size field/terminator are unreliable,
        // draw from the remaining bytes like SimpleText's file-backed path.
        pictureSize = bytes.size() - offset;
        if (pictureSize < 10)
        {
          return false;
        }
        sizeFieldFallbackOut = true;
      }
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
                        int &heightOut,
                        bool &sizeFieldFallbackOut,
                        bool &zeroSizeFieldOut)
  {
    sizeFieldFallbackOut = false;
    zeroSizeFieldOut = false;
    // 1) Raw PICT stream
    if (TryParsePictAt(bytes, 0, pictureOffsetOut, pictureSizeOut, widthOut, heightOut, sizeFieldFallbackOut, zeroSizeFieldOut))
    {
      return true;
    }
    // 2) Classic file format with 512-byte header
    if (bytes.size() > 522 &&
        TryParsePictAt(bytes, 512, pictureOffsetOut, pictureSizeOut, widthOut, heightOut, sizeFieldFallbackOut, zeroSizeFieldOut))
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
  FSSpec spec;
  if (FindChosenSpec(out.displayPath, spec))
  {
    out.spec = spec;
    out.hasSpec = true;
  }
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
  bool usedSizeFallback = false;
  bool zeroSizeField = false;
  if (!ParsePict(bytes, pictureOffset, pictureSize, width, height, usedSizeFallback, zeroSizeField))
  {
    return false;
  }

  if (pictureSize == 0)
  {
    return false;
  }

  out = loka::toolbox::MakeImageFromPictBytes(bytes, pictureOffset, width, height);
  return out.isValid();
}

#if defined(LOKA_RETRO68)
void ToolboxPlatformContext::registerChosenFileSpec(const loka::core::String &displayPath, const FSSpec &spec)
{
  for (std::size_t i = 0; i < gChosenSpecs.size(); ++i)
  {
    if (gChosenSpecs[i].displayPath.equals(displayPath))
    {
      gChosenSpecs[i].spec = spec;
      return;
    }
  }
  SpecBinding binding;
  binding.displayPath = displayPath;
  binding.spec = spec;
  gChosenSpecs.push_back(binding);
}
#endif
