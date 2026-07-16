#include "platform/null/NullPlatformContext.hpp"

#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/null/NullApp.hpp"
#include "platform/null/NullWindow.hpp"

NullPlatformContext::NullPlatformContext() {}

NullPlatformContext::~NullPlatformContext() {}

App *NullPlatformContext::createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const
{
  (void)hInstance;
  (void)nCmdShow;
  return new NullApp(config);
}

Window *NullPlatformContext::createWindow(const WindowProps &props)
{
  return new NullWindow(this, props);
}

loka::app::scene::NodeContext *
NullPlatformContext::createNodeContext(loka::app::scene::Node *node) const
{
  loka::app::scene::NativeNodeContext *context = new loka::app::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}

bool NullPlatformContext::openFile(const loka::file::File &item,
                                   loka::platform::file::FileHandle &out) const
{
  (void)item;
  out = loka::platform::file::FileHandle();
  return false;
}

bool NullPlatformContext::createImageFromBlob(const loka::core::resource::Blob &blob,
                                              loka::core::resource::Image &out) const
{
  (void)blob;
  out = loka::core::resource::Image::Empty();
  return false;
}
