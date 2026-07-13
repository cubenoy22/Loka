#include "SceneOwnershipTests.hpp"
#include <cassert>
#include <cstdio>
#include "app/PlatformContext.hpp"
#include "app/core/App.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"

// LeakSanitizer cannot complete its process scan after fork-based death checks.
#if defined(LOKA_LIFECYCLE_AUDIT) && defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
  static int g_sceneOwnershipDefinitionsAlive = 0;
  static int g_sceneOwnershipScenesAlive = 0;
  static int g_windowRetirementWindowsAlive = 0;

  struct SceneOwnershipDefinition : public loka::app::BoxDefinition
  {
    SceneOwnershipDefinition()
        : loka::app::BoxDefinition()
    {
      ++g_sceneOwnershipDefinitionsAlive;
    }

    SceneOwnershipDefinition(const SceneOwnershipDefinition &other)
        : loka::app::BoxDefinition(other)
    {
      ++g_sceneOwnershipDefinitionsAlive;
    }

    virtual ~SceneOwnershipDefinition()
    {
      --g_sceneOwnershipDefinitionsAlive;
    }

    virtual loka::app::scene::NodeDefinitionBase *clone() const
    {
      return new SceneOwnershipDefinition(*this);
    }
  };

  class SceneOwnershipProbe : public loka::app::scene::Scene
  {
  public:
    SceneOwnershipProbe()
        : loka::app::scene::Scene(new SceneOwnershipDefinition())
    {
      ++g_sceneOwnershipScenesAlive;
    }

    virtual ~SceneOwnershipProbe()
    {
      --g_sceneOwnershipScenesAlive;
    }
  };

  class SceneRetirementCountProbe : public loka::app::scene::Scene
  {
  public:
    explicit SceneRetirementCountProbe(int *destructionCount)
        : loka::app::scene::Scene(new SceneOwnershipDefinition()),
          destructionCount_(destructionCount)
    {
      ++g_sceneOwnershipScenesAlive;
    }

    virtual ~SceneRetirementCountProbe()
    {
      ++*this->destructionCount_;
      --g_sceneOwnershipScenesAlive;
    }

  private:
    int *destructionCount_;
  };

  struct SceneLifecycleObservation
  {
    explicit SceneLifecycleObservation(loka::app::scene::Scene *observedScene)
        : scene(observedScene),
          managerWindow(0),
          notificationCount(0)
    {
      for (int i = 0; i < 2; ++i)
      {
        values[i] = ON_CREATE;
        attachedValues[i] = false;
        ownerWindows[i] = 0;
        managerScenes[i] = 0;
      }
    }

    loka::app::scene::Scene *scene;
    Window *managerWindow;
    int notificationCount;
    SceneLifecycle values[2];
    bool attachedValues[2];
    Window *ownerWindows[2];
    loka::app::scene::Scene *managerScenes[2];
  };

  struct SceneReclaimObservation
  {
    SceneReclaimObservation()
        : compositionAttachCalls(0),
          compositionDetachCalls(0),
          contextAttachCalls(0),
          contextDetachCalls(0),
          platformDestroyCalls(0)
    {
    }

    int compositionAttachCalls;
    int compositionDetachCalls;
    int contextAttachCalls;
    int contextDetachCalls;
    int platformDestroyCalls;
  };

  class SceneReclaimBoundaryNode;

  struct SceneReclaimBoundaryTypeTag
  {
  };

  struct SceneReclaimBoundaryProps : public loka::app::scene::NodePropsBase<SceneReclaimBoundaryProps>
  {
    typedef SceneReclaimBoundaryTypeTag TypeTag;
    typedef SceneReclaimBoundaryNode NodeType;

    explicit SceneReclaimBoundaryProps(SceneReclaimObservation *value = 0)
        : observation(value)
    {
    }

    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
      {
        return this->propsTypeId() < rhs.propsTypeId();
      }
      const SceneReclaimBoundaryProps &other = static_cast<const SceneReclaimBoundaryProps &>(rhs);
      return this->observation < other.observation;
    }

    SceneReclaimObservation *observation;
  };

  class SceneReclaimBoundaryNode
      : public loka::app::scene::StdCompositionBoundaryNodeBase<SceneReclaimBoundaryProps>
  {
  public:
    explicit SceneReclaimBoundaryNode(const SceneReclaimBoundaryProps &props)
        : loka::app::scene::StdCompositionBoundaryNodeBase<SceneReclaimBoundaryProps>(props)
    {
    }

  protected:
    virtual void attachNode(loka::app::scene::NodeComposition &)
    {
      ++this->props.observation->compositionAttachCalls;
    }

    virtual void detachNode(loka::app::scene::NodeComposition &)
    {
      ++this->props.observation->compositionDetachCalls;
    }
  };

  typedef loka::app::scene::BoundaryDefinition<SceneReclaimBoundaryProps, SceneReclaimBoundaryNode>
      SceneReclaimBoundaryDefinition;

  class SceneReclaimProbe : public loka::app::scene::Scene
  {
  public:
    explicit SceneReclaimProbe(SceneReclaimObservation *observation)
        : loka::app::scene::Scene(SceneReclaimBoundaryDefinition(SceneReclaimBoundaryProps(observation)))
    {
      ++g_sceneOwnershipScenesAlive;
    }

    virtual ~SceneReclaimProbe()
    {
      --g_sceneOwnershipScenesAlive;
    }
  };

  class SceneReclaimNodeContext : public loka::app::scene::NodeContext
  {
  public:
    explicit SceneReclaimNodeContext(SceneReclaimObservation *observation)
        : observation_(observation)
    {
    }

    virtual void onNodeAttached()
    {
      ++this->observation_->contextAttachCalls;
    }

    virtual void onNodeDetached()
    {
      ++this->observation_->contextDetachCalls;
    }

  private:
    SceneReclaimObservation *observation_;
  };

  class SceneReclaimPlatformController : public loka::app::scene::IPlatformController
  {
  public:
    explicit SceneReclaimPlatformController(SceneReclaimObservation *observation)
        : observation_(observation)
    {
    }

    virtual void onChange(loka::app::scene::Node *rootNode,
                          loka::app::scene::NodeDirtyFlags,
                          bool)
    {
      rootNode->setContext(new SceneReclaimNodeContext(this->observation_));
    }

    virtual void synchronize() {}

    virtual bool hasPendingSync() const
    {
      return false;
    }

    virtual void destroy()
    {
      ++this->observation_->platformDestroyCalls;
    }

  private:
    SceneReclaimObservation *observation_;
  };

  struct WindowCreatingPlatformContext : public PlatformContext
  {
    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const
    {
      return 0;
    }

    virtual Window *createWindow(const WindowProps &props)
    {
      return new Window(this, props);
    }

    virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *) const
    {
      return 0;
    }

    virtual bool openFile(const loka::file::File &, loka::platform::file::FileHandle &) const
    {
      return false;
    }

    virtual bool createImageFromBlob(const loka::core::resource::Blob &, loka::core::resource::Image &) const
    {
      return false;
    }
  };

  class WindowRetirementProbe : public Window
  {
  public:
    WindowRetirementProbe(PlatformContext *context, const WindowProps &props)
        : Window(context, props)
    {
      ++g_windowRetirementWindowsAlive;
    }

    virtual ~WindowRetirementProbe()
    {
      --g_windowRetirementWindowsAlive;
    }
  };

  class WindowRetirementTestApp : public App
  {
  public:
    WindowRetirementTestApp()
        : App(0),
          quitCalls(0),
          appliedWindow(0),
          applyMenuCalls(0),
          closeDuringReclaim(0),
          reclaimCalls(0),
          windowsAliveDuringNestedFlush(0)
    {
      for (int i = 0; i < 4; ++i)
      {
        reclaimOrder[i] = 0;
      }
    }

    virtual void quit()
    {
      ++quitCalls;
    }

    void install(Window *window)
    {
      group_ = new AppComponentGroup(std::vector<AppComponent *>(1, window));
      this->setActiveWindow(window);
    }

    void install(Window *first, Window *second)
    {
      this->install(first);
      std::vector<AppComponent *> &components =
          const_cast<std::vector<AppComponent *> &>(group_->getComponents());
      components.push_back(second);
    }

    void install(Window *first, Window *second, Window *third)
    {
      this->install(first, second);
      std::vector<AppComponent *> &components =
          const_cast<std::vector<AppComponent *> &>(group_->getComponents());
      components.push_back(third);
    }

    void detachWithoutReselectForMisuseTest(Window *window)
    {
      std::vector<AppComponent *> &components =
          const_cast<std::vector<AppComponent *> &>(group_->getComponents());
      for (std::vector<AppComponent *>::iterator it = components.begin(); it != components.end(); ++it)
      {
        if ((*it)->asWindow() == window)
        {
          components.erase(it);
          return;
        }
      }
    }

    void flush()
    {
      this->flushWindowInvalidations();
    }

    int quitCalls;
    Window *appliedWindow;
    int applyMenuCalls;
    Window *closeDuringReclaim;
    int reclaimCalls;
    int windowsAliveDuringNestedFlush;
    Window *reclaimOrder[4];

    virtual void windowClosed(Window *window)
    {
      assert(reclaimCalls < 4);
      reclaimOrder[reclaimCalls] = window;
      ++reclaimCalls;
      if (closeDuringReclaim)
      {
        Window *next = closeDuringReclaim;
        closeDuringReclaim = 0;
        this->requestWindowClose(next);
        this->flushPendingWindowClosures();
        windowsAliveDuringNestedFlush = g_windowRetirementWindowsAlive;
      }
      App::windowClosed(window);
    }

  protected:
    virtual void applyMenuBar(Window *window)
    {
      ++applyMenuCalls;
      appliedWindow = window;
    }
  };

  struct WindowRetirementNotification
  {
    WindowRetirementNotification()
        : app(0),
          window(0),
          scene(0),
          calls(0)
    {
    }

    WindowRetirementTestApp *app;
    WindowRetirementProbe *window;
    SceneOwnershipProbe *scene;
    int calls;
  };

  void RequestWindowCloseDuringNotification(void *userData)
  {
    WindowRetirementNotification *notification = static_cast<WindowRetirementNotification *>(userData);
    ++notification->calls;
    notification->app->requestWindowClose(notification->window);
    notification->app->requestWindowClose(notification->window);
    assert(g_windowRetirementWindowsAlive == 1);
    assert(g_sceneOwnershipScenesAlive == 1);
    assert(notification->window->scene() == notification->scene);
    assert(notification->scene->getWindow() == notification->window);
  }

#if defined(LOKA_LIFECYCLE_AUDIT) && defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
  enum WindowClosedMisuse
  {
    WINDOW_CLOSED_WITHOUT_DETACH,
    WINDOW_CLOSED_WHILE_ACTIVE
  };

  void AssertWindowClosedMisuseAborts(WindowClosedMisuse misuse)
  {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0)
    {
      WindowCreatingPlatformContext context;
      WindowRetirementTestApp app;
      WindowRetirementProbe *first = new WindowRetirementProbe(&context, WindowProps());
      if (misuse == WINDOW_CLOSED_WITHOUT_DETACH)
      {
        WindowRetirementProbe *second = new WindowRetirementProbe(&context, WindowProps());
        app.install(first, second);
        app.windowClosed(second);
      }
      else
      {
        app.install(first);
        app.detachWithoutReselectForMisuseTest(first);
        app.windowClosed(first);
      }
      _exit(0);
    }

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGABRT);
  }
#endif

  struct ReentrantSceneFlushContext
  {
    ReentrantSceneFlushContext()
        : window(0),
          callbackCalls(0),
          scenesAliveAfterFlush(0)
    {
    }

    Window *window;
    int callbackCalls;
    int scenesAliveAfterFlush;
  };

  void FlushWindowDuringDetach(void *userData)
  {
    ReentrantSceneFlushContext *context = static_cast<ReentrantSceneFlushContext *>(userData);
    ++context->callbackCalls;
    context->window->flushSceneInvalidation();
    context->scenesAliveAfterFlush = g_sceneOwnershipScenesAlive;
  }

  void RecordSceneLifecycleNotification(void *userData)
  {
    SceneLifecycleObservation *observation = static_cast<SceneLifecycleObservation *>(userData);
    const int index = observation->notificationCount;
    assert(index < 2);
    observation->values[index] = observation->scene->getLifecycleState()->get();
    observation->attachedValues[index] = observation->scene->getAttachedState()->get();
    observation->ownerWindows[index] = observation->scene->getWindow();
    Window *managerWindow = observation->managerWindow ? observation->managerWindow : observation->ownerWindows[index];
    observation->managerScenes[index] = managerWindow ? managerWindow->scene() : 0;
    ++observation->notificationCount;
  }
} // namespace

void testWindowDefinitionTransfersSceneOwnershipToWindow()
{
  printf("\n==== [testWindowDefinitionTransfersSceneOwnershipToWindow] start ====\n");

  assert(g_sceneOwnershipDefinitionsAlive == 0);
  {
    WindowDefinition<WindowProps> definition;
    definition.props.scene(SceneOwnershipDefinition());
    assert(g_sceneOwnershipDefinitionsAlive == 1);

    WindowCreatingPlatformContext context;
    Window *window = definition.create(&context);
    assert(window != 0);
    assert(g_sceneOwnershipDefinitionsAlive == 2);

    delete window;
    assert(g_sceneOwnershipDefinitionsAlive == 1);
  }
  assert(g_sceneOwnershipDefinitionsAlive == 0);

  printf("==== [testWindowDefinitionTransfersSceneOwnershipToWindow] end ====\n");
}

void testWindowPropsSceneHandoffIsOneShotAcrossCopies()
{
  printf("\n==== [testWindowPropsSceneHandoffIsOneShotAcrossCopies] start ====\n");

  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps first;
  SceneOwnershipProbe *scene = new SceneOwnershipProbe();
  first.scene(scene);
  WindowProps second(first);

  Window *firstWindow = new Window(&context, first);
  Window *secondWindow = new Window(&context, second);
  assert(firstWindow->scene() == scene);
  assert(secondWindow->scene() == 0);

  delete firstWindow;
  delete secondWindow;
  assert(g_sceneOwnershipScenesAlive == 0);

  printf("==== [testWindowPropsSceneHandoffIsOneShotAcrossCopies] end ====\n");
}

void testWindowRetiresDetachedSceneAtFlushBoundary()
{
  printf("\n==== [testWindowRetiresDetachedSceneAtFlushBoundary] start ====\n");

  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneOwnershipProbe *first = new SceneOwnershipProbe();
  SceneOwnershipProbe *second = new SceneOwnershipProbe();
  props.scene(first);
  Window *window = new Window(&context, props);

  window->sceneManager()->commitTransaction(first, second);
  assert(window->scene() == second);
  assert(g_sceneOwnershipScenesAlive == 2);

  window->flushSceneInvalidation();
  assert(g_sceneOwnershipScenesAlive == 1);

  SceneOwnershipProbe *third = new SceneOwnershipProbe();
  window->sceneManager()->commitTransaction(second, third);
  assert(window->scene() == third);
  assert(g_sceneOwnershipScenesAlive == 2);

  // Window teardown drains both the still-retired Scene and the current Scene.
  delete window;
  assert(g_sceneOwnershipScenesAlive == 0);

  printf("==== [testWindowRetiresDetachedSceneAtFlushBoundary] end ====\n");
}

void testWindowDoesNotReclaimSceneDuringDetachNotification()
{
  printf("\n==== [testWindowDoesNotReclaimSceneDuringDetachNotification] start ====\n");

  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneOwnershipProbe *first = new SceneOwnershipProbe();
  SceneOwnershipProbe *second = new SceneOwnershipProbe();
  props.scene(first);
  Window *window = new Window(&context, props);

  ReentrantSceneFlushContext flushContext;
  flushContext.window = window;
  first->getAttachedState()->bind(&FlushWindowDuringDetach, &flushContext, false);

  window->sceneManager()->commitTransaction(first, second);
  assert(flushContext.callbackCalls == 1);
  assert(flushContext.scenesAliveAfterFlush == 2);
  assert(g_sceneOwnershipScenesAlive == 2);

  window->flushSceneInvalidation();
  assert(g_sceneOwnershipScenesAlive == 1);
  delete window;
  assert(g_sceneOwnershipScenesAlive == 0);

  printf("==== [testWindowDoesNotReclaimSceneDuringDetachNotification] end ====\n");
}

void testWindowReclaimDoesNotNotifySceneLifecycleObservers()
{
  printf("\n==== [testWindowReclaimDoesNotNotifySceneLifecycleObservers] start ====\n");

  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneOwnershipProbe *first = new SceneOwnershipProbe();
  SceneOwnershipProbe *second = new SceneOwnershipProbe();
  SceneLifecycleObservation firstObservation(first);
  SceneLifecycleObservation secondObservation(second);
  props.scene(first);

  assert(first->getLifecycleState()->get() == ON_CREATE);
  assert(!first->getAttachedState()->get());
  assert(first->getWindow() == 0);
  assert(second->getLifecycleState()->get() == ON_CREATE);
  assert(!second->getAttachedState()->get());
  assert(second->getWindow() == 0);
  first->getLifecycleState()->bind(&RecordSceneLifecycleNotification, &firstObservation, false);
  assert(firstObservation.notificationCount == 0);
  Window *window = new Window(&context, props);
  firstObservation.managerWindow = window;
  secondObservation.managerWindow = window;
  assert(firstObservation.notificationCount == 1);
  assert(firstObservation.values[0] == ON_ATTACH);
  assert(firstObservation.attachedValues[0]);
  assert(firstObservation.ownerWindows[0] == window);
  assert(firstObservation.managerScenes[0] == first);

  second->getLifecycleState()->bind(&RecordSceneLifecycleNotification, &secondObservation, false);
  assert(secondObservation.notificationCount == 0);
  window->sceneManager()->commitTransaction(first, second);
  assert(first->getLifecycleState()->get() == ON_DETACH);
  assert(firstObservation.notificationCount == 2);
  assert(firstObservation.values[1] == ON_DETACH);
  assert(!firstObservation.attachedValues[1]);
  assert(firstObservation.ownerWindows[1] == 0);
  assert(firstObservation.managerScenes[1] == first);
  assert(secondObservation.notificationCount == 1);
  assert(secondObservation.values[0] == ON_ATTACH);
  assert(secondObservation.attachedValues[0]);
  assert(secondObservation.ownerWindows[0] == window);
  assert(secondObservation.managerScenes[0] == second);

  window->flushSceneInvalidation();
  assert(g_sceneOwnershipScenesAlive == 1);
  assert(firstObservation.notificationCount == 2);

  delete window;
  assert(g_sceneOwnershipScenesAlive == 0);
  assert(firstObservation.notificationCount == 2);
  assert(secondObservation.notificationCount == 2);
  assert(secondObservation.values[1] == ON_DETACH);
  assert(!secondObservation.attachedValues[1]);
  assert(secondObservation.ownerWindows[1] == 0);
  assert(secondObservation.managerScenes[1] == second);

  printf("==== [testWindowReclaimDoesNotNotifySceneLifecycleObservers] end ====\n");
}

void testWindowReclaimFiresNoSceneCompositionCallbacks()
{
  printf("\n==== [testWindowReclaimFiresNoSceneCompositionCallbacks] start ====\n");

  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneReclaimObservation firstObservation;
  SceneReclaimObservation secondObservation;
  SceneReclaimProbe *first = new SceneReclaimProbe(&firstObservation);
  SceneReclaimProbe *second = new SceneReclaimProbe(&secondObservation);
  SceneReclaimPlatformController firstPlatform(&firstObservation);
  SceneReclaimPlatformController secondPlatform(&secondObservation);
  props.scene(first);
  Window *window = new Window(&context, props);

  first->mount(&firstPlatform);
  assert(firstObservation.compositionAttachCalls == 1);
  assert(firstObservation.contextAttachCalls == 1);
  window->sceneManager()->commitTransaction(first, second);
  assert(firstObservation.compositionDetachCalls == 1);
  assert(firstObservation.contextDetachCalls == 1);
  assert(firstObservation.platformDestroyCalls == 1);

  window->flushSceneInvalidation();
  assert(g_sceneOwnershipScenesAlive == 1);
  assert(firstObservation.compositionDetachCalls == 1);
  assert(firstObservation.contextDetachCalls == 1);
  assert(firstObservation.platformDestroyCalls == 1);

  second->mount(&secondPlatform);
  assert(secondObservation.compositionAttachCalls == 1);
  assert(secondObservation.contextAttachCalls == 1);
  delete window;
  assert(g_sceneOwnershipScenesAlive == 0);
  assert(secondObservation.compositionDetachCalls == 1);
  assert(secondObservation.contextDetachCalls == 1);
  assert(secondObservation.platformDestroyCalls == 1);

  printf("==== [testWindowReclaimFiresNoSceneCompositionCallbacks] end ====\n");
}

void testAppDefersWindowReclaimUntilInvalidationFlush()
{
  printf("\n==== [testAppDefersWindowReclaimUntilInvalidationFlush] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  assert(g_sceneOwnershipScenesAlive == 0);
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneOwnershipProbe *scene = new SceneOwnershipProbe();
  props.scene(scene);
  WindowRetirementProbe *window = new WindowRetirementProbe(&context, props);
  WindowRetirementTestApp app;
  app.install(window);
  assert(app.applyMenuCalls == 1);
  assert(app.appliedWindow == window);

  WindowRetirementNotification notification;
  notification.app = &app;
  notification.window = window;
  notification.scene = scene;
  loka::core::EmitterState close;
  close.bind(&RequestWindowCloseDuringNotification, &notification, false);
  close.emit();

  assert(notification.calls == 1);
  assert(g_windowRetirementWindowsAlive == 1);
  assert(g_sceneOwnershipScenesAlive == 1);
  assert(app.activeWindow() == 0);
  assert(app.appliedWindow == 0);
  assert(app.applyMenuCalls == 2);
  assert(app.quitCalls == 1);

  app.flush();
  assert(app.reclaimCalls == 1);
  assert(g_windowRetirementWindowsAlive == 0);
  assert(g_sceneOwnershipScenesAlive == 0);

  app.flush();
  assert(app.reclaimCalls == 1);

  printf("==== [testAppDefersWindowReclaimUntilInvalidationFlush] end ====\n");
}

void testAppDefersReentrantWindowCloseRequestUntilNextFlush()
{
  printf("\n==== [testAppDefersReentrantWindowCloseRequestUntilNextFlush] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  WindowCreatingPlatformContext context;
  WindowRetirementProbe *first = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementProbe *second = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementTestApp app;
  app.install(first, second);
  assert(app.applyMenuCalls == 1);
  app.closeDuringReclaim = second;

  app.requestWindowClose(first);
  assert(app.activeWindow() == second);
  assert(app.appliedWindow == second);
  assert(app.applyMenuCalls == 2);
  assert(g_windowRetirementWindowsAlive == 2);

  app.flush();
  assert(app.reclaimCalls == 1);
  assert(app.windowsAliveDuringNestedFlush == 2);
  assert(g_windowRetirementWindowsAlive == 1);
  assert(app.activeWindow() == 0);
  assert(app.appliedWindow == 0);
  assert(app.applyMenuCalls == 3);
  assert(app.quitCalls == 1);

  app.flush();
  assert(app.reclaimCalls == 2);
  assert(g_windowRetirementWindowsAlive == 0);

  printf("==== [testAppDefersReentrantWindowCloseRequestUntilNextFlush] end ====\n");
}

void testAppReclaimsWindowCloseBatchInRequestOrder()
{
  printf("\n==== [testAppReclaimsWindowCloseBatchInRequestOrder] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  WindowCreatingPlatformContext context;
  WindowRetirementProbe *first = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementProbe *second = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementProbe *third = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementTestApp app;
  app.install(first, second, third);

  app.requestWindowClose(second);
  app.requestWindowClose(first);
  app.requestWindowClose(third);
  assert(g_windowRetirementWindowsAlive == 3);
  assert(app.quitCalls == 1);

  app.flush();
  assert(app.reclaimCalls == 3);
  assert(app.reclaimOrder[0] == second);
  assert(app.reclaimOrder[1] == first);
  assert(app.reclaimOrder[2] == third);
  assert(g_windowRetirementWindowsAlive == 0);

  printf("==== [testAppReclaimsWindowCloseBatchInRequestOrder] end ====\n");
}

void testAppWindowReclaimDrainsRetiredScenesExactlyOnce()
{
  printf("\n==== [testAppWindowReclaimDrainsRetiredScenesExactlyOnce] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  assert(g_sceneOwnershipScenesAlive == 0);
  int retiredSceneDestructions = 0;
  int currentSceneDestructions = 0;
  WindowCreatingPlatformContext context;
  WindowProps props;
  SceneRetirementCountProbe *retired = new SceneRetirementCountProbe(&retiredSceneDestructions);
  SceneRetirementCountProbe *current = new SceneRetirementCountProbe(&currentSceneDestructions);
  props.scene(retired);
  WindowRetirementProbe *window = new WindowRetirementProbe(&context, props);
  window->sceneManager()->commitTransaction(retired, current);
  WindowRetirementTestApp app;
  app.install(window);

  assert(window->sceneManager()->hasRetiredScenes());
  app.requestWindowClose(window);
  assert(retiredSceneDestructions == 0);
  assert(currentSceneDestructions == 0);
  assert(g_sceneOwnershipScenesAlive == 2);

  app.flush();
  assert(retiredSceneDestructions == 1);
  assert(currentSceneDestructions == 1);
  assert(g_sceneOwnershipScenesAlive == 0);
  assert(app.reclaimCalls == 1);

  app.flush();
  assert(retiredSceneDestructions == 1);
  assert(currentSceneDestructions == 1);
  assert(app.reclaimCalls == 1);

  printf("==== [testAppWindowReclaimDrainsRetiredScenesExactlyOnce] end ====\n");
}

void testAppWindowCloseRequestsAreIdempotent()
{
  printf("\n==== [testAppWindowCloseRequestsAreIdempotent] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  WindowCreatingPlatformContext context;
  WindowRetirementProbe *owned = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementProbe *foreign = new WindowRetirementProbe(&context, WindowProps());
  WindowRetirementTestApp app;
  app.install(owned);

  app.requestWindowClose(foreign);
  app.flush();
  assert(app.activeWindow() == owned);
  assert(app.appliedWindow == owned);
  assert(app.applyMenuCalls == 1);
  assert(app.quitCalls == 0);
  assert(app.reclaimCalls == 0);
  assert(g_windowRetirementWindowsAlive == 2);

  app.requestWindowClose(owned);
  app.requestWindowClose(owned);
  app.flush();
  assert(app.reclaimCalls == 1);
  assert(app.reclaimOrder[0] == owned);
  assert(app.quitCalls == 1);
  assert(g_windowRetirementWindowsAlive == 1);

  app.flush();
  assert(app.reclaimCalls == 1);
  delete foreign;
  assert(g_windowRetirementWindowsAlive == 0);

  printf("==== [testAppWindowCloseRequestsAreIdempotent] end ====\n");
}

void testAppWindowClosedRejectsUndetachedOrActiveWindow()
{
  printf("\n==== [testAppWindowClosedRejectsUndetachedOrActiveWindow] start ====\n");

#if defined(LOKA_LIFECYCLE_AUDIT) && defined(__linux__) && !defined(__SANITIZE_ADDRESS__)
  AssertWindowClosedMisuseAborts(WINDOW_CLOSED_WITHOUT_DETACH);
  AssertWindowClosedMisuseAborts(WINDOW_CLOSED_WHILE_ACTIVE);
#endif

  printf("==== [testAppWindowClosedRejectsUndetachedOrActiveWindow] end ====\n");
}

void testAppDrainsPendingWindowClosuresAtDestruction()
{
  printf("\n==== [testAppDrainsPendingWindowClosuresAtDestruction] start ====\n");

  assert(g_windowRetirementWindowsAlive == 0);
  WindowCreatingPlatformContext context;
  {
    WindowRetirementProbe *window = new WindowRetirementProbe(&context, WindowProps());
    WindowRetirementTestApp app;
    app.install(window);
    app.requestWindowClose(window);
    assert(g_windowRetirementWindowsAlive == 1);
  }
  assert(g_windowRetirementWindowsAlive == 0);

  printf("==== [testAppDrainsPendingWindowClosuresAtDestruction] end ====\n");
}
