#ifndef LOKA_CORE2_SCENE_NODE_GROUP_HPP
#define LOKA_CORE2_SCENE_NODE_GROUP_HPP

#include "../NodeComposition.hpp"
#include "ComposableNode.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct GroupTypeTag
      {
      };

      template <class NodeT>
      struct GroupPropsForTypeTag
      {
      };

      template <class PropsT>
      class GroupNodeBase;

      // Helper props for plain group nodes (no custom fields).
      template <class NodeT>
      struct GroupPropsFor : public NodePropsBase<GroupPropsFor<NodeT> >
      {
        typedef GroupPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      struct GroupProps : public NodePropsBase<GroupProps>
      {
        GroupProps() {}
        typedef GroupTypeTag TypeTag;
        typedef GroupNodeBase<GroupProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      // GroupNodeBase: plain composable node (non-boundary).
      template <class PropsT>
      class GroupNodeBase : public ComposableNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
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
              NodeComposition *parentComp = context.composition();
              assert(parentComp && "GroupNodeBase requires parent composition");
              this->detachNode(*parentComp);
            }
            return;
          }
          NodeComposition *parentComp = context.composition();
          assert(parentComp && "GroupNodeBase requires parent composition");
          this->attachNode(*parentComp);
          NodeComposition::CompositionScope scope(*parentComp);
          this->composeNode(*parentComp);
          // No createNodeTree - parent handles it
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
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_GROUP_HPP
