#ifndef LOKA_CORE2_SCENE_NODE_HEADLESS_HPP
#define LOKA_CORE2_SCENE_NODE_HEADLESS_HPP

#include "app/scene/boundary/BoundaryInnerStateOwner.hpp"
#include "app/scene/NodeState.hpp"
#include "app/scene/nodes/boundary/Boundary.hpp"
#include "app/scene/node/ComposableNode.hpp"

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
      class HeadlessNodeBase : public ComposableNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;

        HeadlessNodeBase(const PropsT &p)
            : ComposableNode(), props(p), stateOwner_(), composed_(false)
        {
          this->stateOwner_.setInvalidateCallback(&HeadlessNodeBase::InvalidateThunk, this);
        }

        virtual ~HeadlessNodeBase()
        {
          this->clearChildren();
          this->releaseNodeStateRegistrations();
        }

        virtual IStateOwner *asStateOwner() { return &this->stateOwner_; }
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

        BoundaryInnerStateOwner stateOwner_;
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
