#ifndef DECLARA_PLATFORMCONTEXT_HPP
#define DECLARA_PLATFORMCONTEXT_HPP

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
  virtual App *getApp(AppBuilder &builder) const = 0;

  // Window生成ファクトリーメソッド（サブクラス実装必須）
  virtual Window *createWindow(const WindowOptions &opts) = 0;
};

#endif // DECLARA_PLATFORMCONTEXT_HPP
