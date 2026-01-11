#ifndef LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP

#include "../NodeComposition.hpp"
#include "Boundary.hpp"
#include "core/Profiler.hpp"

using declara::core::ProfileTicks;
using declara::core::gComposeAttachTicks;
using declara::core::gComposeNodeTicks;
using declara::core::gComposeCreateTicks;
using declara::core::gClearChildTicks;
using declara::core::gBeginCompTicks;
using declara::core::gAddChildTicks;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // Forward declaration so props can alias NodeType
      template <class PropsT>
      class StaticCompositionBoundaryNodeBase;

      struct StaticCompositionTypeTag
      {
      };

      template <class NodeT>
      struct StaticCompositionPropsForTypeTag
      {
      };

      template <class NodeT>
      struct BoundaryPropsForTypeTag
      {
      };

      struct StaticCompositionProps : public NodePropsBase<StaticCompositionProps>
      {
        StaticCompositionProps() {}
        typedef StaticCompositionTypeTag TypeTag;
        typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> StaticCompositionBoundaryNode;
      typedef StaticCompositionBoundaryNode StaticCompositionNode;

      // Helper props for static composition boundary nodes with no custom fields.
      template <class NodeT>
      struct StaticCompositionPropsFor : public NodePropsBase<StaticCompositionPropsFor<NodeT> >
      {
        typedef StaticCompositionPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      // Alias to avoid exposing "Static" in user code.
      // Note: Must inherit from NodePropsBase<BoundaryPropsFor<NodeT>> directly
      // so that createNode() receives the correct Props type for node constructors.
      template <class NodeT>
      struct BoundaryPropsFor : public NodePropsBase<BoundaryPropsFor<NodeT> >
      {
        typedef BoundaryPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      template <class PropsT>
      class StaticCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;
        StaticCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(), props(p), composed_(false) {}
        virtual ~StaticCompositionBoundaryNodeBase() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StaticCompositionProps, StaticCompositionNode>
        virtual void composeNode(NodeComposition &c) {}

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event != COMPOSE_EVENT_ATTACH)
          {
            if (event == COMPOSE_EVENT_DETACH)
            {
              NodeComposition &composition = this->beginComposition(context);
              this->detachNode(composition);
              composed_ = false;
            }
            return;
          }
          if (composed_)
          {
            return;
          }
          long t0 = ProfileTicks();
          this->clearChildren();
          // Reset arena for this boundary compose pass.
          this->nodeArena()->clear();
          gClearChildTicks += ProfileTicks() - t0;
          t0 = ProfileTicks();
          NodeComposition &composition = this->beginComposition(context);
          gBeginCompTicks += ProfileTicks() - t0;
          t0 = ProfileTicks();
          this->attachNode(composition);
          gComposeAttachTicks += ProfileTicks() - t0;
          t0 = ProfileTicks();
          this->composeNode(composition);
          gComposeNodeTicks += ProfileTicks() - t0;
          t0 = ProfileTicks();
          Node *child = composition.createNodeTree();
          gComposeCreateTicks += ProfileTicks() - t0;
          if (child)
          {
            t0 = ProfileTicks();
            this->addChild(child);
            gAddChildTicks += ProfileTicks() - t0;
            this->composeTree(child, context, event, this);
          }
          composed_ = true;
        }
      private:
        bool composed_;
      };

      struct StaticCompositionDefinition : public NodeDefinition<StaticCompositionProps, StaticCompositionNode>
      {
        StaticCompositionDefinition() : NodeDefinition<StaticCompositionProps, StaticCompositionNode>() {}
        StaticCompositionDefinition(const StaticCompositionProps &p) : NodeDefinition<StaticCompositionProps, StaticCompositionNode>(p) {}
      };

      struct StaticCompositionBoundaryDefinition : public BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>
      {
        StaticCompositionBoundaryDefinition() : BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>() {}
        StaticCompositionBoundaryDefinition(const StaticCompositionProps &p) : BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>(p) {}
      };

      inline StaticCompositionDefinition StaticComposition(const StaticCompositionProps &p)
      {
        return StaticCompositionDefinition(p);
      }

      inline StaticCompositionBoundaryDefinition StaticCompositionBoundary(const StaticCompositionProps &p)
      {
        return StaticCompositionBoundaryDefinition(p);
      }

      template <class NodeT>
      inline BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT> StaticCompositionBoundary()
      {
        return BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT> StaticCompositionBoundary(const StaticCompositionPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT>(p);
      }

      // Alias to avoid exposing "Static" in user code.
      template <class NodeT>
      inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary()
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary(const BoundaryPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>(p);
      }

      // Helper base class for nodes using StaticCompositionPropsFor<NodeT>.
      template <class NodeT>
      class StaticCompositionNodeFor : public StaticCompositionBoundaryNodeBase<StaticCompositionPropsFor<NodeT> >
      {
      public:
        typedef StaticCompositionPropsFor<NodeT> PropsType;
        StaticCompositionNodeFor(const PropsType &p)
            : StaticCompositionBoundaryNodeBase<StaticCompositionPropsFor<NodeT> >(p) {}
        virtual ~StaticCompositionNodeFor() {}
      };

      // Alias for boundary nodes without exposing "Static" in user code.
      // Uses BoundaryPropsFor<NodeT> to match user constructor expectations.
      template <class NodeT>
      class BoundaryNodeFor : public StaticCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >
      {
      public:
        typedef BoundaryPropsFor<NodeT> PropsType;
        BoundaryNodeFor(const PropsType &p)
            : StaticCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >(p) {}
        virtual ~BoundaryNodeFor() {}
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
