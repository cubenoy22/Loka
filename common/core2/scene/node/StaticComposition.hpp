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
      class StaticCompositionBoundaryNode;
      typedef StaticCompositionBoundaryNode StaticCompositionNode;
      struct StaticCompositionProps : public NodePropsBase<StaticCompositionProps>
      {
        StaticCompositionProps() {}
        // Required alias for NodePropsBase to construct nodes
        typedef StaticCompositionNode NodeType; // forward-declared below
        bool operator<(const PropsBase &rhs) const
        {
          const StaticCompositionProps *p = dynamic_cast<const StaticCompositionProps *>(&rhs);
          return p ? false : false;
        }
      };

      class StaticCompositionBoundaryNode : public BoundaryNode
      {
      public:
        StaticCompositionProps props;
        StaticCompositionBoundaryNode(const StaticCompositionProps &p) : BoundaryNode(), props(p) {}
        virtual ~StaticCompositionBoundaryNode() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StaticCompositionProps, StaticCompositionNode>
        virtual void composeNode(NodeComposition &c) {}

        virtual void composeWithContext(ComponentContext &context)
        {
          this->clearChildren();
          NodeComposition &composition = this->beginComposition(context);
          this->composeNode(composition);
          Node *child = composition.createNodeTree();
          if (child)
          {
            this->addChild(child);
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

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
