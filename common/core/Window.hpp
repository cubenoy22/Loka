#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "Scene.hpp"
#include "State.hpp"
#include "core/Tracker.hpp"

class Scene;
class Renderer;
class App;

// WindowOptions: 動的タイトル対応・型安全な宣言的ウィンドウオプション
struct WindowOptions
{
  std::string title;
  WindowOptions() : title("") {}
  WindowOptions &setTitle(const std::string &t)
  {
    title = t;
    return *this;
  }
  // 将来: setMinimizable(bool) など拡張可
};

class Window
{
public:
  // Windowクラスのコンストラクタでvisibilityとtitleを初期化
  Window(Renderer *renderer, App *app, const std::string &title = "")
      : renderer_(renderer), scene_(0), visibility(false), app_(app), title("")
  {
    std::vector<StateBase *> states = {&this->title, &this->visibility};
    tracker_ = new StdTracker(states); // 監視対象Stateを渡して初期化
    this->title.set(title);
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
    scene_ = scene;
    rerender();
  }
  void rerender()
  {
    if (scene_)
    {
      scene_->buildContext();
      scene_->renderAll(renderer_);
    }
  }
  Renderer *renderer() const { return renderer_; }
  Scene *scene() const { return scene_; }

  void setApp(App *app) { app_ = app; }
  App *getApp() const { return app_; }

  // visibility: ウィンドウの表示/非表示状態を表す共通プロパティ
  MutableState<bool> visibility;
  // --- 追加: ウィンドウタイトルの状態 ---
  MutableState<std::string> title;

  Tracker *getTracker() const { return tracker_; }

protected:
  Renderer *renderer_;
  Scene *scene_;
  App *app_;         // App をポインタで保持
  Tracker *tracker_; // --- Window専用のtrackerを追加
};

#endif // DECLARA_WINDOW_HPP
