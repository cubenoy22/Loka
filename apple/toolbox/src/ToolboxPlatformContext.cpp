#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "app/AppConfigurable.hpp"
#include "core2/scene/NativeNodeContext.hpp"
#include "core2/scene/Node.hpp"

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

loka::core::scene::NodeContext *ToolboxPlatformContext::createNodeContext(loka::core::scene::Node *node) const
{
  loka::core::scene::NativeNodeContext *context = new loka::core::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
