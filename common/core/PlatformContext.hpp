#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

#include <windows.h>

class AppConfigurable;
class PlatformContext;
class Scene;
class App;
class AppBuilder;
class Window;
class SceneContext;
struct WindowOptions;

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // SceneContext生成API（必ず新規インスタンスを返す。共有リソースは実装側で管理すること）
  virtual SceneContext *createSceneContextForScene(Scene *scene) const = 0;

  // Appインスタンス取得用の純粋仮想関数を追加
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const = 0;

  // Window生成ファクトリーメソッド（サブクラス実装必須）
  // app_がnullの場合はassertで即座に失敗させる設計にする
  virtual Window *createWindow(Scene *initialScene, const WindowOptions &opts) = 0;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
