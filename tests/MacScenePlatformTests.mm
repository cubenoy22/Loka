#include "MacScenePlatformTests.hpp"

#include <cassert>

#include <AppKit/AppKit.h>

#include "app/Box.hpp"
#include "app/Text.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/Scene.hpp"
#include "MacScenePlatformController.hpp"
#include "testing/MacScenePlatformTestAccess.hpp"
#include "loka/core/State.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"

void testMacScenePlatformRelayoutRequest()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));

  loka::app::scene::NodeComposition composition;
  loka::app::BoxDefinition &root = composition.declare(loka::app::Box().testId("RootBox"));
  root << loka::app::Text(&textState).attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD)).testId("MainText");

  NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 240, 120)];
  MacScenePlatformController controller((void *)rootView);
  loka::app::scene::Scene scene(composition.root()->clone());
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
  MacScenePlatformController::flushPendingRelayouts();
  assert(!loka::dsl::testing::MacScenePlatformTestAccess::hasPendingRelayout(controller));

  scene.unmount();
  [rootView release];
  [pool drain];
}
