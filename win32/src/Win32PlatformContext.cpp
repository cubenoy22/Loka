#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"
#include "app/Window.hpp"
#include "app/AppConfigurable.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/NativeNodeContext.hpp"

// 明示的なデフォルトコンストラクタ・デストラクタ実装（リンカエラー回避用）
Win32PlatformContext::Win32PlatformContext() {}
Win32PlatformContext::~Win32PlatformContext() {}

App *Win32PlatformContext::createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const
{
  App *app = new Win32App(config, hInstance, nCmdShow);
  return app;
}

Window *Win32PlatformContext::createWindow(const WindowProps &props)
{
  return new Win32Window(this, props);
}

loka::app::scene::NodeContext *Win32PlatformContext::createNodeContext(loka::app::scene::Node *node) const
{
  loka::app::scene::NativeNodeContext *context = new loka::app::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
