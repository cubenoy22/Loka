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
  Window(PlatformContext *context, declara::core::scene::Scene *initialScene = nullptr, const WindowOptions &options = WindowOptions())
      : context_(context), title("")
  {
    options_ = options;
    tracker_ = new PushStateTracker(makeStateVector(&title, &visibility, 0));
    this->title.set(options.title);
    this->visibility.set(options.visible);
    sceneManager_.getCurrentScene().deferBindWithOld(
        (State<declara::core::scene::Scene *>::OnChangeWithOldFn) & Window::onSceneChangedThunk, this);
    if (initialScene)
      sceneManager_.commitTransaction(nullptr, initialScene);
  }
  virtual ~Window() = default;

  PlatformContext *context() const { return context_; }
  declara::core::scene::Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> visibility;
  MutableState<std::string> title;

  StateTracker *getTracker() const { return tracker_; }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

private:
  static void onSceneChangedThunk(const declara::core::scene::Scene *oldScene, const declara::core::scene::Scene *newScene, void *userData)
  {
    Window *self = static_cast<Window *>(userData);
    if (self)
      self->onSceneChanged(oldScene, newScene);
  }
  void onSceneChanged(const declara::core::scene::Scene *oldScene, const declara::core::scene::Scene *newScene)
  {
    declara::core::scene::Scene *oldS = const_cast<declara::core::scene::Scene *>(oldScene);
    declara::core::scene::Scene *newS = const_cast<declara::core::scene::Scene *>(newScene);
    // TODO: observableにする
    if (oldS && oldS != newS)
    {
      // oldS->onDetach();
    }
    if (newS && oldS != newS)
    {
      // newS->onAttach();
    }
  }

protected:
  PlatformContext *context_;
  StateTracker *tracker_;
  SceneManager2 sceneManager_;
  WindowOptions options_;
};

#endif // DECLARA_WINDOW_HPP
