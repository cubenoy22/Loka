#ifndef DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP

#include "../NodeComposition.hpp"
#include "ComposableNode.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // Forward declaration so StaticCompositionProps can alias NodeType
      class StaticCompositionNode;
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

      class StaticCompositionNode : public ComposableNode
      {
      public:
        StaticCompositionProps props;
        StaticCompositionNode(const StaticCompositionProps &p) : ComposableNode(), props(p) {}
        virtual ~StaticCompositionNode() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StaticCompositionProps, StaticCompositionNode>
        virtual void composeNode(NodeComposition &c) {}

        virtual void composeWithContext(ComponentContext &context)
        {
          clearChildren();
          NodeComposition c;
          c.setContext(&context);
          this->composeNode(c);
          Node *child = c.createNodeTree();
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

      inline StaticCompositionDefinition StaticComposition(const StaticCompositionProps &p)
      {
        return StaticCompositionDefinition(p);
      }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
