#ifndef LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
#define LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP

#include "core/State.hpp"
#include "../Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      // Forward declarations
      class Node;
      class ConditionalNode;
      struct NodeDefinitionBase;

      struct ConditionalTypeTag
      {
      };

      struct ConditionalProps : public NodePropsBase<ConditionalProps>
      {
        typedef ConditionalTypeTag TypeTag;
        typedef ConditionalNode NodeType;
        loka::core::State<bool> *condition;
        NodeDefinitionBase *trueDef;
        NodeDefinitionBase *falseDef;
        ConditionalProps(loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
        ConditionalProps(const loka::core::State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef);
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
          {
            return false;
          }
          const ConditionalProps &other = static_cast<const ConditionalProps &>(rhs);
          return this->condition < other.condition;
        }
      };

      // ConditionalNode: node that switches by condition
      class ConditionalNode : public NestableNode
      {
      public:
        typedef ConditionalTypeTag TypeTag;
        ConditionalProps props;
        Node *activeNode;
        ConditionalNode(const ConditionalProps &p);
        ~ConditionalNode();
        virtual void declareDirtySources(DirtySourceRegistrar &registrar)
        {
          if (this->props.condition)
          {
            registrar.markDirtyOnChange(this->props.condition,
                                        static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_LAYOUT));
          }
        }
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<ConditionalNode>();
        }
        virtual Node *retainedLifecycleBranch(unsigned index);
        /** Re-points borrowed branch definitions and the condition source for
            a retained seat. */
        void applyRetainedProps(const ConditionalProps &nextProps);
        void setCompositionSeatSlot(int slot)
        {
          this->compositionSeatSlot_ = slot;
        }

      protected:
        virtual void evaluateChildrenForScheduledApply(ComponentContext &context,
                                                       BoundaryNode *boundary);
        virtual bool reconcileForScheduledBranchReentry(ComponentContext &context,
                                                        BoundaryNode *boundary);

      public:
        Node *createBranchNode(bool cond);
        void removeActiveNodeFromChildren();
        void render(IPlatformController *controller);
        short layout(IPlatformController *controller, LayoutState &state);

      private:
        void initializeActiveNode();
        bool updateActiveNode(ComponentContext &context,
                              BoundaryNode *boundary,
                              bool reconcileCurrentBranch);
        NodeDefinitionBase *branchDefinition(bool cond) const;
        int compositionSeatSlot_;
        bool activeCondition_;
        bool hasActiveCondition_;
#if defined(TEST_BUILD)
        BoundaryNode *testBoundaryOwner_;
#endif
        bool retainedDetached() const
        {
          return this->lifecycleFact() == NODE_FACT_DETACHED_RETAINED;
        }
      };

      struct ConditionalDefinition : public NodeDefinitionBase
      {
        ConditionalProps props;
        NodeDefinitionBase *ownedTrueDef;
        NodeDefinitionBase *ownedFalseDef;
        ConditionalDefinition(const ConditionalProps &p);
        ConditionalDefinition(const ConditionalDefinition &other);
        ConditionalDefinition &operator=(const ConditionalDefinition &other);
        ~ConditionalDefinition();
        Node *create() const;
        Node *createInPlace(void *mem) const;
        size_t nodeSize() const;
        size_t nodeAlign() const;
        virtual NodeDefinitionBase *clone() const;
        virtual NodeKind nodeKind() const
        {
          return NODE_KIND_UNKNOWN;
        }
        virtual const PropsBase *propsBase() const
        {
          return &this->props;
        }
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const;
        virtual bool repointRetainedNodeDefinition(Node *node) const;
        virtual bool applyPropsToNode(Node *node) const;
        virtual bool isCompatibleWithNode(const Node *node) const
        {
          return node && node->nodeTypeKey() == NodeTypeToken<ConditionalNode>();
        }
        virtual NodeDefinitionBase *retainedDefinitionBranch(unsigned index)
        {
          if (index == 0)
          {
            return this->ownedTrueDef;
          }
          if (index == 1)
          {
            return this->ownedFalseDef;
          }
          return 0;
        }

      private:
        bool assignFrom(const ConditionalProps &nextProps,
                        const NodeDefinitionBase *trueDef,
                        const NodeDefinitionBase *falseDef,
                        const NodeDefinitionBase *nextBase);
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_CONDITIONAL_HPP
