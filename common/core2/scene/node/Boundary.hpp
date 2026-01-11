#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "ComposableNode.hpp"
#include "../BoundState.hpp"
#include "../ComponentContext.hpp"
#include "core/Managed.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateUtil.hpp"
#include "core/Profiler.hpp"

using declara::core::ProfileTicks;
using declara::core::gTreeVirtTicks;
using declara::core::gTreeCtxTicks;
using declara::core::gTreeCompTicks;
using declara::core::gTreeNodeCount;

namespace declara
{
  namespace core
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

        BoundaryNode() : ComposableNode(), tracker_(), scene_(0), parentBoundary_(0), layoutBounds_() {}
        virtual ~BoundaryNode()
        {
          // Detach children before the arena is cleared to avoid touching freed nodes.
          clearChildren();
          clearOwnedStates();
          clearOwnedStateHandles();
        }

        virtual BoundaryNode *asBoundary() { return this; }
        virtual IStateOwner *asStateOwner() { return this; }

        virtual declara::core::StateTracker *tracker() { return &tracker_; }
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

        NodeArena *nodeArena() { return &nodeArena_; }

        static void InvalidateSceneThunk(void *userData);

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
        Managed<MutableState<T> > useManagedState()
        {
          return useManagedStateWithValue(T());
        }

        template <class T>
        Managed<MutableState<T> > useManagedState(const T &initial)
        {
          return useManagedStateWithValue(initial);
        }

      protected:
        virtual void adoptState(StateBase *state)
        {
          if (!state)
          {
            return;
          }
          ownedStates_.push_back(state);
          tracker_.addState(state);
        }

        virtual void adoptStateUnchecked(StateBase *state)
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
          ++gTreeNodeCount;
          long t0 = ProfileTicks();
          BoundaryNode *boundary = node->asBoundary();
          ComposableNode *composable = node->asComposable();
          INestable *nestable = node->asNestable();
          gTreeVirtTicks += ProfileTicks() - t0;

          BoundaryNode *nextBoundary = currentBoundary;
          if (boundary)
          {
            boundary->setParentBoundary(currentBoundary);
            if (currentBoundary)
            {
              boundary->setScene(currentBoundary->getScene());
            }
            nextBoundary = boundary;
          }
          ComponentContext *contextForChildren = &parentContext;
          t0 = ProfileTicks();
          ComponentContext nodeContext(&parentContext);
          nodeContext.setStateOwner(parentContext.stateOwner());
          nodeContext.setBoundary(nextBoundary);
          Scene *scene = nextBoundary ? nextBoundary->getScene() : 0;
          nodeContext.setScene(scene);
          nodeContext.setWindow(parentContext.window());
          nodeContext.setDirtyFlags(parentContext.dirtyFlags());
          gTreeCtxTicks += ProfileTicks() - t0;

          if (composable)
          {
            t0 = ProfileTicks();
            composable->compose(nodeContext, event);
            gTreeCompTicks += ProfileTicks() - t0;
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

        void registerState(StateBase *state)
        {
          tracker_.addState(state);
        }

        void registerStates(StateBase *first, ...)
        {
          StateVector states;
          va_list args;
          va_start(args, first);
          for (StateBase *s = first; s != 0; s = va_arg(args, StateBase *))
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
          virtual StateBase *state() const = 0;
        };

        template <class T>
        struct StateHandle : public StateHandleBase
        {
          Managed<MutableState<T> > handle;
          explicit StateHandle(const Managed<MutableState<T> > &h) : handle(h) {}
          StateBase *state() const { return handle.get(); }
        };

        template <class T>
        BoundState<T> useStateWithValue(const T &initial)
        {
          MutableState<T> *state = new MutableState<T>(initial);
          adoptState(state);
          return BoundState<T>(state, this->tracker());
        }

        template <class T>
        Managed<MutableState<T> > useManagedStateWithValue(const T &initial)
        {
          Managed<MutableState<T> > handle = Managed<MutableState<T> >::Wrap(new MutableState<T>(initial));
          StateHandleBase *entry = new StateHandle<T>(handle);
          ownedStateHandles_.push_back(entry);
          tracker_.addState(entry->state());
          return handle;
        }

        void clearOwnedStates()
        {
          for (size_t i = 0; i < ownedStates_.size(); ++i)
          {
            delete ownedStates_[i];
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

        declara::core::PushStateTracker tracker_;
        std::vector<StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
        Scene *scene_;
        BoundaryNode *parentBoundary_;
        LayoutBounds layoutBounds_;
        NodeArena nodeArena_;
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
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
