#include "ToolboxPlatformContext.hpp"

#include "core/resource/BlobRange.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "platform/file/AppLocation.hpp"
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
    return static_cast<unsigned short>((static_cast<unsigned short>(p[0]) << 8) | static_cast<unsigned short>(p[1]));
  }

  static short ReadS16BE(const unsigned char *p)
  {
    return static_cast<short>(ReadU16BE(p));
  }

  static std::size_t FindPictSizeByTerminator(const std::vector<unsigned char> &bytes,
                                              std::size_t offset,
                                              std::size_t limit)
  {
    // PICT end opcode is 0x00FF on word boundary. The scan stops at the range's
    // end rather than the buffer's, so a picture inside a bag cannot be given
    // an extent that runs into the asset stored after it.
    if (offset + 12 > limit)
    {
      return 0;
    }
    std::size_t last = 0;
    for (std::size_t pos = offset + 10; pos + 1 < limit; pos += 2)
    {
      if (bytes[pos] == 0x00 && bytes[pos + 1] == 0xFF)
      {
        last = (pos + 2) - offset;
      }
    }
    return last;
  }

  static bool TryParsePictAt(const std::vector<unsigned char> &bytes,
                             std::size_t limit,
                             std::size_t offset,
                             std::size_t &pictureOffsetOut,
                             std::size_t &pictureSizeOut,
                             int &widthOut,
                             int &heightOut,
                             bool &sizeFieldFallbackOut,
                             bool &zeroSizeFieldOut)
  {
    if (offset + 10 > limit)
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

    if (!(pictureSize >= 10 && pictureSize <= limit - offset))
    {
      pictureSize = FindPictSizeByTerminator(bytes, offset, limit);
      if (pictureSize >= 10 && pictureSize <= limit - offset)
      {
        sizeFieldFallbackOut = true;
      }
      else
      {
        // Keep stream path permissive: if size field/terminator are unreliable,
        // draw from the rest of the RANGE like SimpleText's file-backed path.
        // Bounded by the range and not the buffer, so a permissive extent still
        // cannot cross into the next asset in a bag.
        pictureSize = limit - offset;
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
                        std::size_t base,
                        std::size_t limit,
                        std::size_t &pictureOffsetOut,
                        std::size_t &pictureSizeOut,
                        int &widthOut,
                        int &heightOut,
                        bool &sizeFieldFallbackOut,
                        bool &zeroSizeFieldOut)
  {
    sizeFieldFallbackOut = false;
    zeroSizeFieldOut = false;
    // Offsets stay absolute within the blob -- `base` is where the range
    // starts, not a new origin. One coordinate system end to end is what keeps
    // the payload from having to add a base back on and double-count it.
    // 1) Raw PICT stream
    if (TryParsePictAt(bytes,
                       limit,
                       base,
                       pictureOffsetOut,
                       pictureSizeOut,
                       widthOut,
                       heightOut,
                       sizeFieldFallbackOut,
                       zeroSizeFieldOut))
    {
      return true;
    }
    // 2) Classic file format with 512-byte header
    if (limit - base > 522
        && TryParsePictAt(bytes,
                          limit,
                          base + 512,
                          pictureOffsetOut,
                          pictureSizeOut,
                          widthOut,
                          heightOut,
                          sizeFieldFallbackOut,
                          zeroSizeFieldOut))
    {
      return true;
    }
    return false;
  }
} // namespace

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
  if (item.base() == loka::file::File::BASE_APPLICATION)
  {
    return loka::platform::file::ResolveApplicationItem(item, out);
  }
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
                                                 std::size_t offset,
                                                 std::size_t length,
                                                 loka::core::resource::Image &out) const
{
  const std::vector<unsigned char> &bytes = blob.bytes();
  out = loka::core::resource::Image::Empty();
  if (!loka::core::resource::BlobRangeIsUsable(bytes.size(), offset, length))
  {
    return false;
  }
  const std::size_t limit = offset + length;

  std::size_t pictureOffset = 0;
  std::size_t pictureSize = 0;
  int width = 0;
  int height = 0;
  bool usedSizeFallback = false;
  bool zeroSizeField = false;
  if (!ParsePict(
          bytes, offset, limit, pictureOffset, pictureSize, width, height, usedSizeFallback, zeroSizeField))
  {
    return false;
  }

  if (pictureSize == 0)
  {
    return false;
  }

  // The extent travels with the offset. It was already computed here and
  // thrown away, which left the draw path clamping to the end of the whole
  // blob -- fine when a blob held one picture, wrong the moment it holds a bag.
  out = loka::toolbox::MakeImageFromPictBlob(blob, pictureOffset, pictureOffset + pictureSize, width, height);
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
