#ifndef DECLARA_WINDOW_HPP
#define DECLARA_WINDOW_HPP

#include <string>
#include "State.hpp"
#include "core/StateTracker.hpp"
#include "core/AppComponent.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/StateUtil.hpp"
#include "core2/scene/Node.hpp"

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

struct WindowProps
{
  MutableState<std::string> *title;
  MutableState<bool> *visibility;
  std::string initialTitle;
  bool initialVisibility;
  bool hasInitialTitle;
  bool hasInitialVisibility;
  declara::core::scene::Scene *initialScene;
  declara::core::scene::NodeDefinitionBase *rootDefinition;

  WindowProps()
      : title(0),
        visibility(0),
        initialTitle(""),
        initialVisibility(true),
        hasInitialTitle(false),
        hasInitialVisibility(false),
        initialScene(0),
        rootDefinition(0)
  {
  }

  WindowProps(const WindowProps &rhs)
      : title(rhs.title),
        visibility(rhs.visibility),
        initialTitle(rhs.initialTitle),
        initialVisibility(rhs.initialVisibility),
        hasInitialTitle(rhs.hasInitialTitle),
        hasInitialVisibility(rhs.hasInitialVisibility),
        initialScene(rhs.initialScene),
        rootDefinition(0)
  {
    if (rhs.rootDefinition)
    {
      rootDefinition = rhs.rootDefinition->clone();
    }
  }

  ~WindowProps()
  {
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
  }

  WindowProps &operator=(const WindowProps &rhs)
  {
    if (this == &rhs)
    {
      return *this;
    }
    title = rhs.title;
    visibility = rhs.visibility;
    initialTitle = rhs.initialTitle;
    initialVisibility = rhs.initialVisibility;
    hasInitialTitle = rhs.hasInitialTitle;
    hasInitialVisibility = rhs.hasInitialVisibility;
    initialScene = rhs.initialScene;
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    if (rhs.rootDefinition)
    {
      rootDefinition = rhs.rootDefinition->clone();
    }
    return *this;
  }

  WindowProps &setTitle(const std::string &t)
  {
    initialTitle = t;
    hasInitialTitle = true;
    return *this;
  }

  WindowProps &setVisibility(bool v)
  {
    initialVisibility = v;
    hasInitialVisibility = true;
    return *this;
  }

  WindowProps &setTitleState(MutableState<std::string> *state)
  {
    title = state;
    return *this;
  }

  WindowProps &setVisibilityState(MutableState<bool> *state)
  {
    visibility = state;
    return *this;
  }

  WindowProps &setScene(declara::core::scene::Scene *scene)
  {
    initialScene = scene;
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    return *this;
  }

  WindowProps &setScene(const declara::core::scene::NodeDefinitionBase &def)
  {
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    rootDefinition = def.clone();
    initialScene = 0;
    return *this;
  }
};

class Window : public AppComponent
{
public:
  Window(PlatformContext *context, const WindowProps &props = WindowProps())
      : context_(context),
        titleStorage_(""),
        visibilityStorage_(true),
        title_(&titleStorage_),
        visibility_(&visibilityStorage_),
        initialScene_(props.initialScene)
  {
    if (props.title)
    {
      title_ = props.title;
    }
    if (props.visibility)
    {
      visibility_ = props.visibility;
    }
    tracker_ = new declara::core::PushStateTracker(makeStateVector(static_cast<StateBase *>(title_), static_cast<StateBase *>(visibility_), 0));
    if (props.hasInitialTitle)
    {
      title_->set(props.initialTitle);
    }
    if (props.hasInitialVisibility)
    {
      visibility_->set(props.initialVisibility);
    }
    sceneManager_.setWindow(this);
    if (initialScene_)
    {
      sceneManager_.commitTransaction(0, initialScene_);
    }
  }
  virtual ~Window() {}

  PlatformContext *context() const { return context_; }
  declara::core::scene::Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> &visibilityState() { return *visibility_; }
  MutableState<std::string> &titleState() { return *title_; }

  declara::core::StateTracker *getTracker() const { return tracker_; }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

private:
protected:
  PlatformContext *context_;
  declara::core::StateTracker *tracker_;
  SceneManager2 sceneManager_;
  MutableState<std::string> titleStorage_;
  MutableState<bool> visibilityStorage_;
  MutableState<std::string> *title_;
  MutableState<bool> *visibility_;
  declara::core::scene::Scene *initialScene_;
};

#endif // DECLARA_WINDOW_HPP
