#ifndef LOKA_WINDOW_HPP
#define LOKA_WINDOW_HPP

#include "State.hpp"
#include "core/StateTracker.hpp"
#include "core/AppComponent.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/StateUtil.hpp"
#include "core2/scene/Node.hpp"
#include "app/Menu.hpp"
#include "loka/core/String.hpp"

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

struct WindowTypeTag
{
};

struct WindowProps
{
  typedef WindowTypeTag TypeTag;
  MutableState<loka::core::String> *titleStatePtr;
  MutableState<bool> *visibilityStatePtr;
  loka::core::String initialTitle;
  bool initialVisibility;
  bool hasInitialTitle;
  bool hasInitialVisibility;
  declara::core::scene::Scene *initialScene;
  declara::core::scene::NodeDefinitionBase *rootDefinition;
  declara::app::MenuBarDefinition *menuBarDefinition;

  WindowProps()
      : titleStatePtr(0),
        visibilityStatePtr(0),
        initialTitle(),
        initialVisibility(true),
        hasInitialTitle(false),
        hasInitialVisibility(false),
        initialScene(0),
        rootDefinition(0),
        menuBarDefinition(0)
  {
  }

  WindowProps(const WindowProps &rhs)
      : titleStatePtr(rhs.titleStatePtr),
        visibilityStatePtr(rhs.visibilityStatePtr),
        initialTitle(rhs.initialTitle),
        initialVisibility(rhs.initialVisibility),
        hasInitialTitle(rhs.hasInitialTitle),
        hasInitialVisibility(rhs.hasInitialVisibility),
        initialScene(rhs.initialScene),
        rootDefinition(0),
        menuBarDefinition(0)
  {
    if (rhs.rootDefinition)
    {
      rootDefinition = rhs.rootDefinition->clone();
    }
    if (rhs.menuBarDefinition)
    {
      menuBarDefinition = rhs.menuBarDefinition->clone();
    }
  }

  ~WindowProps()
  {
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    if (menuBarDefinition)
    {
      delete menuBarDefinition;
      menuBarDefinition = 0;
    }
  }

  WindowProps &operator=(const WindowProps &rhs)
  {
    if (this == &rhs)
    {
      return *this;
    }
    titleStatePtr = rhs.titleStatePtr;
    visibilityStatePtr = rhs.visibilityStatePtr;
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
    if (menuBarDefinition)
    {
      delete menuBarDefinition;
      menuBarDefinition = 0;
    }
    if (rhs.menuBarDefinition)
    {
      menuBarDefinition = rhs.menuBarDefinition->clone();
    }
    return *this;
  }

  WindowProps &title(const loka::core::String &t)
  {
    initialTitle = t;
    hasInitialTitle = true;
    return *this;
  }

  WindowProps &title(const char *t)
  {
    return title(loka::core::String::Literal(t));
  }

  WindowProps &visible(bool v)
  {
    initialVisibility = v;
    hasInitialVisibility = true;
    return *this;
  }

  WindowProps &titleState(MutableState<loka::core::String> *state)
  {
    titleStatePtr = state;
    return *this;
  }

  WindowProps &visibilityState(MutableState<bool> *state)
  {
    visibilityStatePtr = state;
    return *this;
  }

  WindowProps &scene(declara::core::scene::Scene *scene)
  {
    initialScene = scene;
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    return *this;
  }

  WindowProps &scene(const declara::core::scene::NodeDefinitionBase &def)
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

  WindowProps &menuBar(const declara::app::MenuBarDefinition &bar)
  {
    if (menuBarDefinition)
    {
      delete menuBarDefinition;
      menuBarDefinition = 0;
    }
    menuBarDefinition = bar.clone();
    return *this;
  }
};

class Window : public AppComponent
{
public:
  typedef WindowTypeTag TypeTag;
  Window(PlatformContext *context, const WindowProps &props = WindowProps())
      : context_(context),
        titleStorage_(),
        visibilityStorage_(true),
        title_(&titleStorage_),
        visibility_(&visibilityStorage_),
        initialScene_(props.initialScene),
        menuBarDefinition_(0)
  {
    if (props.titleStatePtr)
    {
      title_ = props.titleStatePtr;
    }
    if (props.visibilityStatePtr)
    {
      visibility_ = props.visibilityStatePtr;
    }
    tracker_ = new declara::core::PushStateTracker(makeStateVector(static_cast<StateBase *>(title_), static_cast<StateBase *>(visibility_), STATE_NULL));
    if (props.hasInitialTitle)
    {
      title_->set(props.initialTitle);
    }
    if (props.hasInitialVisibility)
    {
      visibility_->set(props.initialVisibility);
    }
    if (props.menuBarDefinition)
    {
      menuBarDefinition_ = props.menuBarDefinition->clone();
    }
    sceneManager_.setWindow(this);
    if (initialScene_)
    {
      sceneManager_.commitTransaction(0, initialScene_);
    }
  }
  virtual ~Window()
  {
    if (menuBarDefinition_)
    {
      delete menuBarDefinition_;
      menuBarDefinition_ = 0;
    }
  }

  PlatformContext *context() const { return context_; }
  declara::core::scene::Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager2 *sceneManager() { return &sceneManager_; }

  MutableState<bool> &visibilityState() { return *visibility_; }
  MutableState<loka::core::String> &titleState() { return *title_; }
  const declara::app::MenuBarDefinition *menuBar() const { return menuBarDefinition_; }

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
  MutableState<loka::core::String> titleStorage_;
  MutableState<bool> visibilityStorage_;
  MutableState<loka::core::String> *title_;
  MutableState<bool> *visibility_;
  declara::core::scene::Scene *initialScene_;
  declara::app::MenuBarDefinition *menuBarDefinition_;
};

#endif // LOKA_WINDOW_HPP
