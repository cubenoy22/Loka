#include "platform/null/NullPlatformContext.hpp"

#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "platform/StringUTF8.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/null/NullApp.hpp"
#include "platform/null/NullWindow.hpp"

NullPlatformContext::NullPlatformContext()
    : applicationDirectory_(),
      hasApplicationDirectory_(false)
{
}

NullPlatformContext::~NullPlatformContext() {}

void NullPlatformContext::setApplicationDirectory(const loka::core::String &dir)
{
  std::string bytes;
  this->applicationDirectory_ = dir;
  this->hasApplicationDirectory_ =
      loka::platform::CollectUtf8(this->applicationDirectory_, bytes) && !bytes.empty();
}

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
  out = loka::platform::file::FileHandle();
  if (item.base() != loka::file::File::BASE_APPLICATION)
  {
    return false;
  }
  if (!loka::platform::file::ApplicationRelativeIsOpenable(item)
      || !this->hasApplicationDirectory_)
  {
    return false;
  }
  out.displayPath =
      this->applicationDirectory_ + loka::core::String::Literal("/") + item.relativePath();
  out.kind = item.kind();
  return true;
}

bool NullPlatformContext::createImageFromBlob(const loka::core::resource::Blob &blob,
                                              loka::core::resource::Image &out) const
{
  (void)blob;
  out = loka::core::resource::Image::Empty();
  return false;
}
