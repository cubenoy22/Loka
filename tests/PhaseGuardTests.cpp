#include "PhaseGuardTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/Text.hpp"

// Regression pins for the compose/apply phase reentrancy guard (#45 item 1,
// audit B1+B7): a state write that lands while a compose (DETACH teardown) or
// platform apply is in flight must be deferred to the next flush instead of
// synchronously re-entering refresh/apply.

// ============================================================
// Fixtures
// ============================================================

namespace
{

  using namespace loka::app::scene;

  static int g_phaseRootComposeCount = 0;
  static int g_boundaryDetachWrites = 0;
  static bool g_inDetachWrite = false;
  static int g_updateComposeDuringDetachWrite = 0;
  static int g_updateComposeDuringApply = 0;

  class PhaseGuardRootBoundaryNode;
  typedef BoundaryPropsFor<PhaseGuardRootBoundaryNode> PhaseGuardRootProps;

  static PhaseGuardRootBoundaryNode *g_liveBoundary = 0;

  // Boundary whose DETACH teardown writes an observed state — the audit B1
  // "state write inside a DETACH handler" scenario. The same state is toggled
  // from the platform apply window in the apply-phase scenario.
  class PhaseGuardRootBoundaryNode : public BoundaryNodeFor<PhaseGuardRootBoundaryNode>
  {
  public:
    PhaseGuardRootBoundaryNode(const PhaseGuardRootProps &p)
        : BoundaryNodeFor<PhaseGuardRootBoundaryNode>(PhaseGuardRootProps(p)),
          show_(),
          value_()
    {
      this->state(this->show_, true);
      this->state(this->value_, loka::core::String::Literal("A"));
      g_liveBoundary = this;
    }
    virtual ~PhaseGuardRootBoundaryNode()
    {
      if (g_liveBoundary == this)
      {
        g_liveBoundary = 0;
      }
    }

    virtual void composeNode(NodeComposition &c)
    {
      ++g_phaseRootComposeCount;
      using namespace loka::app;
      c.declare(Show(*this->show_.state()) << Text(this->value_.state()));
    }

    virtual void detachNode(NodeComposition &c)
    {
      // Observed-state write while the DETACH compose phase scope is active.
      // Writes the Text-observed value (NOT the Show condition: toggling a
      // live ConditionalNode condition after beginComposition has reset the
      // definition arena is a separate known UAF, tracked in #46).
      if (this->value_.isValid())
      {
        ++g_boundaryDetachWrites;
        g_inDetachWrite = true;
        this->value_.set(loka::core::String::Literal("DetachWrite"), true);
        g_inDetachWrite = false;
      }
      BoundaryNodeFor<PhaseGuardRootBoundaryNode>::detachNode(c);
    }

    // Reentry detector: an UPDATE compose that runs while the DETACH-handler
    // write or the platform apply is still in flight is a synchronous reentry.
    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE)
      {
        if (g_inDetachWrite)
        {
          ++g_updateComposeDuringDetachWrite;
        }
        if (this->isApplyingPlatform())
        {
          ++g_updateComposeDuringApply;
        }
      }
      BoundaryNodeFor<PhaseGuardRootBoundaryNode>::composeWithContext(context, event);
    }

    // Structure-changing write used from the apply window: toggling the Show
    // condition forces a recompose, which makes reentry observable.
    void poke()
    {
      if (this->show_.isValid())
      {
        this->show_.set(!this->show_.get(), true);
      }
    }

  private:
    NodeState<bool> show_;
    NodeState<loka::core::String> value_;
  };

  inline BoundaryDefinition<PhaseGuardRootProps, PhaseGuardRootBoundaryNode> PhaseGuardRoot()
  {
    return Boundary<PhaseGuardRootBoundaryNode>();
  }

  // Platform controller that can write state from inside the apply window
  // (onChange fires while SceneDirector is applying boundary updates).
  class PhaseGuardController : public IPlatformController
  {
  public:
    PhaseGuardController()
        : pokeOnNextChange_(false),
          onChangeCalls_(0),
          nestedCallsDuringPoke_(-1),
          destroyed_(false)
    {
    }

    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      (void)flags;
      (void)fullRebuild;
      ++onChangeCalls_;
      if (pokeOnNextChange_ && g_liveBoundary)
      {
        pokeOnNextChange_ = false;
        const int callsBeforePoke = onChangeCalls_;
        g_liveBoundary->poke();
        // If the mid-apply write synchronously re-entered refresh/apply, a
        // nested onChange fired inside the poke and this delta is nonzero.
        nestedCallsDuringPoke_ = onChangeCalls_ - callsBeforePoke;
      }
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const
    {
      return false;
    }
    virtual void destroy()
    {
      destroyed_ = true;
    }

    bool pokeOnNextChange_;
    int onChangeCalls_;
    int nestedCallsDuringPoke_;
    bool destroyed_;
  };

} // namespace

// ============================================================
// Tests
// ============================================================

void testComposeApplyPhaseGuard()
{
  printf("\n==== [testComposeApplyPhaseGuard] start ====\n");

  // --- DETACH path via updateAttached(false): completes, no reentrant compose ---
  {
    g_phaseRootComposeCount = 0;
    g_boundaryDetachWrites = 0;
    g_updateComposeDuringDetachWrite = 0;
    Scene scene(PhaseGuardRoot());
    PhaseGuardController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_phaseRootComposeCount == 1);
    assert(g_boundaryDetachWrites == 0);

    scene.updateAttached(false);
    assert(g_boundaryDetachWrites == 1);
    // The mid-DETACH observed-state write must not re-enter an UPDATE compose.
    assert(g_updateComposeDuringDetachWrite == 0);
    assert(g_phaseRootComposeCount == 1);

    // Re-attach starts from a clean update state (no stale pending entries).
    scene.updateAttached(true);
    assert(g_phaseRootComposeCount == 2);

    scene.updateAttached(false);
    assert(g_boundaryDetachWrites == 2);
    assert(g_phaseRootComposeCount == 2);
    scene.unmount();
  }

  // --- DETACH path via unmount() while attached: no reentrant compose ---
  {
    g_phaseRootComposeCount = 0;
    g_boundaryDetachWrites = 0;
    g_updateComposeDuringDetachWrite = 0;
    Scene scene(PhaseGuardRoot());
    PhaseGuardController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_phaseRootComposeCount == 1);

    scene.unmount();
    assert(g_boundaryDetachWrites == 1);
    assert(g_updateComposeDuringDetachWrite == 0);
    assert(g_phaseRootComposeCount == 1);
  }

  // --- apply window: a state write during platform apply defers ---
  {
    g_phaseRootComposeCount = 0;
    g_boundaryDetachWrites = 0;
    g_updateComposeDuringApply = 0;
    Scene scene(PhaseGuardRoot());
    PhaseGuardController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(g_phaseRootComposeCount == 1);
    assert(g_liveBoundary != 0);

    // Arm the controller, then trigger a normal update. The resulting flush
    // recomposes once and, during its apply, the controller writes state.
    platform.pokeOnNextChange_ = true;
    g_liveBoundary->poke();
    scene.flushInvalidation();

    // The mid-apply write must not have re-entered refresh/apply
    // synchronously: no UPDATE compose ran inside the apply window and no
    // nested onChange fired inside the poke.
    assert(!platform.pokeOnNextChange_); // the poke actually ran
    assert(g_updateComposeDuringApply == 0);
    assert(platform.nestedCallsDuringPoke_ == 0);
    // Current behavior pin: the deferred entry does NOT survive the in-flight
    // cycle — the end-of-apply transaction clear drops the boundary-update
    // entry that was enqueued during apply, so the next flush applies nothing.
    // This is the commit-phase write-loss half of #45 item 3 ("queue into the
    // next transaction or assert; never silently drop"); that fix must flip
    // this expectation to onChangeAfterFirstFlush + 1, deliberately.
    int onChangeAfterFirstFlush = platform.onChangeCalls_;
    scene.flushInvalidation();
    assert(platform.onChangeCalls_ == onChangeAfterFirstFlush);

    scene.updateAttached(false);
    scene.unmount();
  }

  printf("==== [testComposeApplyPhaseGuard] end ====\n");
}
