#include "MacScenePlatformTests.hpp"

#include <cassert>

#include <AppKit/AppKit.h>

#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "MacScenePlatformController.hpp"
#include "testing/MacScenePlatformTestAccess.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/State.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"

namespace
{
  loka::core::MutableState<loka::core::String> *g_textState = 0;
  loka::core::MutableState<bool> *g_enabledState = 0;
  loka::core::MutableState<bool> *g_swapChildState = 0;

  class DynamicMacTestNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacTestNode> DynamicMacTestProps;

  class DynamicMacTestNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacTestNode>
  {
  public:
    DynamicMacTestNode(const DynamicMacTestProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacTestNode>(DynamicMacTestProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ColumnDefinition stack = loka::app::VStack();
      if (g_textState)
      {
        stack << loka::app::Text(g_textState)
                     .attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD))
                     .testId("MainText");
      }
      if (g_enabledState)
      {
        stack << loka::app::Button("Run").enabled(g_enabledState).testId("MainButton");
      }
      c.declare(stack);
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacTestProps, DynamicMacTestNode> DynamicMacTestBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacTestNode>();
  }

  class DynamicMacSwapNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacSwapNode> DynamicMacSwapProps;

  class DynamicMacSwapNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacSwapNode>
  {
  public:
    DynamicMacSwapNode(const DynamicMacSwapProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacSwapNode>(DynamicMacSwapProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_swapChildState && g_swapChildState->get())
      {
        c.declare(loka::app::EditText(loka::core::StaticState<loka::core::String>(loka::core::String::Literal("Edit")))
                      .controlTag(301)
                      .testId("SwapEdit"));
      }
      else
      {
        c.declare(loka::app::Button("Run").testId("SwapButton"));
      }
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_swapChildState)
      {
        registrar.observe(g_swapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacSwapProps, DynamicMacSwapNode> DynamicMacSwapBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacSwapNode>();
  }

  class DynamicMacSwapHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicMacSwapHostNode> DynamicMacSwapHostProps;

  class DynamicMacSwapHostNode : public loka::app::scene::BoundaryNodeFor<DynamicMacSwapHostNode>
  {
  public:
    DynamicMacSwapHostNode(const DynamicMacSwapHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicMacSwapHostNode>(DynamicMacSwapHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicMacSwapBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacSwapHostProps, DynamicMacSwapHostNode> DynamicMacSwapHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicMacSwapHostNode>();
  }

  ::loka::app::scene::Node *findNodeByTestId(::loka::app::scene::Node *node, const std::string &testId)
  {
    if (!node)
    {
      return 0;
    }
    if (node->testId() == testId)
    {
      return node;
    }
    ::loka::app::scene::INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return 0;
    }
    ::loka::app::scene::Node *child = nestable->childrenHead();
    while (child)
    {
      ::loka::app::scene::Node *found = findNodeByTestId(child, testId);
      if (found)
      {
        return found;
      }
      child = child->nextInComposition;
    }
    return 0;
  }

  loka::core::MutableState<loka::core::String> *g_staticEditTextState = 0;
  loka::core::MutableState<int> *g_staticSelectedIndexState = 0;
  loka::core::MutableState<loka::core::resource::Image> *g_staticImageState = 0;

  class StaticMacControlNode;
  typedef loka::app::scene::StaticCompositionPropsFor<StaticMacControlNode> StaticMacControlProps;

  class StaticMacControlNode : public loka::app::scene::StaticCompositionNodeFor<StaticMacControlNode>
  {
  public:
    StaticMacControlNode(const StaticMacControlProps &p)
        : loka::app::scene::StaticCompositionNodeFor<StaticMacControlNode>(StaticMacControlProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kPopupItems[] = {"One", "Two", "Three"};
      c.declare(
          loka::app::VStack()
          << loka::app::Button("Run").testId("ReuseButton")
          << loka::app::EditText(g_staticEditTextState).controlTag(101).testId("ReuseEdit")
          << loka::app::PopupMenu(kPopupItems, 3).selectedIndex(g_staticSelectedIndexState).testId("ReusePopup")
          << loka::app::ImageView().image(g_staticImageState).size(64, 64).testId("ReuseImage"));
    }
  };

  loka::app::scene::BoundaryDefinition<StaticMacControlProps, StaticMacControlNode> StaticMacControlBoundary()
  {
    return loka::app::scene::StaticCompositionBoundary<StaticMacControlNode>();
  }

  loka::core::MutableState<loka::core::String> *g_staticTextState = 0;
  loka::core::MutableState<loka::core::String> *g_staticCellState = 0;

  class StaticMacCellTextNode;
  typedef loka::app::scene::StaticCompositionPropsFor<StaticMacCellTextNode> StaticMacCellTextProps;

  class StaticMacCellTextNode : public loka::app::scene::StaticCompositionNodeFor<StaticMacCellTextNode>
  {
  public:
    StaticMacCellTextNode(const StaticMacCellTextProps &p)
        : loka::app::scene::StaticCompositionNodeFor<StaticMacCellTextNode>(StaticMacCellTextProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(
          loka::app::VStack()
          << loka::app::Text(g_staticTextState).testId("ReuseText")
          << loka::app::Cell(g_staticCellState).testId("ReuseCell"));
    }
  };

  loka::app::scene::BoundaryDefinition<StaticMacCellTextProps, StaticMacCellTextNode> StaticMacCellTextBoundary()
  {
    return loka::app::scene::StaticCompositionBoundary<StaticMacCellTextNode>();
  }
}

void testMacScenePlatformRelayoutRequest()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));
  g_textState = &textState;
  g_enabledState = 0;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 120)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacTestBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  assert(!loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));
  assert((loka::dsl::testing::MacScenePlatformTestAccess::lastChangeFlags(controller) &
          loka::app::scene::NODE_DIRTY_INITIAL) != 0);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    textState.set(loka::core::String::Literal("A much longer line that should request relayout"));
  }

  assert(loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));
  scene.flushInvalidation();
  assert((loka::dsl::testing::MacScenePlatformTestAccess::lastChangeFlags(controller) &
          loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
  assert(!loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));

  scene.unmount();
  [rootView release];
  [pool drain];
  g_textState = 0;
}

void testMacScenePlatformIgnoresNonLayoutDirtyRequest()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));
  loka::core::MutableState<bool> enabledState(false);
  g_textState = &textState;
  g_enabledState = &enabledState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 120)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacTestBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  assert(!loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    enabledState.set(true);
  }

  assert(!loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));
  scene.flushInvalidation();
  const loka::app::scene::NodeDirtyFlags flags =
      loka::dsl::testing::MacScenePlatformTestAccess::lastChangeFlags(controller);
  assert((flags & loka::app::scene::NODE_DIRTY_PROPS) != 0);
  assert((flags & loka::app::scene::NODE_DIRTY_LAYOUT) == 0);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_textState = 0;
  g_enabledState = 0;
}

void testMacScenePlatformDynamicPropsAndLayoutReuseContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));
  loka::core::MutableState<bool> enabledState(false);
  g_textState = &textState;
  g_enabledState = &enabledState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 120)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacTestBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "MainText");
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  ::loka::app::scene::NodeContext *textContext = textNode->getContext();
  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  assert(textContext != 0);
  assert(buttonContext != 0);
  const NSUInteger initialSubviewCount = [[rootView subviews] count];

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    enabledState.set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  textNode = findNodeByTestId(root, "MainText");
  buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  assert(textNode->getContext() == textContext);
  assert(buttonNode->getContext() == buttonContext);
  assert([[rootView subviews] count] == initialSubviewCount);

  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    textState.set(loka::core::String::Literal("A much longer line that should relayout without rebuilding controls"));
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  textNode = findNodeByTestId(root, "MainText");
  buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  assert(textNode->getContext() == textContext);
  assert(buttonNode->getContext() == buttonContext);
  assert([[rootView subviews] count] == initialSubviewCount);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_textState = 0;
  g_enabledState = 0;
}

void testMacScenePlatformRelayoutReusesControlContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  loka::core::resource::Image initialImage;
  loka::core::MutableState<loka::core::resource::Image> imageState(initialImage);
  g_staticEditTextState = &editTextState;
  g_staticSelectedIndexState = &selectedIndexState;
  g_staticImageState = &imageState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 220)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(StaticMacControlBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);

  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ReuseButton");
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ReuseEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ReusePopup");
  ::loka::app::scene::Node *imageNode = findNodeByTestId(root, "ReuseImage");
  assert(buttonNode != 0);
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(imageNode != 0);

  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  ::loka::app::scene::NodeContext *imageContext = imageNode->getContext();
  assert(buttonContext != 0);
  assert(editContext != 0);
  assert(popupContext != 0);
  assert(imageContext != 0);

  controller.relayout(320, 240);

  assert(buttonNode->getContext() == buttonContext);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert(imageNode->getContext() == imageContext);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_staticEditTextState = 0;
  g_staticSelectedIndexState = 0;
  g_staticImageState = 0;
}

void testMacScenePlatformRelayoutReusesCellAndTextContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Label"));
  loka::core::MutableState<loka::core::String> cellState(loka::core::String::Literal("42"));
  g_staticTextState = &textState;
  g_staticCellState = &cellState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 220)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(StaticMacCellTextBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);

  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "ReuseText");
  ::loka::app::scene::Node *cellNode = findNodeByTestId(root, "ReuseCell");
  assert(textNode != 0);
  assert(cellNode != 0);

  ::loka::app::scene::NodeContext *textContext = textNode->getContext();
  ::loka::app::scene::NodeContext *cellContext = cellNode->getContext();
  assert(textContext != 0);
  assert(cellContext != 0);

  controller.relayout(320, 240);

  assert(textNode->getContext() == textContext);
  assert(cellNode->getContext() == cellContext);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_staticTextState = 0;
  g_staticCellState = 0;
}

void testMacScenePlatformFullRebuildFlagControlsContextReuse()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  loka::core::resource::Image initialImage;
  loka::core::MutableState<loka::core::resource::Image> imageState(initialImage);
  g_staticEditTextState = &editTextState;
  g_staticSelectedIndexState = &selectedIndexState;
  g_staticImageState = &imageState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 220)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(StaticMacControlBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);

  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ReuseButton");
  assert(buttonNode != 0);
  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  assert(buttonContext != 0);

  controller.onChange(root, loka::app::scene::NODE_DIRTY_LAYOUT, false);
  assert(buttonNode->getContext() == buttonContext);

  controller.onChange(root, loka::app::scene::NODE_DIRTY_CHILD, true);
  assert(buttonNode->getContext() != 0);
  assert(buttonNode->getContext() != buttonContext);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_staticEditTextState = 0;
  g_staticSelectedIndexState = 0;
  g_staticImageState = 0;
}

void testMacScenePlatformChildRebuildCleansUpOldContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<bool> swapState(false);
  g_swapChildState = &swapState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 160)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacSwapHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "SwapButton");
  assert(buttonNode != 0);
  assert(buttonNode->getContext() != 0);
  assert([rootView subviews] != nil);
  assert([[rootView subviews] count] == 1);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  boundary = boundary ? boundary->childrenHead() ? boundary->childrenHead()->asBoundary() : 0 : 0;
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    swapState.set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "SwapButton") == 0);
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "SwapEdit");
  assert(editNode != 0);
  assert(editNode->getContext() != 0);
  assert([[rootView subviews] count] == 1);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_swapChildState = 0;
}
