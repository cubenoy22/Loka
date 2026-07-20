// Allocation ratchet scenario: one steady-state UI interaction on the real
// HelloWorld MainNode, driven headlessly, must not exceed a fixed heap
// allocation budget (design ruling 2026-07-20).
//
// The interaction mirrors HelloWorld's probe-button click end to end:
//   handler body (String concat + String::FromInt + state sets)
//   -> tracker commit (propagation/notify/invalidate)
//   -> scene.flushInvalidation() (compose + platform apply).
// The platform controller below flattens every Text node's value at apply
// time through the same CollectUtf8 path a real platform context uses
// (apple/macos/src/context/MacTextContext.mm:34, Win32ButtonContext.cpp:260),
// so the lazy-rope flatten cost is part of the census.
//
// The budget is a CEILING, not a target: see the comment above
// kSteadyStateAllocBudget below and the PR that introduced this ratchet
// ("Pin steady-state per-interaction allocations with a ratchet budget")
// for the full census table and family breakdown.

#include "support/AllocCensus.hpp"

#include "MainNode.hpp"

#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/StringUTF8.hpp"
#include "testing/scene/SceneTestFlow.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace
{
  using loka::app::scene::BoundaryNode;
  using loka::app::scene::INestable;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  // Platform controller that consumes applied scenes the way a real
  // platform does for the census-relevant part: every Text node's value is
  // flattened into a fresh std::string via loka::platform::CollectUtf8 on
  // each apply, which is exactly the shape of MacTextContext/Win32 apply.
  struct AllocPinPlatformController : public loka::app::scene::IPlatformController
  {
    AllocPinPlatformController()
        : changeCalls_(0),
          boundaryApplyCalls_(0),
          textsFlattened_(0),
          textBytes_(0)
    {
    }

    void flattenTextsIn(Node *node)
    {
      if (!node)
      {
        return;
      }
      loka::app::TextNode *text = node->asTextNode();
      if (text && text->props.text_)
      {
        std::string utf8;
        if (loka::platform::CollectUtf8(text->props.text_->get(), utf8))
        {
          ++textsFlattened_;
          textBytes_ += static_cast<unsigned long>(utf8.size());
        }
      }
      INestable *nestable = node->asNestable();
      if (nestable)
      {
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          flattenTextsIn(child);
        }
      }
    }

    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags, bool)
    {
      ++changeCalls_;
      flattenTextsIn(rootNode);
    }

    virtual void onBoundaryApply(Node *,
                                 BoundaryNode *boundary,
                                 const loka::app::scene::BoundaryLocalApplyInfo &,
                                 const loka::app::scene::PlatformApplyPlan &)
    {
      ++boundaryApplyCalls_;
      flattenTextsIn(boundary);
    }

    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy() {}

    int changeCalls_;
    int boundaryApplyCalls_;
    unsigned long textsFlattened_;
    unsigned long textBytes_;
  };

  // One full UI interaction, phased for the census. Mirrors
  // ClickButtonByIdAndFlush (SceneTestFlow.hpp) without the Flow DSL so the
  // capture window contains only framework work.
  int runInteraction(Scene &scene, loka::app::ButtonNode *button, BoundaryNode *rootBoundary)
  {
    {
      loka::core::StateTrackerGuard guard(rootBoundary->tracker());
      allocpin::SetPhase(allocpin::PHASE_HANDLER);
      button->props.onClick_->emit();
      allocpin::SetPhase(allocpin::PHASE_COMMIT);
    } // guard dtor: tracker end() -> recompute/commit/deferred/invalidate
    allocpin::SetPhase(allocpin::PHASE_FLUSH);
    int flushes = 0;
    while (scene.hasPendingInvalidation() && flushes < 8)
    {
      scene.flushInvalidation();
      ++flushes;
    }
    // The interaction must drain fully inside the capture window: a bound hit
    // with work still pending would measure a partial interaction and let the
    // ratchet pass on an undercount. Steady-state HelloWorld settles in one or
    // two flushes; the bound is only a runaway guard, never the exit path.
    assert(!scene.hasPendingInvalidation() &&
           "interaction did not drain within the flush bound; allocation count is partial");
    allocpin::SetPhase(allocpin::PHASE_OUTSIDE);
    return flushes;
  }

  loka::app::ButtonNode *findButton(Scene &scene, const char *testId)
  {
    Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
    assert(root != 0);
    long idMatches = 0;
    long typedMatches = 0;
    loka::app::ButtonNode *button = 0;
    loka::dsl::testing::scene_test_detail::findNodeByIdRecursive<loka::app::ButtonNode>(
        root, std::string(testId), idMatches, typedMatches, button);
    assert(idMatches == 1 && typedMatches == 1 && button != 0);
    return button;
  }
} // namespace

namespace allocpin
{
  void RunZeroAllocPin()
  {
    using loka::app::scene::NodeDefinition;

    NodeDefinition<helloworld::MainProps, helloworld::MainNode> mainDef;
    Scene scene(mainDef.clone());
    AllocPinPlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    // Settle the mount before anything is measured.
    for (int i = 0; scene.hasPendingInvalidation() && i < 8; ++i)
    {
      scene.flushInvalidation();
    }

    BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    assert(rootBoundary != 0 && rootBoundary->tracker() != 0);

    // The probe button increments actionProbeCount_ and rebuilds
    // actionSummary_ via Literal + concat + FromInt (MainNode.cpp:239-242):
    // the canonical steady-state interaction.
    loka::app::ButtonNode *probeButton = findButton(scene, "HelloWorld.LeftPanel.ProbeButton");
    assert(probeButton->props.onClick_ != 0);

    // WARMUP (uncounted): run the full interaction path twice so one-time
    // lazy initialization is out of the way.
    runInteraction(scene, probeButton, rootBoundary);
    runInteraction(scene, probeButton, rootBoundary);

    const unsigned long textsBeforeCaptures = platform.textsFlattened_;

    // Capture 0: first measured interaction (may still contain stragglers).
    // Capture 1: second measured interaction = steady state; this is the
    // pinned quantity. Comparing 0 vs 1 separates warmup residue.
    // Single call site: the backtrace hash includes return addresses, so
    // both captured interactions must run through identical frames for the
    // cross-capture stack matching to work.
    int flushes[2];
    for (int captureIndex = 0; captureIndex < 2; ++captureIndex)
    {
      BeginCapture(captureIndex);
      flushes[captureIndex] = runInteraction(scene, probeButton, rootBoundary);
      EndCapture();
    }
    const int flushes0 = flushes[0];
    const int flushes1 = flushes[1];

    std::printf("scenario: flushes per interaction = %d / %d, platform onChange=%d onBoundaryApply=%d\n",
                flushes0, flushes1, platform.changeCalls_, platform.boundaryApplyCalls_);
    std::printf("scenario: texts flattened during captures = %lu (bytes=%lu)\n",
                platform.textsFlattened_ - textsBeforeCaptures, platform.textBytes_);
    // The apply path must actually have flattened text during the captured
    // interactions, otherwise the census misses the flatten suspects.
    assert(platform.textsFlattened_ > textsBeforeCaptures);

    scene.unmount();

    // THE RATCHET: a CEILING, not a target. This is not "the framework
    // should allocate this much" -- it is "the framework must not allocate
    // MORE than this for one steady-state UI interaction without someone
    // noticing." The budget below is the census recorded when this ratchet
    // was introduced (PR "Pin steady-state per-interaction allocations with
    // a ratchet budget", built from the census probe on
    // probe/zero-alloc-pin @ fffe20c): 47 allocations / 1483 bytes for one
    // probe-button click, steady state (second captured interaction).
    //
    // By family (full per-stack table and fix recipes in the PR body and
    // the design-table record it references):
    //   String value world (literal String nodes + their control blocks,
    //     ConcatString nodes, String::FromInt, the equality-gate flatten in
    //     String::compare before NodeState::set() commits): 28 allocs
    //   tracker/notify (StateBase::notifyHandlers vector snapshot,
    //     PushStateTracker's visiting_ Rb-tree node insert, the
    //     dirtyStates vector copy in PushStateTracker::end()): 6 allocs
    //   SceneDirector bookkeeping (SceneProjectionTransaction::enqueue,
    //     PendingUpdateRootAnalysis::recordSeenRoot vectors): 10 allocs
    // Several of the above fire twice per click: the default
    // flushViewDirtyImmediately() policy runs the whole pipeline once for
    // actionProbeCount_ (int) and once for actionSummary_ (String) inside
    // the same handler call, i.e. two cycles per click. That structural
    // multiplier is noted separately in the PR body and is routed to the
    // WR-4 scene-update redesign as formal input, not fixed here.
    //
    // Any PR that raises steady-state per-interaction allocations trips
    // this ratchet and must, in its OWN PR body, either justify raising the
    // budget below or avoid the extra allocation. Lowering the budget when
    // allocations are removed is encouraged and expected over time.
    enum
    {
      kSteadyStateAllocBudget = 47,
      kSteadyStateByteBudget = 1483
    };

    // Only the second captured interaction is asserted. The probe found
    // capture 0 and capture 1 identical run to run on this machine, but
    // capture 1 is the one documented as steady state (capture 0 can still
    // carry warmup stragglers in principle), so it is the quantity that
    // stays pinned if warmup behavior ever changes.
    const unsigned long steadyAllocs = CaptureAllocCount(1);
    const unsigned long steadyBytes = CaptureAllocBytes(1);
    const bool allocsOverBudget = steadyAllocs > static_cast<unsigned long>(kSteadyStateAllocBudget);
    const bool bytesOverBudget = steadyBytes > static_cast<unsigned long>(kSteadyStateByteBudget);

    // Print the per-stack census on failure only, so a tripped ratchet
    // immediately names the culprit without spamming a passing run.
    if (allocsOverBudget || bytesOverBudget)
    {
      std::printf("allocation ratchet TRIPPED: steady-state interaction used allocs=%lu (budget=%d)"
                  " bytes=%lu (budget=%d); per-stack census follows\n",
                  steadyAllocs, static_cast<int>(kSteadyStateAllocBudget), steadyBytes,
                  static_cast<int>(kSteadyStateByteBudget));
      PrintCensusReport();
    }

    assert(!allocsOverBudget && "allocation ratchet: steady-state UI interaction exceeded the allocation budget");
    assert(!bytesOverBudget && "allocation ratchet: steady-state UI interaction exceeded the byte budget");

    std::printf("allocation ratchet: PASS (allocs=%lu/%d bytes=%lu/%d)\n", steadyAllocs,
                static_cast<int>(kSteadyStateAllocBudget), steadyBytes, static_cast<int>(kSteadyStateByteBudget));
  }
} // namespace allocpin
