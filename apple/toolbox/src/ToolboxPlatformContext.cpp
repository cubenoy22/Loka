#include "ToolboxPlatformContext.hpp"

#include "core/resource/BlobRange.hpp"
#include "PictParser.hpp"
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

  loka::toolbox::pict::PictParseResult picture;
  if (!loka::toolbox::pict::ParsePict(
          bytes, offset, limit, picture))
  {
    return false;
  }

  if (picture.pictureSize == 0)
  {
    return false;
  }

  // The picture's end is the RANGE's end, not `pictureOffset + pictureSize`.
  //
  // The size word is 16-bit and is only meaningful for a version 1 picture; a
  // version 2 picture larger than 65535 bytes carries a truncated one. Deriving
  // the stream's end from it therefore cuts a large picture short. Before this
  // change the parsed size was discarded and the draw path ran to the end of
  // the blob, so such pictures rendered -- clamping to the size word would have
  // been a regression dressed up as a bounds fix.
  //
  // What the clamp is actually for is that a blob may hold a whole LRPK bag, so
  // one asset's picture must not stream into the asset stored after it. The
  // range already says exactly that. Trailing bytes *within* one asset's range
  // are still streamed, as they always were; excluding them needs a real
  // version 2 parse, not a field that cannot describe them.
  out = loka::toolbox::MakeImageFromPictBlob(
      blob, picture.pictureOffset, limit, picture.width, picture.height);
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
