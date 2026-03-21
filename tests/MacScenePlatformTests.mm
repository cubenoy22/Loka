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
#include "../example/HelloWorld/src/MainNode.hpp"

namespace
{
  loka::core::MutableState<loka::core::String> *g_textState = 0;
  loka::core::MutableState<bool> *g_enabledState = 0;
  loka::core::MutableState<bool> *g_swapChildState = 0;
  loka::core::MutableState<bool> *g_foreignSwapChildState = 0;
  loka::core::MutableState<loka::core::String> *g_foreignPersistEditState = 0;
  loka::core::MutableState<int> *g_foreignPersistSelectedIndexState = 0;
  loka::core::MutableState<bool> *g_foreignReorderState = 0;

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

  class DynamicMacForeignSwapNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacForeignSwapNode> DynamicMacForeignSwapProps;

  class DynamicMacForeignSwapNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignSwapNode>
  {
  public:
    DynamicMacForeignSwapNode(const DynamicMacForeignSwapProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignSwapNode>(DynamicMacForeignSwapProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_foreignSwapChildState && g_foreignSwapChildState->get())
      {
        c.declare(loka::app::Text("Dynamic rebuild branch").testId("ForeignSwapText"));
      }
      else
      {
        c.declare(loka::app::Button("Dynamic hidden branch").testId("ForeignSwapButton"));
      }
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignSwapChildState)
      {
        registrar.observe(g_foreignSwapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignSwapProps, DynamicMacForeignSwapNode> DynamicMacForeignSwapBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacForeignSwapNode>();
  }

  class DynamicMacForeignSwapHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicMacForeignSwapHostNode> DynamicMacForeignSwapHostProps;

  class DynamicMacForeignSwapHostNode : public loka::app::scene::BoundaryNodeFor<DynamicMacForeignSwapHostNode>
  {
  public:
    DynamicMacForeignSwapHostNode(const DynamicMacForeignSwapHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicMacForeignSwapHostNode>(DynamicMacForeignSwapHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignSwapChildState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignSwapChildState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::HStack()
                << loka::app::Text("StaticMarker").testId("ForeignStaticMarker")
                << DynamicMacForeignSwapBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignSwapHostProps, DynamicMacForeignSwapHostNode> DynamicMacForeignSwapHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicMacForeignSwapHostNode>();
  }

  class DynamicMacForeignPersistNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacForeignPersistNode> DynamicMacForeignPersistProps;

  class DynamicMacForeignPersistNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignPersistNode>
  {
  public:
    DynamicMacForeignPersistNode(const DynamicMacForeignPersistProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignPersistNode>(DynamicMacForeignPersistProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      loka::app::ColumnDefinition column = loka::app::VStack();
      if (g_foreignSwapChildState && g_foreignSwapChildState->get())
      {
        column << loka::app::Text("Dynamic rebuild branch").testId("ForeignPersistText").tag(20);
      }
      else
      {
        column << loka::app::Button("Dynamic hidden branch").testId("ForeignPersistButton").tag(20);
      }
      column << loka::app::EditText(g_foreignPersistEditState).controlTag(351).testId("ForeignPersistEdit").tag(10)
             << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignPersistPopup").tag(11);
      c.declare(column);
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignSwapChildState)
      {
        registrar.observe(g_foreignSwapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignPersistProps, DynamicMacForeignPersistNode> DynamicMacForeignPersistBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacForeignPersistNode>();
  }

  class DynamicMacForeignPersistHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicMacForeignPersistHostNode> DynamicMacForeignPersistHostProps;

  class DynamicMacForeignPersistHostNode : public loka::app::scene::BoundaryNodeFor<DynamicMacForeignPersistHostNode>
  {
  public:
    DynamicMacForeignPersistHostNode(const DynamicMacForeignPersistHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicMacForeignPersistHostNode>(DynamicMacForeignPersistHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignSwapChildState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignSwapChildState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicMacForeignPersistBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignPersistHostProps, DynamicMacForeignPersistHostNode> DynamicMacForeignPersistHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicMacForeignPersistHostNode>();
  }

  class DynamicMacForeignReorderNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacForeignReorderNode> DynamicMacForeignReorderProps;

  class DynamicMacForeignReorderNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignReorderNode>
  {
  public:
    DynamicMacForeignReorderNode(const DynamicMacForeignReorderProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacForeignReorderNode>(DynamicMacForeignReorderProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      loka::app::ColumnDefinition column = loka::app::VStack();
      if (g_foreignReorderState && g_foreignReorderState->get())
      {
        column << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignReorderPopup").tag(11)
               << loka::app::EditText(g_foreignPersistEditState).controlTag(361).testId("ForeignReorderEdit").tag(10);
      }
      else
      {
        column << loka::app::EditText(g_foreignPersistEditState).controlTag(361).testId("ForeignReorderEdit").tag(10)
               << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignReorderPopup").tag(11);
      }
      c.declare(column);
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignReorderState)
      {
        registrar.observe(g_foreignReorderState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignReorderProps, DynamicMacForeignReorderNode> DynamicMacForeignReorderBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacForeignReorderNode>();
  }

  class DynamicMacForeignReorderHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicMacForeignReorderHostNode> DynamicMacForeignReorderHostProps;

  class DynamicMacForeignReorderHostNode : public loka::app::scene::BoundaryNodeFor<DynamicMacForeignReorderHostNode>
  {
  public:
    DynamicMacForeignReorderHostNode(const DynamicMacForeignReorderHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicMacForeignReorderHostNode>(DynamicMacForeignReorderHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignReorderState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignReorderState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicMacForeignReorderBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicMacForeignReorderHostProps, DynamicMacForeignReorderHostNode> DynamicMacForeignReorderHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicMacForeignReorderHostNode>();
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

  bool captureBitmapByTestId(loka::app::scene::Scene &scene, const char *testId, loka::core::resource::Image &out)
  {
    ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
    assert(root != 0);
    ::loka::app::scene::Node *node = findNodeByTestId(root, std::string(testId));
    assert(node != 0);
    ::loka::app::scene::NodeContext *context = node->getContext();
    assert(context != 0);
    const ::loka::app::scene::ICapturableBitmap *capturable = context->asCapturableBitmap();
    assert(capturable != 0);
    return capturable->captureBitmap(out);
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

  // fullRebuild=true with the same node tree: context is preserved because
  // the composition system (not the platform controller) is responsible for
  // releasing contexts of replaced/retired nodes.  Retained nodes keep theirs.
  controller.onChange(root, loka::app::scene::NODE_DIRTY_CHILD, true);
  assert(buttonNode->getContext() != 0);
  assert(buttonNode->getContext() == buttonContext);

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

void testMacScenePlatformForeignObservedChildRebuildSwapsContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 160)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacForeignSwapHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ForeignSwapButton");
  assert(buttonNode != 0);
  assert(buttonNode->getContext() != 0);
  assert(findNodeByTestId(root, "ForeignSwapText") == 0);
  const NSUInteger initialSubviewCount = [[rootView subviews] count];

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignSwapChildState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignSwapChildState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "ForeignSwapButton") == 0);
  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "ForeignSwapText");
  assert(textNode != 0);
  assert(textNode->getContext() != 0);
  assert([[rootView subviews] count] == initialSubviewCount);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_foreignSwapChildState = 0;
}

void testMacScenePlatformForeignObservedChildRebuildPreservesSiblingContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  g_foreignPersistEditState = &editTextState;
  g_foreignPersistSelectedIndexState = &selectedIndexState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 200)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacForeignPersistHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ForeignPersistButton");
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ForeignPersistEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ForeignPersistPopup");
  assert(buttonNode != 0);
  assert(editNode != 0);
  assert(popupNode != 0);
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  assert(editContext != 0);
  assert(popupContext != 0);
  const NSUInteger initialSubviewCount = [[rootView subviews] count];

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignSwapChildState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignSwapChildState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "ForeignPersistButton") == 0);
  assert(findNodeByTestId(root, "ForeignPersistText") != 0);
  editNode = findNodeByTestId(root, "ForeignPersistEdit");
  popupNode = findNodeByTestId(root, "ForeignPersistPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert([[rootView subviews] count] == initialSubviewCount);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_foreignPersistEditState = 0;
  g_foreignPersistSelectedIndexState = 0;
  g_foreignSwapChildState = 0;
}

void testMacScenePlatformForeignObservedChildReorderPreservesSiblingContexts()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  g_foreignPersistEditState = &editTextState;
  g_foreignPersistSelectedIndexState = &selectedIndexState;

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 200)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(DynamicMacForeignReorderHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ForeignReorderEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ForeignReorderPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  assert(editContext != 0);
  assert(popupContext != 0);
  const NSUInteger initialSubviewCount = [[rootView subviews] count];

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignReorderState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignReorderState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  editNode = findNodeByTestId(root, "ForeignReorderEdit");
  popupNode = findNodeByTestId(root, "ForeignReorderPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert([[rootView subviews] count] == initialSubviewCount);

  scene.unmount();
  [rootView release];
  [pool drain];
  g_foreignPersistEditState = 0;
  g_foreignPersistSelectedIndexState = 0;
  g_foreignReorderState = 0;
}

void testMacScenePlatformHelloWorldCapturesTextAndButtons()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 640, 360)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::NodeDefinition<helloworld::MainProps, helloworld::MainNode> mainDef;
  loka::app::scene::Scene scene(mainDef.clone());
  scene.mount(&controller);
  scene.updateAttached(true);

  loka::core::resource::Image initialMessage;
  loka::core::resource::Image initialProbeButton;
  assert(captureBitmapByTestId(scene, "HelloWorld.LeftPanel.Message", initialMessage));
  assert(initialMessage.isValid());
  assert(initialMessage.width() > 0);
  assert(initialMessage.height() > 0);
  assert(captureBitmapByTestId(scene, "HelloWorld.LeftPanel.ProbeButton", initialProbeButton));
  assert(initialProbeButton.isValid());
  assert(initialProbeButton.width() > 0);
  assert(initialProbeButton.height() > 0);

  loka::app::scene::Scene *scenePtr = &scene;
  loka::dsl::FlowChain<loka::app::scene::Scene *, loka::app::scene::Scene *> addChain =
      loka::dsl::Flow()
      | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.AddButton")).input(&scenePtr);
  assert(addChain.run());

  loka::core::resource::Image updatedMessage;
  assert(captureBitmapByTestId(scene, "HelloWorld.LeftPanel.Message", updatedMessage));
  assert(updatedMessage.isValid());
  assert(updatedMessage.width() > 0);
  assert(updatedMessage.height() > 0);

  loka::dsl::FlowChain<loka::app::scene::Scene *, loka::app::scene::Scene *> probeChain =
      loka::dsl::Flow()
      | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.ProbeButton")).input(&scenePtr);
  assert(probeChain.run());

  loka::dsl::FlowChain<loka::app::scene::Scene *, loka::app::scene::Scene *> disableChain =
      loka::dsl::Flow()
      | loka::dsl::Step(1, loka::dsl::testing::ClickButtonByIdAndFlush("HelloWorld.LeftPanel.ToggleEnabledButton")).input(&scenePtr);
  assert(disableChain.run());

  loka::core::resource::Image disabledProbeButton;
  assert(captureBitmapByTestId(scene, "HelloWorld.LeftPanel.ProbeButton", disabledProbeButton));
  assert(disabledProbeButton.isValid());
  assert(disabledProbeButton.width() > 0);
  assert(disabledProbeButton.height() > 0);

  scene.unmount();
  [rootView release];
  [pool drain];
}
