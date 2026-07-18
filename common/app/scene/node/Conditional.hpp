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
        Node *trueNode_;
        Node *falseNode_;
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
        void updateActiveNode();
        virtual Node *retainedLifecycleBranch(unsigned index);
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<ConditionalNode>();
        }
        /** Re-points borrowed branch definitions and the condition source for
            a retained seat. */
        void applyRetainedProps(const ConditionalProps &nextProps);

      protected:
        virtual void evaluateChildrenForScheduledApply();

      public:
        Node *ensureBranchNode(bool cond, bool &created);
        void removeActiveNodeFromChildren();
        void render(IPlatformController *controller);
        short layout(IPlatformController *controller, LayoutState &state);

      private:
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
        virtual bool applyPropsToNode(Node *node) const;

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
