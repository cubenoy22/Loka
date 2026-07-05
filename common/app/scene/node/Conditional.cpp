#include "Conditional.hpp"
#include "../Node.hpp"
#include <cstdio>
#include <new>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      static void notifyNodeContextAttachedRecursive(Node *node)
      {
        if (!node)
        {
          return;
        }
        if (node->getContext())
        {
          node->getContext()->onNodeAttached();
        }
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          notifyNodeContextAttachedRecursive(child);
        }
      }

      static void notifyNodeContextDetachedRecursive(Node *node)
      {
        if (!node)
        {
          return;
        }
        if (node->getContext())
        {
          node->getContext()->onNodeDetached();
        }
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          notifyNodeContextDetachedRecursive(child);
        }
      }

      static Node *createConditionalNodeRecursiveWithAutoId(NodeDefinitionBase *def, long &autoIdCounter)
      {
        if (!def)
        {
          return 0;
        }

        Node *node = def->create();
        if (def->hasTestId())
        {
          node->setTestId(def->testIdValue());
        }
        else if (def->wantsAutoTestId())
        {
          char sAutoBuf[64];
          snprintf(sAutoBuf, sizeof(sAutoBuf), "auto-%ld", autoIdCounter++);
          node->setTestId(std::string(sAutoBuf));
        }
        INestableDefinition *nestableDef = def->asNestableDefinition();
        INestable *nestableNode = node->asNestable();
        if (!nestableDef || !nestableNode)
        {
          return node;
        }

        NodeDefinitionBase *childDef = nestableDef->childrenHead();
        while (childDef)
        {
          Node *childNode = createConditionalNodeRecursiveWithAutoId(childDef, autoIdCounter);
          if (childNode)
          {
            nestableNode->addChild(childNode);
          }
          childDef = childDef->nextInComposition;
        }
        return node;
      }

      static Node *createConditionalNodeRecursive(NodeDefinitionBase *def)
      {
        long autoIdCounter = 1;
        return createConditionalNodeRecursiveWithAutoId(def, autoIdCounter);
      }

      ConditionalProps::ConditionalProps(loka::core::State<bool> *cond,
                                         NodeDefinitionBase *tDef,
                                         NodeDefinitionBase *fDef)
          : condition(cond),
            trueDef(tDef),
            falseDef(fDef)
      {
      }

      ConditionalProps::ConditionalProps(const loka::core::State<bool> *cond,
                                         NodeDefinitionBase *tDef,
                                         NodeDefinitionBase *fDef)
          : condition(const_cast<loka::core::State<bool> *>(cond)),
            trueDef(tDef),
            falseDef(fDef)
      {
      }

      ConditionalNode::ConditionalNode(const ConditionalProps &p)
          : NestableNode(),
            props(p),
            activeNode(0),
            trueNode_(0),
            falseNode_(0)
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
        if (trueNode_ && trueNode_ != activeNode)
        {
          delete trueNode_;
        }
        if (falseNode_ && falseNode_ != activeNode)
        {
          delete falseNode_;
        }
        trueNode_ = 0;
        falseNode_ = 0;
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

      Node *ConditionalNode::ensureBranchNode(bool cond)
      {
        Node **slot = cond ? &trueNode_ : &falseNode_;
        NodeDefinitionBase *def = cond ? props.trueDef : props.falseDef;
        if (*slot || !def)
        {
          return *slot;
        }
        *slot = createConditionalNodeRecursive(def);
        return *slot;
      }

      void ConditionalNode::removeActiveNodeFromChildren()
      {
        if (!activeNode)
        {
          return;
        }
        notifyNodeContextDetachedRecursive(activeNode);
        children_.remove(activeNode);
      }

      void ConditionalNode::updateActiveNode()
      {
        if (!props.condition)
        {
          return;
        }
        bool cond = props.condition->get();
        Node *nextNode = ensureBranchNode(cond);
        if (nextNode == activeNode)
        {
          return;
        }
        removeActiveNodeFromChildren();
        activeNode = nextNode;
        if (activeNode)
        {
          activeNode->markPendingAttachForCompose();
          addChild(activeNode);
          notifyNodeContextAttachedRecursive(activeNode);
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
          : props(p),
            ownedTrueDef(p.trueDef ? p.trueDef->clone() : 0),
            ownedFalseDef(p.falseDef ? p.falseDef->clone() : 0)
      {
        props.trueDef = ownedTrueDef;
        props.falseDef = ownedFalseDef;
      }

      ConditionalDefinition::ConditionalDefinition(const ConditionalDefinition &other)
          : NodeDefinitionBase(other),
            props(other.props),
            ownedTrueDef(other.ownedTrueDef ? other.ownedTrueDef->clone() : 0),
            ownedFalseDef(other.ownedFalseDef ? other.ownedFalseDef->clone() : 0)
      {
        props.trueDef = ownedTrueDef;
        props.falseDef = ownedFalseDef;
      }

      ConditionalDefinition &ConditionalDefinition::operator=(const ConditionalDefinition &other)
      {
        if (this != &other)
        {
          NodeDefinitionBase::operator=(other);
          delete ownedTrueDef;
          delete ownedFalseDef;
          props = other.props;
          ownedTrueDef = other.ownedTrueDef ? other.ownedTrueDef->clone() : 0;
          ownedFalseDef = other.ownedFalseDef ? other.ownedFalseDef->clone() : 0;
          props.trueDef = ownedTrueDef;
          props.falseDef = ownedFalseDef;
        }
        return *this;
      }

      ConditionalDefinition::~ConditionalDefinition()
      {
        delete ownedTrueDef;
        delete ownedFalseDef;
      }

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
