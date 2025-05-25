#pragma once

#include <map>

// --- 汎用Props基底 ---
struct PropsBase
{
  virtual ~PropsBase() {}
  typedef SceneNode *(*NodeFactoryFunc)(const PropsBase &);
  virtual NodeFactoryFunc nodeFactory() const = 0;
  virtual bool operator<(const PropsBase &rhs) const = 0;
};

// --- テンプレート共通化: NodePropsBase ---
template <class PropsT>
struct NodePropsBase : public PropsBase
{
  static SceneNode *createNode(const PropsBase &base)
  {
    const PropsT &props = static_cast<const PropsT &>(base);
    return PropsT::createNode(props);
  }
  static NodeFactoryFunc staticFactory() { return &NodePropsBase::createNode; }
  NodeFactoryFunc nodeFactory() const { return staticFactory(); }
};

// C++98向けstatic_assert風マクロ
#define STATIC_ASSERT(expr, msg) typedef char static_assert_##msg[(expr) ? 1 : -1]

// --- NodeDefinition: Props/Nodeの外部ラッパー（Propsをメンバーとして持つインスタンス型） ---
template <class PropsT, class NodeT>
struct NodeDefinition
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
  SceneNode *create() const { return new NodeT(props); }
};

class SceneNodeFactory
{
public:
  typedef PropsBase::NodeFactoryFunc NodeFactoryFunc;
  static SceneNodeFactory *instance()
  {
    static SceneNodeFactory *inst = new SceneNodeFactory();
    return inst;
  }
  SceneNode *getOrCreate(const PropsBase *props)
  {
    NodeFactoryFunc key = props->nodeFactory();
    std::map<const PropsBase *, SceneNode *, PropsLess> &group = nodeCache[key];
    std::map<const PropsBase *, SceneNode *, PropsLess>::iterator it = group.find(props);
    if (it != group.end())
      return it->second;
    SceneNode *node = key(*props);
    group[props] = node;
    return node;
  }
  void clearAll()
  {
    for (NodeCacheType::iterator kit = nodeCache.begin(); kit != nodeCache.end(); ++kit)
    {
      std::map<const PropsBase *, SceneNode *, PropsLess> &group = kit->second;
      for (std::map<const PropsBase *, SceneNode *, PropsLess>::iterator it = group.begin(); it != group.end(); ++it)
      {
        delete it->second;
      }
      group.clear();
    }
    nodeCache.clear();
  }

private:
  struct PropsLess
  {
    bool operator()(const PropsBase *a, const PropsBase *b) const { return a < b; }
  };
  typedef std::map<const PropsBase *, SceneNode *, PropsLess> GroupMapType;
  typedef std::map<NodeFactoryFunc, GroupMapType> NodeCacheType;
  NodeCacheType nodeCache;
};
