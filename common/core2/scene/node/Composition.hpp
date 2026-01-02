#ifndef DECLARA_CORE2_SCENE_NODE_COMPOSITION_HPP
#define DECLARA_CORE2_SCENE_NODE_COMPOSITION_HPP

#include "../NodeComposition.hpp"
#include "ComposableNode.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      template <class PropsT>
      class CompositionNodeBase;

      // Helper props for plain composition nodes (no custom fields).
      template <class NodeT>
      struct CompositionPropsFor : public NodePropsBase<CompositionPropsFor<NodeT> >
      {
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const CompositionPropsFor<NodeT> *p = dynamic_cast<const CompositionPropsFor<NodeT> *>(&rhs);
          return p ? false : false;
        }
      };

      struct CompositionProps : public NodePropsBase<CompositionProps>
      {
        CompositionProps() {}
        typedef CompositionNodeBase<CompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const CompositionProps *p = dynamic_cast<const CompositionProps *>(&rhs);
          return p ? false : false;
        }
      };

      // CompositionNodeBase: plain composable node (non-boundary).
      template <class PropsT>
      class CompositionNodeBase : public ComposableNode
      {
      public:
        PropsT props;
        CompositionNodeBase(const PropsT &p) : ComposableNode(), props(p) {}
        virtual ~CompositionNodeBase() {}

        // Build node definitions into composition container (default: no children)
        virtual void composeNode(NodeComposition &c) { (void)c; }

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
          }
        }
      };

      typedef CompositionNodeBase<CompositionProps> CompositionNode;

      struct CompositionDefinition : public NodeDefinition<CompositionProps, CompositionNode>
      {
        CompositionDefinition() : NodeDefinition<CompositionProps, CompositionNode>() {}
        CompositionDefinition(const CompositionProps &p) : NodeDefinition<CompositionProps, CompositionNode>(p) {}
      };

      inline CompositionDefinition Composition(const CompositionProps &p)
      {
        return CompositionDefinition(p);
      }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_COMPOSITION_HPP
