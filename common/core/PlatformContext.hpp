#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

#include <windows.h>
// AppConfigurableを前方宣言に変更して循環参照を解消
class AppConfigurable;

class PlatformContext;
class Scene;
class App;
class AppBuilder;
class Window;
struct WindowOptions;

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // Scene ライフサイクルイベント
  virtual void onSceneCreate(class Scene *scene) = 0;
  virtual void onSceneAttach(class Scene *scene) = 0;
  virtual void onSceneDetach(class Scene *scene) = 0;
  virtual void onSceneDestroy(class Scene *scene) = 0;

  // Appインスタンス取得用の純粋仮想関数を追加
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const = 0;

  // Window生成ファクトリーメソッド（サブクラス実装必須）
  // app_がnullの場合はassertで即座に失敗させる設計にする
  virtual Window *createWindow(Scene *initialScene, const WindowOptions &opts) = 0;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
