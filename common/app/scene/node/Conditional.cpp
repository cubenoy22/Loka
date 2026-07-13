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
            falseNode_(0),
            boundCondition_(0)
      {
        this->bindCondition();
        updateActiveNode();
      }

      ConditionalNode::~ConditionalNode()
      {
        this->unbindCondition();
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

      void ConditionalNode::onCompositionAttached()
      {
        this->bindCondition();
        this->updateActiveNode();
      }

      void ConditionalNode::onCompositionDetached()
      {
        this->unbindCondition();
      }

      void ConditionalNode::bindCondition()
      {
        if (this->boundCondition_ == this->props.condition)
        {
          return;
        }
        this->unbindCondition();
        if (this->props.condition)
        {
          this->props.condition->bind(&ConditionalNode::onConditionChanged, this, false);
          this->boundCondition_ = this->props.condition;
        }
      }

      void ConditionalNode::unbindCondition()
      {
        if (!this->boundCondition_)
        {
          return;
        }
        this->boundCondition_->unbind(&ConditionalNode::onConditionChanged, this);
        this->boundCondition_ = 0;
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
            ownedTrueDef(0),
            ownedFalseDef(0)
      {
        props.trueDef = 0;
        props.falseDef = 0;
        this->assignFrom(p, p.trueDef, p.falseDef, 0);
      }

      ConditionalDefinition::ConditionalDefinition(const ConditionalDefinition &other)
          : NodeDefinitionBase(),
            props(other.props),
            ownedTrueDef(0),
            ownedFalseDef(0)
      {
        props.trueDef = 0;
        props.falseDef = 0;
        // A C++98 constructor cannot report failure, so a branch clone OOM
        // degrades this copy to an EMPTY conditional (condition kept, both
        // branches null) - never a half pair. Same policy as WindowProps
        // copies. Fallible callers must go through clone(), which is
        // all-or-nothing.
        this->assignFrom(other.props, other.ownedTrueDef, other.ownedFalseDef, &other);
      }

      ConditionalDefinition &ConditionalDefinition::operator=(const ConditionalDefinition &other)
      {
        if (this != &other)
        {
          this->assignFrom(other.props, other.ownedTrueDef, other.ownedFalseDef, &other);
        }
        return *this;
      }

      bool ConditionalDefinition::assignFrom(const ConditionalProps &nextProps,
                                             const NodeDefinitionBase *trueDef,
                                             const NodeDefinitionBase *falseDef,
                                             const NodeDefinitionBase *nextBase)
      {
        loka::core::OwnedDef<NodeDefinitionBase> nextTrueDef(trueDef ? trueDef->clone() : 0);
        if (trueDef && !nextTrueDef.isSet())
        {
          return false;
        }
        loka::core::OwnedDef<NodeDefinitionBase> nextFalseDef(falseDef ? falseDef->clone() : 0);
        if (falseDef && !nextFalseDef.isSet())
        {
          return false;
        }

        NodeDefinitionBase *oldTrueDef = ownedTrueDef;
        NodeDefinitionBase *oldFalseDef = ownedFalseDef;
        if (nextBase)
        {
          NodeDefinitionBase::operator=(*nextBase);
        }
        props = nextProps;
        ownedTrueDef = nextTrueDef.take();
        ownedFalseDef = nextFalseDef.take();
        props.trueDef = ownedTrueDef;
        props.falseDef = ownedFalseDef;
        delete oldTrueDef;
        delete oldFalseDef;
        return true;
      }

      NodeDefinitionBase *ConditionalDefinition::clone() const
      {
        ConditionalDefinition *copy = new ConditionalDefinition(ConditionalProps(props.condition, 0, 0));
        if (!copy)
        {
          return 0;
        }
        if (!copy->assignFrom(props, ownedTrueDef, ownedFalseDef, this))
        {
          delete copy;
          return 0;
        }
        return copy;
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
        return detail::AlignOf<ConditionalNode>::value;
      }

    } // namespace scene
  } // namespace app
} // namespace loka
