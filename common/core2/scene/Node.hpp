#ifndef DECLARA_CORE2_SCENE_NODE_HPP
#define DECLARA_CORE2_SCENE_NODE_HPP

// C++98向けstatic_assert風マクロ
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]

namespace declara
{
  namespace core
  {
    namespace scene
    {

      class Node
      {
      public:
        virtual ~Node() {}
        virtual void compose() {}
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
        // Node生成APIの型消去（必要ならvirtual Node* create() = 0; なども追加可）
      };

      // --- NodeDefinition: Props/Nodeの外部ラッパー（Propsをメンバーとして持つインスタンス型） ---
      template <class PropsT, class NodeT>
      struct NodeDefinition : public NodeDefinitionBase
      {
        typedef PropsT PropsType;
        typedef NodeT NodeType;

        // PropsTとNodeTのTypeTag一致を静的にチェック
        STATIC_ASSERT((typename PropsT::TypeTag *)0 == (typename NodeT::TypeTag *)0, props_node_type_mismatch);
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
      };

      // --- 子を持てるNode/Definition用インターフェース ---
      struct INestable
      {
        virtual ~INestable() {}
        virtual void addChild(Node *child) = 0;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_HPP
