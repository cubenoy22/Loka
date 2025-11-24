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
      typedef NodeDefinitionBase *(*StaticComposeFunc)(NodeComposition &, void *);

      struct StaticCompositionProps : public NodePropsBase<StaticCompositionProps>
      {
        StaticComposeFunc compose;
        void *userData;
        StaticCompositionProps() : compose(0), userData(0) {}
        StaticCompositionProps(StaticComposeFunc f, void *ud) : compose(f), userData(ud) {}
        bool operator<(const PropsBase &rhs) const
        {
          const StaticCompositionProps *p = dynamic_cast<const StaticCompositionProps *>(&rhs);
          if (!p)
            return false;
          if (compose != p->compose)
            return compose < p->compose;
          return userData < p->userData;
        }
      };

      class StaticCompositionNode : public ComposableNode
      {
      public:
        StaticCompositionProps props;
        StaticCompositionNode(const StaticCompositionProps &p) : ComposableNode(), props(p) {}
        virtual ~StaticCompositionNode() {}

        virtual void compose()
        {
          clearChildren();
          if (!props.compose)
            return;
          NodeComposition c;
          NodeDefinitionBase *def = props.compose(c, props.userData);
          if (!def)
          {
            def = c.root();
          }
          if (!def)
            return;
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
