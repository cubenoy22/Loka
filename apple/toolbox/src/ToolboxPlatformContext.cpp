#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "loka/platform/file/FileHandle.hpp"
#include "app/AppConfigurable.hpp"
#include "app/scene/NativeNodeContext.hpp"
#include "app/scene/Node.hpp"

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
