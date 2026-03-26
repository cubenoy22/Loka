#ifndef LOKA_WINDOW_HPP
#define LOKA_WINDOW_HPP

#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "app/AppConfigurable.hpp"
#include "app/AppComponent.hpp"
#include "app/SceneManager.hpp"
#include "loka/core/util/StateUtil.hpp"
#include "app/scene/Node.hpp"
#include "app/Menu.hpp"
#include "app/window/ability/DebugStatsControl.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Frame.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
    }
  }
}

class PlatformContext;

// Forward declarations for platform-specific windows (for asXxx methods)
class ToolboxWindow;
class Win32Window;
class MacWindow;
class Window;

struct WindowTypeTag
{
};

struct WindowProps
{
  typedef WindowTypeTag TypeTag;
  typedef void (*OnIdleFn)(Window *window, double elapsedSeconds, void *userData);
  typedef bool (*OnKeyPressFn)(Window *window, char key, void *userData);
  loka::core::MutableState<loka::core::String> *titleStatePtr;
  loka::core::MutableState<bool> *visibilityStatePtr;
  loka::core::MutableState<loka::core::Frame> *frameStatePtr;
  loka::core::String initialTitle;
  bool initialVisibility;
  bool hasInitialTitle;
  bool hasInitialVisibility;
  int positionX;
  int positionY;
  int width;
  int height;
  loka::app::IdlePolicy idlePolicyValue;
  OnIdleFn onIdleFn;
  void *onIdleUserData;
  OnKeyPressFn onKeyPressFn;
  void *onKeyPressUserData;
  loka::app::scene::Scene *initialScene;
  loka::app::scene::NodeDefinitionBase *rootDefinition;
  loka::app::MenuBarDefinition *menuBarDefinition;

  WindowProps()
      : titleStatePtr(0),
        visibilityStatePtr(0),
        frameStatePtr(0),
        initialTitle(),
        initialVisibility(true),
        hasInitialTitle(false),
        hasInitialVisibility(false),
        positionX(-1),
        positionY(-1),
        width(-1),
        height(-1),
        idlePolicyValue(loka::app::IdlePolicy::none()),
        onIdleFn(0),
        onIdleUserData(0),
        onKeyPressFn(0),
        onKeyPressUserData(0),
        initialScene(0),
        rootDefinition(0),
        menuBarDefinition(0)
  {
  }

  WindowProps(const WindowProps &rhs)
      : titleStatePtr(rhs.titleStatePtr),
        visibilityStatePtr(rhs.visibilityStatePtr),
        frameStatePtr(rhs.frameStatePtr),
        initialTitle(rhs.initialTitle),
        initialVisibility(rhs.initialVisibility),
        hasInitialTitle(rhs.hasInitialTitle),
        hasInitialVisibility(rhs.hasInitialVisibility),
        positionX(rhs.positionX),
        positionY(rhs.positionY),
        width(rhs.width),
        height(rhs.height),
        idlePolicyValue(rhs.idlePolicyValue),
        onIdleFn(rhs.onIdleFn),
        onIdleUserData(rhs.onIdleUserData),
        onKeyPressFn(rhs.onKeyPressFn),
        onKeyPressUserData(rhs.onKeyPressUserData),
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
    frameStatePtr = rhs.frameStatePtr;
    initialTitle = rhs.initialTitle;
    initialVisibility = rhs.initialVisibility;
    hasInitialTitle = rhs.hasInitialTitle;
    hasInitialVisibility = rhs.hasInitialVisibility;
    positionX = rhs.positionX;
    positionY = rhs.positionY;
    width = rhs.width;
    height = rhs.height;
    idlePolicyValue = rhs.idlePolicyValue;
    onIdleFn = rhs.onIdleFn;
    onIdleUserData = rhs.onIdleUserData;
    onKeyPressFn = rhs.onKeyPressFn;
    onKeyPressUserData = rhs.onKeyPressUserData;
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

  WindowProps &position(int x, int y)
  {
    positionX = x;
    positionY = y;
    return *this;
  }

  WindowProps &size(int width, int height)
  {
    this->width = width;
    this->height = height;
    return *this;
  }

  WindowProps &frame(int x, int y, int width, int height)
  {
    position(x, y);
    size(width, height);
    return *this;
  }

  WindowProps &idlePolicy(const loka::app::IdlePolicy &policy)
  {
    idlePolicyValue = policy;
    return *this;
  }

  WindowProps &onIdle(OnIdleFn fn, void *userData)
  {
    onIdleFn = fn;
    onIdleUserData = userData;
    return *this;
  }

  WindowProps &onKeyPress(OnKeyPressFn fn, void *userData)
  {
    onKeyPressFn = fn;
    onKeyPressUserData = userData;
    return *this;
  }

  WindowProps &titleState(loka::core::MutableState<loka::core::String> *state)
  {
    titleStatePtr = state;
    return *this;
  }

  WindowProps &visibilityState(loka::core::MutableState<bool> *state)
  {
    visibilityStatePtr = state;
    return *this;
  }

  WindowProps &frameState(loka::core::MutableState<loka::core::Frame> *state)
  {
    frameStatePtr = state;
    return *this;
  }

  WindowProps &scene(loka::app::scene::Scene *scene)
  {
    initialScene = scene;
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    return *this;
  }

  WindowProps &scene(const loka::app::scene::NodeDefinitionBase &def)
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

  WindowProps &menuBar(const loka::app::MenuBarDefinition &bar)
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
        frameState_(),
        frameStatePtr_(&frameState_),
        positionX_(props.positionX),
        positionY_(props.positionY),
        width_(props.width),
        height_(props.height),
        idlePolicy_(props.idlePolicyValue),
        onIdleFn_(props.onIdleFn),
        onIdleUserData_(props.onIdleUserData),
        onKeyPressFn_(props.onKeyPressFn),
        onKeyPressUserData_(props.onKeyPressUserData),
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
    if (props.frameStatePtr)
    {
      frameStatePtr_ = props.frameStatePtr;
    }
    loka::core::PushStateTracker *pushTracker = new loka::core::PushStateTracker();
    pushTracker->addState(title_);
    pushTracker->addState(visibility_);
    tracker_ = pushTracker;
    if (props.hasInitialTitle)
    {
      title_->set(props.initialTitle);
    }
    if (props.hasInitialVisibility)
    {
      visibility_->set(props.initialVisibility);
    }
    if (positionX_ >= 0 || positionY_ >= 0 || width_ > 0 || height_ > 0)
    {
      loka::core::Frame frame = frameStatePtr_->get();
      if (positionX_ >= 0 && positionY_ >= 0)
      {
        frame.x = positionX_;
        frame.y = positionY_;
      }
      if (width_ > 0 && height_ > 0)
      {
        frame.width = width_;
        frame.height = height_;
      }
      frameStatePtr_->set(frame);
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
  loka::app::scene::Scene *scene() const { return sceneManager_.getCurrentScene().get(); }
  SceneManager *sceneManager() { return &sceneManager_; }
  bool flushSceneInvalidation();
  bool hasPendingSceneInvalidation() const
  {
    const loka::app::scene::Scene *current = this->scene();
    return current ? current->hasPendingInvalidation() : false;
  }
  virtual bool hasPendingScenePlatformSync() const { return false; }
  virtual void synchronizeScenePlatform() {}

  loka::core::MutableState<bool> &visibilityState() { return *visibility_; }
  loka::core::MutableState<loka::core::String> &titleState() { return *title_; }
  loka::core::MutableState<loka::core::Frame> &frameState() { return *frameStatePtr_; }
  const loka::app::MenuBarDefinition *menuBar() const { return menuBarDefinition_; }

  loka::core::StateTracker *getTracker() const { return tracker_; }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

  // Type casts (avoid dynamic_cast for 68k performance)
  virtual Window *asWindow() { return this; }
  virtual ToolboxWindow *asToolboxWindow() { return 0; }
  virtual Win32Window *asWin32Window() { return 0; }
  virtual MacWindow *asMacWindow() { return 0; }
  virtual loka::app::IDebugStatsControl *asDebugStatsControl() { return 0; }
  virtual const loka::app::IDebugStatsControl *asDebugStatsControl() const { return 0; }

  bool hasPosition() const
  {
    return frameStatePtr_->get().hasPosition();
  }
  bool hasSize() const
  {
    return frameStatePtr_->get().hasSize();
  }
  int positionX() const { return frameStatePtr_->get().x; }
  int positionY() const { return frameStatePtr_->get().y; }
  int width() const { return frameStatePtr_->get().width; }
  int height() const { return frameStatePtr_->get().height; }
  const loka::app::IdlePolicy &idlePolicy() const { return idlePolicy_; }
  bool handleIdle(double elapsedSeconds)
  {
    if (!onIdleFn_)
    {
      return false;
    }
    onIdleFn_(this, elapsedSeconds, onIdleUserData_);
    return true;
  }
  bool handleKeyPress(char key)
  {
    if (!onKeyPressFn_)
    {
      return false;
    }
    return onKeyPressFn_(this, key, onKeyPressUserData_);
  }

private:
protected:
  PlatformContext *context_;
  loka::core::StateTracker *tracker_;
  SceneManager sceneManager_;
  loka::core::MutableState<loka::core::String> titleStorage_;
  loka::core::MutableState<bool> visibilityStorage_;
  loka::core::MutableState<loka::core::Frame> frameState_;
  loka::core::MutableState<loka::core::String> *title_;
  loka::core::MutableState<bool> *visibility_;
  loka::core::MutableState<loka::core::Frame> *frameStatePtr_;
  int positionX_;
  int positionY_;
  int width_;
  int height_;
  loka::app::IdlePolicy idlePolicy_;
  WindowProps::OnIdleFn onIdleFn_;
  void *onIdleUserData_;
  WindowProps::OnKeyPressFn onKeyPressFn_;
  void *onKeyPressUserData_;
  loka::app::scene::Scene *initialScene_;
  loka::app::MenuBarDefinition *menuBarDefinition_;
};

#endif // LOKA_WINDOW_HPP
