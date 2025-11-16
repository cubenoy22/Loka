#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "State.hpp"
#include "core/StateTracker.hpp"
#include "core/AppComponent.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/StateUtil.hpp"

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
  Window(PlatformContext *context, declara::core::scene::Scene *initialScene = 0, const WindowOptions &options = WindowOptions())
      : context_(context), title(""), initialScene_(initialScene)
  {
    options_ = options;
    tracker_ = new PushStateTracker(makeStateVector(static_cast<StateBase *>(&title), static_cast<StateBase *>(&visibility), 0));
    this->title.set(options.title);
    this->visibility.set(options.visible);
    if (initialScene)
      sceneManager_.commitTransaction(0, initialScene);
  }
  virtual ~Window() {}

  PlatformContext *context() const { return context_; }
  declara::core::scene::Scene *scene() const { return initialScene_; }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> visibility;
  MutableState<std::string> title;

  StateTracker *getTracker() const { return tracker_; }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

private:
protected:
  PlatformContext *context_;
  StateTracker *tracker_;
  SceneManager2 sceneManager_;
  WindowOptions options_;
  declara::core::scene::Scene *initialScene_;
};

#endif // DECLARA_WINDOW_HPP
