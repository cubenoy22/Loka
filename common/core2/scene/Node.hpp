#ifndef DECLARA_CORE2_SCENE_NODE_HPP
#define DECLARA_CORE2_SCENE_NODE_HPP

// C++98向けstatic_assert風マクロ
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]

#include <cstddef>
#include <vector>

#include "../../core/State.hpp"
#include "StreamView.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // DirtyType: Nodeのdirty状態を表す（C++98互換）
      enum DirtyType
      {
        NONE = 0x00,
        PROPS = 0x01,
        CHILD = 0x02,
        LAYOUT = 0x04,
        MYSELF = 0xFF // 全dirty
      };

      struct NodeContext; // Opaque type

      // Minimal NodeContext implementation
      struct NodeContext
      {
        // TODO: Add actual context data
        virtual ~NodeContext() {}
      };

      class Node
      {
      public:
        NodeContext *context;
        MutableState<DirtyType> dirty;
        virtual ~Node() {}
        virtual void compose() {}

        Node() : context(0), dirty(NONE) {}
      };

      // --- 汎用Props基底 ---
      struct PropsBase
      {
        virtual ~PropsBase() {}
        typedef Node *(*NodeFactoryFunc)(const PropsBase &);
        virtual NodeFactoryFunc nodeFactory() const = 0;
        virtual bool operator<(const PropsBase &rhs) const = 0;
      };

      // --- テンプレート共通化: NodePropsBase ---
      template <class PropsT>
      struct NodePropsBase : public PropsBase
      {
        static Node *createNode(const PropsBase &base)
        {
          const PropsT &props = static_cast<const PropsT &>(base);
          return PropsT::createNode(props);
        }
        static NodeFactoryFunc staticFactory() { return &NodePropsBase::createNode; }
        NodeFactoryFunc nodeFactory() const { return staticFactory(); }
      };

      // --- NodeDefinitionの型消去基底 ---
      struct NodeDefinitionBase
      {
        virtual ~NodeDefinitionBase() {}
        virtual Node *create() const = 0;
      };

      // --- NodeDefinition: Props/Nodeの外部ラッパー（Propsをメンバーとして持つインスタンス型） ---
      template <class PropsT, class NodeT>
      struct NodeDefinition : public NodeDefinitionBase
      {
        typedef PropsT PropsType;
        typedef NodeT NodeType;

        // PropsT/NodeTにTypeTagが存在する場合のみ静的チェック（SFINAE）
#ifdef DECLARA_NODEDEF_CHECK_TYPETAG
        STATIC_ASSERT((typename PropsT::TypeTag *)0 == (typename NodeT::TypeTag *)0, props_node_type_mismatch);
#endif
        PropsT props;
        NodeDefinition() : props() {}
        NodeDefinition(const PropsT &p) : props(p) {}
        NodeDefinition &set(const PropsT &p)
        {
          props = p;
          return *this;
        }
        template <typename F>
        NodeDefinition &mutate(F f)
        {
          f(props);
          return *this;
        }
        Node *create() const { return new NodeT(props); }
      };

      // --- 子を持てるNodeDefinition用インターフェース ---
      struct INestableDefinition
      {
        virtual ~INestableDefinition() {}
        virtual void addChild(NodeDefinitionBase *child) = 0;
        virtual const std::vector<NodeDefinitionBase *> &getChildren() const = 0;

        // 既存
        INestableDefinition &operator<<(NodeDefinitionBase &child);
        INestableDefinition &operator<<(const NodeDefinitionBase &child);

        // 明示的な単体用operator<<（begin/end不要）
        // 型安全のため、NodeDefinitionBaseのみ受け入れる形に統一

        // vector<NodeDefinitionBase*>専用 明示的オーバーロード（C++98対応）
        INestableDefinition &operator<<(const std::vector<NodeDefinitionBase *> &container);

        // 追加: StreamView用
      };

      // --- 子を持てるNode/Definition用インターフェース ---
      struct INestable
      {
        virtual ~INestable() {}
        virtual void addChild(Node *child) = 0;
        virtual const std::vector<Node *> &getChildren() const = 0;
      };

      // --- 子を保持するNodeのヘルパー実装 ---
      class NestableNode : public Node, public INestable
      {
      public:
        NestableNode() : Node() {}
        virtual ~NestableNode()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
        }

        virtual void addChild(Node *child)
        {
          if (child)
          {
            children_.push_back(child);
          }
        }

        virtual const std::vector<Node *> &getChildren() const { return children_; }

      protected:
        std::vector<Node *> children_;
      };

      inline INestableDefinition &INestableDefinition::operator<<(NodeDefinitionBase &child)
      {
        addChild(&child);
        return *this;
      }
      inline INestableDefinition &INestableDefinition::operator<<(const NodeDefinitionBase &child)
      {
        addChild(const_cast<NodeDefinitionBase *>(&child));
        return *this;
      }

      // 汎用コンテナ（vector, initializer_list等）用operator<<の実装
      inline INestableDefinition &INestableDefinition::operator<<(const std::vector<NodeDefinitionBase *> &container)
      {
        for (size_t i = 0; i < container.size(); ++i)
        {
          addChild(container[i]);
        }
        return *this;
      }
    } // namespace scene
  } // namespace core
} // namespace declara

// Conditional.hppの実装をインクルード（前方宣言後）
#include "node/Conditional.hpp"

// ConditionalNode のインライン実装
namespace declara
{
  namespace core
  {
    namespace scene
    {
      inline ConditionalProps::ConditionalProps(State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(cond), trueDef(tDef), falseDef(fDef) {}

      inline ConditionalProps::ConditionalProps(const State<bool> *cond, NodeDefinitionBase *tDef, NodeDefinitionBase *fDef)
          : condition(const_cast<State<bool> *>(cond)), trueDef(tDef), falseDef(fDef) {}

      inline ConditionalNode::ConditionalNode(const ConditionalProps &p)
          : props(p), activeNode(0)
      {
        if (props.condition)
        {
          props.condition->deferBind(&ConditionalNode::onConditionChanged, this);
        }
        updateActiveNode();
      }

      inline ConditionalNode::~ConditionalNode()
      {
        if (props.condition)
        {
          props.condition->deferUnbind(&ConditionalNode::onConditionChanged, this);
        }
      }

      inline void ConditionalNode::onConditionChanged(void *userData)
      {
        ConditionalNode *self = static_cast<ConditionalNode *>(userData);
        if (self)
          self->updateActiveNode();
      }

      inline void ConditionalNode::compose()
      {
        updateActiveNode();
        if (activeNode)
          activeNode->compose();
      }

      inline void ConditionalNode::updateActiveNode()
      {
        if (props.condition && props.trueDef && props.falseDef)
        {
          if (props.condition->get())
            activeNode = props.trueDef->create();
          else
            activeNode = props.falseDef->create();
        }
      }

      inline ConditionalDefinition::ConditionalDefinition(const ConditionalProps &p) : props(p) {}

      inline Node *ConditionalDefinition::create() const { return new ConditionalNode(props); }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_HPP
