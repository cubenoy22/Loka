#ifndef DECLARA_WIN32PLATFORMCONTEXT_HPP
#define DECLARA_WIN32PLATFORMCONTEXT_HPP
#include "core/PlatformContext.hpp"
#include "core/Window.hpp"
#include "Win32App.hpp"

// 前方宣言（Win32Windowは別途定義）
class Win32Window;
class AppConfigurable;
class Scene;

class Win32PlatformContext : public PlatformContext
{
public:
  Win32PlatformContext();
  ~Win32PlatformContext();
  App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const override;

  // Scene ライフサイクルイベント (PlatformContext純粋仮想の実装)
  void onSceneCreate(Scene *scene) override;
  void onSceneAttach(Scene *scene) override;
  void onSceneDetach(Scene *scene) override;
  void onSceneDestroy(Scene *scene) override;

  Window *createWindow(Scene *initialScene, const WindowOptions &opts) override;
};

#endif // DECLARA_WIN32PLATFORMCONTEXT_HPP
