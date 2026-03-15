#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "ComposableNode.hpp"
#include "../BoundState.hpp"
#include "../ComponentContext.hpp"
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

        BoundaryNode() : ComposableNode(), tracker_(), scene_(0), parentBoundary_(0), layoutBounds_(), observedDirtyFlags_(NODE_DIRTY_NONE)
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
        void markViewDirty(NodeDirtyFlags flags);
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
          layoutBounds_.x = x;
          layoutBounds_.y = y;
          layoutBounds_.width = width < 0 ? 0 : width;
          layoutBounds_.height = height < 0 ? 0 : height;
          layoutBounds_.valid = true;
        }
        void clearLayoutBounds() { layoutBounds_ = LayoutBounds(); }
        const LayoutBounds &layoutBounds() const { return layoutBounds_; }
        bool hasLayoutBounds() const { return layoutBounds_.valid; }
        void clearObservedDirtyFlags() { observedDirtyFlags_ = NODE_DIRTY_NONE; }
        void clearObservedStateEntries()
        {
          for (size_t i = 0; i < observedStateEntries_.size(); ++i)
          {
            ObservedStateEntry &entry = observedStateEntries_[i];
            if (entry.state && entry.binding)
            {
              entry.binding->boundary = 0;
              entry.binding->state = 0;
              entry.binding->flags = NODE_DIRTY_NONE;
              entry.binding = 0;
            }
          }
          observedStateEntries_.clear();
        }
        void addObservedDirtyFlags(NodeDirtyFlags flags)
        {
          if (flags == NODE_DIRTY_NONE)
          {
            return;
          }
          observedDirtyFlags_ = static_cast<NodeDirtyFlags>(observedDirtyFlags_ | flags);
        }
        void registerObservedState(loka::core::StateBase *state, NodeDirtyFlags flags)
        {
          if (!state || flags == NODE_DIRTY_NONE)
          {
            return;
          }
          this->addObservedDirtyFlags(flags);
          for (size_t i = 0; i < observedStateEntries_.size(); ++i)
          {
            if (observedStateEntries_[i].state == state)
            {
              observedStateEntries_[i].flags = static_cast<NodeDirtyFlags>(observedStateEntries_[i].flags | flags);
              if (observedStateEntries_[i].binding)
              {
                observedStateEntries_[i].binding->flags = observedStateEntries_[i].flags;
              }
              return;
            }
          }
          ObservedStateEntry entry;
          entry.state = state;
          entry.flags = flags;
          entry.binding = new ObservedStateBinding();
          entry.binding->boundary = this;
          entry.binding->state = state;
          entry.binding->flags = flags;
          state->deferBind(&BoundaryNode::ObservedStateChangedThunk, entry.binding);
          observedStateEntries_.push_back(entry);
        }
        NodeDirtyFlags observedDirtyFlags() const { return observedDirtyFlags_; }
        NodeDirtyFlags observedDirtyFlagsForCommittedStates() const
        {
          NodeDirtyFlags flags = NODE_DIRTY_NONE;
          const loka::core::PushStateTracker *pushTracker = this->tracker_.asPushTracker();
          if (!pushTracker)
          {
            return flags;
          }
          const std::vector<loka::core::StateBase *> &dirtyStates = pushTracker->committedDirtyStates();
          for (size_t stateIndex = 0; stateIndex < dirtyStates.size(); ++stateIndex)
          {
            loka::core::StateBase *dirtyState = dirtyStates[stateIndex];
            for (size_t entryIndex = 0; entryIndex < observedStateEntries_.size(); ++entryIndex)
            {
              if (observedStateEntries_[entryIndex].state == dirtyState)
              {
                flags = static_cast<NodeDirtyFlags>(flags | observedStateEntries_[entryIndex].flags);
              }
            }
          }
          return flags;
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

      protected:
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
            boundary->clearObservedStateEntries();
            nextBoundary = boundary;
          }
          if (nextBoundary)
          {
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

        struct ObservedStateBinding
        {
          ObservedStateBinding() : boundary(0), state(0), flags(NODE_DIRTY_NONE) {}
          BoundaryNode *boundary;
          loka::core::StateBase *state;
          NodeDirtyFlags flags;
        };

        struct ObservedStateEntry
        {
          ObservedStateEntry() : state(0), flags(NODE_DIRTY_NONE), binding(0) {}
          loka::core::StateBase *state;
          NodeDirtyFlags flags;
          ObservedStateBinding *binding;
        };

        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
        Scene *scene_;
        BoundaryNode *parentBoundary_;
        LayoutBounds layoutBounds_;
        NodeDirtyFlags observedDirtyFlags_;
        std::vector<ObservedStateEntry> observedStateEntries_;
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
