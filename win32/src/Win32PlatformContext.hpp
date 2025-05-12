#pragma once
#include "core/PlatformContext.hpp"
#include "core/Window.hpp"
#include "Win32App.hpp"

// 前方宣言（Win32Windowは別途定義）
class Win32Window;

class Win32PlatformContext : public PlatformContext
{
public:
  Win32PlatformContext();
  ~Win32PlatformContext();
  App *getApp(AppBuilder &builder) const override;

  // Scene ライフサイクルイベント (PlatformContext純粋仮想の実装)
  void onSceneCreate(class Scene *scene) override;
  void onSceneAttach(class Scene *scene) override;
  void onSceneDetach(class Scene *scene) override;
  void onSceneDestroy(class Scene *scene) override;

  Window *createWindow(const WindowOptions &opts) override;

private:
  mutable Win32App *app_;
};
