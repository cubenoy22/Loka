#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "../PlatformController.hpp"
#include "ComposableNode.hpp"
#include "../BoundState.hpp"
#include "../ComponentContext.hpp"
#include "../PlatformApplyPlan.hpp"
#include "BoundaryApplyInfo.hpp"
#include "BoundaryCompositionState.hpp"
#include "BoundaryObservedState.hpp"
#include "BoundaryRuntimeState.hpp"
#include "BoundaryStateTypes.hpp"
#include "loka/core/Managed.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/core/util/StateUtil.hpp"
#include "loka/core/Profiler.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;

      // NodeArena: pre-allocated memory block for batch node creation
      class NodeArena
      {
      public:
        NodeArena() : buffer_(0), raw_(0), size_(0), offset_(0) {}
        ~NodeArena() { clear(); }

        static size_t normalizeAlign(size_t align)
        {
          size_t minAlign = sizeof(void *);
          if (minAlign < 2)
          {
            minAlign = 2;
          }
          if (align < minAlign)
          {
            align = minAlign;
          }
          if ((align & (align - 1)) != 0)
          {
            size_t p2 = 1;
            while (p2 < align)
            {
              p2 <<= 1;
            }
            align = p2;
          }
          return align;
        }

        void reserve(size_t totalSize)
        {
          clear();
          if (totalSize > 0)
          {
            // Allocate with extra padding and align the base pointer.
            const size_t kArenaAlign = 16;
            size_t rawSize = totalSize + kArenaAlign;
            raw_ = new char[rawSize];
            size_t rawAddr = reinterpret_cast<size_t>(raw_);
            size_t alignedAddr = (rawAddr + (kArenaAlign - 1)) & ~(kArenaAlign - 1);
            buffer_ = reinterpret_cast<char *>(alignedAddr);
            size_ = rawSize - (alignedAddr - rawAddr);
            offset_ = 0;
          }
        }

        void *allocate(size_t size, size_t align)
        {
          if (!buffer_ || size == 0)
          {
            return 0;
          }
          align = normalizeAlign(align);
          // Align the offset
          size_t mask = align - 1;
          size_t aligned = (offset_ + mask) & ~mask;
          if (aligned + size > size_)
          {
            return 0; // Arena full
          }
          void *ptr = buffer_ + aligned;
          offset_ = aligned + size;
          return ptr;
        }

        void registerNode(Node *node)
        {
          if (node)
          {
            nodes_.push_back(node);
          }
        }

        void clear()
        {
          // Call destructors in creation order so parents can safely detach children.
          for (size_t i = 0; i < nodes_.size(); ++i)
          {
            nodes_[i]->~Node();
          }
          nodes_.clear();
          if (raw_)
          {
            delete[] raw_;
          }
          else
          {
            delete[] buffer_;
          }
          buffer_ = 0;
          raw_ = 0;
          size_ = 0;
          offset_ = 0;
        }

        bool hasCapacity() const { return buffer_ != 0; }

      private:
        char *buffer_;
        char *raw_;
        size_t size_;
        size_t offset_;
        std::vector<Node *> nodes_;
      };

      class StateArena
      {
      public:
        StateArena() : buffer_(0), raw_(0), size_(0), offset_(0), states_() {}
        ~StateArena() { clear(); }

        void reserve(size_t totalSize)
        {
          if (buffer_ || totalSize == 0)
          {
            return;
          }
          if (totalSize > 0)
          {
            const size_t kArenaAlign = 16;
            size_t rawSize = totalSize + kArenaAlign;
            raw_ = new char[rawSize];
            size_t rawAddr = reinterpret_cast<size_t>(raw_);
            size_t alignedAddr = (rawAddr + (kArenaAlign - 1)) & ~(kArenaAlign - 1);
            buffer_ = reinterpret_cast<char *>(alignedAddr);
            size_ = rawSize - (alignedAddr - rawAddr);
            offset_ = 0;
          }
        }

        void *allocate(size_t size, size_t align)
        {
          if (!buffer_ || size == 0)
          {
            return 0;
          }
          align = NodeArena::normalizeAlign(align);
          size_t mask = align - 1;
          size_t aligned = (offset_ + mask) & ~mask;
          if (aligned + size > size_)
          {
            return 0;
          }
          void *ptr = buffer_ + aligned;
          offset_ = aligned + size;
          return ptr;
        }

        void registerState(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          if (state && destroy)
          {
            StateEntry entry;
            entry.state = state;
            entry.destroy = destroy;
            states_.push_back(entry);
          }
        }

        void clear()
        {
          for (size_t i = 0; i < states_.size(); ++i)
          {
            if (states_[i].destroy)
            {
              states_[i].destroy(states_[i].state);
            }
          }
          states_.clear();
          if (raw_)
          {
            delete[] raw_;
          }
          else
          {
            delete[] buffer_;
          }
          buffer_ = 0;
          raw_ = 0;
          size_ = 0;
          offset_ = 0;
        }

        bool hasCapacity() const { return buffer_ != 0; }

      private:
        struct StateEntry
        {
          StateEntry() : state(0), destroy(0) {}
          loka::core::StateBase *state;
          void (*destroy)(loka::core::StateBase *);
        };
        char *buffer_;
        char *raw_;
        size_t size_;
        size_t offset_;
        std::vector<StateEntry> states_;
      };

      // BoundaryNode: owns a local tracker for its subtree.
      class BoundaryNode : public ComposableNode, public IStateOwner
      {
      public:
        typedef BoundaryUpdateResult::BoundsHint LayoutBounds;
        typedef BoundaryLocalApplyPaintKind LocalApplyPaintKind;
        typedef BoundaryLocalApplyInfo LocalApplyInfo;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_NONE = scene::LOCAL_APPLY_PAINT_NONE;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_GENERIC = scene::LOCAL_APPLY_PAINT_GENERIC;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_OPAQUE = scene::LOCAL_APPLY_PAINT_OPAQUE;
        static const LocalApplyPaintKind LOCAL_APPLY_PAINT_COMPOSITED = scene::LOCAL_APPLY_PAINT_COMPOSITED;
        BoundaryNode() : ComposableNode(), tracker_(), runtimeState_(), updateState_(), compositionState_(), observedState_()
        {
          this->tracker_.setInvalidateCallback(&BoundaryNode::InvalidateSceneThunk, this);
        }
        virtual ~BoundaryNode()
        {
          clearObservedStateEntries();
          // Detach children before the arena is cleared to avoid touching freed nodes.
          clearChildren();
          nodeArena_.clear();
          clearOwnedStates();
          clearOwnedStateHandles();
          stateArena_.clear();
        }

        virtual BoundaryNode *asBoundary() { return this; }
        virtual IStateOwner *asStateOwner() { return this; }

        virtual loka::core::StateTracker *tracker() { return &tracker_; }
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
          this->updateState_.selectLocalOpaqueCoverageHint(info.isLocalPaintRoot,
                                                           info.hasOpaqueCoverageHint,
                                                           opaqueByHint);
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
          info.paintIsOpaque = info.paintKind == LOCAL_APPLY_PAINT_OPAQUE || info.paintKind == LOCAL_APPLY_PAINT_COMPOSITED;
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
        void setFrozen(bool frozen) { this->runtimeState_.setFrozen(frozen); }
        bool isFrozen() const { return this->runtimeState_.isFrozen(); }
        bool isApplyingPlatform() const { return this->updateState_.isApplying(); }
        bool isComposingPhase() const { return this->updateState_.isComposing(); }
        BoundaryComposePhaseScope beginComposePhaseScope() { return this->updateState_.beginComposeScope(); }
        BoundaryApplyPhaseScope beginApplyPhaseScope() { return this->updateState_.beginApplyScope(); }
        Scene *scene() const { return this->runtimeState_.currentScene(); }
        Scene *getScene() const
        {
          if (this->runtimeState_.hasScene())
          {
            return this->runtimeState_.currentScene();
          }
          return this->runtimeState_.currentParentBoundary() ? this->runtimeState_.currentParentBoundary()->getScene() : 0;
        }
        void setScene(Scene *scene) { this->runtimeState_.setScene(scene); }
        BoundaryNode *parentBoundary() const { return this->runtimeState_.currentParentBoundary(); }
        void setParentBoundary(BoundaryNode *parent) { this->runtimeState_.setParentBoundary(parent); }
        void setLayoutBounds(int x, int y, int width, int height)
        {
          const int normalizedWidth = width < 0 ? 0 : width;
          const int normalizedHeight = height < 0 ? 0 : height;
          const bool changed = this->runtimeState_.setLayoutBounds(x, y, normalizedWidth, normalizedHeight);
          updateState_.noteLayoutBoundsTransition(changed,
                                                 this->runtimeState_.hasParentBoundary(),
                                                 x,
                                                 y,
                                                 normalizedWidth,
                                                 normalizedHeight);
        }
        void clearLayoutBounds()
        {
          const bool changed = this->runtimeState_.clearLayoutBounds();
          updateState_.noteLayoutBoundsCleared(changed, this->runtimeState_.hasParentBoundary());
        }
        const LayoutBounds &layoutBounds() const { return this->runtimeState_.currentLayoutBounds(); }
        bool hasLayoutBounds() const { return this->runtimeState_.hasLayoutBounds(); }
        void clearObservedDirtyFlags() { observedState_.clearDirtyFlags(); }
        void addPendingDirtyFlags(NodeDirtyFlags flags)
        {
          updateState_.addPendingDirtyFlags(flags);
        }
        NodeDirtyFlags pendingDirtyFlags() const { return updateState_.pendingDirtyFlags(); }
        void clearPendingUpdateState() { updateState_.clearPending(); }
        bool isUpdateRequested() const { return updateState_.isRequested(); }
        void setUpdateRequested(bool value) { updateState_.setRequested(value); }
        BoundaryNode *nextPendingBoundary() const { return updateState_.nextPendingBoundary(); }
        void setNextPendingBoundary(BoundaryNode *next) { updateState_.setNextPendingBoundary(next); }
        BoundaryComposeResult &composeResult() { return compositionState_.composeResult(); }
        const BoundaryComposeResult &composeResult() const { return compositionState_.composeResult(); }
        BoundaryUpdateResult &updateResult() { return updateState_.updateResult(); }
        const BoundaryUpdateResult &updateResult() const { return updateState_.updateResult(); }
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
        void beginPlatformApply() { updateState_.beginApply(); }
        void endPlatformApply() { updateState_.endApply(); }
        void beginObservedStatePass() { observedState_.beginPass(); }
        void clearObservedStateEntries() { observedState_.clearEntries(&BoundaryNode::ObservedStateChangedThunk); }
        void addObservedDirtyFlags(NodeDirtyFlags flags)
        {
          observedState_.addDirtyFlags(flags);
        }
        void registerObservedState(loka::core::StateBase *state, NodeDirtyFlags flags)
        {
          observedState_.registerState(this, state, flags, &BoundaryNode::ObservedStateChangedThunk);
        }
        NodeDirtyFlags observedDirtyFlags() const { return observedState_.currentDirtyFlags(); }
        NodeDirtyFlags observedDirtyFlagsForCommittedStates() const
        {
          const loka::core::PushStateTracker *pushTracker = this->tracker_.asPushTracker();
          return observedState_.dirtyFlagsForCommittedStates(pushTracker);
        }

        NodeArena *nodeArena() { return &nodeArena_; }
        virtual void *allocateStateMemory(size_t size, size_t align) { return stateArena_.allocate(size, align); }
        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          stateArena_.registerState(state, destroy);
        }
        virtual void reserveStateArena(size_t totalSize)
        {
          if (!stateArena_.hasCapacity())
          {
            stateArena_.reserve(totalSize);
          }
        }

        static void InvalidateSceneThunk(void *userData);
        static void ObservedStateChangedThunk(void *userData);
        static void ObservedStateDeferredInvalidateThunk(void *userData);

        template <class T>
        BoundState<T> useState()
        {
          return useStateWithValue(T());
        }

        template <class T>
        BoundState<T> useState(const T &initial)
        {
          return useStateWithValue(initial);
        }

        template <class T>
        loka::core::Managed<loka::core::MutableState<T> > useManagedState()
        {
          return useManagedStateWithValue(T());
        }

        template <class T>
        loka::core::Managed<loka::core::MutableState<T> > useManagedState(const T &initial)
        {
          return useManagedStateWithValue(initial);
        }

        NodeCompositionTransaction &compositionTransaction() { return compositionState_.compositionTransaction(); }
        const NodeCompositionTransaction &compositionTransaction() const { return compositionState_.compositionTransaction(); }
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
        bool rebuildCompositionChildrenFromCurrentSnapshot(ComponentContext &context, std::vector<Node *> &retainedChildren)
        {
          INestable *root = compositionRootNestable();
          INestableDefinition *currentRoot = compositionState_.currentRootNestableDefinition();
          if (!root || !currentRoot)
          {
            return false;
          }

          BoundaryLocalRebuildPlan plan;
          if (!buildLocalRebuildPlan(context, *currentRoot, plan))
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
        bool hasLocalCompositionDiff() const
        {
          return compositionState_.hasLocalCompositionDiff();
        }
        bool canApplyLocalCompositionDiff() const
        {
          return compositionState_.canApplyLocalCompositionDiff();
        }
        bool canPreserveNativeContexts() const
        {
          return compositionState_.canPreserveNativeContexts();
        }
        NodeCompositionSnapshot &previousCompositionSnapshot() { return compositionState_.previousCompositionSnapshot(); }
        const NodeCompositionSnapshot &previousCompositionSnapshot() const { return compositionState_.previousCompositionSnapshot(); }
        NodeCompositionSnapshot &currentCompositionSnapshot() { return compositionState_.currentCompositionSnapshot(); }
        const NodeCompositionSnapshot &currentCompositionSnapshot() const { return compositionState_.currentCompositionSnapshot(); }

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

        virtual void applyPendingStructure(const PlatformApplyPlan &)
        {
        }

        virtual void applyPendingLayout(const PlatformApplyPlan &)
        {
        }

        virtual void applyPendingLocalPaint(const PlatformApplyPlan &)
        {
        }

        virtual void applyPendingOpaquePaint(const PlatformApplyPlan &plan)
        {
          this->applyPendingLocalPaint(plan);
        }

        virtual void applyPendingCompositedPaint(const PlatformApplyPlan &plan)
        {
          this->applyPendingLocalPaint(plan);
        }

        bool buildLocalRebuildPlan(ComponentContext &context,
                                   const INestableDefinition &currentRoot,
                                   BoundaryLocalRebuildPlan &plan)
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
              BoundaryLocalRebuildPlanEntry entry;
              entry.node = existing;
              entry.action = BoundaryLocalRebuildPlanEntry::ACTION_RETAIN;
              entry.tag = definition->nodeTag();
              plan.entries.push_back(entry);
            }
            else
            {
              NodeComposition composition;
              Node *created = composition.createNodeFromDefinition(definition);
              if (!created)
              {
                return false;
              }
              BoundaryLocalRebuildPlanEntry entry;
              entry.node = created;
              entry.previousNode = existing;
              entry.action = existing ? BoundaryLocalRebuildPlanEntry::ACTION_REPLACE : BoundaryLocalRebuildPlanEntry::ACTION_ATTACH;
              entry.tag = definition->nodeTag();
              plan.entries.push_back(entry);
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
              BoundaryLocalRebuildPlanEntry entry;
              entry.node = liveChild;
              entry.action = BoundaryLocalRebuildPlanEntry::ACTION_RETIRE;
              entry.tag = liveChild->nodeTag();
              plan.entries.push_back(entry);
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
            if (plan.entries[i].action == BoundaryLocalRebuildPlanEntry::ACTION_RETIRE)
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
            if (entry.action == BoundaryLocalRebuildPlanEntry::ACTION_ATTACH ||
                entry.action == BoundaryLocalRebuildPlanEntry::ACTION_REPLACE)
            {
              this->composeTree(entry.node, context, COMPOSE_EVENT_ATTACH, this);
            }
          }
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            BoundaryLocalRebuildPlanEntry &entry = plan.entries[i];
            if (entry.action == BoundaryLocalRebuildPlanEntry::ACTION_REPLACE && entry.previousNode)
            {
              this->composeTree(entry.previousNode, context, COMPOSE_EVENT_DETACH, this);
              if (context.platformController())
              {
                context.platformController()->releaseNodeContexts(entry.previousNode);
              }
              if (!entry.previousNode->isArenaAllocated())
              {
                delete entry.previousNode;
              }
            }
            else if (entry.action == BoundaryLocalRebuildPlanEntry::ACTION_RETIRE && entry.node)
            {
              this->composeTree(entry.node, context, COMPOSE_EVENT_DETACH, this);
              if (context.platformController())
              {
                context.platformController()->releaseNodeContexts(entry.node);
              }
              if (!entry.node->isArenaAllocated())
              {
                delete entry.node;
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

        virtual void reserveStates(size_t count)
        {
          ownedStates_.reserve(ownedStates_.size() + count);
          tracker_.reserveStates(count);
        }

        static void composeTree(Node *node, ComponentContext &parentContext, ComposeEvent event, BoundaryNode *currentBoundary)
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

          BoundaryNode *nextBoundary = currentBoundary;
          if (boundary)
          {
            boundary->setParentBoundary(currentBoundary);
            if (currentBoundary)
            {
              boundary->setScene(currentBoundary->getScene());
            }
            boundary->clearObservedDirtyFlags();
            boundary->clearPhaseResults();
            boundary->beginObservedStatePass();
            nextBoundary = boundary;
          }
          BoundaryComposePhaseScope composeScope = boundary
                                                       ? boundary->beginComposePhaseScope()
                                                       : BoundaryComposePhaseScope(0);
          if (boundary)
          {
            boundary->beginComposeResult(event, parentContext.dirtyFlags());
          }
          if (nextBoundary)
          {
            nextBoundary->noteLocalPaintWork();
            class LocalObservedStateRegistrar : public ObservedStateRegistrar
            {
            public:
              explicit LocalObservedStateRegistrar(BoundaryNode *boundary)
                  : boundary_(boundary) {}

              virtual void observe(loka::core::StateBase *state, NodeDirtyFlags flags)
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
            LocalObservedStateRegistrar registrar(nextBoundary);
            node->declareObservedStates(registrar);
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
            composeTree(child, *contextForChildren, event, nextBoundary);
          }
        }

        void registerState(loka::core::StateBase *state)
        {
          tracker_.addState(state);
        }

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

        void rebuildCompositionTransactionFromSnapshots()
        {
          compositionState_.rebuildTransaction();
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

        template <class T>
        struct StateHandle : public StateHandleBase
        {
          loka::core::Managed<loka::core::MutableState<T> > handle;
          explicit StateHandle(const loka::core::Managed<loka::core::MutableState<T> > &h) : handle(h) {}
          loka::core::StateBase *state() const { return handle.get(); }
        };

        template <class T>
        BoundState<T> useStateWithValue(const T &initial)
        {
          loka::core::MutableState<T> *state = new loka::core::MutableState<T>(initial);
          adoptState(state);
          return BoundState<T>(state, this->tracker(), this);
        }

        template <class T>
        loka::core::Managed<loka::core::MutableState<T> > useManagedStateWithValue(const T &initial)
        {
          loka::core::Managed<loka::core::MutableState<T> > handle = loka::core::Managed<loka::core::MutableState<T> >::Wrap(new loka::core::MutableState<T>(initial));
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

        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
        BoundaryRuntimeState runtimeState_;
        BoundaryUpdateState updateState_;
        BoundaryCompositionState compositionState_;
        BoundaryObservedState observedState_;
        NodeArena nodeArena_;
        StateArena stateArena_;
      };

      template <class PropsT, class NodeT>
      struct BoundaryDefinition : public NodeDefinition<PropsT, NodeT>
      {
        typedef NodeDefinition<PropsT, NodeT> BaseType;
        typedef int IsBoundaryDefinition;
        BoundaryDefinition() : BaseType() {}
        BoundaryDefinition(const PropsT &p) : BaseType(p) {}
        virtual NodeDefinitionBase *clone() const { return new BoundaryDefinition(*this); }
        virtual bool isBoundary() const { return true; }
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
