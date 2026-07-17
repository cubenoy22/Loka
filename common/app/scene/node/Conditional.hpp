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
      class ConditionalNode : public NestableNode
      {
      public:
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
        static void onConditionChanged(void *userData);
        void compose();
        void updateActiveNode();
        virtual Node *retainedLifecycleBranch(unsigned index);

      protected:
        /** The condition must fall silent once this conditional is off the
            tree for good — the successor to the compose-detach unbind. */
        virtual void onLifecycleFactChanged(NodeLifecycleFact previous, NodeLifecycleFact next);

      public:
        Node *ensureBranchNode(bool cond, bool &created);
        void removeActiveNodeFromChildren();
        void render(IPlatformController *controller);
        short layout(IPlatformController *controller, LayoutState &state);

      private:
        void bindCondition();
        void unbindCondition();
        bool retainedDetached() const
        {
          return this->lifecycleFact() == NODE_FACT_DETACHED_RETAINED;
        }

        loka::core::State<bool> *boundCondition_;
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
          return 0;
        }
        virtual bool hasEquivalentProps(const NodeDefinitionBase &other) const
        {
          (void)other;
          return false;
        }
        virtual bool applyPropsToNode(Node *node) const
        {
          (void)node;
          return false;
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
