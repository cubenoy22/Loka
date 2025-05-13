#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"          // Win32Windowの実装があれば
#include "core/Window.hpp"          // Windowクラスの宣言を明示的に追加
#include "core/AppConfigurable.hpp" // AppConfigurableの宣言を明示的に追加

// 明示的なデフォルトコンストラクタ・デストラクタ実装（リンカエラー回避用）
Win32PlatformContext::Win32PlatformContext() {}
Win32PlatformContext::~Win32PlatformContext() {}

App *Win32PlatformContext::createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const
{
  App *app = new Win32App(config, hInstance, nCmdShow);
  return app;
}

Window *Win32PlatformContext::createWindow(const WindowOptions &opts)
{
  return new Win32Window(this, opts);
}

void Win32PlatformContext::onSceneCreate(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneAttach(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneDetach(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneDestroy(Scene *scene) { /* stub: implement as needed */ }
