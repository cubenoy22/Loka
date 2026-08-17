#ifndef LOKA_WINDOW_HPP
#define LOKA_WINDOW_HPP

#include <cassert>
#include <new>
#include "core/diag/LifecycleAudit.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/AppComponent.hpp"
#include "app/core/SceneManager.hpp"
#include "core/util/StateUtil.hpp"
#include "core/util/OwnedDef.hpp"
#include "app/scene/Node.hpp"
#include "app/Menu.hpp"
#include "core/String.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
    }
  } // namespace app
} // namespace loka

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
  loka::core::State<loka::core::String> *displayTitleStatePtr;
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
  loka::app::scene::NodeDefinitionBase *rootDefinition;
  loka::app::MenuBarDefinition *menuBarDefinition;

private:
  struct InitialSceneHandoff
  {
    explicit InitialSceneHandoff(loka::app::scene::Scene *value)
        : scene(value),
          references(1)
    {
    }

    loka::app::scene::Scene *scene;
    unsigned long references;
  };

  mutable InitialSceneHandoff *initialSceneHandoff_;

  void retainInitialSceneHandoff()
  {
    if (this->initialSceneHandoff_)
    {
      ++this->initialSceneHandoff_->references;
    }
  }

  void releaseInitialSceneHandoff()
  {
    if (!this->initialSceneHandoff_)
    {
      return;
    }
    --this->initialSceneHandoff_->references;
    if (this->initialSceneHandoff_->references == 0)
    {
      delete this->initialSceneHandoff_->scene;
      delete this->initialSceneHandoff_;
    }
    this->initialSceneHandoff_ = 0;
  }

  static bool tryCloneOwnedDefinitions(const WindowProps &rhs,
                                       loka::app::scene::NodeDefinitionBase *&outRootDefinition,
                                       loka::app::MenuBarDefinition *&outMenuBarDefinition)
  {
    outRootDefinition = 0;
    outMenuBarDefinition = 0;
    if (rhs.rootDefinition)
    {
      outRootDefinition = rhs.rootDefinition->clone();
      if (!outRootDefinition)
      {
        return false;
      }
    }
    if (rhs.menuBarDefinition)
    {
      outMenuBarDefinition = rhs.menuBarDefinition->clone();
      if (!outMenuBarDefinition)
      {
        delete outRootDefinition;
        outRootDefinition = 0;
        return false;
      }
    }
    return true;
  }

public:
  WindowProps()
      : titleStatePtr(0),
        displayTitleStatePtr(0),
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
        rootDefinition(0),
        menuBarDefinition(0),
        initialSceneHandoff_(0)
  {
  }

  WindowProps(const WindowProps &rhs)
      : titleStatePtr(rhs.titleStatePtr),
        displayTitleStatePtr(rhs.displayTitleStatePtr),
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
        rootDefinition(0),
        menuBarDefinition(0),
        initialSceneHandoff_(rhs.initialSceneHandoff_)
  {
    this->retainInitialSceneHandoff();
    loka::app::scene::NodeDefinitionBase *nextRootDefinition = 0;
    loka::app::MenuBarDefinition *nextMenuBarDefinition = 0;
    if (tryCloneOwnedDefinitions(rhs, nextRootDefinition, nextMenuBarDefinition))
    {
      rootDefinition = nextRootDefinition;
      menuBarDefinition = nextMenuBarDefinition;
    }
  }

  ~WindowProps()
  {
    this->releaseInitialSceneHandoff();
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
    loka::app::scene::NodeDefinitionBase *nextRootDefinition = 0;
    loka::app::MenuBarDefinition *nextMenuBarDefinition = 0;
    if (!tryCloneOwnedDefinitions(rhs, nextRootDefinition, nextMenuBarDefinition))
    {
      return *this;
    }
    titleStatePtr = rhs.titleStatePtr;
    displayTitleStatePtr = rhs.displayTitleStatePtr;
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
    InitialSceneHandoff *nextInitialSceneHandoff = rhs.initialSceneHandoff_;
    if (nextInitialSceneHandoff)
    {
      ++nextInitialSceneHandoff->references;
    }
    this->releaseInitialSceneHandoff();
    this->initialSceneHandoff_ = nextInitialSceneHandoff;
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    rootDefinition = nextRootDefinition;
    if (menuBarDefinition)
    {
      delete menuBarDefinition;
      menuBarDefinition = 0;
    }
    menuBarDefinition = nextMenuBarDefinition;
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

  /** Supplies a read-only title projection without changing the application's
      logical title state. Native windows observe this state when present;
      titleState() remains the application-owned read/write surface. */
  WindowProps &displayTitleState(loka::core::State<loka::core::String> *state)
  {
    displayTitleStatePtr = state;
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
    if (this->peekInitialScene() != scene)
    {
      this->releaseInitialSceneHandoff();
      if (scene)
      {
        this->initialSceneHandoff_ = new (std::nothrow) InitialSceneHandoff(scene);
        if (!this->initialSceneHandoff_)
        {
          delete scene;
        }
      }
    }
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    return *this;
  }

  /** Returns the Scene awaiting ownership transfer without consuming it. */
  loka::app::scene::Scene *peekInitialScene() const
  {
    return this->initialSceneHandoff_ ? this->initialSceneHandoff_->scene : 0;
  }

  /**
   * Transfers the initial Scene exactly once across all copies of these props.
   * Window construction is the normal consumer.
   */
  loka::app::scene::Scene *takeInitialScene() const
  {
    loka::app::scene::Scene *result = this->peekInitialScene();
    if (this->initialSceneHandoff_)
    {
      this->initialSceneHandoff_->scene = 0;
    }
    return result;
  }

  /** Transfers ownership of the cloned root definition to the caller. */
  loka::app::scene::NodeDefinitionBase *takeRootDefinition()
  {
    loka::app::scene::NodeDefinitionBase *result = rootDefinition;
    rootDefinition = 0;
    return result;
  }

  WindowProps &scene(const loka::app::scene::NodeDefinitionBase &def)
  {
    loka::app::scene::NodeDefinitionBase *nextRootDefinition = def.clone();
    if (!nextRootDefinition)
    {
      return *this;
    }
    if (rootDefinition)
    {
      delete rootDefinition;
      rootDefinition = 0;
    }
    rootDefinition = nextRootDefinition;
    this->releaseInitialSceneHandoff();
    return *this;
  }

  WindowProps &menuBar(const loka::app::MenuBarDefinition &bar)
  {
    loka::app::MenuBarDefinition *nextMenuBarDefinition = bar.clone();
    if (!nextMenuBarDefinition)
    {
      return *this;
    }
    if (menuBarDefinition)
    {
      delete menuBarDefinition;
      menuBarDefinition = 0;
    }
    menuBarDefinition = nextMenuBarDefinition;
    return *this;
  }
};

class Window : public AppComponent LOKA_AUDITED_AS(Window)
{
  /** Owns native State observer registrations for one Window lifetime.

      A native backend registers each logical-to-native door here once. The
      ledger removes every deferred observer before Window-owned State storage
      is reclaimed, including when a borrowed State outlives the Window. */
  class NativeStateObserverLedger
  {
  public:
    NativeStateObserverLedger()
        : count_(0)
    {
    }

    ~NativeStateObserverLedger()
    {
      this->detach();
    }

    void observe(const loka::core::StateBase &state,
                 loka::core::StateBase::OnChangeFn callback,
                 void *userData)
    {
      assert(this->count_ < MAX_OBSERVERS && "Window native observer ledger capacity exceeded");
      if (this->count_ >= MAX_OBSERVERS)
      {
        return;
      }
      Entry &entry = this->entries_[this->count_++];
      entry.state = &state;
      entry.callback = callback;
      entry.userData = userData;
      state.deferBind(callback, userData);
    }

    void detach()
    {
      while (this->count_ > 0)
      {
        Entry &entry = this->entries_[--this->count_];
        entry.state->deferUnbind(entry.callback, entry.userData);
        entry.state = 0;
        entry.callback = 0;
        entry.userData = 0;
      }
    }

  private:
    enum
    {
      MAX_OBSERVERS = 3
    };

    struct Entry
    {
      const loka::core::StateBase *state;
      loka::core::StateBase::OnChangeFn callback;
      void *userData;
    };

    Entry entries_[MAX_OBSERVERS];
    std::size_t count_;

    NativeStateObserverLedger(const NativeStateObserverLedger &);
    NativeStateObserverLedger &operator=(const NativeStateObserverLedger &);
  };

public:
  typedef WindowTypeTag TypeTag;

  /** Returns the common frame used when WindowProps leaves it unspecified. */
  static loka::core::Frame defaultFrame()
  {
    return loka::core::Frame(50, 50, 300, 300);
  }

  Window(PlatformContext *context, const WindowProps &props = WindowProps())
      : context_(context),
        tracker_(0),
        titleStorage_(),
        visibilityStorage_(true),
        frameState_(),
        title_(&titleStorage_),
        displayTitle_(&titleStorage_),
        visibility_(&visibilityStorage_),
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
        menuBarDefinition_(0)
  {
    if (props.titleStatePtr)
    {
      title_ = props.titleStatePtr;
    }
    displayTitle_ = props.displayTitleStatePtr ? props.displayTitleStatePtr : title_;
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
    pushTracker->addState(displayTitle_);
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
      menuBarDefinition_.reset(props.menuBarDefinition->clone());
    }
    sceneManager_.setWindow(this);
    loka::app::scene::Scene *initialScene = props.takeInitialScene();
    if (initialScene)
    {
      sceneManager_.commitTransaction(0, initialScene);
    }
  }
  virtual ~Window()
  {
    if (this->tracker_)
    {
      assert(this->tracker_->phase() == loka::core::TRACKER_IDLE &&
             "Window cannot be destroyed during its StateTracker transaction");
      delete this->tracker_;
      this->tracker_ = 0;
    }
    menuBarDefinition_.reset();
  }

  PlatformContext *context() const
  {
    return context_;
  }
  loka::app::scene::Scene *scene() const
  {
    return sceneManager_.getCurrentScene().get();
  }
  SceneManager *sceneManager()
  {
    return &sceneManager_;
  }
  bool flushSceneInvalidation();
  bool hasPendingSceneInvalidation() const
  {
    const loka::app::scene::Scene *current = this->scene();
    return (current && current->hasPendingInvalidation()) || sceneManager_.hasRetiredScenes();
  }
  virtual bool hasPendingScenePlatformSync() const
  {
    return false;
  }
  virtual void synchronizeScenePlatform() {}
  virtual void drainNativeRetirements() {}

  loka::core::MutableState<bool> &visibilityState()
  {
    return *visibility_;
  }
  loka::core::MutableState<loka::core::String> &titleState()
  {
    return *title_;
  }
  /** The read-only title projected by native backends. This equals
      titleState() unless WindowProps supplied an explicit display title. */
  const loka::core::State<loka::core::String> &displayTitleState() const
  {
    return *displayTitle_;
  }
  loka::core::MutableState<loka::core::Frame> &frameState()
  {
    return *frameStatePtr_;
  }
  const loka::app::MenuBarDefinition *menuBar() const
  {
    return menuBarDefinition_.get();
  }

  loka::core::StateTracker *getTracker() const
  {
    return tracker_;
  }

  virtual void onCreate() {}
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onDestroy() {}

  // Type casts (avoid dynamic_cast for 68k performance)
  virtual Window *asWindow()
  {
    return this;
  }
  virtual ToolboxWindow *asToolboxWindow()
  {
    return 0;
  }
  virtual Win32Window *asWin32Window()
  {
    return 0;
  }
  virtual MacWindow *asMacWindow()
  {
    return 0;
  }

  /** The axes of the display a window is drawn into, for tests and
      diagnostics that need to state which ones a platform answers. Retrieval
      is per-axis and typed (see queryDisplayScalePercent and friends); this
      enumeration exists to describe the family, not to fetch through. New axes
      go before DISPLAY_FEATURE_COUNT. */
  enum DisplayFeature
  {
    DISPLAY_FEATURE_SCALE,
    DISPLAY_FEATURE_DEPTH,
    DISPLAY_FEATURE_APPEARANCE,
    DISPLAY_FEATURE_COUNT
  };

  /** There is deliberately no unknown member. A platform that cannot tell
      light from dark declines the query instead, so "we cannot tell" can never
      be mistaken for "the user chose light" (PHILOSOPHY "Absent Is Not A
      Value"). */
  enum DisplayAppearance
  {
    DISPLAY_APPEARANCE_LIGHT,
    DISPLAY_APPEARANCE_DARK
  };

  /** Facts about what this window is drawing into. Each reports presence and
      value together, so the two cannot disagree: a platform that cannot answer
      an axis leaves the base implementation in place and declines, and `out`
      is untouched. A caller must not supply a fallback of its own and treat it
      as a reading.

      Density is an integer percentage of the platform's own natural density,
      not physical DPI. DPI is not comparable across targets: 144 dpi is 2x on
      macOS and Classic, whose reference is 72, and 1.5x on Windows, whose
      reference is 96 -- so an asset cannot pick a representation from a dpi
      number. A percentage says the same thing everywhere (100 is unscaled, 200
      is double), stays exact in integers for every scale these platforms
      report, and keeps software floating point off 68k entirely.

      These are plain values, not State. Making them observable would wire
      display facts into the recompose graph and re-import the invalidation that
      draw-time selection removes; an application that must react does so
      through its own State, set from its own callback (#194). */
  virtual bool queryDisplayScalePercent(int &out) const
  {
    (void)out;
    return false;
  }
  virtual bool queryDisplayDepth(int &out) const
  {
    (void)out;
    return false;
  }
  virtual bool queryDisplayAppearance(DisplayAppearance &out) const
  {
    (void)out;
    return false;
  }

  /** Whether this window can answer one axis, derived from the queries
      themselves rather than maintained beside them. Deliberately not virtual:
      a platform that overrode this separately could contradict its own
      getters.

      The switch has no default label on purpose, so adding an axis without
      wiring it here fails the build -- `-Wswitch` is part of the
      `-Wall -Wextra` floor in cmake/LokaWarnings.cmake. */
  bool hasDisplayFeature(DisplayFeature feature) const
  {
    switch (feature)
    {
    case DISPLAY_FEATURE_SCALE:
    {
      int scalePercent;
      return this->queryDisplayScalePercent(scalePercent);
    }
    case DISPLAY_FEATURE_DEPTH:
    {
      int depth;
      return this->queryDisplayDepth(depth);
    }
    case DISPLAY_FEATURE_APPEARANCE:
    {
      DisplayAppearance appearance;
      return this->queryDisplayAppearance(appearance);
    }
    case DISPLAY_FEATURE_COUNT:
      break;
    }
    return false;
  }

  bool hasPosition() const
  {
    return frameStatePtr_->get().hasPosition();
  }
  bool hasSize() const
  {
    return frameStatePtr_->get().hasSize();
  }
  int positionX() const
  {
    return frameStatePtr_->get().x;
  }
  int positionY() const
  {
    return frameStatePtr_->get().y;
  }
  int width() const
  {
    return frameStatePtr_->get().width;
  }
  int height() const
  {
    return frameStatePtr_->get().height;
  }
  const loka::app::IdlePolicy &idlePolicy() const
  {
    return idlePolicy_;
  }
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
  void observeNativeState(const loka::core::StateBase &state,
                          loka::core::StateBase::OnChangeFn callback,
                          void *userData)
  {
    this->nativeStateObservers_.observe(state, callback, userData);
  }

  void detachNativeStateObservers()
  {
    this->nativeStateObservers_.detach();
  }

  PlatformContext *context_;
  loka::core::StateTracker *tracker_;
  SceneManager sceneManager_;
  loka::core::MutableState<loka::core::String> titleStorage_;
  loka::core::MutableState<bool> visibilityStorage_;
  loka::core::MutableState<loka::core::Frame> frameState_;
  loka::core::MutableState<loka::core::String> *title_;
  loka::core::State<loka::core::String> *displayTitle_;
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
  loka::core::OwnedDef<loka::app::MenuBarDefinition> menuBarDefinition_;
  NativeStateObserverLedger nativeStateObservers_;
};

#endif // LOKA_WINDOW_HPP
