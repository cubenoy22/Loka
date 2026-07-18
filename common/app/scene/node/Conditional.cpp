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
        // Each node's own destructor is the last retire door (writes RETIRED
        // before releasing its context), so parked branches need no explicit
        // subtree mark here.
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

      void ConditionalNode::onLifecycleFactChanged(NodeLifecycleFact previous, NodeLifecycleFact next)
      {
        (void)previous;
        if (next == NODE_FACT_RETIRED)
        {
          this->unbindCondition();
        }
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

      void ConditionalNode::applyRetainedProps(const ConditionalProps &nextProps)
      {
        this->props = nextProps;
        this->bindCondition();
      }

      Node *ConditionalNode::ensureBranchNode(bool cond, bool &created)
      {
        created = false;
        Node **slot = cond ? &trueNode_ : &falseNode_;
        NodeDefinitionBase *def = cond ? props.trueDef : props.falseDef;
        if (*slot || !def)
        {
          return *slot;
        }
        *slot = createConditionalNodeRecursive(def);
        created = *slot != 0;
        return *slot;
      }

      void ConditionalNode::removeActiveNodeFromChildren()
      {
        if (!activeNode)
        {
          return;
        }
        children_.remove(activeNode);
        if (!this->retainedDetached())
        {
          NotifySubtreeNodeDetached(activeNode);
        }
      }

      void ConditionalNode::updateActiveNode()
      {
        if (!props.condition)
        {
          return;
        }
        bool cond = props.condition->get();
        bool created = false;
        Node *nextNode = ensureBranchNode(cond, created);
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
          if (this->retainedDetached())
          {
            // Adopted while an ancestor keeps this conditional hidden: the
            // subtree inherits the detached-retained fact (born-hidden).
            // The ancestor's re-attach walk restores ATTACHED for the
            // then-active path.
            Node::MarkSubtreeLifecycleFact(activeNode, NODE_FACT_DETACHED_RETAINED);
          }
          else if (!created)
          {
            // Re-entry: the subtree kept its contexts across the retained
            // detach; first entry is announced by setContext(). While an
            // ancestor keeps this conditional hidden, swaps stay silent —
            // the ancestor's re-attach walk shows the then-active path.
            NotifySubtreeNodeAttached(activeNode);
          }
        }
      }

      Node *ConditionalNode::retainedLifecycleBranch(unsigned index)
      {
        Node *branches[2];
        unsigned count = 0;
        if (trueNode_ && trueNode_ != activeNode)
        {
          branches[count++] = trueNode_;
        }
        if (falseNode_ && falseNode_ != activeNode)
        {
          branches[count++] = falseNode_;
        }
        return index < count ? branches[index] : 0;
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

      bool ConditionalDefinition::hasEquivalentProps(const NodeDefinitionBase &other) const
      {
        const PropsBase *otherProps = other.propsBase();
        if (!otherProps || otherProps->propsTypeId() != this->props.propsTypeId())
        {
          return false;
        }
        const ConditionalProps &otherConditionalProps = static_cast<const ConditionalProps &>(*otherProps);
        return this->props.condition == otherConditionalProps.condition;
      }

      bool ConditionalDefinition::applyPropsToNode(Node *node) const
      {
        if (!node || node->nodeTypeKey() != NodeTypeToken<ConditionalNode>())
        {
          return false;
        }
        ConditionalNode *conditional = static_cast<ConditionalNode *>(node);
        conditional->applyRetainedProps(this->props);
        conditional->setNodeTag(this->nodeTag());
        conditional->setNativeLifetimeHint(this->nativeLifetimeHint());
        return true;
      }

      ConditionalDefinition::~ConditionalDefinition()
      {
        delete ownedTrueDef;
        delete ownedFalseDef;
      }

      Node *ConditionalDefinition::create() const
      {
        Node *node = new ConditionalNode(props);
        if (node)
        {
          node->setNodeTag(this->nodeTag());
          node->setNativeLifetimeHint(this->nativeLifetimeHint());
        }
        return node;
      }

      Node *ConditionalDefinition::createInPlace(void *mem) const
      {
        Node *node = new (mem) ConditionalNode(props);
        if (node)
        {
          node->setNodeTag(this->nodeTag());
          node->setNativeLifetimeHint(this->nativeLifetimeHint());
        }
        return node;
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
