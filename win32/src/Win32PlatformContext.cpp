#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"
#include "core/Window.hpp"
#include "core/AppConfigurable.hpp"

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
