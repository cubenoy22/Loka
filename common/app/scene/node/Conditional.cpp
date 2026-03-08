#include "Conditional.hpp"
#include "../Node.hpp"
#include <new>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      static Node *createConditionalNodeRecursive(NodeDefinitionBase *def)
      {
        if (!def)
        {
          return 0;
        }

        Node *node = def->create();
        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();
        if (!nestableDef || !nestableNode)
        {
          return node;
        }

        NodeDefinitionBase *childDef = nestableDef->childrenHead();
        while (childDef)
        {
          Node *childNode = createConditionalNodeRecursive(childDef);
          if (childNode)
          {
            nestableNode->addChild(childNode);
          }
          childDef = childDef->nextInComposition;
        }
        return node;
      }

      ConditionalProps::ConditionalProps(loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(cond), trueDef(tDef), falseDef(fDef) {}

      ConditionalProps::ConditionalProps(const loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(const_cast<loka::core::State<bool> *>(cond)), trueDef(tDef), falseDef(fDef) {}

      ConditionalNode::ConditionalNode(const ConditionalProps &p)
          : NestableNode(), props(p), activeNode(0)
      {
        if (props.condition)
        {
          props.condition->bind(&ConditionalNode::onConditionChanged, this, false);
        }
        updateActiveNode();
      }

      ConditionalNode::~ConditionalNode()
      {
        if (props.condition)
        {
          props.condition->unbind(&ConditionalNode::onConditionChanged, this);
        }
        activeNode = 0;
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
        Node *nextNode = createConditionalNodeRecursive(def);
        clearChildrenInternal(false);
        activeNode = nextNode;
        if (activeNode)
        {
          addChild(activeNode);
        }
      }

      void ConditionalNode::render(IPlatformController *controller)
      {
        if (activeNode)
        {
          activeNode->render(controller);
        }
      }

      short ConditionalNode::layout(IPlatformController *controller, LayoutState &state)
      {
        if (activeNode)
        {
          return activeNode->layout(controller, state);
        }
        return 0;
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
  } // namespace app
} // namespace loka
