#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "State.hpp"
#include "core/StateTracker.hpp"
#include "SceneManager.hpp"
#include "core/AppComponent.hpp" // AppComponentを継承するためにインクルード追加

class Scene;
class PlatformContext;

// WindowOptions: 動的タイトル対応・型安全な宣言的ウィンドウオプション
struct WindowOptions
{
  std::string title;
  bool visible;
  WindowOptions() : title(""), visible(true) {}
  WindowOptions &setTitle(const std::string &t)
  {
    title = t;
    return *this;
  }
  WindowOptions &setVisibility(bool v)
  {
    visible = v;
    return *this;
  }
  // 将来: setMinimizable(bool) など拡張可
};

class Window : public AppComponent
{
public:
  Window(PlatformContext *context, const WindowOptions &options = WindowOptions())
      : context_(context), title("")
  {
    std::vector<StateBase *> states = {&this->title, &this->visibility};
    tracker_ = new PushStateTracker(states); // 監視対象Stateを渡して初期化
    this->title.set(options.title);
    this->visibility.set(options.visible);
  }
  virtual ~Window() = default;

  // タイトル変更時のthunk（State<std::string>用）
  static void TitleChangedThunk(void *userData)
  {
    Window *self = static_cast<Window *>(userData);
    if (self)
    {
      // Stateから値を取得しtitleに反映
      // ここではState<std::string>*が必要だが、
      // 実装側で適切にキャストして使うこと
    }
  }

  void setScene(Scene *scene)
  {
    // SceneManager経由でトランザクションとして切り替え
    Scene *from = sceneManager_.getCurrentScene() ? sceneManager_.getCurrentScene()->get() : 0;
    sceneManager_.commitTransaction(new SceneTransaction(from, scene));
  }
  void rerender()
  {
    // SceneManager経由で現在のSceneを取得
    Scene *scene = sceneManager_.getCurrentScene() ? sceneManager_.getCurrentScene()->get() : 0;
    if (scene)
    {
      // ここで描画処理やPlatformContext連携を行う場合は適宜追加
    }
  }
  PlatformContext *context() const { return context_; }
  Scene *scene() const { return sceneManager_.getCurrentScene() ? sceneManager_.getCurrentScene()->get() : 0; }
  SceneManager *sceneManager() { return &sceneManager_; }

  // visibility: ウィンドウの表示/非表示状態を表す共通プロパティ
  MutableState<bool> visibility;
  // --- 追加: ウィンドウタイトルの状態 ---
  MutableState<std::string> title;

  StateTracker *getTracker() const { return tracker_; }

  virtual void onRun() {}

protected:
  PlatformContext *context_;
  StateTracker *tracker_; // --- Window専用のtrackerを追加
  SceneManager sceneManager_;
};

#endif // DECLARA_WINDOW_HPP
