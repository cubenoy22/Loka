#include "StartupRedrawTests.hpp"
#include <cassert>
#include <cstdio>

// ---------------------------------------------------------------------------
// Startup redraw count tests
//
// Portable model of the ToolboxWindow invalidation state machine.
// No Mac headers needed — purely tests the flag-based draw decisions that
// control how many full draws happen when a Classic window first appears.
//
// Before fix: 3+ full draws on startup
// After fix:  1 full draw on startup
// ---------------------------------------------------------------------------

namespace
{
  struct StartupRedrawSimulator
  {
    bool needsInvalidate_;
    bool skipNextUpdateDraw_;
    int fullDrawCount_;

    StartupRedrawSimulator()
        : needsInvalidate_(false),
          skipNextUpdateDraw_(false),
          fullDrawCount_(0)
    {
    }

    // Models requestInvalidateWithReason (called from onChange with fullRebuild)
    void requestInvalidate()
    {
      needsInvalidate_ = true;
    }

    // Models draw() — direct draw, does not touch invalidation flags
    void draw()
    {
      ++fullDrawCount_;
    }

    // Models flushInvalidate() — only draws if needsInvalidate_ is true
    void flushInvalidate()
    {
      if (!needsInvalidate_)
      {
        return;
      }
      needsInvalidate_ = false;
      skipNextUpdateDraw_ = true;
      draw();
    }

    // Models updateEvt handling
    void handleUpdateEvt()
    {
      if (skipNextUpdateDraw_)
      {
        skipNextUpdateDraw_ = false;
        return;
      }
      draw();
    }
  };

  struct ToolboxFollowupRedrawSimulator
  {
    bool skipNextUpdateDraw_;
    int drawCount_;

    ToolboxFollowupRedrawSimulator()
        : skipNextUpdateDraw_(false),
          drawCount_(0)
    {
    }

    void draw()
    {
      ++drawCount_;
    }

    void flushInvalidateWithLegacySkip()
    {
      skipNextUpdateDraw_ = true;
      draw();
    }

    void flushInvalidateWithoutSkip()
    {
      draw();
    }

    void handleUpdateEvt()
    {
      if (skipNextUpdateDraw_)
      {
        skipNextUpdateDraw_ = false;
        return;
      }
      draw();
    }
  };

  struct ToolboxChildDirtyInvalidationSimulator
  {
    enum InvalidateMode
    {
      INVALIDATE_NONE = 0,
      INVALIDATE_RECT = 1,
      INVALIDATE_FULL = 2
    };

    static InvalidateMode chooseInvalidateMode(bool hasRootNode, bool fullRebuild, bool hasChildDirty)
    {
      if (!hasRootNode || fullRebuild || hasChildDirty)
      {
        return INVALIDATE_FULL;
      }
      return INVALIDATE_RECT;
    }
  };
} // namespace

void testStartupRedrawCount_Before()
{
  printf("\n==== [testStartupRedrawCount_Before] start ====\n");

  // Simulate the CURRENT ToolboxApp::run() startup sequence:
  //   open()                    -> NewWindow(visible=true), queues updateEvt
  //   ensureSceneMounted()      -> onChange(INITIAL, fullRebuild=true) -> requestInvalidate
  //   draw()                    -> explicit draw (does NOT clear needsInvalidate_)
  //   --- event loop ---
  //   nullEvent -> flushInvalidate()  -> draws again because needsInvalidate_ still set
  //   updateEvt from NewWindow        -> skipNextUpdateDraw_ consumed by flushInvalidate
  //   recompose fires requestInvalidate -> another flushInvalidate -> draws again

  StartupRedrawSimulator sim;

  // Phase 1: startup, before event loop
  sim.requestInvalidate(); // ensureSceneMounted -> onChange(fullRebuild=true)
  sim.draw();              // explicit draw() call at ToolboxApp.cpp:54

  // Phase 2: first event-loop iteration (nullEvent)
  sim.flushInvalidate(); // needsInvalidate_ was still true -> draws again

  // Phase 3: updateEvt from NewWindow(visible=true)
  sim.handleUpdateEvt(); // skipNextUpdateDraw_ was set by flushInvalidate -> skipped

  // Phase 4: recompose cycle fires another requestInvalidate during startup
  sim.requestInvalidate(); // dynamic boundary state change during first compose cycle
  sim.flushInvalidate();   // draws again

  printf("  Before: fullDrawCount = %d (expected >= 3)\n", sim.fullDrawCount_);
  assert(sim.fullDrawCount_ >= 3);

  printf("==== [testStartupRedrawCount_Before] PASSED ====\n");
}

void testStartupRedrawCount_After()
{
  printf("\n==== [testStartupRedrawCount_After] start ====\n");

  // Simulate the FIXED startup sequence:
  //   open()
  //   ensureSceneMounted()      -> onChange(fullRebuild=true) -> requestInvalidate
  //   flushInvalidate()         -> clears needsInvalidate_, draws, sets skipNextUpdateDraw_
  //   --- event loop ---
  //   updateEvt from NewWindow  -> skipped by skipNextUpdateDraw_
  //   no excess requestInvalidate because dynamic root downgrade prevents fullRebuild

  StartupRedrawSimulator sim;

  // Phase 1: startup, before event loop
  sim.requestInvalidate(); // ensureSceneMounted -> onChange(fullRebuild=true)
  sim.flushInvalidate();   // FIXED: use flushInvalidate instead of raw draw()
                           // clears needsInvalidate_, draws, sets skipNextUpdateDraw_

  // Phase 2: updateEvt from NewWindow(visible=true)
  sim.handleUpdateEvt(); // skipNextUpdateDraw_ is set -> skipped

  // Phase 3: if recompose fires, dynamic root boundary downgrade means
  // fullRebuild=false, so only rect invalidation occurs (not modelled here
  // as full draw), and needsInvalidate_ stays false.

  printf("  After: fullDrawCount = %d (expected 1)\n", sim.fullDrawCount_);
  assert(sim.fullDrawCount_ == 1);

  printf("==== [testStartupRedrawCount_After] PASSED ====\n");
}

void testToolboxChildDirtyInvalidationPrefersFullRedraw()
{
  printf("\n==== [testToolboxChildDirtyInvalidationPrefersFullRedraw] start ====\n");

  const ToolboxChildDirtyInvalidationSimulator::InvalidateMode childDirtyMode =
      ToolboxChildDirtyInvalidationSimulator::chooseInvalidateMode(true, false, true);
  const ToolboxChildDirtyInvalidationSimulator::InvalidateMode propsOnlyMode =
      ToolboxChildDirtyInvalidationSimulator::chooseInvalidateMode(true, false, false);

  printf("  child dirty mode = %d (expected full)\n", static_cast<int>(childDirtyMode));
  printf("  props only mode = %d (expected rect)\n", static_cast<int>(propsOnlyMode));

  assert(childDirtyMode == ToolboxChildDirtyInvalidationSimulator::INVALIDATE_FULL);
  assert(propsOnlyMode == ToolboxChildDirtyInvalidationSimulator::INVALIDATE_RECT);

  printf("==== [testToolboxChildDirtyInvalidationPrefersFullRedraw] PASSED ====\n");
}

void testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw()
{
  printf("\n==== [testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw] start ====\n");

  ToolboxFollowupRedrawSimulator legacy;
  legacy.flushInvalidateWithLegacySkip();
  legacy.handleUpdateEvt();
  printf("  legacy drawCount = %d (expected 1)\n", legacy.drawCount_);
  assert(legacy.drawCount_ == 1);

  ToolboxFollowupRedrawSimulator fixed;
  fixed.flushInvalidateWithoutSkip();
  fixed.handleUpdateEvt();
  printf("  fixed drawCount = %d (expected 2)\n", fixed.drawCount_);
  assert(fixed.drawCount_ == 2);

  printf("==== [testToolboxManualInvalidateDoesNotSkipFollowupUpdateDraw] PASSED ====\n");
}

// ---------------------------------------------------------------------------
// Auto control-id policy (issue #120)
//
// Portable model, same philosophy as the redraw simulator above: the
// allocator is plain C++ shared with ToolboxScenePlatformController.
// The first sequence below is exactly the one that used to collide —
// the controller re-announced the explicit base every frame while
// contexts allocated lazily and cached their id, so the first control
// revealed by a Show flip received an id that was still live.
// ---------------------------------------------------------------------------

#include "../apple/toolbox/src/ToolboxControlIdAllocator.hpp"

void testToolboxAutoControlIdsNeverReissueLiveIds()
{
  ToolboxControlIdAllocator ids(128);

  ids.raiseBaseAbove(0); // frame 1, no explicit tags
  const short outerAdd = ids.allocate();
  const short toggle = ids.allocate();

  ids.raiseBaseAbove(0); // frame 2: cached contexts skip allocation
  const short revealedAdd = ids.allocate(); // Show reveal
  assert(revealedAdd != outerAdd && revealedAdd != toggle &&
         "a control revealed after re-announcing the base must not receive a live id");

  // Recycling: a released id may be reissued, live ids stay reserved.
  ids.release(outerAdd);
  const short reused = ids.allocate();
  assert(reused == outerAdd);
  const short another = ids.allocate();
  assert(another != toggle && another != reused && "live ids stay reserved");

  // A late explicit tag lifts the base; freed ids below it are discarded.
  ids.release(reused);
  ids.raiseBaseAbove(1000);
  const short afterRaise = ids.allocate();
  assert(afterRaise >= 1001 && "freed ids below a raised base are not reissued");
  ids.release(afterRaise);
  assert(ids.allocate() == afterRaise);
}
