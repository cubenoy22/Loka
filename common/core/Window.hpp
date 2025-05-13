#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "State.hpp"
#include "core/StateTracker.hpp"
#include "core/AppComponent.hpp"
#include "core/SceneManager2.hpp"

class Scene;
class PlatformContext;

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
  Window(PlatformContext *context, Scene *initialScene = nullptr, const WindowOptions &options = WindowOptions())
      : context_(context), title("")
  {
    options_ = options;
    std::vector<StateBase *> states = {&this->title, &this->visibility};
    tracker_ = new PushStateTracker(states); // 監視対象Stateを渡して初期化
    this->title.set(options.title);
    this->visibility.set(options.visible);
    if (initialScene)
    {
      sceneManager_.commitTransaction(nullptr, initialScene);
    }
  }
  virtual ~Window() = default;

  PlatformContext *context() const { return context_; }
  Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> visibility;
  MutableState<std::string> title;

  StateTracker *getTracker() const { return tracker_; }

  // onRunは廃止

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

protected:
  PlatformContext *context_;
  StateTracker *tracker_;
  SceneManager2 sceneManager_;
  WindowOptions options_;
};

#endif // DECLARA_WINDOW_HPP
