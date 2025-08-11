#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

#include <windows.h>

class AppConfigurable;
class PlatformContext;
class App;
class AppBuilder;
class Window;
class SceneContext;
struct WindowOptions;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class Scene;
    }
  }
}

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // Appインスタンス取得用の純粋仮想関数を追加
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const = 0;

  // Window生成ファクトリーメソッド（サブクラス実装必須）
  // app_がnullの場合はassertで即座に失敗させる設計にする
  virtual Window *createWindow(declara::core::scene::Scene *initialScene, const WindowOptions &opts) = 0;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
