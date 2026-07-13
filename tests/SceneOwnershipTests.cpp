#include "SceneOwnershipTests.hpp"
#include <cassert>
#include <cstdio>
#include "app/PlatformContext.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/Scene.hpp"

namespace
{
  static int g_sceneOwnershipDefinitionsAlive = 0;
  static int g_sceneOwnershipScenesAlive = 0;

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
