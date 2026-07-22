#include "PhaseGuardTests.hpp"
#include <cassert>
#include <cstdio>
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/RecordingPlatformController.hpp"
#ifdef LOKA_LIFECYCLE_AUDIT
#include "core/LokaAlloc.hpp"
#endif

// #45 item 1 / W1-1 (salvaged from PR #51 on the abandoned
// integration/scene-update-hardening branch, ported to the post-#63
// current/next update cycle): updateAttached(false) tears down and DESTROYS the
// root boundary (teardownComposition -> DestroyHeapNode(rootNode_)). A boundary
// update queued before detach lives in the director's updateTransaction_ as a
// RAW BoundaryNode* target. Without clearing it on detach, a re-attach + flush
// swaps that stale transaction active and dispatches through the freed boundary
// -- a use-after-free. unmount() already calls clearMountedUpdateState(); detach
// must match.
//
// Deterministic pin: the queued update must be gone after detach
// (hasRequestedInput() == false) -- RED before the fix (the stale entry, whose
// target boundary was just freed, is retained), GREEN after. The re-attach +
// flush tail then confirms the recovered path re-attaches and flushes cleanly
// with no stale-target dispatch. (This idle detach/re-attach case is the W1-1
// residual; a re-entrant detach/unmount DURING an in-flight compose/apply cycle
// is a separate, deeper UAF -- shared with unmount() -- tracked elsewhere.)
void testDetachClearsQueuedBoundaryUpdate()
{
  printf("\n==== [testDetachClearsQueuedBoundaryUpdate] start ====\n");
#ifdef LOKA_LIFECYCLE_AUDIT
  const int totalLiveBefore = loka::core::LokaAllocAuditTotalLiveCount();
#endif
  {
    loka::app::scene::Scene scene(new loka::app::BoxDefinition());
    SceneTestSupport::RecordingPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(!loka::dsl::testing::SceneTestAccess::hasRequestedInput(scene));

    // Queue a boundary update targeting the current root boundary.
    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
    assert(loka::dsl::testing::SceneTestAccess::hasRequestedInput(scene));

    // Detach destroys that root boundary; the queued update must be dropped so
    // it cannot outlive its target.
    scene.updateAttached(false);
    assert(!loka::dsl::testing::SceneTestAccess::hasRequestedInput(scene));

    // Full path: re-attach builds a fresh root, then flush. Before the fix this
    // swaps the stale transaction active and dispatches through the freed
    // boundary (heap-use-after-free / ASan).
    scene.updateAttached(true);
    scene.flushInvalidation();

    scene.unmount();
  }
#ifdef LOKA_LIFECYCLE_AUDIT
  assert(loka::core::LokaAllocAuditTotalLiveCount() == totalLiveBefore);
  loka::core::LokaAllocAuditCheckpoint("testDetachClearsQueuedBoundaryUpdate");
#endif
  printf("==== [testDetachClearsQueuedBoundaryUpdate] end ====\n");
}
