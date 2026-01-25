#ifndef LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
#define LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP

#include "core/State.hpp"
#include "../Node.hpp"

namespace loka
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
        loka::core::State<bool> *condition;
        NodeDefinitionBase *trueDef;
        NodeDefinitionBase *falseDef;
        ConditionalProps(loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
        ConditionalProps(const loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
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
        Node *createInPlace(void *mem) const;
        size_t nodeSize() const;
        size_t nodeAlign() const;
        virtual NodeDefinitionBase *clone() const { return new ConditionalDefinition(*this); }
      };

    } // namespace scene
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
