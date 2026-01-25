#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
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
