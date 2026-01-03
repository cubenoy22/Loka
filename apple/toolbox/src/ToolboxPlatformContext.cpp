#include "ToolboxPlatformContext.hpp"
#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"
#include "core/AppConfigurable.hpp"
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

declara::core::scene::NodeContext *ToolboxPlatformContext::createNodeContext(declara::core::scene::Node *node) const
{
  declara::core::scene::NativeNodeContext *context = new declara::core::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
