#include "Conditional.hpp"
#include "../Node.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/boundary/detail/BoundaryParkedBranchLedger.hpp"
#include <cstdio>
#include <new>

namespace loka
{
  namespace app
  {
    namespace scene
    {
#if defined(TEST_BUILD)
      class ConditionalTestBoundaryOwner : public BoundaryNode
      {
      protected:
        virtual void composeWithContext(ComponentContext &, ComposeEvent)
        {
        }
      };
#endif

#if defined(TEST_BUILD)
      void Node::EvaluateChildrenForScheduledApplySubtree(Node *node)
      {
        if (!node)
        {
          return;
        }
        ComponentContext context;
        node->evaluateChildrenForScheduledApply(context, 0);
        INestable *nestable = node->asNestable();
        if (!nestable)
        {
          return;
        }
        for (Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
        {
          EvaluateChildrenForScheduledApplySubtree(child);
        }
      }
#endif

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
            compositionSeatSlot_(-1),
            activeCondition_(false),
            hasActiveCondition_(false)
#if defined(TEST_BUILD)
            , testBoundaryOwner_(0)
#endif
      {
        this->initializeActiveNode();
      }

      ConditionalNode::~ConditionalNode()
      {
#if defined(TEST_BUILD)
        delete this->testBoundaryOwner_;
        this->testBoundaryOwner_ = 0;
#endif
        this->activeNode = 0;
      }

      void ConditionalNode::evaluateChildrenForScheduledApply(ComponentContext &context,
                                                               BoundaryNode *boundary)
      {
        this->updateActiveNode(context, boundary, false);
      }

      bool ConditionalNode::reconcileForScheduledBranchReentry(ComponentContext &context,
                                                               BoundaryNode *boundary)
      {
        this->updateActiveNode(context, boundary, true);
        return true;
      }

      Node *ConditionalNode::retainedLifecycleBranch(unsigned index)
      {
#if defined(TEST_BUILD)
        return this->testBoundaryOwner_
                   ? this->testBoundaryOwner_->retainedLifecycleBranch(index)
                   : 0;
#else
        (void)index;
        return 0;
#endif
      }

      void ConditionalNode::applyRetainedProps(const ConditionalProps &nextProps)
      {
        this->props = nextProps;
      }

      NodeDefinitionBase *ConditionalNode::branchDefinition(bool cond) const
      {
        return cond ? this->props.trueDef : this->props.falseDef;
      }

      Node *ConditionalNode::createBranchNode(bool cond)
      {
        return createConditionalNodeRecursive(this->branchDefinition(cond));
      }

      void ConditionalNode::initializeActiveNode()
      {
        if (!this->props.condition)
        {
          return;
        }
        this->activeCondition_ = this->props.condition->get();
        this->hasActiveCondition_ = true;
        this->activeNode = this->createBranchNode(this->activeCondition_);
        if (this->activeNode)
        {
          this->addChild(this->activeNode);
        }
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

      void ConditionalNode::retireDetachedBranchForReplacement(
          ComponentContext &context,
          BoundaryNode *boundary,
          Node *branch)
      {
        if (!boundary || !branch)
        {
          return;
        }
        boundary->composeTree(branch, context, COMPOSE_EVENT_DETACH, boundary);
        boundary->retireDetachedNode(context, branch);
      }

      void ConditionalNode::attachActiveBranchForScheduledApply(bool reentered)
      {
        if (!this->activeNode)
        {
          return;
        }
        this->activeNode->markPendingAttachForCompose();
        this->addChild(this->activeNode);
        if (this->retainedDetached())
        {
          // Adopted while an ancestor keeps this conditional hidden: the
          // subtree inherits the detached-retained fact (born-hidden).
          // The ancestor's re-attach walk restores ATTACHED for the
          // then-active path.
          Node::MarkSubtreeLifecycleFact(this->activeNode, NODE_FACT_DETACHED_RETAINED);
        }
        else if (reentered)
        {
          // Re-entry: the subtree kept its contexts across the retained
          // detach; first entry is announced by setContext(). While an
          // ancestor keeps this conditional hidden, swaps stay silent —
          // the ancestor's re-attach walk shows the then-active path.
          NotifySubtreeNodeAttached(this->activeNode);
        }
      }

      bool ConditionalNode::updateActiveNode(ComponentContext &context,
                                             BoundaryNode *boundary,
                                             bool reconcileCurrentBranch)
      {
        bool shouldReconcile = true;
#if defined(TEST_BUILD)
        if (!boundary)
        {
          if (!this->testBoundaryOwner_)
          {
            this->testBoundaryOwner_ = new ConditionalTestBoundaryOwner();
          }
          boundary = this->testBoundaryOwner_;
          shouldReconcile = false;
        }
#endif
        if (!this->props.condition || !boundary)
        {
          return false;
        }
        const bool cond = this->props.condition->get();
        NodeDefinitionBase *definition = this->branchDefinition(cond);
        if (this->hasActiveCondition_ && cond == this->activeCondition_)
        {
          if (shouldReconcile && reconcileCurrentBranch && this->activeNode && definition)
          {
            if (!boundary->reconcileParkedBranch(context, this->activeNode, definition))
            {
              Node *failedBranch = this->activeNode;
              this->removeActiveNodeFromChildren();
              this->activeNode = 0;
              this->retireDetachedBranchForReplacement(context, boundary, failedBranch);
              this->activeNode = this->createBranchNode(cond);
              this->attachActiveBranchForScheduledApply(false);
              return true;
            }
          }
          return false;
        }

        Node *incoming = boundary->takeParkedBranch(
            BoundaryParkedBranchKey(this->nodeTag(),
                                    this->compositionSeatSlot_,
                                    ConditionalProps::staticTypeId()));
        if (incoming &&
            (!definition || !definition->isCompatibleWithNode(incoming)))
        {
          this->retireDetachedBranchForReplacement(context, boundary, incoming);
          incoming = 0;
        }
        Node *outgoing = this->activeNode;
        this->removeActiveNodeFromChildren();
        if (outgoing)
        {
          boundary->parkBranch(
              BoundaryParkedBranchKey(this->nodeTag(),
                                      this->compositionSeatSlot_,
                                      ConditionalProps::staticTypeId()),
              outgoing);
        }

        this->activeNode = incoming;
        bool reentered = this->activeNode != 0;
        if (!this->activeNode)
        {
          this->activeNode = this->createBranchNode(cond);
        }
        this->activeCondition_ = cond;
        this->hasActiveCondition_ = true;
        if (this->activeNode)
        {
          if (shouldReconcile && reentered && definition)
          {
            if (!boundary->reconcileParkedBranch(context, this->activeNode, definition))
            {
              Node *failedBranch = this->activeNode;
              this->activeNode = 0;
              this->retireDetachedBranchForReplacement(context, boundary, failedBranch);
              this->activeNode = this->createBranchNode(cond);
              reentered = false;
            }
          }
        }
        this->attachActiveBranchForScheduledApply(reentered);
        return true;
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

      bool ConditionalDefinition::repointRetainedNodeDefinition(Node *node) const
      {
        if (!node || node->nodeTypeKey() != NodeTypeToken<ConditionalNode>())
        {
          return false;
        }
        ConditionalNode *conditional = static_cast<ConditionalNode *>(node);
        conditional->applyRetainedProps(this->props);
        conditional->setCompositionSeatSlot(this->compositionSeatSlot());
        return true;
      }

      bool ConditionalDefinition::applyPropsToNode(Node *node) const
      {
        if (!this->repointRetainedNodeDefinition(node))
        {
          return false;
        }
        ConditionalNode *conditional = static_cast<ConditionalNode *>(node);
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
          static_cast<ConditionalNode *>(node)->setCompositionSeatSlot(this->compositionSeatSlot());
          node->setPropsTypeId(ConditionalProps::staticTypeId());
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
          static_cast<ConditionalNode *>(node)->setCompositionSeatSlot(this->compositionSeatSlot());
          node->setPropsTypeId(ConditionalProps::staticTypeId());
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
