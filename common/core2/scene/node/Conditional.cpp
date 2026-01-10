#include "Conditional.hpp"
#include "../Node.hpp"
#include <new>

namespace declara
{
  namespace core
  {
    namespace scene
    {
      ConditionalProps::ConditionalProps(State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(cond), trueDef(tDef), falseDef(fDef) {}

      ConditionalProps::ConditionalProps(const State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(const_cast<State<bool> *>(cond)), trueDef(tDef), falseDef(fDef) {}

      ConditionalNode::ConditionalNode(const ConditionalProps &p)
          : props(p), activeNode(0) {}

      ConditionalNode::~ConditionalNode()
      {
        delete activeNode;
      }

      void ConditionalNode::onConditionChanged(void *userData)
      {
        ConditionalNode *self = static_cast<ConditionalNode *>(userData);
        if (self)
        {
          self->updateActiveNode();
        }
      }

      void ConditionalNode::compose()
      {
        updateActiveNode();
      }

      void ConditionalNode::updateActiveNode()
      {
        if (!props.condition)
        {
          return;
        }
        bool cond = props.condition->get();
        NodeDefinitionBase *def = cond ? props.trueDef : props.falseDef;
        delete activeNode;
        activeNode = def ? def->create() : 0;
      }

      ConditionalDefinition::ConditionalDefinition(const ConditionalProps &p)
          : props(p) {}

      Node *ConditionalDefinition::create() const
      {
        return new ConditionalNode(props);
      }

      Node *ConditionalDefinition::createInPlace(void *mem) const
      {
        return new (mem) ConditionalNode(props);
      }

      size_t ConditionalDefinition::nodeSize() const
      {
        return sizeof(ConditionalNode);
      }

      size_t ConditionalDefinition::nodeAlign() const
      {
        return AlignOf<ConditionalNode>::value;
      }

    } // namespace scene
  } // namespace core
} // namespace declara
