#ifndef LOKA_CORE2_SCENE_NODE_HEADLESS_HPP
#define LOKA_CORE2_SCENE_NODE_HEADLESS_HPP

#include <vector>
#include "../NodeState.hpp"
#include "../StateOwner.hpp"
#include "../nodes/boundary/Boundary.hpp"
#include "ComposableNode.hpp"
#include "loka/core/StateTracker.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <class NodeT>
      struct HeadlessPropsForTypeTag
      {
      };

      template <class NodeT>
      struct HeadlessPropsFor : public NodePropsBase<HeadlessPropsFor<NodeT> >
      {
        typedef HeadlessPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
          {
            return false;
          }
          return false;
        }
      };

      template <class PropsT>
      class HeadlessNodeBase : public ComposableNode, public IStateOwner
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;

        HeadlessNodeBase(const PropsT &p)
            : ComposableNode(), props(p), tracker_(), ownedStates_(), composed_(false)
        {
          this->tracker_.setInvalidateCallback(&HeadlessNodeBase::InvalidateThunk, this);
        }

        virtual ~HeadlessNodeBase()
        {
          this->releaseNodeStateRegistrations();
          this->clearOwnedStates();
        }

        virtual IStateOwner *asStateOwner() { return this; }
        virtual loka::core::StateTracker *tracker() { return &this->tracker_; }
        virtual void composeNode(NodeComposition &c) { (void)c; }

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event == COMPOSE_EVENT_DETACH)
          {
            NodeComposition &composition = this->beginComposition(context);
            this->detachNode(composition);
            composed_ = false;
            return;
          }
          if (event == COMPOSE_EVENT_UPDATE)
          {
            if (!composed_)
            {
              return;
            }
            loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
            for (Node *child = it.next(); child; child = it.next())
            {
              ComposeBridge::Run(child, context, event, context.boundary());
            }
            return;
          }
          if (event != COMPOSE_EVENT_ATTACH)
          {
            return;
          }
          if (composed_)
          {
            return;
          }
          this->clearChildren();
          NodeComposition &composition = this->beginComposition(context);
          this->attachNode(composition);
          {
            NodeComposition::CompositionScope scope(composition);
            this->composeNode(composition);
          }
          context.setComposition(&composition);
          Node *child = composition.createNodeTree();
          if (child)
          {
            this->addChild(child);
            ComposeBridge::Run(child, context, event, context.boundary());
          }
          context.setComposition(0);
          composed_ = true;
        }

        virtual void adoptState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          this->ownedStates_.push_back(state);
          this->tracker_.addState(state);
        }

        virtual void adoptStateUnchecked(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          this->ownedStates_.push_back(state);
          this->tracker_.addStateUnchecked(state);
        }

        virtual void releaseState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          for (size_t i = 0; i < this->ownedStates_.size();)
          {
            if (this->ownedStates_[i] == state)
            {
              this->ownedStates_.erase(this->ownedStates_.begin() + i);
            }
            else
            {
              ++i;
            }
          }
          this->tracker_.removeState(state);
          if (!state->isArenaAllocated())
          {
            delete state;
          }
        }

        virtual void reserveStates(size_t count)
        {
          this->ownedStates_.reserve(this->ownedStates_.size() + count);
          this->tracker_.reserveStates(count);
        }

        virtual void reserveStateArena(size_t totalSize)
        {
          (void)totalSize;
        }

        virtual void *allocateStateMemory(size_t size, size_t align)
        {
          (void)size;
          (void)align;
          return 0;
        }

        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          (void)state;
          (void)destroy;
        }

      protected:
        void clearOwnedStates()
        {
          for (size_t i = 0; i < this->ownedStates_.size(); ++i)
          {
            delete this->ownedStates_[i];
          }
          this->ownedStates_.clear();
        }

      private:
        struct ComposeBridge : public BoundaryNode
        {
          static void Run(Node *node, ComponentContext &context, ComposeEvent event, BoundaryNode *currentBoundary)
          {
            BoundaryNode::composeTree(node, context, event, currentBoundary);
          }
        };

        static void InvalidateThunk(void *userData)
        {
          HeadlessNodeBase *self = static_cast<HeadlessNodeBase *>(userData);
          if (!self)
          {
            return;
          }
          const AttachedContext *attached = self->attachedContext();
          if (!attached || !attached->boundary())
          {
            return;
          }
          attached->boundary()->markViewDirty(NODE_DIRTY_CHILD);
        }

        loka::core::PushStateTracker tracker_;
        std::vector<loka::core::StateBase *> ownedStates_;
        bool composed_;
      };

      template <class NodeT>
      inline NodeDefinition<HeadlessPropsFor<NodeT>, NodeT> Headless()
      {
        return NodeDefinition<HeadlessPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline NodeDefinition<HeadlessPropsFor<NodeT>, NodeT> Headless(const HeadlessPropsFor<NodeT> &p)
      {
        return NodeDefinition<HeadlessPropsFor<NodeT>, NodeT>(p);
      }

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_HEADLESS_HPP
