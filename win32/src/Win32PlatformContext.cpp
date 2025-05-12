#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp" // Win32Windowの実装があれば
#include "core/Window.hpp" // Windowクラスの宣言を明示的に追加

// 明示的なデフォルトコンストラクタ・デストラクタ実装（リンカエラー回避用）
Win32PlatformContext::Win32PlatformContext() : app_(nullptr) {}
Win32PlatformContext::~Win32PlatformContext() { delete app_; }

App *Win32PlatformContext::getApp(AppBuilder &builder) const
{
  if (!app_)
  {
    // HINSTANCE, nCmdShowは本来外部から渡すべき。ここではstub値
    app_ = new Win32App(builder, nullptr, 0);
  }
  return app_;
}

Window *Win32PlatformContext::createWindow(const WindowOptions &opts)
{
  return new Win32Window(app_, this, opts.title, /*hwnd=*/0, opts.visible);
}

void Win32PlatformContext::onSceneCreate(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneAttach(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneDetach(Scene *scene) { /* stub: implement as needed */ }
void Win32PlatformContext::onSceneDestroy(Scene *scene) { /* stub: implement as needed */ }
