#include "MacPlatformContext.hpp"
#include "MacApp.hpp"
#include "MacWindow.hpp"
#include "loka/platform/file/FileHandle.hpp"
#include "app/AppConfigurable.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/NativeNodeContext.hpp"

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
