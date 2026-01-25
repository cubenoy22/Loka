#include "MacPlatformContext.hpp"
#include "MacApp.hpp"
#include "MacWindow.hpp"
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
