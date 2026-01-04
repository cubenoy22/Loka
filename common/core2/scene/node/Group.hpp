#ifndef LOKA_CORE2_SCENE_NODE_GROUP_HPP
#define LOKA_CORE2_SCENE_NODE_GROUP_HPP

#include "../NodeComposition.hpp"
#include "ComposableNode.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      template <class PropsT>
      class GroupNodeBase;

      // Helper props for plain group nodes (no custom fields).
      template <class NodeT>
      struct GroupPropsFor : public NodePropsBase<GroupPropsFor<NodeT> >
      {
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const GroupPropsFor<NodeT> *p = dynamic_cast<const GroupPropsFor<NodeT> *>(&rhs);
          return p ? false : false;
        }
      };

      struct GroupProps : public NodePropsBase<GroupProps>
      {
        GroupProps() {}
        typedef GroupNodeBase<GroupProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          const GroupProps *p = dynamic_cast<const GroupProps *>(&rhs);
          return p ? false : false;
        }
      };

      // GroupNodeBase: plain composable node (non-boundary).
      template <class PropsT>
      class GroupNodeBase : public ComposableNode
      {
      public:
        PropsT props;
        GroupNodeBase(const PropsT &p) : ComposableNode(), props(p) {}
        virtual ~GroupNodeBase() {}

        // Build node definitions into composition container (default: no children)
        virtual void composeNode(NodeComposition &c) { (void)c; }

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event != COMPOSE_EVENT_ATTACH)
          {
            if (event == COMPOSE_EVENT_DETACH)
            {
              NodeComposition &composition = this->beginComposition(context);
              this->detachNode(composition);
            }
            return;
          }
          this->clearChildren();
          NodeComposition &composition = this->beginComposition(context);
          this->attachNode(composition);
          this->composeNode(composition);
          Node *child = composition.createNodeTree();
          if (child)
          {
            this->addChild(child);
          }
        }
      };

      typedef GroupNodeBase<GroupProps> GroupNode;

      struct GroupDefinition : public NodeDefinition<GroupProps, GroupNode>
      {
        GroupDefinition() : NodeDefinition<GroupProps, GroupNode>() {}
        GroupDefinition(const GroupProps &p) : NodeDefinition<GroupProps, GroupNode>(p) {}
      };

      inline GroupDefinition Group(const GroupProps &p)
      {
        return GroupDefinition(p);
      }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_GROUP_HPP
