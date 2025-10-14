#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"
#include "core/Window.hpp"
#include "core/AppConfigurable.hpp"
#include "core2/scene/Node.hpp"

// 明示的なデフォルトコンストラクタ・デストラクタ実装（リンカエラー回避用）
Win32PlatformContext::Win32PlatformContext() {}
Win32PlatformContext::~Win32PlatformContext() {}

App *Win32PlatformContext::createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const
{
  App *app = new Win32App(config, hInstance, nCmdShow);
  return app;
}

Window *Win32PlatformContext::createWindow(declara::core::scene::Scene *initialScene, const WindowOptions &opts)
{
  return new Win32Window(this, initialScene, opts);
}

declara::core::scene::NodeContext *Win32PlatformContext::createNodeContext(declara::core::scene::Node *node) const
{
  // TODO: 実際のNodeContext実装が必要
  return new declara::core::scene::NodeContext();
}
