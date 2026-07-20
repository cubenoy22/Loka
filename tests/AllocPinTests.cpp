// Zero-allocation pin scenario: one steady-state UI interaction on the real
// HelloWorld MainNode, driven headlessly, must perform ZERO shared-heap
// allocations after warmup (design ruling 2026-07-20).
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
// EXPECTED RED TODAY: the final assert is the pin; the census printed just
// before it is the deliverable. Do not merge while red.

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

    PrintCensusReport();

    scene.unmount();

    // THE PIN (expected to fail today): after warmup, one steady-state UI
    // interaction (state set -> notify/propagation -> recompose -> apply)
    // must perform zero shared-heap allocations.
    assert(CaptureAllocCount(1) == 0
           && "zero-allocation pin: steady-state UI interaction performed shared-heap allocations");

    std::printf("zero-allocation pin: PASS\n");
  }
} // namespace allocpin
