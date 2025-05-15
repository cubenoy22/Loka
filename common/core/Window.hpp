#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "State.hpp"
#include "core/StateTracker.hpp"
#include "core/AppComponent.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/StateUtil.hpp"

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
    tracker_ = new PushStateTracker(makeStateVector(&title, &visibility, 0));
    this->title.set(options.title);
    this->visibility.set(options.visible);
    sceneManager_.getCurrentScene().deferBindWithOld(
        (State<Scene *>::OnChangeWithOldFn) & Window::onSceneChangedThunk, this);
    if (initialScene)
      sceneManager_.commitTransaction(nullptr, initialScene);
  }
  virtual ~Window() = default;

  PlatformContext *context() const { return context_; }
  Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> visibility;
  MutableState<std::string> title;

  StateTracker *getTracker() const { return tracker_; }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

private:
  static void onSceneChangedThunk(const Scene *oldScene, const Scene *newScene, void *userData)
  {
    Window *self = static_cast<Window *>(userData);
    if (self)
      self->onSceneChanged(oldScene, newScene);
  }
  void onSceneChanged(const Scene *oldScene, const Scene *newScene)
  {
    Scene *oldS = const_cast<Scene *>(oldScene);
    Scene *newS = const_cast<Scene *>(newScene);
    if (oldS && oldS != newS)
    {
      oldS->onDetach();
    }
    if (newS && oldS != newS)
    {
      newS->onAttach();
    }
  }

protected:
  PlatformContext *context_;
  StateTracker *tracker_;
  SceneManager2 sceneManager_;
  WindowOptions options_;
};

#endif // DECLARA_WINDOW_HPP
