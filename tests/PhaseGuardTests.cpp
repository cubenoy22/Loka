#include "testing/app/SceneManagerTestAccess.hpp"
#include "support/TestVerify.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "app/scene/node/ComponentNode.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "PhaseGuardTests.hpp"
#include "support/WindowAdmissionTestApp.hpp"
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
// with no stale-target dispatch. Scene replacement during apply is pinned below.

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

namespace
{
  /** Stack-owned observation outlives the scene and every callback it records. */
  struct ApplyStructureObservation
  {
    explicit ApplyStructureObservation(bool replaceScene)
        : tracker(), pulse(0), window(0), scene(0), replacement(0),
          replace(replaceScene), callbackCalls(0), sceneDestructions(0),
          phaseAtCallback(loka::app::scene::SceneDirector::UPDATE_CYCLE_IDLE),
          rootSurvived(false), phaseSurvived(false), applyCompletedOnRoot(false)
    {
      this->tracker.addState(&this->pulse);
    }

    loka::core::PushStateTracker tracker;
    loka::core::MutableState<int> pulse;
    NullWindow *window;
    loka::app::scene::Scene *scene;
    loka::app::scene::Scene *replacement;
    const bool replace;
    int callbackCalls;
    int sceneDestructions;
    loka::app::scene::SceneDirector::UpdateCyclePhase phaseAtCallback;
    bool rootSurvived;
    bool phaseSurvived;
    bool applyCompletedOnRoot;
  };

  struct ApplyStructureTypeTag {};
  class ApplyStructureNode;
  struct ApplyStructureProps : public loka::app::scene::NodePropsBase<ApplyStructureProps>
  {
    typedef ApplyStructureTypeTag TypeTag;
    typedef ApplyStructureNode NodeType;
    explicit ApplyStructureProps(ApplyStructureObservation *value = 0) : observation(value) {}
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
        return this->propsTypeId() < rhs.propsTypeId();
      return this->observation < static_cast<const ApplyStructureProps &>(rhs).observation;
    }
    ApplyStructureObservation *observation;
  };

  class ApplyStructureNode : public loka::app::scene::ComponentNodeWithProps<ApplyStructureProps>
  {
    typedef loka::app::scene::ComponentNodeWithProps<ApplyStructureProps> Base;
  public:
    typedef ApplyStructureTypeTag TypeTag;
    explicit ApplyStructureNode(const ApplyStructureProps &p) : Base(p) {}
    virtual void declareBindings(loka::app::scene::BindingToken &token)
    {
      token.watch(this->props.observation->pulse, this, &ApplyStructureNode::requestStructure);
    }
    virtual void composeChildren(loka::app::scene::NodeComposition &composition)
    {
      // The clone returns NodeDefinitionBase; the fixture needs no wrapper
      // return type from declare (the typed wrapper downcast is separate).
      composition.declare(static_cast<const loka::app::scene::NodeDefinitionBase &>(loka::app::Button("A")));
    }
    void requestStructure()
    {
      // Copy the stack-owned observation before requesting retirement. Access
      // no node members after the request, so the pin adds no lifetime hazard.
      ApplyStructureObservation *observation = this->props.observation;
      ++observation->callbackCalls;
      observation->phaseAtCallback =
          loka::dsl::testing::SceneTestAccess::director(*observation->scene).phase();
      if (observation->replace)
        observation->window->sceneManager()->commitTransaction(observation->scene, observation->replacement);
      else
        observation->scene->updateAttached(false);
    }
  };

  typedef loka::app::scene::NodeDefinition<ApplyStructureProps, ApplyStructureNode> ApplyStructureDefinition;

  class ApplyStructureScene : public loka::app::scene::Scene
  {
  public:
    explicit ApplyStructureScene(ApplyStructureObservation &observation)
        : loka::app::scene::Scene(new ApplyStructureDefinition(ApplyStructureProps(&observation))),
          observation_(observation) {}
    virtual ~ApplyStructureScene() { ++this->observation_.sceneDestructions; }
  private:
    ApplyStructureObservation &observation_;
  };

  /** Publishes a native-side fact at the first hook after beginApplyCycle. */
  class ApplyStructureController : public NullScenePlatformController
  {
  public:
    explicit ApplyStructureController(ApplyStructureObservation &observation)
        : observation_(observation) {}
    virtual void beginApplyCycle()
    {
      if (this->observation_.callbackCalls)
        return;
      typedef loka::dsl::testing::SceneTestAccess Access;
      loka::app::scene::Node *root = Access::rootNode(*this->observation_.scene);
      {
        loka::core::StateTrackerGuard guard(&this->observation_.tracker);
        this->observation_.pulse.set(this->observation_.pulse.get() + 1);
      }
      this->observation_.rootSurvived = root && root == Access::rootNode(*this->observation_.scene);
      this->observation_.phaseSurvived = Access::director(*this->observation_.scene).phase() ==
          loka::app::scene::SceneDirector::UPDATE_CYCLE_APPLY;
    }
    // This probe observes the global projection completion hook. Null normally
    // skips that hook for a paint-only plan, independently of Scene lifetime.
    virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return false; }
    virtual void onChange(loka::app::scene::Node *root,
                          loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      if (this->observation_.window && root)
      {
        LOKA_VERIFY(root == loka::dsl::testing::SceneTestAccess::rootNode(*this->observation_.window->scene()));
        LOKA_VERIFY(this->observation_.window->scene()->getAttachedState()->get());
      }
      if (this->observation_.callbackCalls && !this->observation_.sceneDestructions &&
          this->observation_.rootSurvived && root &&
          root == loka::dsl::testing::SceneTestAccess::rootNode(*this->observation_.scene))
        this->observation_.applyCompletedOnRoot = true;
      NullScenePlatformController::onChange(root, flags, fullRebuild);
    }
  private:
    ApplyStructureObservation &observation_;
  };

  void VerifyStructureRequestedDuringApply(bool replace)
  {
    ApplyStructureObservation observation(replace);
    ApplyStructureController controller(observation);
    NullPlatformContext context;
    WindowProps props;
    observation.scene = new ApplyStructureScene(observation);
    props.scene(observation.scene);
    NullWindow window(&context, props, &controller);
    observation.window = &window;
    WindowAdmissionTestApp admission(window);
    if (replace)
      observation.replacement = new loka::app::scene::Scene(loka::app::Button("B").clone());
    observation.scene->requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
    admission.flush();
    printf("P%d: callbacks=%d phase=%d rootSurvived=%d phaseSurvived=%d applyCompletedOnRoot=%d sceneDestructions=%d retired=%lu currentIsB=%d attached=%d\n",
           replace ? 2 : 3, observation.callbackCalls, static_cast<int>(observation.phaseAtCallback),
           observation.rootSurvived, observation.phaseSurvived, observation.applyCompletedOnRoot,
           observation.sceneDestructions, static_cast<unsigned long>(loka::app::testing::SceneManagerTestAccess::retiredSceneCount(*window.sceneManager())),
           window.scene() == observation.replacement, window.scene()->getAttachedState()->get());
    fflush(stdout);
    LOKA_VERIFY(observation.callbackCalls == 1);
    LOKA_VERIFY(observation.phaseAtCallback == loka::app::scene::SceneDirector::UPDATE_CYCLE_APPLY);
    LOKA_VERIFY(observation.rootSurvived);
    LOKA_VERIFY(observation.phaseSurvived);
    LOKA_VERIFY(observation.applyCompletedOnRoot);
    LOKA_VERIFY(observation.sceneDestructions == 0);
    if (replace)
    {
      LOKA_VERIFY(window.scene() == observation.scene);
      LOKA_VERIFY(window.scene()->getAttachedState()->get());
      LOKA_VERIFY(window.sceneManager()->hasPendingReplacement());
      admission.flush();
      LOKA_VERIFY(window.scene() == observation.replacement);
      LOKA_VERIFY(observation.sceneDestructions == 0);
      LOKA_VERIFY(loka::app::testing::SceneManagerTestAccess::retiredSceneCount(*window.sceneManager()) == 1);
      admission.flush();
      LOKA_VERIFY(observation.sceneDestructions == 1);
      LOKA_VERIFY(loka::app::testing::SceneManagerTestAccess::retiredSceneCount(*window.sceneManager()) == 0);
    }
    else
    {
      admission.flush();
      LOKA_VERIFY(!window.scene()->getAttachedState()->get());
      LOKA_VERIFY(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()) == 0);
    }
  }
}

void testSceneReplacementRequestedDuringApplyIsAppliedAtNextAdmission()
{
  VerifyStructureRequestedDuringApply(true);
}

namespace
{
  /** The callback borrows fixture-owned windows; the target owns the candidate. */
  class CallbackAdmissionController : public NullScenePlatformController
  {
  public:
    CallbackAdmissionController() : running(0), target(0), app(0), candidate(0), calls(0) {}
    virtual void beginApplyCycle()
    {
      if (this->calls++)
        return;
      typedef loka::dsl::testing::SceneTestAccess Access;
      loka::app::scene::Scene *scene = this->running->scene();
      loka::app::scene::Node *root = Access::rootNode(*scene);
      loka::app::scene::Scene *oldTarget = this->target->scene();
      // Keep an eligible retiree so the nested admission also tests reclaim exclusion.
      LOKA_VERIFY(this->target->sceneManager()->commitTransaction(
          0, new loka::app::scene::Scene(loka::app::Button("Superseded").clone())));
      LOKA_VERIFY(this->target->sceneManager()->commitTransaction(0, this->candidate));
      typedef loka::app::testing::SceneManagerTestAccess SeatAccess;
      const size_t retirees = SeatAccess::retiredSceneCount(*this->target->sceneManager());
      LOKA_VERIFY(retirees == 1);
      this->target->flushSceneInvalidation();
      // Simulate a nested native pump, including inside a direct Scene run.
      if (this->app)
        this->app->flushWindowInvalidations();
      const bool rootSurvived = root && root == Access::rootNode(*scene);
      const bool targetDeferred = this->target->scene() == oldTarget;
      printf("App admission pin: crossWindow=%d rootSurvived=%d targetDeferred=%d\n",
             this->target != this->running, rootSurvived, targetDeferred);
      fflush(stdout);
      // Stop before returning to a run with a dangling reference on the red implementation.
      LOKA_VERIFY(rootSurvived);
      LOKA_VERIFY(targetDeferred);
      LOKA_VERIFY(SeatAccess::retiredSceneCount(*this->target->sceneManager()) == retirees);
    }
    Window *running;
    Window *target;
    WindowAdmissionTestApp *app;
    loka::app::scene::Scene *candidate;
    int calls;
  };

  void VerifyCallbackAdmission(bool crossWindow)
  {
    NullPlatformContext context;
    CallbackAdmissionController controller;
    WindowProps props;
    props.scene(new loka::app::scene::Scene(loka::app::Button("X").clone()));
    NullWindow running(&context, props, &controller);
    WindowProps otherProps;
    otherProps.scene(new loka::app::scene::Scene(loka::app::Button("Y").clone()));
    NullWindow other(&context, otherProps);
    WindowAdmissionTestApp app(running, &other);
    controller.app = &app;
    controller.running = &running;
    controller.target = crossWindow ? &other : &running;
    controller.candidate = new loka::app::scene::Scene(loka::app::Button("Replacement").clone());
    loka::app::scene::Scene *oldTarget = controller.target->scene();
    if (crossWindow)
    {
      // Admit Y as well: a per-window apply/run loop would install Y after X's callback.
      other.scene()->requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
      running.scene()->requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
      app.flush();
    }
    else
    {
      // No App admission is active; a direct Scene run must keep its root alive.
      running.scene()->invalidate();
    }
    LOKA_VERIFY(controller.calls == 1);
    LOKA_VERIFY(controller.target->scene() == oldTarget);
    LOKA_VERIFY(controller.target->sceneManager()->hasPendingReplacement());
    app.flush();
    LOKA_VERIFY(controller.target->scene() == controller.candidate);
    app.flush();
    LOKA_VERIFY(!controller.target->sceneManager()->hasRetiredScenes());
  }
}

void testDirectSceneRunDefersReplacementUntilAppAdmission()
{
  VerifyCallbackAdmission(false);
}

void testCrossWindowCallbackDefersReplacementUntilNextAppAdmission()
{
  VerifyCallbackAdmission(true);
}
