#include "HelloWorldResponsiveTests.hpp"

#include "../example/HelloWorld/src/MainNode.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/OwnedDef.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
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
  LOKA_VERIFY(mainPanels->props.effectiveAxis() == loka::app::STACK_AXIS_ROW);
  loka::app::scene::Node *leftPanel = mainPanels->childrenHead();
  LOKA_VERIFY(leftPanel != 0);
  loka::app::scene::Node *rightPanel = leftPanel->nextInComposition;
  LOKA_VERIFY(rightPanel != 0);
  LOKA_VERIFY(rightPanel->nextInComposition == 0);
  const loka::core::Frame wideDefault(50, 50, 420, 330);
  driveNativeFrameAndLayout(*window, platform, wideDefault);
  LOKA_VERIFY(mainPanels->props.effectiveAxis() == loka::app::STACK_AXIS_ROW);
  LOKA_VERIFY(platform.lastOnChangeFlags() == loka::app::scene::NODE_DIRTY_LAYOUT);
  const unsigned long wideLayoutCount = platform.onChangeCallCount();

  const loka::core::Frame narrowFrame(50, 50, 399, 330);
  loka::app::testing::WindowTestAccess::storeNativeFrame(*window, narrowFrame);
  LOKA_VERIFY(findMainPanelsScroll(*window->scene()) == mainPanelsScroll);
  LOKA_VERIFY(findMainPanels(*window->scene()) == mainPanels);
  LOKA_VERIFY(mainPanelsScroll->childrenHead() == mainPanels);
  LOKA_VERIFY(mainPanels->props.effectiveAxis() == loka::app::STACK_AXIS_COLUMN);
  LOKA_VERIFY(mainPanels->childrenHead() == leftPanel);
  LOKA_VERIFY(leftPanel->nextInComposition == rightPanel);
  // One platform notification per flip: the retained Stack's LAYOUT dirt.
  // The initial helper drives its own layout call explicitly; these flips
  // publish only the frame so this count isolates State-driven delivery.
  LOKA_VERIFY(platform.onChangeCallCount() == wideLayoutCount + 1);
  LOKA_VERIFY(platform.lastOnChangeFlags() == loka::app::scene::NODE_DIRTY_LAYOUT);

  loka::app::testing::WindowTestAccess::storeNativeFrame(*window, wideDefault);
  LOKA_VERIFY(findMainPanelsScroll(*window->scene()) == mainPanelsScroll);
  LOKA_VERIFY(findMainPanels(*window->scene()) == mainPanels);
  LOKA_VERIFY(mainPanelsScroll->childrenHead() == mainPanels);
  LOKA_VERIFY(mainPanels->props.effectiveAxis() == loka::app::STACK_AXIS_ROW);
  LOKA_VERIFY(mainPanels->childrenHead() == leftPanel);
  LOKA_VERIFY(leftPanel->nextInComposition == rightPanel);
  LOKA_VERIFY(platform.onChangeCallCount() == wideLayoutCount + 2);
  LOKA_VERIFY(platform.lastOnChangeFlags() == loka::app::scene::NODE_DIRTY_LAYOUT);

  delete window;
}

namespace
{
  class NarrowMountMainNode : public helloworld::MainNode
  {
  public:
    explicit NarrowMountMainNode(const helloworld::MainProps &props)
        : helloworld::MainNode(props) {}

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      helloworld::MainNode::composeNode(composition);
      // Inspect the declaration before a late immediate watch could trigger
      // another composition and hide the first frame's axis.
      loka::app::scene::INestableDefinition *root =
          composition.root()->asNestableDefinition();
      LOKA_VERIFY(root != 0);
      loka::app::scene::NodeDefinitionBase *scroll = root->childrenHead();
      LOKA_VERIFY(scroll != 0);
      const std::string scrollId = scroll->testIdValue();
      LOKA_VERIFY(scrollId == "HelloWorld.MainPanelsScroll");
      loka::app::scene::INestableDefinition *scrollChildren = scroll->asNestableDefinition();
      LOKA_VERIFY(scrollChildren != 0);
      loka::app::scene::NodeDefinitionBase *panels = scrollChildren->childrenHead();
      LOKA_VERIFY(panels != 0);
      const std::string panelsId = panels->testIdValue();
      LOKA_VERIFY(panelsId == "HelloWorld.MainPanels");
      const loka::app::scene::PropsBase *props = panels->propsBase();
      LOKA_VERIFY(props && props->propsTypeId() == loka::app::StackProps::staticTypeId());
      LOKA_VERIFY(static_cast<const loka::app::StackProps *>(props)->effectiveAxis() ==
                  loka::app::STACK_AXIS_COLUMN);
    }
  };
} // namespace

void testHelloWorldNarrowMountComposesColumnFirst()
{
  NullPlatformContext context;
  NullScenePlatformController platform;
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(
      loka::app::scene::BoundaryDefinition<helloworld::MainProps, NarrowMountMainNode>().clone());
  const bool rootCloned = root.isSet();
  LOKA_VERIFY(rootCloned);
  WindowProps props;
  props.frame(50, 50, 399, 330);
  NullWindow window(&context, props, &platform);
  WindowAdmissionTestApp admission(window);
  // Publish before attach; inspect the first tree without a resize/layout pass.
  loka::app::testing::WindowTestAccess::storeNativeFrame(
      window, loka::core::Frame(50, 50, 399, 330));
  window.sceneManager()->commitTransaction(0, new loka::app::scene::Scene(root.take()));
  admission.flush();
  LOKA_VERIFY(findMainPanels(*window.scene())->props.effectiveAxis() ==
              loka::app::STACK_AXIS_COLUMN);
}
