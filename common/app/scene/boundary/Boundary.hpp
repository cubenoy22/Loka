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
              retiredSubtreesHead_(0),
              retiredSubtreesTail_(0),
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
          return compositionState_.findCurrentDefinitionByTag(tag);
        }
        NodeDefinitionBase *currentCompositionRootDefinition() const
        {
          return compositionState_.currentCompositionSnapshot().root();
        }
        bool applyCurrentDefinitionPropsToLiveChild(NodeTag tag)
        {
          Node *liveChild = findCompositionChildByTag(tag);
          NodeDefinitionBase *definition = findCurrentCompositionDefinitionByTag(tag);
          if (!liveChild || !definition)
          {
            return false;
          }
          return definition->applyPropsToNode(liveChild);
        }
        bool applyCurrentRootDefinitionPropsToLiveRoot()
        {
          Node *liveRoot = compositionRootNode();
          NodeDefinitionBase *definition = currentCompositionRootDefinition();
          if (!liveRoot || !definition)
          {
            return false;
          }
          if (compositionRootNestable() || definition->asNestableDefinition())
          {
            return false;
          }
          return definition->applyPropsToNode(liveRoot);
        }
        bool rebuildCompositionChildrenFromCurrentSnapshot(ComponentContext &context,
                                                           std::vector<Node *> &retainedChildren)
        {
          INestable *root = compositionRootNestable();
          INestableDefinition *currentRoot = compositionState_.currentRootNestableDefinition();
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
          if (context.platformController())
          {
            context.platformController()->releaseNodeContexts(liveRoot);
          }
          if (!liveRoot->isArenaAllocated())
          {
            delete liveRoot;
          }
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

          NodeDefinitionBase *definition = currentRoot.childrenHead();
          while (definition)
          {
            Node *existing = findCompositionChildByTag(definition->nodeTag());
            if (existing && definition->isCompatibleWithNode(existing))
            {
              plan.entries.push_back(BoundaryLocalRebuildPlanEntry::retain(existing, definition->nodeTag()));
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
                  existing ? BoundaryLocalRebuildPlanEntry::replace(created, existing, definition->nodeTag())
                           : BoundaryLocalRebuildPlanEntry::attach(created, definition->nodeTag()));
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
            if (findCurrentCompositionDefinitionByTag(liveChild->nodeTag()) == 0)
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
              NodeDefinitionBase *retainedDefinition = findCurrentCompositionDefinitionByTag(plan.entries[i].tag);
              if (retainedDefinition && !retainedDefinition->applyPropsToNode(plan.entries[i].node))
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
              if (context.platformController())
              {
                context.platformController()->releaseNodeContexts(detachedNode);
              }
              if (!detachedNode->isArenaAllocated())
              {
                delete detachedNode;
              }
              else
              {
                this->retireArenaSubtree(detachedNode);
              }
            }
          }
          return true;
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
          if (event == COMPOSE_EVENT_ATTACH)
          {
            node->onCompositionAttached();
          }
          else if (event == COMPOSE_EVENT_DETACH)
          {
            node->onCompositionDetached();
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

        void retireArenaSubtree(Node *node);
        void destroyRetiredSubtree(Node *node);
        void drainAllRetiredSubtrees();
        void releaseOwnedNodeStorage();

        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
        BoundaryRuntimeState runtimeState_;
        BoundaryUpdateState updateState_;
        BoundaryCompositionState compositionState_;
        BoundaryObservedState observedState_;
        NodeArena nodeArena_;
        StateArena stateArena_;
        Node *retiredSubtreesHead_;
        Node *retiredSubtreesTail_;
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
