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
#include "core/Managed.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateUtil.hpp"
#include "core/Profiler.hpp"
#include "platform/debug/DebugLog.hpp"

namespace loka
{
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
              retiredSubtreesHead_(0),
              retiredSubtreesTail_(0),
              retiredGenerations_(),
              drainingRetiredSubtrees_(false)
        {
          this->tracker_.setInvalidateCallback(&BoundaryNode::InvalidateSceneThunk, this);
        }
        virtual ~BoundaryNode()
        {
          clearObservedStateEntries();
          this->releaseOwnedNodeStorage();
          releaseNodeStateRegistrations();
          clearOwnedStates();
          clearOwnedStateHandles();
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
        void completeComposeResult(bool preservedNativeContexts)
        {
          compositionState_.completeCompose(preservedNativeContexts);
        }
        void clearPhaseResults()
        {
          compositionState_.clearResult();
          updateState_.clearResult();
        }
        void noteLocalPaintWork()
        {
          assert(updateState_.canMutateLocalPaintMetadata());
          updateState_.noteLocalPaintWork();
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
        void registerBranchSeatConditionSources()
        {
          const std::vector<BoundaryBranchSeatPlanEntry> &plans = this->branchSeats_.plans();
          for (size_t i = 0; i < plans.size(); ++i)
          {
            if (!plans[i].condition)
            {
              continue;
            }
            this->registerObservedState(
                plans[i].condition,
                static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_LAYOUT));
          }
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
                                            Node *active,
                                            bool condition)
        {
          this->branchSeats_.registerRuntime(plan, parent, active, condition);
        }
        void appendNestedBranchSeatPlan(NodeComposition &composition)
        {
          composition.assignCompositionSeatSlots();
          this->branchSeats_.append(composition.root());
        }
        bool evaluateBranchSeatsForScheduledApply(ComponentContext &context)
        {
          return this->applyCurrentBranchSeatPlan(context);
        }
        /** Reclaims the queue snapshot owned by this Boundary at the head of
            the next tracker run. Retirees added while draining wait for a
            later tracker run. */
        void drainRetiredSubtreesAtNextTrackerRun();
        virtual void *allocateStateMemory(size_t size, size_t align)
        {
          return stateArena_.allocate(size, align);
        }
        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          stateArena_.registerState(state, destroy);
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

        template <class T> loka::core::Managed<loka::core::MutableState<T> > dangerouslyUseManagedState()
        {
          return dangerouslyUseManagedStateWithValue(T());
        }

        template <class T>
        loka::core::Managed<loka::core::MutableState<T> > dangerouslyUseManagedState(const T &initial)
        {
          return dangerouslyUseManagedStateWithValue(initial);
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
        bool applyRetainFastPathDefinitions()
        {
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
            const bool applied = entry->equivalentProps
                                     ? definition->repointRetainedNodeDefinition(liveNode)
                                     : definition->applyPropsToNode(liveNode);
            if (!applied)
            {
              return false;
            }
          }
          return true;
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
          if (!buildLocalRebuildPlan(*currentRoot, plan))
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

          NodeComposition composition;
          Node *created = composition.createNodeFromDefinition(currentRoot);
          if (!created)
          {
            return false;
          }
          if (!this->replaceChild(liveRoot, created))
          {
            delete created;
            return false;
          }
          this->composeTree(created, context, COMPOSE_EVENT_ATTACH, this);
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
          if (!this->applyCurrentBranchSeatPlan(context))
          {
            return false;
          }
          this->rebuildCurrentCompositionDiff();

          if (mode == LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS)
          {
            if (!this->canApplyLocalCompositionDiff())
            {
              return false;
            }
            if (this->localCompositionDiff()->isCompatibleRetainOnly())
            {
              if (!this->applyRetainFastPathDefinitions())
              {
                return false;
              }
              this->promoteCurrentCompositionSnapshot();
              loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
              for (Node *child = it.next(); child; child = it.next())
              {
                this->composeTree(child, context, event, this);
              }
              return true;
            }
          }

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

        bool buildLocalRebuildPlan(const INestableDefinition &currentRoot, BoundaryLocalRebuildPlan &plan)
        {
          // This translates the current desired child set into a concrete
          // boundary-local apply plan. It intentionally stays one level above
          // raw NodeCompositionDiff entries because apply needs live-node and
          // ownership details such as previousNode for replacement cleanup.
          plan.clear();
          plan.reserve(currentRoot.childrenCount());

          const NodeCompositionDiff *diff = this->localCompositionDiff();
          NodeCompositionDiff::Entry *singleEntry = diff ? diff->entriesHead() : 0;
          NodeDefinitionBase *definition = currentRoot.childrenHead();
          while (definition)
          {
            const BoundaryBranchSeatPlanEntry *seatPlan = this->branchSeatPlan(definition);
            BoundaryBranchSeatRuntimeEntry *seatRuntime =
                seatPlan ? this->branchSeats_.findRuntime(seatPlan->key) : 0;
            Node *existing = seatPlan
                                 ? (seatRuntime ? seatRuntime->active : 0)
                                 : findCompositionChildByTag(definition->nodeTag());
            if (!seatPlan && !existing && definition->nodeTag() == NODE_TAG_NONE &&
                diff && !diff->fullRebuild && diff->entryCount() == 1 && singleEntry &&
                singleEntry->action == NodeCompositionDiff::ACTION_RETAIN &&
                singleEntry->compatibleType && singleEntry->previousIndex == 0 &&
                singleEntry->currentIndex == 0)
            {
              INestable *root = compositionRootNestable();
              existing = root && root->childrenCount() == 1 ? root->childrenHead() : 0;
            }
            if (existing && (seatPlan || definition->isCompatibleWithNode(existing)))
            {
              plan.entries.push_back(
                  BoundaryLocalRebuildPlanEntry::retain(existing, definition, definition->nodeTag()));
            }
            else
            {
              NodeComposition composition;
              Node *created = composition.createNodeFromDefinition(definition);
              if (!created)
              {
                return false;
              }
              plan.entries.push_back(
                  existing ? BoundaryLocalRebuildPlanEntry::replace(
                                 created, existing, definition, definition->nodeTag())
                           : BoundaryLocalRebuildPlanEntry::attach(
                                 created, definition, definition->nodeTag()));
            }
            definition = definition->nextInComposition;
          }

          INestable *root = compositionRootNestable();
          if (!root)
          {
            return false;
          }
          loka::dsl::CompositionCursor<Node> it(root->childrenHead(), root->childrenCount());
          for (Node *liveChild = it.next(); liveChild; liveChild = it.next())
          {
            bool retainedByPlan = false;
            for (size_t i = 0; i < plan.entries.size(); ++i)
            {
              if (plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RETAIN &&
                  plan.entries[i].node == liveChild)
              {
                retainedByPlan = true;
                break;
              }
            }
            if (!retainedByPlan && findCurrentCompositionDefinitionByTag(liveChild->nodeTag()) == 0)
            {
              plan.entries.push_back(BoundaryLocalRebuildPlanEntry::retire(liveChild, liveChild->nodeTag()));
            }
          }
          return true;
        }
        bool applyLocalRebuildPlan(ComponentContext &context,
                                   INestable &root,
                                   BoundaryLocalRebuildPlan &plan,
                                   std::vector<Node *> &retainedChildren)
        {
          std::vector<Node *> detachedChildren;
          root.detachChildrenTo(detachedChildren);
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            if (!plan.entries[i].keepsLiveNode())
            {
              continue;
            }
            if (plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RETAIN)
            {
              NodeDefinitionBase *retainedDefinition = plan.entries[i].definition;
              if (!retainedDefinition ||
                  (!retainedDefinition->asBranchSeatDefinition() &&
                   !retainedDefinition->applyPropsToNode(plan.entries[i].node)))
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

        bool buildParkedBranchReentryPlan(INestable &root,
                                          INestableDefinition &desiredRoot,
                                          BoundaryLocalRebuildPlan &plan)
        {
          plan.clear();
          plan.reserve(desiredRoot.childrenCount());

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
            const BoundaryBranchSeatPlanEntry *seatPlan = this->branchSeatPlan(definition);
            BoundaryBranchSeatRuntimeEntry *seatRuntime =
                seatPlan ? this->branchSeats_.findRuntime(seatPlan->key) : 0;
            Node *existing = seatRuntime ? seatRuntime->active : 0;
            if (!seatPlan && !existing && definition->nodeTag() != NODE_TAG_NONE)
            {
              for (size_t i = 0; i < liveChildren.size(); ++i)
              {
                if (liveChildren[i] && liveChildren[i]->nodeTag() == definition->nodeTag())
                {
                  existing = liveChildren[i];
                  break;
                }
              }
            }
            else if (!seatPlan && !existing && slot < liveChildren.size() &&
                     liveChildren[slot] && liveChildren[slot]->nodeTag() == NODE_TAG_NONE)
            {
              existing = liveChildren[slot];
            }

            if (existing && (seatPlan || definition->isCompatibleWithNode(existing)))
            {
              plan.entries.push_back(
                  BoundaryLocalRebuildPlanEntry::retain(existing, definition, definition->nodeTag()));
            }
            else
            {
              NodeComposition composition;
              Node *created = composition.createNodeFromDefinition(definition);
              if (!created)
              {
                return false;
              }
              plan.entries.push_back(
                  existing ? BoundaryLocalRebuildPlanEntry::replace(
                                 created, existing, definition, definition->nodeTag())
                           : BoundaryLocalRebuildPlanEntry::attach(
                                 created, definition, definition->nodeTag()));
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
          if (!this->buildParkedBranchReentryPlan(*root, *desiredRoot, plan))
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
            if (entry.action == BoundaryLocalRebuildPlanEntry::ACTION_RETAIN &&
                !this->reconcileParkedBranch(context, entry.node, entry.definition))
            {
              return false;
            }
          }
          return true;
        }

        bool createCurrentBranch(ComponentContext &context,
                                 const BoundaryBranchSeatPlanEntry &plan,
                                 bool condition,
                                 Node *parent,
                                 Node *&created)
        {
          created = 0;
          NodeDefinitionBase *definition = plan.branch(condition).definition;
          if (!definition)
          {
            return true;
          }
          NodeComposition composition;
          composition.setContext(&context);
          created = composition.createNodeFromDefinition(definition);
          return created != 0;
        }

        void retireSeatBranch(ComponentContext &context,
                              const BoundaryParkedBranchKey &key,
                              bool condition,
                              Node *branch)
        {
          if (!branch)
          {
            return;
          }
          this->branchSeats_.eraseOwnedBranch(key, condition);
          this->composeTree(branch, context, COMPOSE_EVENT_DETACH, this);
          this->retireDetachedNode(context, branch);
        }

        bool replaceSeatBranch(ComponentContext &context,
                               const BoundaryBranchSeatPlanEntry &plan,
                               BoundaryBranchSeatRuntimeEntry &runtime,
                               bool nextCondition,
                               bool parkOutgoing)
        {
          const BoundaryParkedBranchKey runtimeKey = runtime.key;
          Node *runtimeParent = runtime.parent;
          Node *outgoing = runtime.active;
          const bool outgoingCondition = runtime.activeCondition;
          Node *incoming = this->takeParkedBranch(plan.key);
          NodeDefinitionBase *definition = plan.branch(nextCondition).definition;
          if (incoming &&
              (!definition || !definition->isCompatibleWithNode(incoming) ||
               !this->reconcileParkedBranch(context, incoming, definition)))
          {
            this->retireSeatBranch(context, plan.key, nextCondition, incoming);
            incoming = 0;
          }
          if (!incoming && !this->createCurrentBranch(context,
                                                      plan,
                                                      nextCondition,
                                                      runtimeParent,
                                                      incoming))
          {
            return false;
          }
          if (!incoming)
          {
            return false;
          }

          INestable *parent = runtimeParent ? runtimeParent->asNestable() : 0;
          if (!parent || !parent->replaceChild(outgoing, incoming))
          {
            return false;
          }

          if (outgoing)
          {
            if (parkOutgoing)
            {
              NotifySubtreeNodeDetached(outgoing);
              this->parkBranch(plan.key, outgoing, outgoingCondition);
            }
            else
            {
              this->retireSeatBranch(context,
                                     plan.key,
                                     outgoingCondition,
                                     outgoing);
            }
          }
          BoundaryBranchSeatRuntimeEntry *committedRuntime =
              this->branchSeats_.findRuntime(runtimeKey);
          assert(committedRuntime &&
                 "replacing a branch must preserve its definition-side seat mapping");
          if (!committedRuntime)
          {
            return false;
          }
          committedRuntime->active = incoming;
          committedRuntime->activeCondition = nextCondition;
          committedRuntime->appliedGeneration = this->branchSeats_.generation();
          incoming->markPendingAttachForCompose();
          return true;
        }

        bool applyBranchSeat(ComponentContext &context,
                             NodeDefinitionBase *definition)
        {
          const BoundaryBranchSeatPlanEntry *plan = this->branchSeatPlan(definition);
          if (!plan || !plan->condition)
          {
            return false;
          }
          BoundaryBranchSeatRuntimeEntry *runtime = this->branchSeats_.findRuntime(plan->key);
          if (!runtime || !runtime->active)
          {
            return false;
          }
          if (!this->branchSeats_.isLive(*runtime))
          {
            return true;
          }

          const bool condition = plan->condition->get();
          if (condition != runtime->activeCondition)
          {
            const bool parkOutgoing =
                !plan->branch(runtime->activeCondition).policies.destroyOnDetach;
            return this->replaceSeatBranch(context,
                                           *plan,
                                           *runtime,
                                           condition,
                                           parkOutgoing);
          }

          if (runtime->appliedGeneration == this->branchSeats_.generation())
          {
            return true;
          }
          NodeDefinitionBase *branchDefinition = plan->branch(condition).definition;
          if (branchDefinition &&
              this->reconcileParkedBranch(context, runtime->active, branchDefinition))
          {
            runtime->appliedGeneration = this->branchSeats_.generation();
            return true;
          }
          return this->replaceSeatBranch(context,
                                         *plan,
                                         *runtime,
                                         condition,
                                         false);
        }

        bool applyCurrentBranchSeatPlan(ComponentContext &context)
        {
          const std::vector<BoundaryBranchSeatPlanEntry> &plans = this->branchSeats_.plans();
          for (size_t i = 0; i < plans.size(); ++i)
          {
            BoundaryBranchSeatRuntimeEntry *runtime = this->branchSeats_.findRuntime(plans[i].key);
            if (!runtime || !this->branchSeats_.isLive(*runtime))
            {
              continue;
            }
            NodeDefinitionBase *definition = this->findBranchSeatDefinition(plans[i].key);
            if (!definition || !this->applyBranchSeat(context, definition))
            {
              return false;
            }
          }
          for (unsigned i = 0; BoundaryParkedBranchLedger::Entry *parked = this->parkedBranches_.entry(i); ++i)
          {
            BoundaryBranchSeatPlanEntry *plan = this->branchSeats_.findPlan(parked->key);
            if (!plan || !plan->branch(parked->condition).policies.deliverWhileDetached)
            {
              continue;
            }
            NodeDefinitionBase *branchDefinition = plan->branch(parked->condition).definition;
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
            NodeDefinitionBase *found =
                this->findBranchSeatDefinitionRecursive(seat->branchDefinition(false), key);
            return found ? found
                         : this->findBranchSeatDefinitionRecursive(seat->branchDefinition(true), key);
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
          delete state;
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
              boundary->registerBranchSeatConditionSources();
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
                boundary_->registerState(state);
                boundary_->registerObservedState(state, flags);
              }

            private:
              BoundaryNode *boundary_;
            };
            LocalDirtySourceRegistrar registrar(nextBoundary);
            node->declareDirtySources(registrar);
          }
          ComponentContext *contextForChildren = &parentContext;
          ComponentContext nodeContext(&parentContext);
          {
            PROFILE_SECTION("ctx");
            nodeContext.setStateOwner(parentContext.stateOwner());
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
          if (!nestable)
          {
            return;
          }
          loka::dsl::CompositionCursor<Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            ComposeEvent childEvent = child->resolveChildComposeEvent(event);
            composeTree(child, *contextForChildren, childEvent, nextBoundary);
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
          this->registerBranchSeatConditionSources();
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
        struct StateHandleBase
        {
          virtual ~StateHandleBase() {}
          virtual loka::core::StateBase *state() const = 0;
        };

        template <class T> struct StateHandle : public StateHandleBase
        {
          loka::core::Managed<loka::core::MutableState<T> > handle;
          explicit StateHandle(const loka::core::Managed<loka::core::MutableState<T> > &h)
              : handle(h)
          {
          }
          loka::core::StateBase *state() const
          {
            return handle.get();
          }
        };

        template <class T> NodeState<T> dangerouslyUseStateWithValue(const T &initial)
        {
          loka::core::MutableState<T> *state = new loka::core::MutableState<T>(initial);
          adoptState(state);
          return NodeState<T>(state, this->tracker(), this);
        }

        template <class T>
        loka::core::Managed<loka::core::MutableState<T> > dangerouslyUseManagedStateWithValue(const T &initial)
        {
          loka::core::Managed<loka::core::MutableState<T> > handle =
              loka::core::Managed<loka::core::MutableState<T> >::Wrap(new loka::core::MutableState<T>(initial));
          StateHandleBase *entry = new StateHandle<T>(handle);
          ownedStateHandles_.push_back(entry);
          tracker_.addState(entry->state());
          return handle;
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
            delete state;
          }
          ownedStates_.clear();
        }

        void clearOwnedStateHandles()
        {
          for (size_t i = 0; i < ownedStateHandles_.size(); ++i)
          {
            delete ownedStateHandles_[i];
          }
          ownedStateHandles_.clear();
        }

        void retireSubtree(Node *node);
        void destroyRetiredSubtree(Node *node);
        void drainAllRetiredSubtrees();
        void releaseOwnedNodeStorage();

        void parkBranch(const BoundaryParkedBranchKey &key, Node *branch, bool condition)
        {
          this->parkedBranches_.park(key, branch, condition);
        }
        Node *takeParkedBranch(const BoundaryParkedBranchKey &key)
        {
          return this->parkedBranches_.take(key);
        }
        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
        BoundaryRuntimeState runtimeState_;
        BoundaryUpdateState updateState_;
        BoundaryCompositionState compositionState_;
        BoundaryObservedState observedState_;
        BoundaryParkedBranchLedger parkedBranches_;
        BoundaryBranchSeatState branchSeats_;
        NodeArena nodeArena_;
        StateArena stateArena_;
        Node *retiredSubtreesHead_;
        Node *retiredSubtreesTail_;
        std::vector<detail::NodeArena::RetiredNodeGeneration> retiredGenerations_;
        bool drainingRetiredSubtrees_;

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
