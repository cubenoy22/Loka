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

loka::core::scene::NodeContext *MacPlatformContext::createNodeContext(loka::core::scene::Node *node) const
{
  loka::core::scene::NativeNodeContext *context = new loka::core::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
