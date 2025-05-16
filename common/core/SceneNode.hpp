#ifndef DECLARA_SCENENODE_HPP
#define DECLARA_SCENENODE_HPP

#include <set>

// --- SceneNodeAllocator: NodeT専用のノード生成インターフェース ---
template <class NodeT>
class SceneNodeAllocator
{
public:
  virtual ~SceneNodeAllocator() {}
  virtual NodeT *allocNode(void *userInfo = 0) = 0;
};

// --- SceneNodeReusePool: NodeT専用の再利用・一時保持戦略 ---
template <class NodeT>
class SceneNodeReusePool
{
public:
  virtual ~SceneNodeReusePool() {}
  virtual void push(NodeT *node) = 0;
  virtual NodeT *pop() = 0;
};

// --- SceneNodeZeroReusePool: pop()は常にnullptr、push()は何もしない最小実装 ---
template <class NodeT>
class SceneNodeZeroReusePool : public SceneNodeReusePool<NodeT>
{
public:
  void push(NodeT * /*node*/) {}
  NodeT *pop() { return 0; }
};

// --- SceneNodeDeallocator: SceneNode*専用の解放戦略 ---
class SceneNode;
class SceneNodeDeallocator
{
public:
  virtual ~SceneNodeDeallocator() {}
  virtual void scheduleDelete(SceneNode *node) = 0;
};

// --- SceneNodeDefaultDeallocator: 即時deleteするだけの実装 ---
class SceneNodeDefaultDeallocator : public SceneNodeDeallocator
{
public:
  void scheduleDelete(SceneNode *node) { delete node; }
};

// --- SceneNodePool: 利用中ノードの一元管理プール ---
template <class NodeT>
class SceneNodePool
{
public:
  virtual ~SceneNodePool() {}
  virtual void add(NodeT *node) = 0;
  virtual void remove(NodeT *node) = 0;
  virtual bool contains(NodeT *node) const = 0;
  // 必要に応じて: 全ノード列挙、クリア等
};

// --- SceneNodeDefaultPool: 単純なstd::setで管理するデフォルト実装 ---
template <class NodeT>
class SceneNodeDefaultPool : public SceneNodePool<NodeT>
{
public:
  void add(NodeT *node) { nodes_.insert(node); }
  void remove(NodeT *node) { nodes_.erase(node); }
  bool contains(NodeT *node) const { return nodes_.find(node) != nodes_.end(); }
  void clear() { nodes_.clear(); }

private:
  std::set<NodeT *> nodes_;
};

// --- SceneNodeController: Poolも必須引数に変更 ---
template <class AllocatorType, class NodeT = typename AllocatorType::NodeType>
class SceneNodeController
{
public:
  typedef NodeT NodeType;
  typedef SceneNodeReusePool<NodeT> ReusePoolType;
  typedef SceneNodePool<NodeT> PoolType;
  SceneNodeController(
      AllocatorType *allocator,
      ReusePoolType *reusePool,
      SceneNodeDeallocator *deallocator,
      PoolType *nodePool)
      : allocator_(allocator), reusePool_(reusePool), deallocator_(deallocator), nodePool_(nodePool) {}

  NodeType *acquireNode(void *userInfo = 0)
  {
    NodeType *node = reusePool_->pop();
    if (node)
    {
      nodePool_->add(node);
      return node;
    }
    node = allocator_->allocNode(userInfo);
    nodePool_->add(node);
    return node;
  }
  void releaseNode(NodeType *node)
  {
    nodePool_->remove(node);
    reusePool_->push(node);
    deallocator_->scheduleDelete(node);
  }

private:
  AllocatorType *allocator_;
  ReusePoolType *reusePool_;
  SceneNodeDeallocator *deallocator_;
  PoolType *nodePool_;
};

#endif // DECLARA_SCENENODE_HPP
