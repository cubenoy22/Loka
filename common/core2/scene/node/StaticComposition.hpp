#ifndef DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP

#include "../NodeComposition.hpp"
#include "Boundary.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // Forward declaration so props can alias NodeType
      template <class PropsT>
      class StaticCompositionBoundaryNodeBase;

      struct StaticCompositionProps : public NodePropsBase<StaticCompositionProps>
      {
        StaticCompositionProps() {}
        typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const StaticCompositionProps *p = dynamic_cast<const StaticCompositionProps *>(&rhs);
          return p ? false : false;
        }
      };

      typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> StaticCompositionBoundaryNode;
      typedef StaticCompositionBoundaryNode StaticCompositionNode;

      // Helper props for static composition boundary nodes with no custom fields.
      template <class NodeT>
      struct StaticCompositionPropsFor : public NodePropsBase<StaticCompositionPropsFor<NodeT> >
      {
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const StaticCompositionPropsFor<NodeT> *p = dynamic_cast<const StaticCompositionPropsFor<NodeT> *>(&rhs);
          return p ? false : false;
        }
      };

      template <class PropsT>
      class StaticCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        PropsT props;
        StaticCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(), props(p) {}
        virtual ~StaticCompositionBoundaryNodeBase() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StaticCompositionProps, StaticCompositionNode>
        virtual void composeNode(NodeComposition &c) {}

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event != COMPOSE_EVENT_ATTACH)
          {
            return;
          }
          this->clearChildren();
          NodeComposition &composition = this->beginComposition(context);
          this->prepareNode(composition);
          this->composeNode(composition);
          Node *child = composition.createNodeTree();
          if (child)
          {
            this->addChild(child);
            this->composeTree(child, context, event, this);
          }
        }
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

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
