#include "HelloWorldResponsiveTests.hpp"

#include "../example/HelloWorld/src/MainNode.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/TestVerify.hpp"
#include "testing/app/WindowTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  void driveNativeFrameAndLayout(NullWindow &window,
                                 NullScenePlatformController &platform,
                                 const loka::core::Frame &frame)
  {
    loka::app::testing::WindowTestAccess::storeNativeFrame(window, frame);
    loka::app::scene::Scene *scene = window.scene();
    LOKA_VERIFY(scene != 0);
    // The null rail has no native resize loop. Mirror the real rails' order:
    // publish the frame fact first, then lay out the now-current logical tree.
    platform.onChange(
        loka::dsl::testing::SceneTestAccess::rootNode(*scene),
        loka::app::scene::NODE_DIRTY_LAYOUT,
        false);
  }

  void findMainPanelsRecursive(loka::app::scene::Node *node,
                               long &idMatches,
                               long &typedMatches,
                               loka::app::StackNode *&result)
  {
    if (!node)
    {
      return;
    }
    if (node->testId() == "HelloWorld.MainPanels")
    {
      ++idMatches;
      loka::app::StackNode *stack = node->asStackNode();
      if (stack)
      {
        ++typedMatches;
        if (!result)
        {
          result = stack;
        }
      }
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      findMainPanelsRecursive(child, idMatches, typedMatches, result);
    }
  }

  loka::app::StackNode *findMainPanels(loka::app::scene::Scene &scene)
  {
    long idMatches = 0;
    long typedMatches = 0;
    loka::app::StackNode *stack = 0;
    findMainPanelsRecursive(loka::dsl::testing::SceneTestAccess::rootNode(scene),
                            idMatches,
                            typedMatches,
                            stack);
    LOKA_VERIFY(idMatches == 1);
    LOKA_VERIFY(typedMatches == 1);
    LOKA_VERIFY(stack != 0);
    return stack;
  }

  void findMainPanelsScrollRecursive(loka::app::scene::Node *node,
                                     long &idMatches,
                                     long &typedMatches,
                                     loka::app::ScrollViewNode *&result)
  {
    if (!node)
    {
      return;
    }
    if (node->testId() == "HelloWorld.MainPanelsScroll")
    {
      ++idMatches;
      loka::app::ScrollViewNode *scrollView = node->asScrollViewNode();
      if (scrollView)
      {
        ++typedMatches;
        if (!result)
        {
          result = scrollView;
        }
      }
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      findMainPanelsScrollRecursive(child, idMatches, typedMatches, result);
    }
  }

  loka::app::ScrollViewNode *findMainPanelsScroll(loka::app::scene::Scene &scene)
  {
    long idMatches = 0;
    long typedMatches = 0;
    loka::app::ScrollViewNode *scrollView = 0;
    findMainPanelsScrollRecursive(
        loka::dsl::testing::SceneTestAccess::rootNode(scene),
        idMatches,
        typedMatches,
        scrollView);
    LOKA_VERIFY(idMatches == 1);
    LOKA_VERIFY(typedMatches == 1);
    LOKA_VERIFY(scrollView != 0);
    return scrollView;
  }
} // namespace

void testHelloWorldResponsivePanelsFollowNativeFrameAndRetainSeats()
{
  NullPlatformContext context;
  NullScenePlatformController platform;
  loka::app::scene::NodeDefinitionBase *rootDefinition =
      loka::app::scene::Boundary<helloworld::MainNode>().clone();
  LOKA_VERIFY(rootDefinition != 0);
  WindowProps props;
  props.frame(50, 50, 420, 330);
  props.scene(new loka::app::scene::Scene(rootDefinition));
  NullWindow *window = new NullWindow(&context, props, &platform);
  LOKA_VERIFY(window->scene() != 0);
  window->scene()->updateAttached(true);

  loka::app::ScrollViewNode *mainPanelsScroll =
      findMainPanelsScroll(*window->scene());
  loka::app::StackNode *mainPanels = findMainPanels(*window->scene());
  LOKA_VERIFY(mainPanelsScroll->childrenHead() == mainPanels);
  LOKA_VERIFY(mainPanels->nextInComposition == 0);
  LOKA_VERIFY(mainPanels->props.axis_ == loka::app::STACK_AXIS_ROW);
  loka::app::scene::Node *leftPanel = mainPanels->childrenHead();
  LOKA_VERIFY(leftPanel != 0);
  loka::app::scene::Node *rightPanel = leftPanel->nextInComposition;
  LOKA_VERIFY(rightPanel != 0);
  LOKA_VERIFY(rightPanel->nextInComposition == 0);
  const loka::core::Frame wideDefault(50, 50, 420, 330);
  driveNativeFrameAndLayout(*window, platform, wideDefault);
  LOKA_VERIFY(mainPanels->props.axis_ == loka::app::STACK_AXIS_ROW);
  LOKA_VERIFY((platform.lastOnChangeFlags() &
               loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
  const unsigned long wideLayoutCount = platform.onChangeCallCount();

  const loka::core::Frame narrowFrame(50, 50, 399, 330);
  driveNativeFrameAndLayout(*window, platform, narrowFrame);
  LOKA_VERIFY(findMainPanelsScroll(*window->scene()) == mainPanelsScroll);
  LOKA_VERIFY(findMainPanels(*window->scene()) == mainPanels);
  LOKA_VERIFY(mainPanelsScroll->childrenHead() == mainPanels);
  LOKA_VERIFY(mainPanels->props.axis_ == loka::app::STACK_AXIS_COLUMN);
  LOKA_VERIFY(mainPanels->childrenHead() == leftPanel);
  LOKA_VERIFY(leftPanel->nextInComposition == rightPanel);
  // A flip is two platform notifications: the scene delivers the axis
  // props/layout change from the recompose apply (PROPS|LAYOUT - the
  // rail-facing relayout request the wide route never needs), then the
  // helper mirrors the rail's own resize-driven layout pass.
  LOKA_VERIFY(platform.onChangeCallCount() == wideLayoutCount + 2);
  LOKA_VERIFY((platform.lastOnChangeFlags() &
               loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  driveNativeFrameAndLayout(*window, platform, wideDefault);
  LOKA_VERIFY(findMainPanelsScroll(*window->scene()) == mainPanelsScroll);
  LOKA_VERIFY(findMainPanels(*window->scene()) == mainPanels);
  LOKA_VERIFY(mainPanelsScroll->childrenHead() == mainPanels);
  LOKA_VERIFY(mainPanels->props.axis_ == loka::app::STACK_AXIS_ROW);
  LOKA_VERIFY(mainPanels->childrenHead() == leftPanel);
  LOKA_VERIFY(leftPanel->nextInComposition == rightPanel);
  LOKA_VERIFY(platform.onChangeCallCount() == wideLayoutCount + 4);
  LOKA_VERIFY((platform.lastOnChangeFlags() &
               loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  delete window;
}
