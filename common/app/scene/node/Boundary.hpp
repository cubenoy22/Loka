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
#include "BoundaryCompositionState.hpp"
#include "BoundaryObservedState.hpp"
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
        struct LayoutBounds
        {
          int x;
          int y;
          int width;
          int height;
          bool valid;
          LayoutBounds() : x(0), y(0), width(0), height(0), valid(false) {}
        };
        struct LocalRebuildPlanEntry
        {
          enum Action
          {
            ACTION_RETAIN = 0,
            ACTION_ATTACH = 1,
            ACTION_REPLACE = 2,
            ACTION_RETIRE = 3
          };
          LocalRebuildPlanEntry() : node(0), previousNode(0), action(ACTION_RETAIN), tag(NODE_TAG_NONE) {}
          Node *node;
          Node *previousNode;
          Action action;
          NodeTag tag;
        };
        struct LocalRebuildPlan
        {
          // Temporary apply plan derived from the current snapshot and live
          // children. NodeCompositionDiff remains the comparison summary;
          // LocalRebuildPlan is the boundary-local execution form.
          std::vector<LocalRebuildPlanEntry> entries;
          void reserve(size_t count)
          {
            entries.reserve(count);
          }
          void clear()
          {
            entries.clear();
          }
        };
        BoundaryNode() : ComposableNode(), tracker_(), scene_(0), parentBoundary_(0), layoutBounds_(), updateState_(), compositionState_(), frozen_(false), observedState_()
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
        virtual void applyPendingUpdate(const PlatformApplyPlan &) {}
        void markViewDirty(NodeDirtyFlags flags);
        void setFrozen(bool frozen) { this->frozen_ = frozen; }
        bool isFrozen() const { return this->frozen_; }
        bool isApplyingPlatform() const { return this->updateState_.phase.isApplying(); }
        bool isComposingPhase() const { return this->updateState_.phase.isComposing(); }
        BoundaryComposePhaseScope beginComposePhaseScope() { return BoundaryComposePhaseScope(&this->updateState_.phase); }
        BoundaryApplyPhaseScope beginApplyPhaseScope() { return BoundaryApplyPhaseScope(&this->updateState_.phase); }
        Scene *scene() const { return scene_; }
        Scene *getScene() const
        {
          if (scene_)
          {
            return scene_;
          }
          return parentBoundary_ ? parentBoundary_->getScene() : 0;
        }
        void setScene(Scene *scene) { scene_ = scene; }
        BoundaryNode *parentBoundary() const { return parentBoundary_; }
        void setParentBoundary(BoundaryNode *parent) { parentBoundary_ = parent; }
        void setLayoutBounds(int x, int y, int width, int height)
        {
          const int normalizedWidth = width < 0 ? 0 : width;
          const int normalizedHeight = height < 0 ? 0 : height;
          const bool changed = !layoutBounds_.valid ||
                               layoutBounds_.x != x ||
                               layoutBounds_.y != y ||
                               layoutBounds_.width != normalizedWidth ||
                               layoutBounds_.height != normalizedHeight;
          layoutBounds_.x = x;
          layoutBounds_.y = y;
          layoutBounds_.width = normalizedWidth;
          layoutBounds_.height = normalizedHeight;
          layoutBounds_.valid = true;
          if (changed)
          {
            updateState_.result.actualBoundsChanged = true;
            if (parentBoundary_)
            {
              updateState_.result.affectsAncestorLayout = true;
            }
          }
        }
        void clearLayoutBounds()
        {
          if (layoutBounds_.valid)
          {
            layoutBounds_ = LayoutBounds();
            updateState_.result.actualBoundsChanged = true;
            if (parentBoundary_)
            {
              updateState_.result.affectsAncestorLayout = true;
            }
            return;
          }
          layoutBounds_ = LayoutBounds();
        }
        const LayoutBounds &layoutBounds() const { return layoutBounds_; }
        bool hasLayoutBounds() const { return layoutBounds_.valid; }
        void clearObservedDirtyFlags() { observedState_.dirtyFlags = NODE_DIRTY_NONE; }
        void addPendingDirtyFlags(NodeDirtyFlags flags)
        {
          if (flags == NODE_DIRTY_NONE)
          {
            return;
          }
          updateState_.pending.dirtyFlags = static_cast<NodeDirtyFlags>(updateState_.pending.dirtyFlags | flags);
        }
        NodeDirtyFlags pendingDirtyFlags() const { return updateState_.pending.dirtyFlags; }
        void clearPendingUpdateState() { updateState_.clearPending(); }
        bool isUpdateRequested() const { return updateState_.pending.requested; }
        void setUpdateRequested(bool value) { updateState_.pending.requested = value; }
        BoundaryNode *nextPendingBoundary() const { return updateState_.pending.nextBoundary; }
        void setNextPendingBoundary(BoundaryNode *next) { updateState_.pending.nextBoundary = next; }
        BoundaryComposeResult &composeResult() { return compositionState_.result; }
        const BoundaryComposeResult &composeResult() const { return compositionState_.result; }
        BoundaryUpdateResult &updateResult() { return updateState_.result; }
        const BoundaryUpdateResult &updateResult() const { return updateState_.result; }
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
          assert(!updateState_.phase.isApplying());
          updateState_.result.paint.hasPaintWork = true;
        }
        void noteCompositedPaint()
        {
          assert(!updateState_.phase.isApplying());
          updateState_.result.paint.hasPaintWork = true;
          updateState_.result.paint.requiresCompositedPaint = true;
        }
        void beginPlatformApply() { updateState_.phase.beginApply(); }
        void endPlatformApply() { updateState_.phase.endApply(); }
        void beginObservedStatePass() { observedState_.beginPass(); }
        void clearObservedStateEntries() { observedState_.clearEntries(); }
        void addObservedDirtyFlags(NodeDirtyFlags flags)
        {
          observedState_.addDirtyFlags(flags);
        }
        void registerObservedState(loka::core::StateBase *state, NodeDirtyFlags flags)
        {
          observedState_.registerState(this, state, flags, &BoundaryNode::ObservedStateChangedThunk);
        }
        NodeDirtyFlags observedDirtyFlags() const { return observedState_.dirtyFlags; }
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

        NodeCompositionTransaction &compositionTransaction() { return compositionState_.transaction; }
        const NodeCompositionTransaction &compositionTransaction() const { return compositionState_.transaction; }
        const NodeCompositionDiff *localCompositionDiff() const
        {
          return compositionState_.transaction.diff().valid ? &compositionState_.transaction.diff() : 0;
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
        bool rebuildCompositionChildrenFromCurrentSnapshot(ComponentContext &context, std::vector<Node *> &retainedChildren)
        {
          INestable *root = compositionRootNestable();
          INestableDefinition *currentRoot = compositionState_.currentSnapshot.root()
                                                 ? compositionState_.currentSnapshot.root()->asNestableDefinition()
                                                 : 0;
          if (!root || !currentRoot)
          {
            return false;
          }

          LocalRebuildPlan plan;
          if (!buildLocalRebuildPlan(context, *currentRoot, plan))
          {
            return false;
          }
          return applyLocalRebuildPlan(context, *root, plan, retainedChildren);
        }
        bool hasLocalCompositionDiff() const
        {
          return localCompositionDiff() != 0;
        }
        bool canApplyLocalCompositionDiff() const
        {
          const NodeCompositionDiff *diff = localCompositionDiff();
          return diff != 0 && !diff->fullRebuild && !diff->empty() && !diff->hasIncompatibleRetain();
        }
        NodeCompositionSnapshot &previousCompositionSnapshot() { return compositionState_.previousSnapshot; }
        const NodeCompositionSnapshot &previousCompositionSnapshot() const { return compositionState_.previousSnapshot; }
        NodeCompositionSnapshot &currentCompositionSnapshot() { return compositionState_.currentSnapshot; }
        const NodeCompositionSnapshot &currentCompositionSnapshot() const { return compositionState_.currentSnapshot; }

      protected:
        bool buildLocalRebuildPlan(ComponentContext &context,
                                   const INestableDefinition &currentRoot,
                                   LocalRebuildPlan &plan)
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
              LocalRebuildPlanEntry entry;
              entry.node = existing;
              entry.action = LocalRebuildPlanEntry::ACTION_RETAIN;
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
              LocalRebuildPlanEntry entry;
              entry.node = created;
              entry.previousNode = existing;
              entry.action = existing ? LocalRebuildPlanEntry::ACTION_REPLACE : LocalRebuildPlanEntry::ACTION_ATTACH;
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
              LocalRebuildPlanEntry entry;
              entry.node = liveChild;
              entry.action = LocalRebuildPlanEntry::ACTION_RETIRE;
              entry.tag = liveChild->nodeTag();
              plan.entries.push_back(entry);
            }
          }
          return true;
        }
        bool applyLocalRebuildPlan(ComponentContext &context,
                                   INestable &root,
                                   LocalRebuildPlan &plan,
                                   std::vector<Node *> &retainedChildren)
        {
          std::vector<Node *> detachedChildren;
          root.detachChildrenTo(detachedChildren);
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            if (plan.entries[i].action == LocalRebuildPlanEntry::ACTION_RETIRE)
            {
              continue;
            }
            if (plan.entries[i].action == LocalRebuildPlanEntry::ACTION_RETAIN)
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
            LocalRebuildPlanEntry &entry = plan.entries[i];
            if (entry.action == LocalRebuildPlanEntry::ACTION_ATTACH ||
                entry.action == LocalRebuildPlanEntry::ACTION_REPLACE)
            {
              this->composeTree(entry.node, context, COMPOSE_EVENT_ATTACH, this);
            }
          }
          for (size_t i = 0; i < plan.entries.size(); ++i)
          {
            LocalRebuildPlanEntry &entry = plan.entries[i];
            if (entry.action == LocalRebuildPlanEntry::ACTION_REPLACE && entry.previousNode)
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
            else if (entry.action == LocalRebuildPlanEntry::ACTION_RETIRE && entry.node)
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
              boundary->completeComposeResult(boundary->canApplyLocalCompositionDiff());
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
        Scene *scene_;
        BoundaryNode *parentBoundary_;
        LayoutBounds layoutBounds_;
        BoundaryUpdateState updateState_;
        BoundaryCompositionState compositionState_;
        bool frozen_;
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
