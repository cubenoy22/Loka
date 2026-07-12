#include "SceneOwnershipTests.hpp"
#include <cassert>
#include <cstdio>
#include "app/PlatformContext.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/nodes/nestable/Box.hpp"
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
