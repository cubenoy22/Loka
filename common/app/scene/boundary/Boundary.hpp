#ifndef LOKA_CORE2_SCENE_BOUNDARY_BOUNDARY_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "core/diag/LifecycleAudit.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/node/ComposableNode.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/context/ComponentContext.hpp"
#include "app/scene/projection/PlatformApplyPlan.hpp"
#include "app/scene/boundary/BoundaryApplyInfo.hpp"
#include "app/scene/boundary/detail/BoundaryCompositionState.hpp"
#include "app/scene/boundary/detail/BoundaryObservedState.hpp"
#include "app/scene/boundary/detail/BoundaryRuntimeState.hpp"
#include "app/scene/boundary/detail/BoundaryParkedBranchLedger.hpp"
#include "app/scene/boundary/detail/BoundaryBranchSeatState.hpp"
#include "app/scene/boundary/BoundaryStateTypes.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateUtil.hpp"
#include "core/Profiler.hpp"
#include "platform/debug/DebugLog.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class OwnershipDump;
    }
  } // namespace dsl

  namespace app
  {
    namespace scene
    {
      class Scene;

      // BoundaryNode: owns a local tracker for its subtree.
      class BoundaryNode : public ComposableNode, public IStateOwner LOKA_AUDITED_AS(BoundaryNode)
      {
      public:
        typedef BoundaryUpdateResult::BoundsHint LayoutBounds;
        typedef BoundaryLocalApplyPaintKind LocalApplyPaintKind;
        typedef BoundaryLocalApplyInfo LocalApplyInfo;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_NONE = scene::LOCAL_APPLY_PAINT_NONE;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_GENERIC = scene::LOCAL_APPLY_PAINT_GENERIC;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_OPAQUE = scene::LOCAL_APPLY_PAINT_OPAQUE;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_COMPOSITED = scene::LOCAL_APPLY_PAINT_COMPOSITED;
        BoundaryNode()
            : ComposableNode(),
              tracker_(),
              runtimeState_(),
              updateState_(),
              compositionState_(),
              observedState_(),
              parkedBranches_(),
              branchSeats_(),
              holdLedger_(this),
              pendingHeldReleasesHead_(0),
              pendingHeldReleasesTail_(0),
              retiredSubtreesHead_(0),
              retiredSubtreesTail_(0),
              retiredGenerations_(),
              drainingRetiredSubtrees_(false)
        {
          this->tracker_.setInvalidateCallback(&BoundaryNode::InvalidateSceneThunk, this);
        }
        virtual ~BoundaryNode()
        {
          this->holdLedger_.auditEmptyBeforeReclaim();
#ifdef LOKA_LIFECYCLE_AUDIT
          assert(!this->pendingHeldReleasesHead_ &&
                 "a Boundary must drain its Held releasers before reclamation");
#endif
          clearObservedStateEntries();
          this->releaseOwnedNodeStorage();
          releaseNodeStateRegistrations();
          clearOwnedStates();
          stateArena_.clear();
        }

        virtual BoundaryNode *asBoundary()
        {
          return this;
        }
        virtual IStateOwner *asStateOwner()
        {
          return this;
        }
        virtual Node *retainedLifecycleBranch(unsigned index)
        {
          return this->parkedBranches_.branch(index);
        }
#ifdef TEST_BUILD
        unsigned parkedBranchCountForTesting() const
        {
          return this->parkedBranches_.countForTesting();
        }
        unsigned parkedBranchArmForTesting(unsigned index) const
        {
          const BoundaryParkedBranchLedger::Entry *entry =
              this->parkedBranches_.entry(index);
          assert(entry && "parked branch test index must name a ledger row");
          return entry ? entry->arm : 0;
        }
#endif

        virtual loka::core::StateTracker *tracker()
        {
          return &tracker_;
        }
        virtual bool flushViewDirtyImmediately(NodeDirtyFlags flags) const
        {
          (void)flags;
          return true;
        }
        virtual void applyPendingUpdate(const PlatformApplyPlan &plan)
        {
          const LocalApplyInfo info = this->localApplyInfo(plan);
          if (info.hasStructureWork)
          {
            this->applyPendingStructureInfo(info, plan);
          }
          if (info.hasLayoutWork)
          {
            this->applyPendingLayoutInfo(info, plan);
          }
          switch (info.paintKind)
          {
          case LOCAL_APPLY_PAINT_COMPOSITED:
            this->applyPendingCompositedPaintInfo(info, plan);
            break;
          case LOCAL_APPLY_PAINT_OPAQUE:
            this->applyPendingOpaquePaintInfo(info, plan);
            break;
          case LOCAL_APPLY_PAINT_GENERIC:
            this->applyPendingLocalPaintInfo(info, plan);
            break;
          case LOCAL_APPLY_PAINT_NONE:
          default:
            break;
          }
        }
        bool hasLocalApplyStructureWork(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasStructureWork;
        }
        bool hasLocalApplyLayoutWork(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasLayoutWork;
        }
        bool hasLocalApplyPaintWork(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasPaintWork();
        }
        LocalApplyInfo localApplyInfo(const PlatformApplyPlan &plan) const
        {
          LocalApplyInfo info;
          info.isLocalStructureRoot = plan.hasLocalStructureWork(this);
          info.isLocalLayoutRoot = plan.hasLocalLayoutWork(this);
          info.isLocalPaintRoot = plan.hasLocalPaintWork(this);
          info.hasStructureWork = info.isLocalStructureRoot;
          info.hasLayoutWork = info.isLocalLayoutRoot;
          bool opaqueByHint = false;
          this->updateState_.selectLocalOpaqueCoverageHint(
              info.isLocalPaintRoot, info.hasOpaqueCoverageHint, opaqueByHint);
          if (!info.isLocalPaintRoot)
          {
            info.paintKind = LOCAL_APPLY_PAINT_NONE;
          }
          else if (plan.requiresCompositedPaint())
          {
            info.paintKind = LOCAL_APPLY_PAINT_COMPOSITED;
          }
          else if (plan.isOpaqueLocalPaint() || opaqueByHint)
          {
            info.paintKind = LOCAL_APPLY_PAINT_OPAQUE;
          }
          else
          {
            info.paintKind = LOCAL_APPLY_PAINT_GENERIC;
          }
          info.paintIsOpaque =
              info.paintKind == LOCAL_APPLY_PAINT_OPAQUE || info.paintKind == LOCAL_APPLY_PAINT_COMPOSITED;
          this->updateState_.selectLocalApplyBoundsHint(info.hasPaintWork(),
                                                        info.hasLayoutWork,
                                                        info.bounds,
                                                        info.usesPaintBoundsHint,
                                                        info.hasPaintSpecificBoundsHint);
          if (info.usesPaintBoundsHint)
          {
            info.boundsKind = scene::LOCAL_APPLY_BOUNDS_PAINT;
          }
          else if (info.bounds)
          {
            info.boundsKind = scene::LOCAL_APPLY_BOUNDS_LAYOUT;
          }
          assert(info.hasAnyWork() == plan.hasAnyLocalWork(this));
          return info;
        }
        LocalApplyPaintKind localApplyPaintKind(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).paintKind;
        }
        bool requiresLocalCompositedPaint(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasCompositedPaintWork();
        }
        bool hasLocalOpaquePaintWork(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasOpaquePaintWork();
        }
        bool hasLocalOpaquePaintHint(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasOpaqueCoverageHint;
        }
        bool localApplyPaintIsOpaque(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).paintIsOpaque;
        }
        bool hasLocalApplyBoundsHint(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).hasBoundsHint();
        }
        const LayoutBounds *localApplyBoundsHint(const PlatformApplyPlan &plan) const
        {
          return this->localApplyInfo(plan).bounds;
        }
        void markViewDirty(NodeDirtyFlags flags);
        void setFrozen(bool frozen)
        {
          this->runtimeState_.setFrozen(frozen);
        }
        bool isFrozen() const
        {
          return this->runtimeState_.isFrozen();
        }
        bool isApplyingPlatform() const
        {
          return this->updateState_.isApplying();
        }
        bool isComposingPhase() const
        {
          return this->updateState_.isComposing();
        }
        BoundaryComposePhaseScope beginComposePhaseScope()
        {
          return this->updateState_.beginComposeScope();
        }
        BoundaryApplyPhaseScope beginApplyPhaseScope()
        {
          return this->updateState_.beginApplyScope();
        }
        Scene *scene() const
        {
          return this->runtimeState_.currentScene();
        }
        Scene *getScene() const
        {
          if (this->runtimeState_.hasScene())
          {
            return this->runtimeState_.currentScene();
          }
          return this->runtimeState_.currentParentBoundary() ? this->runtimeState_.currentParentBoundary()->getScene()
                                                             : 0;
        }
        void setScene(Scene *scene)
        {
          this->runtimeState_.setScene(scene);
        }
        BoundaryNode *parentBoundary() const
        {
          return this->runtimeState_.currentParentBoundary();
        }
        void setParentBoundary(BoundaryNode *parent)
        {
          this->runtimeState_.setParentBoundary(parent);
        }
        void setLayoutBounds(int x, int y, int width, int height)
        {
          const int normalizedWidth = width < 0 ? 0 : width;
          const int normalizedHeight = height < 0 ? 0 : height;
          const bool changed = this->runtimeState_.setLayoutBounds(x, y, normalizedWidth, normalizedHeight);
          updateState_.noteLayoutBoundsTransition(
              changed, this->runtimeState_.hasParentBoundary(), x, y, normalizedWidth, normalizedHeight);
        }
        void clearLayoutBounds()
        {
          const bool changed = this->runtimeState_.clearLayoutBounds();
          updateState_.noteLayoutBoundsCleared(changed, this->runtimeState_.hasParentBoundary());
        }
        const LayoutBounds &layoutBounds() const
        {
          return this->runtimeState_.currentLayoutBounds();
        }
        bool hasLayoutBounds() const
        {
          return this->runtimeState_.hasLayoutBounds();
        }
        void clearObservedDirtyFlags()
        {
          observedState_.clearDirtyFlags();
        }
        BoundaryComposeResult &composeResult()
        {
          return compositionState_.composeResult();
        }
        const BoundaryComposeResult &composeResult() const
        {
          return compositionState_.composeResult();
        }
        BoundaryUpdateResult &updateResult()
        {
          return updateState_.updateResult();
        }
        const BoundaryUpdateResult &updateResult() const
        {
          return updateState_.updateResult();
        }
        void beginComposeResult(ComposeEvent event, NodeDirtyFlags dirtyFlags)
        {
          compositionState_.beginCompose(event, dirtyFlags);
        }
        /** Completes the open compose window. With an allocation failure or
            boundary-plan refusal recorded, it converts the compose into a
            projection failure instead; the body lives in Boundary.cpp because
            the conversion records the deferred full rebuild on the Scene. */
        void completeComposeResult(bool preservedNativeContexts);
        /** Allocation white flag (#132 ruling 3): a state or node failed to
            materialize during this boundary's open compose window. The flag
            only records; completeComposeResult() converts it. */
        void noteComposeAllocationFailure()
        {
          compositionState_.noteAllocationFailure();
        }
        /** Deterministic capability refusal: a contextless compose window
            reached a branch seat it cannot resolve without a boundary plan.
            Distinct from the allocation white flag (which is transient);
            shares the projection-failure recovery because both must defer a
            full rebuild. */
        void noteComposeBoundaryPlanRequired()
        {
          compositionState_.noteBoundaryPlanRequired();
        }
        virtual void noteStateAllocationFailure()
        {
          this->noteComposeAllocationFailure();
        }
        void clearPhaseResults()
        {
          compositionState_.clearResult();
          updateState_.clearResult();
        }
        /** The structure self-report is a per-cycle fact. The walk path
            clears the whole phase result at cycle entry, but the direct-root
            UPDATE path deliberately preserves its results across cycles --
            there, only the structure bit may be reset, or a single rebuild
            would escalate every later paint-only update through the
            layout/ensure pass forever (#279 review). */
        void clearStructureWorkForCycle()
        {
          updateState_.clearStructureWork();
        }
        void noteLocalPaintWork()
        {
          assert(updateState_.canMutateLocalPaintMetadata());
          updateState_.noteLocalPaintWork();
        }
        void noteLocalStructureWork()
        {
          assert(updateState_.canMutateLocalPaintMetadata());
          updateState_.noteLocalStructureWork();
        }
        void noteCompositedPaint()
        {
          assert(updateState_.canMutateLocalPaintMetadata());
          updateState_.noteCompositedPaint();
        }
        void noteOpaquePaintCoverage(bool opaque)
        {
          assert(updateState_.canMutateLocalPaintMetadata());
          updateState_.noteOpaquePaintCoverage(opaque);
        }
        void beginPlatformApply()
        {
          updateState_.beginApply();
        }
        void endPlatformApply()
        {
          updateState_.endApply();
        }
        void beginObservedStatePass()
        {
          observedState_.beginPass();
        }
        void clearObservedStateEntries()
        {
          observedState_.clearEntries(&BoundaryNode::ObservedStateChangedThunk);
        }
        void addObservedDirtyFlags(NodeDirtyFlags flags)
        {
          observedState_.addDirtyFlags(flags);
        }
        void registerObservedState(loka::core::StateBase *state, NodeDirtyFlags flags)
        {
          observedState_.registerState(this, state, flags, &BoundaryNode::ObservedStateChangedThunk);
        }
        /** Removes both ancestor edges held for one state owned by an inner
            scope. Safe during Boundary teardown after the observed ledger has
            already been cleared. */
        void forgetInnerOwnedState(loka::core::StateBase *state)
        {
          this->tracker_.removeState(state);
          this->observedState_.forgetState(
              state, &BoundaryNode::ObservedStateChangedThunk);
        }
        /** Receives a committed transaction from an attached inner owner
            without exposing the Boundary's observed-state ledger. */
        void noteInnerTrackerCommit(
            const loka::core::PushStateTracker *innerTracker)
        {
          NodeDirtyFlags flags =
              this->observedState_.dirtyFlagsForCommittedStates(innerTracker);
          if (flags == NODE_DIRTY_NONE)
          {
            flags = this->observedDirtyFlags();
          }
          if (flags != NODE_DIRTY_NONE)
          {
            this->markViewDirty(flags);
          }
        }
        void registerBranchSeatDirtySources()
        {
          const std::vector<BoundaryBranchSeatPlanEntry> &plans = this->branchSeats_.plans();
          for (size_t i = 0; i < plans.size(); ++i)
          {
            if (!plans[i].dirtySource)
            {
              continue;
            }
            this->registerObservedState(
                plans[i].dirtySource,
                static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_LAYOUT));
          }
        }
        /** Registers a node's declared external dirty sources against the owning
            boundary, wiring each observed state so a later mutation marks that
            boundary's view dirty. Shared by the generic composeTree walk and the
            direct-root compose path (Scene::prepareRootBoundaryCompose, #127) so
            both re-register on every non-DETACH compose window, paired with
            beginObservedStatePass(). */
        static void declareBoundaryDirtySources(Node *node, BoundaryNode *owner)
        {
          if (!node || !owner)
          {
            return;
          }
          class LocalDirtySourceRegistrar : public DirtySourceRegistrar
          {
          public:
            explicit LocalDirtySourceRegistrar(BoundaryNode *boundary)
                : boundary_(boundary)
            {
            }

            virtual void markDirtyOnChange(loka::core::StateBase *state, NodeDirtyFlags flags)
            {
              if (!boundary_ || !state)
              {
                return;
              }
              boundary_->registerObservedState(state, flags);
            }

          private:
            BoundaryNode *boundary_;
          };
          LocalDirtySourceRegistrar registrar(owner);
          node->declareDirtySources(registrar);
        }
        NodeDirtyFlags observedDirtyFlags() const
        {
          return observedState_.currentDirtyFlags();
        }
        NodeDirtyFlags observedDirtyFlagsForCommittedStates() const
        {
          const loka::core::PushStateTracker *pushTracker = this->tracker_.asPushTracker();
          return observedState_.dirtyFlagsForCommittedStates(pushTracker);
        }

        NodeArena *nodeArena()
        {
          return &nodeArena_;
        }
        const BoundaryBranchSeatPlanEntry *branchSeatPlan(NodeDefinitionBase *definition) const
        {
          if (!definition)
          {
            return 0;
          }
          IBranchSeatDefinition *seat = definition->asBranchSeatDefinition();
          if (!seat)
          {
            return 0;
          }
          return this->branchSeats_.findPlan(
              BoundaryParkedBranchKey(definition->nodeTag(),
                                      definition->compositionSeatSlot(),
                                      seat->branchSeatTypeId()));
        }
        void registerMaterializedBranchSeat(const BoundaryBranchSeatPlanEntry &plan,
                                            Node *parent,
                                            Node *active)
        {
          this->branchSeats_.registerRuntime(plan, parent, active);
        }
        void appendNestedBranchSeatPlan(NodeComposition &composition)
        {
          composition.assignCompositionSeatSlots();
          this->branchSeats_.append(composition.root());
        }
        bool evaluateBranchSeatsForScheduledApply(ComponentContext &context)
        {
          return this->applyCurrentBranchSeatPlan(context, 0);
        }
        /** Reclaims the queue snapshot owned by this Boundary at the head of
            the next tracker run. Retirees added while draining wait for a
            later tracker run. */
        void drainRetiredSubtreesAtNextTrackerRun();
        /** Runs the queued releasers of blocks this Boundary is the last
            dropping owner for. Reclamation paths call it before destroying
            the Boundary so no releaser ever runs from a destructor. */
        void drainPendingHeldReleases();
        virtual loka::core::HoldLedger *holdLedger()
        {
          return &this->holdLedger_;
        }
        virtual void reserveHeldArena(size_t totalSize)
        {
          stateArena_.reserve(totalSize);
        }
        virtual void *allocateHeldMemory(size_t size, size_t align)
        {
          return stateArena_.allocate(size, align);
        }
        virtual void registerHeldMemory(
            loka::core::detail::HeldBlockBase *block)
        {
          stateArena_.registerHeld(block);
        }
        virtual void retireHeldBlock(
            loka::core::detail::HeldBlockBase *block);
        virtual void *allocateStateMemory(size_t size, size_t align)
        {
          return stateArena_.allocate(size, align);
        }
        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          stateArena_.registerState(state, destroy);
        }
        /** Releases only the enclosing arena's storage row. The logical
            ownership row belongs to a Boundary-inner owner and must already
            be detached there. */
        void releaseInnerArenaStateMemory(loka::core::StateBase *state)
        {
          assert(state && state->isArenaAllocated() &&
                 "inner arena release requires an arena state");
          stateArena_.releaseState(state);
        }
        virtual void reserveStateArena(size_t totalSize)
        {
          stateArena_.reserve(totalSize);
        }

        static void
        composeSubtree(Node *node, ComponentContext &parentContext, ComposeEvent event, BoundaryNode *currentBoundary)
        {
          composeTree(node, parentContext, event, currentBoundary);
        }

        static void InvalidateSceneThunk(void *userData);
        static void ObservedStateChangedThunk(void *userData);
        static void ObservedStateDeferredInvalidateThunk(void *userData);

        template <class T> NodeState<T> dangerouslyUseState()
        {
          return dangerouslyUseStateWithValue(T());
        }

        template <class T> NodeState<T> dangerouslyUseState(const T &initial)
        {
          return dangerouslyUseStateWithValue(initial);
        }

        bool hasCompositionDiffState() const
        {
          return compositionState_.hasCompositionDiffState();
        }
        const NodeCompositionDiff *localCompositionDiff() const
        {
          return compositionState_.localCompositionDiff();
        }
        Node *compositionRootNode() const
        {
          return this->childrenHead();
        }
        INestable *compositionRootNestable() const
        {
          Node *root = compositionRootNode();
          return root ? root->asNestable() : 0;
        }
        Node *findCompositionChildByTag(NodeTag tag) const
        {
          if (tag == NODE_TAG_NONE)
          {
            return 0;
          }
          INestable *nestable = compositionRootNestable();
          if (!nestable)
          {
            return 0;
          }
          loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            if (child->nodeTag() == tag)
            {
              return child;
            }
          }
          return 0;
        }
        static bool LiveSubtreeContains(Node *root, const Node *candidate)
        {
          if (!root || !candidate)
          {
            return false;
          }
          if (root == candidate)
          {
            return true;
          }
          INestable *nestable = root->asNestable();
          for (Node *child = nestable ? nestable->childrenHead() : 0;
               child;
               child = child->nextInComposition)
          {
            if (LiveSubtreeContains(child, candidate))
            {
              return true;
            }
          }
          return false;
        }
        bool runtimeIsExcluded(
            const BoundaryBranchSeatRuntimeEntry &runtime,
            const BoundaryLocalRebuildExclusions *exclusions) const
        {
          if (!exclusions)
          {
            return false;
          }
          for (size_t i = 0; i < exclusions->roots.size(); ++i)
          {
            if (LiveSubtreeContains(exclusions->roots[i], runtime.active))
            {
              return true;
            }
          }
          return false;
        }
        BoundaryBranchSeatRuntimeEntry *runtimeForCurrentPlan(
            const BoundaryBranchSeatPlanEntry &plan,
            const BoundaryLocalRebuildExclusions *exclusions)
        {
          BoundaryBranchSeatRuntimeEntry *runtime =
              this->branchSeats_.findRuntime(plan.key);
          return runtime && !this->runtimeIsExcluded(*runtime, exclusions)
                     ? runtime
                     : 0;
        }
        void buildLocalRebuildExclusions(
            const INestableDefinition &currentRoot,
            BoundaryLocalRebuildExclusions &exclusions)
        {
          exclusions.clear();
          INestable *root = this->compositionRootNestable();
          Node *runtimeParent = this->compositionRootNode();
          if (!root || !runtimeParent)
          {
            return;
          }

          for (Node *live = root->childrenHead();
               live;
               live = live->nextInComposition)
          {
            bool retained = false;
            for (NodeDefinitionBase *definition = currentRoot.childrenHead();
                 definition && !retained;
                 definition = definition->nextInComposition)
            {
              NodeDefinitionBase *effectiveDefinition = definition;
              IBranchPolicyScopeDefinition *scope =
                  definition->asBranchPolicyScopeDefinition();
              if (scope)
              {
                effectiveDefinition = scope->scopedBranchDefinition();
              }
              const BoundaryBranchSeatPlanEntry *seatPlan =
                  this->branchSeatPlan(effectiveDefinition);
              if (seatPlan)
              {
                BoundaryBranchSeatRuntimeEntry *runtime =
                    this->branchSeats_.findRuntime(seatPlan->key);
                retained = runtime && runtime->parent == runtimeParent &&
                           runtime->active == live;
              }
              else if (effectiveDefinition->nodeTag() != NODE_TAG_NONE)
              {
                retained = effectiveDefinition->nodeTag() == live->nodeTag() &&
                           effectiveDefinition->isCompatibleWithNode(live);
              }
              else if (currentRoot.childrenCount() == 1 &&
                       root->childrenCount() == 1)
              {
                retained = effectiveDefinition->isCompatibleWithNode(live);
              }
            }
            if (!retained)
            {
              exclusions.roots.push_back(live);
            }
          }
        }
        NodeDefinitionBase *findCurrentCompositionDefinitionByTag(NodeTag tag) const
        {
          if (tag == NODE_TAG_NONE)
          {
            return 0;
          }
          NodeDefinitionBase *root = this->composition().root();
          INestableDefinition *nestable = root ? root->asNestableDefinition() : 0;
          NodeDefinitionBase *child = nestable ? nestable->childrenHead() : 0;
          while (child)
          {
            if (child->nodeTag() == tag)
            {
              return child;
            }
            child = child->nextInComposition;
          }
          return 0;
        }
        NodeDefinitionBase *currentCompositionRootDefinition() const
        {
          return this->composition().root();
        }
        bool resolveRetainedDiffEntry(const NodeCompositionDiff::Entry &entry,
                                      Node *&liveNode,
                                      NodeDefinitionBase *&definition) const
        {
          liveNode = 0;
          definition = 0;
          Node *liveRoot = this->compositionRootNode();
          NodeDefinitionBase *currentRoot = this->currentCompositionRootDefinition();
          if (!liveRoot || !currentRoot)
          {
            return false;
          }

          INestable *liveNestable = liveRoot->asNestable();
          INestableDefinition *currentNestable = currentRoot->asNestableDefinition();
          if (!currentNestable)
          {
            /* A definition without materialized children (a compose-once
               boundary: its runtime children come from attach compose, not
               from the definition) has exactly one retained seat — the root
               itself. The live node being nestable is expected there. */
            liveNode = liveRoot;
            definition = currentRoot;
            return true;
          }
          if (!liveNestable)
          {
            return false;
          }

          if (entry.tag != NODE_TAG_NONE)
          {
            definition = this->findCurrentCompositionDefinitionByTag(entry.tag);
          }
          else
          {
            int slot = 0;
            NodeDefinitionBase *currentChild = currentNestable->childrenHead();
            while (slot < entry.currentIndex)
            {
              currentChild = currentChild ? currentChild->nextInComposition : 0;
              ++slot;
            }
            definition = currentChild;
          }
          if (!definition)
          {
            return false;
          }

          const BoundaryBranchSeatPlanEntry *seatPlan = this->branchSeatPlan(definition);
          if (seatPlan)
          {
            const BoundaryBranchSeatRuntimeEntry *runtime =
                this->branchSeats_.findRuntime(seatPlan->key);
            liveNode = runtime ? runtime->active : 0;
            return liveNode != 0;
          }

          if (entry.tag != NODE_TAG_NONE)
          {
            liveNode = this->findCompositionChildByTag(entry.tag);
          }
          else
          {
            int slot = 0;
            loka::dsl::CompositionCursor<Node> liveIt(
                liveNestable->childrenHead(), liveNestable->childrenCount());
            while (slot < entry.currentIndex)
            {
              liveIt.next();
              ++slot;
            }
            liveNode = liveIt.next();
          }
          return liveNode != 0;
        }

        static NodeDefinitionBase *definitionAtIndex(
            NodeDefinitionBase *root,
            int index)
        {
          if (!root || index < 0)
          {
            return 0;
          }
          INestableDefinition *nestable = root->asNestableDefinition();
          if (!nestable)
          {
            return index == 0 ? root : 0;
          }
          NodeDefinitionBase *definition = nestable->childrenHead();
          while (definition && index > 0)
          {
            definition = definition->nextInComposition;
            --index;
          }
          return definition;
        }

        static Node *nodeAtIndex(INestable *parent, int index)
        {
          if (!parent || index < 0)
          {
            return 0;
          }
          Node *node = parent->childrenHead();
          while (node && index > 0)
          {
            node = node->nextInComposition;
            --index;
          }
          return node;
        }

        static bool applyRetainedNodeDefinition(
            Node *liveNode,
            NodeDefinitionBase *previousDefinition,
            NodeDefinitionBase *currentDefinition)
        {
          const bool equivalentProps =
              previousDefinition->hasEquivalentProps(*currentDefinition);
          return equivalentProps
                     ? currentDefinition->repointRetainedNodeDefinition(liveNode)
                     : currentDefinition->applyPropsToNode(liveNode);
        }

        static bool repointDeepEquivalentSiblings(
            Node *liveNode,
            NodeDefinitionBase *currentDefinition)
        {
          while (liveNode && currentDefinition)
          {
            NodeDefinitionBase *nextCurrent =
                currentDefinition->nextInComposition;
            IBranchPolicyScopeDefinition *scope =
                currentDefinition->asBranchPolicyScopeDefinition();
            if (scope)
            {
              currentDefinition = scope->scopedBranchDefinition();
            }
            if (!currentDefinition)
            {
              return false;
            }
            if (!currentDefinition->asBranchSeatDefinition())
            {
              if (!currentDefinition->repointRetainedNodeDefinition(liveNode))
              {
                return false;
              }
              if (!currentDefinition->isBoundary())
              {
                INestable *liveNestable = liveNode->asNestable();
                INestableDefinition *currentNestable =
                    currentDefinition->asNestableDefinition();
                if ((liveNestable == 0) != (currentNestable == 0))
                {
                  return false;
                }
                if (liveNestable &&
                    !repointDeepEquivalentSiblings(
                        liveNestable->childrenHead(),
                        currentNestable->childrenHead()))
                {
                  return false;
                }
              }
            }
            liveNode = liveNode->nextInComposition;
            currentDefinition = nextCurrent;
          }
          return liveNode == 0 && currentDefinition == 0;
        }

        bool applyRetainedDefinitionTree(ComponentContext &context,
                                         Node *liveNode,
                                         NodeDefinitionBase *previousDefinition,
                                         NodeDefinitionBase *currentDefinition)
        {
          if (!liveNode || !previousDefinition || !currentDefinition)
          {
            return false;
          }
          if (currentDefinition->asBranchSeatDefinition())
          {
            return true;
          }
          if (currentDefinition->isBoundary() || liveNode->asBoundary())
          {
            return applyRetainedNodeDefinition(
                liveNode, previousDefinition, currentDefinition);
          }

          // Descendant reconciliation uses definitions and the retained child
          // chain, not this node's current props. Every successful descent
          // applies this node last so a child refusal leaves it untouched.
          INestable *liveNestable = liveNode->asNestable();
          INestableDefinition *previousNestable =
              previousDefinition->asNestableDefinition();
          INestableDefinition *currentNestable =
              currentDefinition->asNestableDefinition();
          if (!liveNestable || !previousNestable || !currentNestable)
          {
            return liveNestable == 0 &&
                   previousNestable == 0 &&
                   currentNestable == 0 &&
                   applyRetainedNodeDefinition(
                       liveNode, previousDefinition, currentDefinition);
          }

          NodeCompositionDiff childDiff;
          if (!detail::buildChildDiffByTag(previousNestable,
                                           currentNestable,
                                           childDiff))
          {
            if (detail::haveDeepEquivalentChildren(previousNestable,
                                                   currentNestable))
            {
              if (!repointDeepEquivalentSiblings(
                      liveNestable->childrenHead(),
                      currentNestable->childrenHead()))
              {
                return false;
              }
              return applyRetainedNodeDefinition(
                  liveNode, previousDefinition, currentDefinition);
            }

            BoundaryLocalRebuildPlan replaceAllPlan;
            if (!this->buildParkedBranchReentryPlan(
                    context,
                    liveNode,
                    *liveNestable,
                    *currentNestable,
                    replaceAllPlan,
                    RETAINED_CHILD_PLAN_REPLACE_ALL))
            {
              return false;
            }
            std::vector<Node *> retainedChildren;
            if (!this->applyLocalRebuildPlan(
                    context,
                    *liveNestable,
                    replaceAllPlan,
                    retainedChildren,
                    false))
            {
              return false;
            }
            return applyRetainedNodeDefinition(
                liveNode, previousDefinition, currentDefinition);
          }
          if (childDiff.empty())
          {
            return applyRetainedNodeDefinition(
                liveNode, previousDefinition, currentDefinition);
          }

          if (childDiff.isCompatibleRetainOnly())
          {
            for (NodeCompositionDiff::Entry *entry = childDiff.entriesHead();
                 entry;
                 entry = entry->nextInComposition)
            {
              NodeDefinitionBase *previousChild = definitionAtIndex(
                  previousDefinition, entry->previousIndex);
              NodeDefinitionBase *currentChild = definitionAtIndex(
                  currentDefinition, entry->currentIndex);
              if (!previousChild || !currentChild)
              {
                return false;
              }
              if (currentChild->asBranchSeatDefinition())
              {
                continue;
              }
              Node *liveChild = nodeAtIndex(
                  liveNestable, entry->currentIndex);
              if (!this->applyRetainedDefinitionTree(
                      context, liveChild, previousChild, currentChild))
              {
                return false;
              }
            }
            return applyRetainedNodeDefinition(
                liveNode, previousDefinition, currentDefinition);
          }

          BoundaryLocalRebuildPlan plan;
          if (!this->buildParkedBranchReentryPlan(
                  context,
                  liveNode,
                  *liveNestable,
                  *currentNestable,
                  plan,
                  RETAINED_CHILD_PLAN_PRESERVE_MATCHES))
          {
            return false;
          }
          std::vector<Node *> retainedChildren;
          if (!this->applyLocalRebuildPlan(
                  context, *liveNestable, plan, retainedChildren, false))
          {
            return false;
          }

          for (NodeCompositionDiff::Entry *entry = childDiff.entriesHead();
               entry;
               entry = entry->nextInComposition)
          {
            if (entry->action != NodeCompositionDiff::ACTION_RETAIN)
            {
              continue;
            }
            NodeDefinitionBase *previousChild = definitionAtIndex(
                previousDefinition, entry->previousIndex);
            NodeDefinitionBase *currentChild = definitionAtIndex(
                currentDefinition, entry->currentIndex);
            if (!previousChild || !currentChild ||
                currentChild->asBranchSeatDefinition())
            {
              if (currentChild && currentChild->asBranchSeatDefinition())
              {
                continue;
              }
              return false;
            }
            if (entry->currentIndex < 0 ||
                static_cast<size_t>(entry->currentIndex) >= plan.entries.size())
            {
              return false;
            }
            BoundaryLocalRebuildPlanEntry &planEntry =
                plan.entries[static_cast<size_t>(entry->currentIndex)];
            if (!planEntry.keepsLiveNode() ||
                !this->applyRetainedDefinitionTree(
                    context,
                    planEntry.node,
                    previousChild,
                    currentChild))
            {
              return false;
            }
          }
          return applyRetainedNodeDefinition(
              liveNode, previousDefinition, currentDefinition);
        }

        bool applyRetainFastPathDefinitions(ComponentContext &context)
        {
          NodeDefinitionBase *previousRoot =
              this->previousCompositionSnapshot().root();
          for (NodeCompositionDiff::Entry *entry = this->localCompositionDiff()->entriesHead();
               entry;
               entry = entry->nextInComposition)
          {
            if (entry->action != NodeCompositionDiff::ACTION_RETAIN)
            {
              continue;
            }
            Node *liveNode = 0;
            NodeDefinitionBase *definition = 0;
            if (!this->resolveRetainedDiffEntry(*entry, liveNode, definition))
            {
              return false;
            }
            if (definition->asBranchSeatDefinition())
            {
              // The seat was applied from the definition-side plan before the
              // composition diff. Its live node is the active branch, not a
              // runtime representation of the seat.
              continue;
            }
            NodeDefinitionBase *previousDefinition = definitionAtIndex(
                previousRoot, entry->previousIndex);
            if (!this->applyRetainedDefinitionTree(
                    context, liveNode, previousDefinition, definition))
            {
              return false;
            }
          }
          return true;
        }
        /** Materializes a fresh node during a local rebuild through a
            contextless temporary composition (intentional: the diff must not
            re-enter the arena/context). Because that composition has no
            ComponentContext, node materialization cannot reach this boundary
            on its own. We consume the completed materialization result here:
            its refusal flags are the monotonic OR across the whole subtree,
            and let this boundary (this IS the owning boundary) route a
            refusal into the projection-failure terminal the context-carrying
            paths use. The composition still carries no ComponentContext and
            no boundary, so
            the branch-seat plan lookup / materialized-seat registration inside
            createNodeRecursive stay disabled exactly as on main — the only new
            effect is the completed value being returned. */
        Node *materializeLocalRebuildNode(NodeDefinitionBase *definition)
        {
          NodeComposition composition;
          NodeMaterializationResult result =
              composition.createNodeFromDefinitionResult(definition);
          if (result.requiresBoundaryPlan)
          {
            this->noteComposeBoundaryPlanRequired();
          }
          else if (!result.root || result.allocationFailed)
          {
            this->noteComposeAllocationFailure();
          }
          return result.root;
        }
        bool rebuildCompositionChildrenFromCurrentSnapshot(ComponentContext &context,
                                                           std::vector<Node *> &retainedChildren)
        {
          INestable *root = compositionRootNestable();
          NodeDefinitionBase *currentRootDefinition = this->composition().root();
          INestableDefinition *currentRoot =
              currentRootDefinition ? currentRootDefinition->asNestableDefinition() : 0;
          if (!root || !currentRoot)
          {
            return false;
          }

          BoundaryLocalRebuildPlan plan;
          BoundaryLocalRebuildExclusions exclusions;
          this->buildLocalRebuildExclusions(*currentRoot, exclusions);
          if (!buildLocalRebuildPlan(context, *currentRoot, exclusions, plan))
          {
            return false;
          }
          return applyLocalRebuildPlan(context, *root, plan, retainedChildren);
        }
        bool rebuildCompositionRootFromCurrentSnapshot(ComponentContext &context, std::vector<Node *> &retainedChildren)
        {
          Node *liveRoot = compositionRootNode();
          NodeDefinitionBase *currentRoot = currentCompositionRootDefinition();
          if (!liveRoot || !currentRoot)
          {
            return false;
          }
          if (compositionRootNestable() || currentRoot->asNestableDefinition())
          {
            return false;
          }
          if (currentRoot->isCompatibleWithNode(liveRoot))
          {
            if (!currentRoot->applyPropsToNode(liveRoot))
            {
              return false;
            }
            retainedChildren.push_back(liveRoot);
            return true;
          }

          Node *created = this->materializeLocalRebuildNode(currentRoot);
          if (!created)
          {
            return false;
          }
          if (!this->replaceChild(liveRoot, created))
          {
            // Error unwind: `created` is arena- or gate-created; free through
            // the door its storage came from.
            DestroyHeapNode(created);
            return false;
          }
          this->composeTree(created, context, COMPOSE_EVENT_ATTACH, this);
          this->retireParkedBranchForRemovedSeat(context, liveRoot);
          this->composeTree(liveRoot, context, COMPOSE_EVENT_DETACH, this);
          this->retireDetachedNode(context, liveRoot);
          return true;
        }
        bool canApplyLocalCompositionDiff() const
        {
          return compositionState_.canApplyLocalCompositionDiff();
        }
        bool canPreserveNativeContexts() const
        {
          if (compositionState_.canPreserveNativeContexts())
          {
            return true;
          }
          bool sawBoundaryChild = false;
          loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            BoundaryNode *childBoundary = child->asBoundary();
            if (!childBoundary)
            {
              continue;
            }
            sawBoundaryChild = true;
            const BoundaryComposeResult &childResult = childBoundary->composeResult();
            if (!childResult.composed || !childResult.preservedNativeContexts)
            {
              return false;
            }
          }
          return sawBoundaryChild;
        }
        NodeCompositionSnapshot &previousCompositionSnapshot()
        {
          return compositionState_.previousCompositionSnapshot();
        }
        const NodeCompositionSnapshot &previousCompositionSnapshot() const
        {
          return compositionState_.previousCompositionSnapshot();
        }
        NodeCompositionSnapshot &currentCompositionSnapshot()
        {
          return compositionState_.currentCompositionSnapshot();
        }
        const NodeCompositionSnapshot &currentCompositionSnapshot() const
        {
          return compositionState_.currentCompositionSnapshot();
        }

      protected:
        enum LocalRecomposeMode
        {
          LOCAL_RECOMPOSE_APPLY_SNAPSHOT = 0,
          LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS = 1
        };

        /** Declares the desired definitions for a boundary-local recompose. */
        virtual void declareLocalRecomposition(NodeComposition &composition)
        {
          (void)composition;
        }

        /** Rebuilds and applies this Boundary's current local composition.
            Returns false without promoting the snapshot when no local plan
            could be applied. */
        bool recomposeLocalComposition(ComponentContext &context,
                                       ComposeEvent event,
                                       LocalRecomposeMode mode)
        {
          NodeComposition &composition = this->beginComposition(context);
          {
            NodeComposition::CompositionScope scope(composition);
            this->declareLocalRecomposition(composition);
          }
          this->captureCurrentCompositionSnapshot();
          this->rebuildCurrentCompositionDiff();
          BoundaryLocalRebuildExclusions exclusions;
          INestableDefinition *currentRoot =
              this->currentCompositionSnapshot().root()
                  ? this->currentCompositionSnapshot().root()->asNestableDefinition()
                  : 0;
          if (currentRoot)
          {
            this->buildLocalRebuildExclusions(*currentRoot, exclusions);
          }
          const NodeCompositionDiff *diff = this->localCompositionDiff();
          const bool canApplyRetainedTree =
              diff && diff->isCompatibleRetainOnly();
          if (!this->applyCurrentBranchSeatPlan(context, &exclusions))
          {
            return false;
          }
          if (mode == LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS &&
              !this->canApplyLocalCompositionDiff())
          {
            return false;
          }

          if (canApplyRetainedTree &&
              this->applyRetainFastPathDefinitions(context))
          {
            this->promoteCurrentCompositionSnapshot();
            loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
            for (Node *child = it.next(); child; child = it.next())
            {
              this->composeTree(child, context, event, this);
            }
            return true;
          }

          // A recursive refusal re-enters the existing local rebuild below.
          // The branch-seat plan above remains a one-shot operation, and the
          // snapshot is promoted only after one of the apply paths completes.
          std::vector<Node *> retainedChildren;
          if (!this->rebuildCompositionChildrenFromCurrentSnapshot(context, retainedChildren)
              && !this->rebuildCompositionRootFromCurrentSnapshot(context, retainedChildren))
          {
            return false;
          }
          this->promoteCurrentCompositionSnapshot();
          for (size_t i = 0; i < retainedChildren.size(); ++i)
          {
            if (retainedChildren[i])
            {
              this->composeTree(retainedChildren[i], context, event, this);
            }
          }
          return true;
        }

        /** Retires the complete arena allocation and ledger for clock-boundary reclaim. */
        void retireOwnedNodeGeneration(ComponentContext &context);
        void retireOwnedNodeGeneration()
        {
          ComponentContext context;
          this->retireOwnedNodeGeneration(context);
        }

        /** Releases native contexts from an already detached node, then queues it
            for clock-boundary reclaim regardless of heap or arena storage. */
        void retireDetachedNode(ComponentContext &context, Node *node);

        virtual void applyPendingStructureInfo(const LocalApplyInfo &, const PlatformApplyPlan &plan)
        {
          this->applyPendingStructure(plan);
        }

        virtual void applyPendingLayoutInfo(const LocalApplyInfo &, const PlatformApplyPlan &plan)
        {
          this->applyPendingLayout(plan);
        }

        virtual void applyPendingLocalPaintInfo(const LocalApplyInfo &, const PlatformApplyPlan &plan)
        {
          this->applyPendingLocalPaint(plan);
        }

        virtual void applyPendingOpaquePaintInfo(const LocalApplyInfo &, const PlatformApplyPlan &plan)
        {
          this->applyPendingOpaquePaint(plan);
        }

        virtual void applyPendingCompositedPaintInfo(const LocalApplyInfo &, const PlatformApplyPlan &plan)
        {
          this->applyPendingCompositedPaint(plan);
        }

        virtual void applyPendingStructure(const PlatformApplyPlan &) {}

        virtual void applyPendingLayout(const PlatformApplyPlan &) {}

        virtual void applyPendingLocalPaint(const PlatformApplyPlan &) {}

        virtual void applyPendingOpaquePaint(const PlatformApplyPlan &plan)
        {
          this->applyPendingLocalPaint(plan);
        }

        virtual void applyPendingCompositedPaint(const PlatformApplyPlan &plan)
        {
          this->applyPendingLocalPaint(plan);
        }

        static void destroyUncommittedLocalRebuildCandidates(
            BoundaryLocalRebuildPlan &plan);

        class UncommittedLocalRebuildPlanGuard
        {
        public:
          explicit UncommittedLocalRebuildPlanGuard(
              BoundaryLocalRebuildPlan &plan)
              : plan_(&plan)
          {
          }

          ~UncommittedLocalRebuildPlanGuard()
          {
            if (this->plan_)
            {
              BoundaryNode::destroyUncommittedLocalRebuildCandidates(
                  *this->plan_);
            }
          }

          void disarm()
          {
            this->plan_ = 0;
          }

        private:
          BoundaryLocalRebuildPlan *plan_;
        };

        bool buildLocalRebuildPlan(ComponentContext &context,
                                   const INestableDefinition &currentRoot,
                                   const BoundaryLocalRebuildExclusions &exclusions,
                                   BoundaryLocalRebuildPlan &plan)
        {
          // This translates the current desired child set into a concrete
          // boundary-local apply plan. It intentionally stays one level above
          // raw NodeCompositionDiff entries because apply needs live-node and
          // ownership details such as previousNode for replacement cleanup.
          plan.clear();
          plan.reserve(currentRoot.childrenCount());

          // Own the nodes this pass materializes until the plan is
          // handed off intact. A mid-build abort (a later child refuses)
          // otherwise drops the raw-vector plan with earlier plain children's
          // contextless heap nodes still live and unparented -- a leak the
          // downstream full-rebuild fallback never reclaims, since those nodes
          // are in no arena ledger (#150).
          //
          // Freed only when arenaOwner()==0 (heap provenance). Arena candidates
          // stay ledger-owned and are reclaimed by the generation retire, so
          // freeing them here would double-destruct. Seat candidates are now
          // exclusively plan-owned too: their runtime facts remain in the
          // registration plan until the structural commit, so an abort cannot
          // leave runtime->active pointing at a discarded candidate.
          UncommittedLocalRebuildPlanGuard uncommittedGuard(plan);

          INestable *root = compositionRootNestable();
          Node *runtimeParent = compositionRootNode();
          if (!root || !runtimeParent)
          {
            return false;
          }

          const NodeCompositionDiff *diff = this->localCompositionDiff();
          NodeCompositionDiff::Entry *singleEntry = diff ? diff->entriesHead() : 0;
          NodeDefinitionBase *definition = currentRoot.childrenHead();
          while (definition)
          {
            NodeDefinitionBase *effectiveDefinition = definition;
            IBranchPolicyScopeDefinition *scope =
                definition->asBranchPolicyScopeDefinition();
            if (scope)
            {
              effectiveDefinition = scope->scopedBranchDefinition();
            }
            const BoundaryBranchSeatPlanEntry *seatPlan =
                this->branchSeatPlan(effectiveDefinition);
            BoundaryBranchSeatRuntimeEntry *seatRuntime =
                seatPlan ? this->runtimeForCurrentPlan(*seatPlan, &exclusions) : 0;
            Node *existing = seatPlan
                                 ? (seatRuntime ? seatRuntime->active : 0)
                                 : findCompositionChildByTag(effectiveDefinition->nodeTag());
            bool reconcileScopedAnonymous = false;
            if (!seatPlan && !existing &&
                effectiveDefinition->nodeTag() == NODE_TAG_NONE &&
                diff && !diff->fullRebuild && diff->entryCount() == 1 && singleEntry &&
                singleEntry->previousIndex == 0 && singleEntry->currentIndex == 0 &&
                (scope ||
                 (singleEntry->action == NodeCompositionDiff::ACTION_RETAIN &&
                  singleEntry->compatibleType)))
            {
              existing = root && root->childrenCount() == 1 ? root->childrenHead() : 0;
              // A misplaced scope dissolves into its Fragment. Reusing that
              // runtime root is safe only when its desired subtree is also
              // reconciled; applying Fragment props alone cannot update it.
              reconcileScopedAnonymous = scope && existing;
            }
            if (existing &&
                (seatPlan || effectiveDefinition->isCompatibleWithNode(existing)))
            {
              const bool reconcileRetainedSubtree =
                  reconcileScopedAnonymous || existing->asBoundarySectionNode();
              plan.entries.push_back(
                  reconcileRetainedSubtree
                      ? BoundaryLocalRebuildPlanEntry::reconcile(
                            existing,
                            effectiveDefinition,
                            effectiveDefinition->nodeTag())
                      : BoundaryLocalRebuildPlanEntry::retain(
                            existing,
                            effectiveDefinition,
                            effectiveDefinition->nodeTag()));
            }
            else
            {
              Node *created = 0;
              if (seatPlan)
              {
                if (!seatPlan->dirtySource)
                {
                  return false;
                }
                if (!this->createCurrentBranch(context,
                                               *seatPlan,
                                               runtimeParent,
                                               created,
                                               &plan.branchSeatRegistrations))
                {
                  return false;
                }
                plan.branchSeatRegistrations.record(*seatPlan,
                                                    runtimeParent,
                                                    created);
              }
              else
              {
                created = this->materializeLocalRebuildNode(effectiveDefinition);
                if (!created)
                {
                  return false;
                }
              }
              plan.entries.push_back(
                  existing ? BoundaryLocalRebuildPlanEntry::replace(
                                 created,
                                 existing,
                                 effectiveDefinition,
                                 effectiveDefinition->nodeTag())
                           : BoundaryLocalRebuildPlanEntry::attach(
                                 created,
                                 effectiveDefinition,
                                 effectiveDefinition->nodeTag()));
            }
            definition = definition->nextInComposition;
          }

          loka::dsl::CompositionCursor<Node> it(root->childrenHead(), root->childrenCount());
          for (Node *liveChild = it.next(); liveChild; liveChild = it.next())
          {
            bool representedByPlan = false;
            for (size_t i = 0; i < plan.entries.size(); ++i)
            {
              BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
              // A replacement's previous node already has one detach/retire
              // path in the plan and must not be appended as a second retire.
              if ((entry.keepsLiveNode() && entry.node == liveChild) ||
                  entry.detachedNode() == liveChild)
              {
                representedByPlan = true;
                break;
              }
            }
            if (!representedByPlan &&
                findCurrentCompositionDefinitionByTag(liveChild->nodeTag()) == 0)
            {
              plan.entries.push_back(BoundaryLocalRebuildPlanEntry::retire(liveChild, liveChild->nodeTag()));
            }
          }
          // Runtime publication is part of the structural commit. Reserve its
          // vector storage while this operation can still refuse, so cleanup
          // and publication after detach cannot allocate or partially commit.
          this->branchSeats_.reserveRuntimeRegistrations(
              plan.branchSeatRegistrations.count());
          uncommittedGuard.disarm();
          return true;
        }
        bool applyLocalRebuildPlan(ComponentContext &context,
                                   INestable &root,
                                   BoundaryLocalRebuildPlan &plan,
                                   std::vector<Node *> &retainedChildren,
                                   bool applyRetainedDefinitions = true)
        {
          // Structure self-report: the root-level diff cannot see what this
          // plan is about to do, so any entry that materializes or retires a
          // node must be declared here or the cycle's apply claims paint-only
          // and skip-capable platforms never run the ensure pass (#277).
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            if (plan.entries[i].requiresAttachCompose() ||
                plan.entries[i].detachedNode())
            {
              this->noteLocalStructureWork();
              break;
            }
          }
          std::vector<Node *> detachedChildren;
          root.detachChildrenTo(detachedChildren);
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            if (!plan.entries[i].keepsLiveNode())
            {
              continue;
            }
            if (plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RETAIN ||
                plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RECONCILE)
            {
              NodeDefinitionBase *retainedDefinition = plan.entries[i].definition;
              const bool reconciled =
                  plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RECONCILE;
              if (!retainedDefinition)
              {
                return false;
              }
              if (reconciled &&
                  !this->reconcileParkedBranch(
                      context, plan.entries[i].node, retainedDefinition))
              {
                return false;
              }
              if (!reconciled && applyRetainedDefinitions &&
                  !retainedDefinition->asBranchSeatDefinition() &&
                  !retainedDefinition->applyPropsToNode(plan.entries[i].node))
              {
                return false;
              }
              retainedChildren.push_back(plan.entries[i].node);
            }
            root.addChild(plan.entries[i].node);
          }

          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
            Node *detachedNode = entry.detachedNode();
            if (detachedNode)
            {
              this->retireParkedBranchForRemovedSeat(context, detachedNode);
            }
          }
          plan.branchSeatRegistrations.commitTo(this->branchSeats_);
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
            if (entry.requiresAttachCompose())
            {
              this->composeTree(entry.node, context, COMPOSE_EVENT_ATTACH, this);
            }
          }
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
            Node *detachedNode = entry.detachedNode();
            if (detachedNode)
            {
              this->composeTree(detachedNode, context, COMPOSE_EVENT_DETACH, this);
              this->retireDetachedNode(context, detachedNode);
            }
          }
          return true;
        }

        enum RetainedChildPlanMode
        {
          RETAINED_CHILD_PLAN_PRESERVE_MATCHES,
          RETAINED_CHILD_PLAN_REPLACE_ALL
        };

        bool buildParkedBranchReentryPlan(
            ComponentContext &context,
            Node *runtimeParent,
            INestable &root,
            INestableDefinition &desiredRoot,
            BoundaryLocalRebuildPlan &plan,
            RetainedChildPlanMode mode)
        {
          plan.clear();
          plan.reserve(desiredRoot.childrenCount());

          // This builder may materialize more than one candidate before a
          // later definition refuses. Until the complete plan is ready, those
          // candidates have no owner except the plan itself.
          UncommittedLocalRebuildPlanGuard uncommittedGuard(plan);

          std::vector<Node *> liveChildren;
          loka::dsl::CompositionCursor<Node> liveIt(root.childrenHead(), root.childrenCount());
          for (Node *live = liveIt.next(); live; live = liveIt.next())
          {
            liveChildren.push_back(live);
          }

          NodeDefinitionBase *definition = desiredRoot.childrenHead();
          size_t slot = 0;
          while (definition)
          {
            NodeDefinitionBase *effectiveDefinition = definition;
            IBranchPolicyScopeDefinition *scope =
                definition->asBranchPolicyScopeDefinition();
            if (scope)
            {
              effectiveDefinition = scope->scopedBranchDefinition();
            }
            const BoundaryBranchSeatPlanEntry *seatPlan =
                this->branchSeatPlan(effectiveDefinition);
            BoundaryBranchSeatRuntimeEntry *seatRuntime =
                seatPlan ? this->branchSeats_.findRuntime(seatPlan->key) : 0;
            Node *existing =
                mode == RETAINED_CHILD_PLAN_REPLACE_ALL
                    ? (slot < liveChildren.size() ? liveChildren[slot] : 0)
                    : (seatRuntime ? seatRuntime->active : 0);
            if (mode == RETAINED_CHILD_PLAN_PRESERVE_MATCHES &&
                !seatPlan && !existing &&
                effectiveDefinition->nodeTag() != NODE_TAG_NONE)
            {
              for (size_t i = 0; i < liveChildren.size(); ++i)
              {
                if (liveChildren[i] &&
                    liveChildren[i]->nodeTag() == effectiveDefinition->nodeTag())
                {
                  existing = liveChildren[i];
                  break;
                }
              }
            }
            else if (mode == RETAINED_CHILD_PLAN_PRESERVE_MATCHES &&
                     !seatPlan && !existing && slot < liveChildren.size() &&
                     liveChildren[slot] && liveChildren[slot]->nodeTag() == NODE_TAG_NONE)
            {
              existing = liveChildren[slot];
            }

            if (mode == RETAINED_CHILD_PLAN_PRESERVE_MATCHES && existing &&
                (seatPlan || effectiveDefinition->isCompatibleWithNode(existing)))
            {
              plan.entries.push_back(
                  scope
                      ? BoundaryLocalRebuildPlanEntry::reconcile(
                            existing,
                            effectiveDefinition,
                            effectiveDefinition->nodeTag())
                      : BoundaryLocalRebuildPlanEntry::retain(
                            existing,
                            effectiveDefinition,
                            effectiveDefinition->nodeTag()));
            }
            else
            {
              Node *created = 0;
              if (seatPlan)
              {
                if (!seatPlan->dirtySource || !runtimeParent ||
                    !this->createCurrentBranch(
                        context,
                        *seatPlan,
                        runtimeParent,
                        created,
                        &plan.branchSeatRegistrations))
                {
                  return false;
                }
                plan.branchSeatRegistrations.record(
                    *seatPlan, runtimeParent, created);
              }
              else
              {
                created = this->materializeLocalRebuildNode(effectiveDefinition);
              }
              if (!created)
              {
                return false;
              }
              plan.entries.push_back(
                  existing ? BoundaryLocalRebuildPlanEntry::replace(
                                 created,
                                 existing,
                                 effectiveDefinition,
                                 effectiveDefinition->nodeTag())
                           : BoundaryLocalRebuildPlanEntry::attach(
                                 created,
                                 effectiveDefinition,
                                 effectiveDefinition->nodeTag()));
            }
            definition = definition->nextInComposition;
            ++slot;
          }

          for (size_t i = 0; i < liveChildren.size(); ++i)
          {
            bool represented = false;
            for (size_t planIndex = 0; planIndex < plan.entries.size(); ++planIndex)
            {
              if (plan.entries[planIndex].node == liveChildren[i] ||
                  plan.entries[planIndex].previousNode == liveChildren[i])
              {
                represented = true;
                break;
              }
            }
            if (!represented)
            {
              plan.entries.push_back(
                  BoundaryLocalRebuildPlanEntry::retire(liveChildren[i], liveChildren[i]->nodeTag()));
            }
          }
          this->branchSeats_.reserveRuntimeRegistrations(
              plan.branchSeatRegistrations.count());
          uncommittedGuard.disarm();
          return true;
        }

        bool reconcileParkedBranch(ComponentContext &context,
                                   Node *node,
                                   NodeDefinitionBase *definition)
        {
          if (!node || !definition || !definition->applyPropsToNode(node))
          {
            return false;
          }
          if (node->asComposable())
          {
            return true;
          }

          INestable *root = node->asNestable();
          INestableDefinition *desiredRoot = definition->asNestableDefinition();
          if (!root || !desiredRoot)
          {
            return root == 0 && desiredRoot == 0;
          }

          BoundaryLocalRebuildPlan plan;
          if (!this->buildParkedBranchReentryPlan(
                  context,
                  node,
                  *root,
                  *desiredRoot,
                  plan,
                  RETAINED_CHILD_PLAN_PRESERVE_MATCHES))
          {
            return false;
          }
          std::vector<Node *> retainedChildren;
          if (!this->applyLocalRebuildPlan(context, *root, plan, retainedChildren))
          {
            return false;
          }
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
            // A dissolved seat has no runtime node at this level. Its value-key
            // plan owns nested branch reconciliation later in this same pass.
            if (entry.action == BoundaryLocalRebuildPlanEntry::ACTION_RETAIN &&
                !entry.definition->asBranchSeatDefinition() &&
                !this->reconcileParkedBranch(context, entry.node, entry.definition))
            {
              return false;
            }
          }
          return true;
        }

        bool createCurrentBranch(ComponentContext &context,
                                 const BoundaryBranchSeatPlanEntry &plan,
                                 Node *parent,
                                 Node *&created,
                                 BoundaryBranchSeatRuntimeRegistrationPlan *registrations = 0)
        {
          (void)parent;
          created = 0;
          loka::app::FragmentDefinition emptyBranch;
          NodeDefinitionBase *definition =
              plan.materializedBranchDefinition(emptyBranch);
          NodeComposition composition;
          composition.setContext(&context);
          composition.collectBranchSeatRegistrationsIn(registrations);
          assert(context.boundary() == this);
          NodeMaterializationResult result =
              composition.createNodeFromDefinitionResult(definition);
          if (result.requiresBoundaryPlan)
          {
            this->noteComposeBoundaryPlanRequired();
          }
          if (result.allocationFailed)
          {
            this->noteComposeAllocationFailure();
          }
          created = result.root;
          return created != 0;
        }

        bool isMaterializedEmptyBranch(Node *node) const
        {
          INestable *nestable = node ? node->asNestable() : 0;
          return node &&
                 node->propsTypeId() == loka::app::FragmentProps::staticTypeId() &&
                 nestable && nestable->childrenCount() == 0;
        }

        void retireSeatBranchRoot(ComponentContext &context, Node *branch)
        {
          if (!branch)
          {
            return;
          }
          this->composeTree(branch, context, COMPOSE_EVENT_DETACH, this);
          this->retireDetachedNode(context, branch);
        }

        /** Commits descendant seat death across both Boundary-owned ledgers.
            Runtime mappings are erased one at a time without allocation; each
            erased key first drains both of its descendant arms and then every
            parked resident for that key through the ordinary retire door. */
        void retireOwnedSeatDescendants(
            ComponentContext &context,
            const BoundaryParkedBranchKey &ownerKey,
            unsigned ownerArm)
        {
          BoundaryParkedBranchKey nestedKey;
          unsigned nestedArmCount = 0;
          while (this->branchSeats_.eraseOneOwnedRuntime(ownerKey,
                                                         ownerArm,
                                                         nestedKey,
                                                         nestedArmCount))
          {
            for (unsigned arm = 0; arm < nestedArmCount; ++arm)
            {
              this->retireOwnedSeatDescendants(context, nestedKey, arm);
              Node *parkedBranch = 0;
              while ((parkedBranch = this->parkedBranches_.take(nestedKey, arm)) != 0)
              {
                this->retireSeatBranchRoot(context, parkedBranch);
              }
            }
          }
        }

        void retireSeatBranch(ComponentContext &context,
                              const BoundaryParkedBranchKey &key,
                              unsigned arm,
                              Node *branch)
        {
          this->retireOwnedSeatDescendants(context, key, arm);
          this->retireSeatBranchRoot(context, branch);
        }

        void drainParkedSeat(ComponentContext &context,
                             const BoundaryParkedBranchKey &key,
                             unsigned armCount)
        {
          for (unsigned arm = 0; arm < armCount; ++arm)
          {
            Node *parkedBranch = 0;
            while ((parkedBranch = this->parkedBranches_.take(key, arm)) != 0)
            {
              this->retireSeatBranch(context, key, arm, parkedBranch);
            }
          }
        }

        void retireParkedBranchForRemovedSeat(ComponentContext &context,
                                              Node *activeBranch)
        {
          BoundaryParkedBranchKey key;
          unsigned activeArm = 0;
          bool hasActiveArm = false;
          unsigned armCount = 0;
          if (!this->branchSeats_.eraseRuntimeForActive(activeBranch,
                                                        key,
                                                        activeArm,
                                                        hasActiveArm,
                                                        armCount))
          {
            return;
          }
          if (hasActiveArm)
          {
            this->retireOwnedSeatDescendants(context, key, activeArm);
          }
          for (unsigned arm = 0; arm < armCount; ++arm)
          {
            if (hasActiveArm && arm == activeArm)
            {
              continue;
            }
            Node *parkedBranch = 0;
            while ((parkedBranch = this->parkedBranches_.take(key, arm)) != 0)
            {
              this->retireSeatBranch(context, key, arm, parkedBranch);
            }
          }
        }

        /** The retained-detach line, as one door. Parking keeps a branch's
            states warm for re-entry, but detach removes Held slots (axiom 14,
            R3): a parked branch is a cross-tick retention, and cross-tick
            retention re-acquires through the owner at its next ATTACH. The
            fact walk and the slot drop travel together so no park site can
            take one without the other; the last drop queues the releaser on
            the owner's existing pool, never firing here. Children only, like
            the fact walk: a nested parked branch already crossed this line
            when it parked. */
        void detachRetainedSubtree(Node *node)
        {
          NotifySubtreeNodeDetached(node);
          DropRetainedHeldSlots(node);
          // A last drop inside the branch queues its releaser on the nearest
          // enclosing Boundary -- which may itself be inside the branch being
          // parked, where the live-tree drain walk will never reach it again.
          // The queue must ride a clock that is still live, so the parking
          // boundary adopts it; the entries run at this boundary's next
          // drain, which is the tick boundary the drop was aimed at.
          AdoptParkedPendingReleases(node, *this);
        }

        static void AdoptParkedPendingReleases(Node *node, BoundaryNode &pool)
        {
          if (!node)
          {
            return;
          }
          BoundaryNode *nested = node->asBoundary();
          if (nested && nested != &pool)
          {
            while (loka::core::detail::HeldBlockBase *block =
                       nested->pendingHeldReleasesHead_)
            {
              nested->pendingHeldReleasesHead_ = block->retireNext();
              if (!nested->pendingHeldReleasesHead_)
              {
                nested->pendingHeldReleasesTail_ = 0;
              }
              block->setRetireNext(0);
              pool.retireHeldBlock(block);
            }
          }
          INestable *nestable = node->asNestable();
          for (Node *child = nestable ? nestable->childrenHead() : 0;
               child;
               child = child->nextInComposition)
          {
            AdoptParkedPendingReleases(child, pool);
          }
        }

        static void DropRetainedHeldSlots(Node *node)
        {
          if (!node)
          {
            return;
          }
          IStateOwner *owner = node->asStateOwner();
          if (owner)
          {
            owner->detachHeldResources();
          }
          INestable *nestable = node->asNestable();
          for (Node *child = nestable ? nestable->childrenHead() : 0;
               child;
               child = child->nextInComposition)
          {
            DropRetainedHeldSlots(child);
          }
        }

        /** Installs the plan's selected arm in place of the runtime's active
            one. `drainParkedArmCount` > 0 means the seat is rebuilding under a
            new shape: every arm parked under the old shape (0..count-1) is
            retired after the outgoing arm and before the incoming arm's nested
            mappings are committed -- after, so a failed materialization
            leaves the old ledger untouched; before the commit, so the retire
            under (key, arm) cannot erase the fresh mappings. `runtime` is read
            once up front: the ledger it lives in is edited below. */
        bool replaceSeatBranch(ComponentContext &context,
                               const BoundaryBranchSeatPlanEntry &plan,
                               const BoundaryBranchSeatRuntimeEntry &runtime,
                               bool parkOutgoing,
                               bool reuseParked,
                               unsigned drainParkedArmCount = 0)
        {
          Node *runtimeParent = runtime.parent;
          Node *outgoing = runtime.active;
          const unsigned outgoingArm = runtime.activeArm;
          const bool hadOutgoingArm = runtime.hasActiveArm;
          Node *incoming = plan.hasSelectedArm && reuseParked
                               ? this->takeParkedBranch(plan.key, plan.selectedArm)
                               : 0;
          NodeDefinitionBase *definition = plan.hasSelectedArm
                                               ? plan.branch(plan.selectedArm).definition
                                               : 0;
          if (incoming &&
              ((!definition && !this->isMaterializedEmptyBranch(incoming)) ||
               (definition &&
                (!definition->isCompatibleWithNode(incoming) ||
                 !this->reconcileParkedBranch(context, incoming, definition)))))
          {
            this->retireSeatBranch(context, plan.key, plan.selectedArm, incoming);
            incoming = 0;
          }
          // Nested seats inside the incoming arm are staged, not published:
          // when the seat rebuilds in place (shape mismatch, same arm index)
          // the outgoing arm is retired under the very owner pair
          // (plan.key, selectedArm) the new nested mappings would carry, and
          // retireOwnedSeatDescendants() would erase them with the old ones.
          // The local-rebuild path stages for the same reason (#511).
          BoundaryBranchSeatRuntimeRegistrationPlan nestedRegistrations;
          if (!incoming && !this->createCurrentBranch(context,
                                                      plan,
                                                      runtimeParent,
                                                      incoming,
                                                      &nestedRegistrations))
          {
            return false;
          }
          if (!incoming)
          {
            return false;
          }
          // Runtime publication is part of the structural commit: reserve the
          // ledger storage now, while nothing has been replaced or retired, so
          // the commit after cleanup cannot allocate (the local-rebuild path's
          // rule, applyLocalRebuildPlan).
          this->branchSeats_.reserveRuntimeRegistrations(nestedRegistrations.count());

          INestable *parent = runtimeParent ? runtimeParent->asNestable() : 0;
          if (!parent || !parent->replaceChild(outgoing, incoming))
          {
            return false;
          }

          if (outgoing)
          {
            if (parkOutgoing && hadOutgoingArm)
            {
              this->detachRetainedSubtree(outgoing);
              this->parkBranch(plan.key, outgoing, outgoingArm);
            }
            else if (hadOutgoingArm)
            {
              this->retireSeatBranch(context,
                                     plan.key,
                                     outgoingArm,
                                     outgoing);
            }
            else
            {
              this->retireSeatBranchRoot(context, outgoing);
            }
          }
          if (drainParkedArmCount > 0)
          {
            this->drainParkedSeat(context, plan.key, drainParkedArmCount);
          }
          nestedRegistrations.commitTo(this->branchSeats_);
          BoundaryBranchSeatRuntimeEntry *committedRuntime =
              this->branchSeats_.findRuntime(plan.key);
          assert(committedRuntime &&
                 "replacing a branch must preserve its definition-side seat mapping");
          if (!committedRuntime)
          {
            return false;
          }
          committedRuntime->active = incoming;
          committedRuntime->activeArm = plan.selectedArm;
          committedRuntime->hasActiveArm = plan.hasSelectedArm;
          committedRuntime->shape = plan.shape;
          committedRuntime->appliedGeneration = this->branchSeats_.generation();
          incoming->markPendingAttachForCompose();
          return true;
        }

        bool applyBranchSeat(ComponentContext &context,
                             NodeDefinitionBase *definition,
                             BoundaryBranchSeatRuntimeEntry &runtime)
        {
          const BoundaryBranchSeatPlanEntry *plan = this->branchSeatPlan(definition);
          if (!plan || !plan->dirtySource)
          {
            return false;
          }
          if (!runtime.active)
          {
            return false;
          }
          if (!this->branchSeats_.isLive(runtime))
          {
            return true;
          }

          BoundaryBranchSeatPlanEntry *mutablePlan =
              this->branchSeats_.findPlan(plan->key);
          if (!mutablePlan)
          {
            return false;
          }
          if (runtime.appliedGeneration == this->branchSeats_.generation())
          {
            mutablePlan->snapshotSelection();
          }

          if (!runtime.shape.matches(mutablePlan->shape))
          {
            // Rebuild under the new shape; the old shape's parked arms are
            // drained inside the replacement, between the outgoing retire and
            // the commit of the incoming arm's nested mappings.
            return this->replaceSeatBranch(context,
                                           *mutablePlan,
                                           runtime,
                                           false,
                                           false,
                                           runtime.shape.armCount);
          }

          if (mutablePlan->hasSelectedArm != runtime.hasActiveArm ||
              (mutablePlan->hasSelectedArm &&
               mutablePlan->selectedArm != runtime.activeArm))
          {
            const bool parkOutgoing =
                runtime.hasActiveArm &&
                !mutablePlan->branch(runtime.activeArm).policies.destroyOnDetach;
            return this->replaceSeatBranch(context,
                                           *mutablePlan,
                                           runtime,
                                           parkOutgoing,
                                           true);
          }

          if (runtime.appliedGeneration == this->branchSeats_.generation())
          {
            return true;
          }
          NodeDefinitionBase *branchDefinition = mutablePlan->hasSelectedArm
                                                     ? mutablePlan->branch(
                                                           mutablePlan->selectedArm)
                                                           .definition
                                                     : 0;
          if (!branchDefinition &&
              this->isMaterializedEmptyBranch(runtime.active))
          {
            runtime.appliedGeneration = this->branchSeats_.generation();
            return true;
          }
          if (branchDefinition &&
              this->reconcileParkedBranch(context, runtime.active, branchDefinition))
          {
            runtime.appliedGeneration = this->branchSeats_.generation();
            return true;
          }
          return this->replaceSeatBranch(context,
                                         *mutablePlan,
                                         runtime,
                                         false,
                                         true);
        }

        bool applyCurrentBranchSeatPlan(
            ComponentContext &context,
            const BoundaryLocalRebuildExclusions *exclusions)
        {
          const std::vector<BoundaryBranchSeatPlanEntry> &plans = this->branchSeats_.plans();
          for (size_t i = 0; i < plans.size(); ++i)
          {
            BoundaryBranchSeatRuntimeEntry *runtime =
                this->runtimeForCurrentPlan(plans[i], exclusions);
            if (!runtime || !this->branchSeats_.isLive(*runtime))
            {
              continue;
            }
            NodeDefinitionBase *definition = this->findBranchSeatDefinition(plans[i].key);
            if (!definition || !this->applyBranchSeat(context, definition, *runtime))
            {
              return false;
            }
          }
          for (unsigned i = 0; BoundaryParkedBranchLedger::Entry *parked = this->parkedBranches_.entry(i); ++i)
          {
            BoundaryBranchSeatPlanEntry *plan = this->branchSeats_.findPlan(parked->key);
            BoundaryBranchSeatRuntimeEntry *runtime =
                plan ? this->runtimeForCurrentPlan(*plan, exclusions) : 0;
            if (!plan || !runtime ||
                !plan->branch(parked->arm).policies.deliverWhileDetached)
            {
              continue;
            }
            NodeDefinitionBase *branchDefinition = plan->branch(parked->arm).definition;
            if (branchDefinition &&
                !this->reconcileParkedBranch(context, parked->branch, branchDefinition))
            {
              return false;
            }
          }
          return true;
        }

        NodeDefinitionBase *findBranchSeatDefinition(const BoundaryParkedBranchKey &key) const
        {
          return this->findBranchSeatDefinitionRecursive(this->currentCompositionRootDefinition(), key);
        }

        NodeDefinitionBase *findBranchSeatDefinitionRecursive(NodeDefinitionBase *definition,
                                                              const BoundaryParkedBranchKey &key) const
        {
          if (!definition)
          {
            return 0;
          }
          IBranchSeatDefinition *seat = definition->asBranchSeatDefinition();
          if (seat)
          {
            BoundaryParkedBranchKey candidate(definition->nodeTag(),
                                              definition->compositionSeatSlot(),
                                              seat->branchSeatTypeId());
            if (candidate.matches(key))
            {
              return definition;
            }
            for (unsigned arm = 0; arm < seat->armCount(); ++arm)
            {
              NodeDefinitionBase *found =
                  this->findBranchSeatDefinitionRecursive(seat->armDefinition(arm), key);
              if (found)
              {
                return found;
              }
            }
            return 0;
          }
          IBranchPolicyScopeDefinition *scope = definition->asBranchPolicyScopeDefinition();
          if (scope)
          {
            return this->findBranchSeatDefinitionRecursive(scope->scopedBranchDefinition(), key);
          }
          INestableDefinition *nestable = definition->asNestableDefinition();
          for (NodeDefinitionBase *child = nestable ? nestable->childrenHead() : 0;
               child;
               child = child->nextInComposition)
          {
            NodeDefinitionBase *found = this->findBranchSeatDefinitionRecursive(child, key);
            if (found)
            {
              return found;
            }
          }
          return 0;
        }

        virtual void adoptState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          ownedStates_.push_back(state);
          tracker_.addState(state);
        }

        virtual void adoptStateUnchecked(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          ownedStates_.push_back(state);
          tracker_.addStateUnchecked(state);
        }

        virtual void releaseState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          for (size_t i = 0; i < ownedStates_.size();)
          {
            if (ownedStates_[i] == state)
            {
              ownedStates_.erase(ownedStates_.begin() + i);
            }
            else
            {
              ++i;
            }
          }
          tracker_.removeState(state);
          if (state->isArenaAllocated())
          {
            stateArena_.releaseState(state);
            return;
          }
          DestroyAdoptedHeapState(state);
        }

        virtual void reserveStates(size_t count)
        {
          ownedStates_.reserve(ownedStates_.size() + count);
          tracker_.reserveStates(count);
        }

        static void
        composeTree(Node *node, ComponentContext &parentContext, ComposeEvent event, BoundaryNode *currentBoundary)
        {
          if (!node)
          {
            return;
          }
          BoundaryNode *boundary;
          ComposableNode *composable;
          INestable *nestable;
          {
            PROFILE_SECTION("virt");
            boundary = node->asBoundary();
            composable = node->asComposable();
            nestable = node->asNestable();
          }

          IStateOwner *nodeStateOwner = node->asStateOwner();
          if (nodeStateOwner && nodeStateOwner != boundary)
          {
            nodeStateOwner->attachEnclosingBoundary(currentBoundary);
          }
          if (nodeStateOwner && event != COMPOSE_EVENT_DETACH)
          {
            nodeStateOwner->attachEnclosingHoldOwner(
                parentContext.stateOwner());
          }

          if (boundary && event == COMPOSE_EVENT_DETACH)
          {
            boundary->setScene(0);
            boundary->setParentBoundary(0);
            boundary->clearObservedStateEntries();
          }
          // ComposeEvent is an input to the lifecycle state machine, not an
          // observable of its own: ATTACH passes the compose door; nodes and
          // contexts hear changes through the two fact observation points.
          if (event == COMPOSE_EVENT_ATTACH)
          {
            node->applyLifecycleFact(NODE_FACT_ATTACHED);
          }

          BoundaryNode *nextBoundary = currentBoundary;
          if (boundary)
          {
            if (event != COMPOSE_EVENT_DETACH)
            {
              boundary->setParentBoundary(currentBoundary);
              if (currentBoundary)
              {
                boundary->setScene(currentBoundary->getScene());
              }
            }
            boundary->clearObservedDirtyFlags();
            boundary->clearPhaseResults();
            if (event != COMPOSE_EVENT_DETACH)
            {
              boundary->beginObservedStatePass();
              boundary->registerBranchSeatDirtySources();
            }
            nextBoundary = boundary;
          }
          BoundaryComposePhaseScope composeScope =
              boundary ? boundary->beginComposePhaseScope() : BoundaryComposePhaseScope(0);
          if (boundary)
          {
            boundary->beginComposeResult(event, parentContext.dirtyFlags());
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
            loka::platform::DebugLogBoundaryComposeDispatch(static_cast<void *>(boundary),
                                                            static_cast<unsigned int>(event),
                                                            static_cast<unsigned int>(parentContext.dirtyFlags()),
                                                            boundary == currentBoundary ? 1 : 0);
            if (boundary->childrenHead() && boundary->childrenCount() == 1)
            {
              Node *firstChild = boundary->childrenHead();
              if (firstChild && firstChild->testId() == "PendingDefaultApplyText")
              {
                loka::platform::DebugLogSceneRootIdentity(static_cast<void *>(boundary->scene()),
                                                          static_cast<void *>(boundary),
                                                          static_cast<unsigned int>(boundary->kind()),
                                                          "pending-default-boundary-compose",
                                                          boundary->previousCompositionSnapshot().root() ? 1 : 0,
                                                          boundary->currentCompositionSnapshot().root() ? 1 : 0,
                                                          boundary->hasCompositionDiffState() ? 0 : 1,
                                                          static_cast<unsigned int>(boundary->childrenCount()),
                                                          static_cast<unsigned int>(firstChild->kind()),
                                                          firstChild->testId().c_str());
              }
            }
#endif
          }
          if (nextBoundary && event != COMPOSE_EVENT_DETACH)
          {
            nextBoundary->noteLocalPaintWork();
            declareBoundaryDirtySources(node, nextBoundary);
          }
          const bool routesInnerStateOwner =
              nodeStateOwner && nodeStateOwner != boundary;
          ComponentContext *contextForChildren = &parentContext;
          ComponentContext nodeContext(&parentContext);
          {
            PROFILE_SECTION("ctx");
            nodeContext.setStateOwner(routesInnerStateOwner
                                          ? nodeStateOwner
                                          : parentContext.stateOwner());
            nodeContext.setBoundary(nextBoundary);
            nodeContext.setPlatformController(parentContext.platformController());
            Scene *scene = nextBoundary ? nextBoundary->getScene() : 0;
            nodeContext.setScene(scene);
            nodeContext.setWindow(parentContext.window());
            nodeContext.setDirtyFlags(parentContext.dirtyFlags());
            nodeContext.setComposition(parentContext.composition());
          }

          if (composable)
          {
            NodeComposition *composition = nodeContext.composition();
            if (composition)
            {
              NodeComposition::CompositionScope scope(*composition);
              composable->compose(nodeContext, event);
            }
            else
            {
              composable->compose(nodeContext, event);
            }
            if (boundary)
            {
              boundary->completeComposeResult(boundary->canPreserveNativeContexts());
            }
            contextForChildren = &nodeContext;
          }
          else if (routesInnerStateOwner)
          {
            contextForChildren = &nodeContext;
          }
          if (!nestable)
          {
            if (event == COMPOSE_EVENT_DETACH && nodeStateOwner)
            {
              nodeStateOwner->detachHeldResources();
            }
            return;
          }
          loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            ComposeEvent childEvent = child->resolveChildComposeEvent(event);
            composeTree(child, *contextForChildren, childEvent, nextBoundary);
          }
          if (event == COMPOSE_EVENT_DETACH && nodeStateOwner)
          {
            nodeStateOwner->detachHeldResources();
          }
        }

        void registerState(loka::core::StateBase *state)
        {
          tracker_.addState(state);
        }

        // Varargs must end with a typed null terminator (use STATE_NULL from
        // core/util/StateUtil.hpp); a missing terminator reads past the argument list.
        void registerStates(loka::core::StateBase *first, ...)
        {
          loka::core::StateVector states;
          va_list args;
          va_start(args, first);
          for (loka::core::StateBase *s = first; s != 0; s = va_arg(args, loka::core::StateBase *))
          {
            if (s)
            {
              states.push_back(s);
            }
          }
          va_end(args);
          for (size_t i = 0; i < states.size(); ++i)
          {
            tracker_.addState(states[i]);
          }
        }

        void captureCurrentCompositionSnapshot()
        {
          this->composition().assignCompositionSeatSlots();
          this->branchSeats_.capture(this->composition().root());
          this->registerBranchSeatDirtySources();
          compositionState_.captureCurrentSnapshot(this->composition());
        }

        void rebuildCurrentCompositionDiff()
        {
          compositionState_.rebuildLocalCompositionDiff();
        }

        void promoteCurrentCompositionSnapshot()
        {
          compositionState_.promoteCurrentSnapshot();
        }

      private:
        template <class T> NodeState<T> dangerouslyUseStateWithValue(const T &initial)
        {
          loka::core::MutableState<T> *state = new loka::core::MutableState<T>(initial);
          adoptState(state);
          return NodeState<T>(state, this->tracker(), this);
        }

        void clearOwnedStates()
        {
          for (size_t i = 0; i < ownedStates_.size(); ++i)
          {
            loka::core::StateBase *state = ownedStates_[i];
            if (!state)
            {
              continue;
            }
            if (state->isArenaAllocated())
            {
              continue;
            }
            DestroyAdoptedHeapState(state);
          }
          ownedStates_.clear();
        }

        void retireSubtree(Node *node);
        void destroyRetiredSubtree(Node *node);
        void drainAllRetiredSubtrees();
        void releaseOwnedNodeStorage();

        void parkBranch(const BoundaryParkedBranchKey &key, Node *branch, unsigned arm)
        {
          this->parkedBranches_.park(key, branch, arm);
        }
        Node *takeParkedBranch(const BoundaryParkedBranchKey &key, unsigned arm)
        {
          return this->parkedBranches_.take(key, arm);
        }
        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        BoundaryRuntimeState runtimeState_;
        BoundaryUpdateState updateState_;
        BoundaryCompositionState compositionState_;
        BoundaryObservedState observedState_;
        BoundaryParkedBranchLedger parkedBranches_;
        BoundaryBranchSeatState branchSeats_;
        NodeArena nodeArena_;
        StateArena stateArena_;
        loka::core::HoldLedger holdLedger_;
        loka::core::detail::HeldBlockBase *pendingHeldReleasesHead_;
        loka::core::detail::HeldBlockBase *pendingHeldReleasesTail_;
        Node *retiredSubtreesHead_;
        Node *retiredSubtreesTail_;
        std::vector<detail::NodeArena::RetiredNodeGeneration> retiredGenerations_;
        bool drainingRetiredSubtrees_;

        friend class ::loka::dsl::testing::OwnershipDump;

      };

      template <class PropsT, class NodeT> struct BoundaryDefinition : public NodeDefinition<PropsT, NodeT>
      {
        typedef NodeDefinition<PropsT, NodeT> BaseType;
        typedef int IsBoundaryDefinition;
        BoundaryDefinition()
            : BaseType()
        {
        }
        BoundaryDefinition(const PropsT &p)
            : BaseType(p)
        {
        }
        virtual NodeDefinitionBase *clone() const
        {
          return new BoundaryDefinition(*this);
        }
        virtual bool isBoundary() const
        {
          return true;
        }
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_BOUNDARY_HPP
