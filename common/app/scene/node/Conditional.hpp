#ifndef LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
#define LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP

#include "loka/core/State.hpp"
#include "../Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      // Forward declarations
      class Node;
      struct NodeDefinitionBase;

      struct ConditionalProps
      {
        loka::core::State<bool> *condition;
        NodeDefinitionBase *trueDef;
        NodeDefinitionBase *falseDef;
        ConditionalProps(loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
        ConditionalProps(const loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
      };

      // ConditionalNode: node that switches by condition
      class ConditionalNode : public NestableNode
      {
      public:
        ConditionalProps props;
        Node *activeNode;
        ConditionalNode(const ConditionalProps &p);
        ~ConditionalNode();
        virtual void declareObservedStates(ObservedStateRegistrar &registrar)
        {
          if (this->props.condition)
          {
            registrar.observe(this->props.condition, NODE_DIRTY_CHILD);
          }
        }
        static void onConditionChanged(void *userData);
        void compose();
        void updateActiveNode();
        void render(IPlatformController *controller);
        short layout(IPlatformController *controller, LayoutState &state);
      };

      struct ConditionalDefinition : public NodeDefinitionBase
      {
        ConditionalProps props;
        ConditionalDefinition(const ConditionalProps &p);
        Node *create() const;
        Node *createInPlace(void *mem) const;
        size_t nodeSize() const;
        size_t nodeAlign() const;
        virtual NodeDefinitionBase *clone() const { return new ConditionalDefinition(*this); }
        virtual const PropsBase *propsBase() const { return 0; }
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const
        {
          (void)other;
          return false;
        }
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
