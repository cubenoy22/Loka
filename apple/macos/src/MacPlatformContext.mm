#include "MacPlatformContext.hpp"
#include "MacApp.hpp"
#include "MacWindow.hpp"
#include "core/AppConfigurable.hpp"
#include "core2/scene/Node.hpp"
#include "core2/scene/NativeNodeContext.hpp"

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

declara::core::scene::NodeContext *MacPlatformContext::createNodeContext(declara::core::scene::Node *node) const
{
  declara::core::scene::NativeNodeContext *context = new declara::core::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
