#ifndef DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP
#define DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP

#include "core/State.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // Forward declarations
      class Node;
      struct NodeDefinitionBase;

      struct ConditionalProps
      {
        State<bool> *condition;
        NodeDefinitionBase *trueDef;
        NodeDefinitionBase *falseDef;
        ConditionalProps(State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
        ConditionalProps(const State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
      };

      // ConditionalNode: node that switches by condition
      class ConditionalNode : public Node
      {
      public:
        ConditionalProps props;
        Node *activeNode;
        ConditionalNode(const ConditionalProps &p);
        ~ConditionalNode();
        static void onConditionChanged(void *userData);
        void compose();
        void updateActiveNode();
      };

      struct ConditionalDefinition : public NodeDefinitionBase
      {
        ConditionalProps props;
        ConditionalDefinition(const ConditionalProps &p);
        Node *create() const;
        virtual NodeDefinitionBase *clone() const { return new ConditionalDefinition(*this); }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP
