#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"
#include "core/Window.hpp"
#include "core/AppConfigurable.hpp"
#include "core2/scene/Node.hpp"
#include "core2/scene/NativeNodeContext.hpp"

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

declara::core::scene::NodeContext *Win32PlatformContext::createNodeContext(declara::core::scene::Node *node) const
{
  declara::core::scene::NativeNodeContext *context = new declara::core::scene::NativeNodeContext();
  if (context)
  {
    context->setOwner(node);
  }
  return context;
}
