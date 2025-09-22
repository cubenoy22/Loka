#ifndef DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP
#define DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP

#include "core/State.hpp"
#include "core2/scene/Node.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      struct ConditionalProps
      {
        State<bool> *condition;
        NodeDefinitionBase *trueDef;
        NodeDefinitionBase *falseDef;
        ConditionalProps(State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
            : condition(cond), trueDef(tDef), falseDef(fDef) {}
        ConditionalProps(const State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
            : condition(const_cast<State<bool> *>(cond)), trueDef(tDef), falseDef(fDef) {}
      };

      // ConditionalNode: 条件分岐ノード
      class ConditionalNode : public declara::core::scene::Node
      {
      public:
        ConditionalProps props;
        declara::core::scene::Node *activeNode;
        ConditionalNode(const ConditionalProps &p)
            : props(p), activeNode(0)
        {
          if (props.condition)
          {
            props.condition->deferBind(&ConditionalNode::onConditionChanged, this);
          }
          updateActiveNode();
        }
        ~ConditionalNode()
        {
          if (props.condition)
          {
            props.condition->deferUnbind(&ConditionalNode::onConditionChanged, this);
          }
        }
        static void onConditionChanged(void *userData)
        {
          ConditionalNode *self = static_cast<ConditionalNode *>(userData);
          if (self)
            self->updateActiveNode();
        }
        void compose()
        {
          updateActiveNode();
          if (activeNode)
            activeNode->compose();
        }
        void updateActiveNode()
        {
          if (props.condition && props.trueDef && props.falseDef)
          {
            if (props.condition->get())
              activeNode = props.trueDef->create();
            else
              activeNode = props.falseDef->create();
          }
        }
      };

      struct ConditionalDefinition : public declara::core::scene::NodeDefinitionBase
      {
        ConditionalProps props;
        ConditionalDefinition(const ConditionalProps &p) : props(p) {}
        declara::core::scene::Node *create() const { return new ConditionalNode(props); }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_CONDITIONAL_HPP
