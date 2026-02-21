#include "MacPlatformContext.hpp"
#include "MacApp.hpp"
#include "MacWindow.hpp"
#include "loka/platform/file/FileHandle.hpp"
#include "app/AppConfigurable.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/NativeNodeContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include <AppKit/AppKit.h>

MacPlatformContext::MacPlatformContext() {}
MacPlatformContext::~MacPlatformContext() {}

App *MacPlatformContext::createApp(AppConfigurable *config, HINSTANCE, int) const
{
  return new MacApp(config);
}

Window *MacPlatformContext::createWindow(const WindowProps &props)
{
  return new MacWindow(this, props);
}

loka::app::scene::NodeContext *MacPlatformContext::createNodeContext(loka::app::scene::Node *node) const
{
  loka::app::scene::NativeNodeContext *context = new loka::app::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}

bool MacPlatformContext::openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
{
  out.displayPath = item.toString();
  out.kind = item.kind();
  return !out.displayPath.empty();
}

namespace
{
  void ReleaseNSImage(void *handle, void *)
  {
    if (handle)
    {
      [(NSImage *)handle release];
    }
  }
}

bool MacPlatformContext::createImageFromBlob(const loka::core::resource::Blob &blob,
                                             loka::core::resource::Image &out) const
{
  out = loka::core::resource::Image::Empty();
  if (!blob.isValid())
  {
    return false;
  }
  const std::vector<unsigned char> &bytes = blob.bytes();
  if (bytes.empty())
  {
    return false;
  }

  NSData *data = [NSData dataWithBytes:&bytes[0] length:bytes.size()];
  if (!data)
  {
    return false;
  }
  NSImage *image = [[NSImage alloc] initWithData:data];
  if (!image)
  {
    return false;
  }
  NSSize size = [image size];
  out = loka::core::resource::Image::FromNative(image,
                                                static_cast<int>(size.width),
                                                static_cast<int>(size.height),
                                                &ReleaseNSImage,
                                                0);
  return out.isValid();
}
