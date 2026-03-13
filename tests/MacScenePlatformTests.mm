#include "MacScenePlatformTests.hpp"

#include <cassert>

#include <AppKit/AppKit.h>

#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "MacScenePlatformController.hpp"
#include "testing/MacScenePlatformTestAccess.hpp"
#include "loka/core/State.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"

namespace
{
  loka::core::MutableState<loka::core::String> *g_textState = 0;
  loka::core::MutableState<bool> *g_enabledState = 0;

  class DynamicMacTestNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicMacTestNode> DynamicMacTestProps;

  class DynamicMacTestNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicMacTestNode>
  {
  public:
    DynamicMacTestNode(const DynamicMacTestProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicMacTestNode>(DynamicMacTestProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_textState)
      {
        c.declare(loka::app::Text(g_textState)
                      .attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD))
                      .testId("MainText"));
      }
      if (g_enabledState)
      {
        c.declare(loka::app::Button("Run").enabled(g_enabledState).testId("MainButton"));
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicMacTestProps, DynamicMacTestNode> DynamicMacTestBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicMacTestNode>();
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
