#ifndef DECLARA_SCENENODE_HPP
#define DECLARA_SCENENODE_HPP

#include <set>
#include "core/State.hpp"

// --- SceneNodeAllocator: NodeT専用のノード生成インターフェース ---
template <class NodeT>
class SceneNodeAllocator
{
public:
  typedef NodeT NodeType;
  virtual ~SceneNodeAllocator() {}
  virtual NodeT *allocNode(void *userInfo = 0) = 0;
  // 型安全なInitInfo用のデフォルト実装（必要に応じてサブクラスでオーバーロード）
  template <typename InitInfo>
  NodeT *allocNode(const InitInfo &info) { return 0; }
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
class SceneNodeGroup;
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

// --- SceneNode: UIツリーの最小単位（再利用ヒント対応） ---
class SceneNode
{
public:
  enum NodeReuseHint
  {
    Default,  // 通常再利用
    Stable,   // 差分チェック不要（絶対に変わらない）
    Singleton // 解放しない（ViewModel的）
  };

  enum Phase
  {
    CREATED,
    ATTACHED,
    DETACHED,
    RECYCLED,
    PURGED,
    DESTROYED
  };

  SceneNode()
      : reuseHint_(Default), reusePriority_(50), currentPhase_(CREATED) {}
  virtual ~SceneNode() {}

  // --- NodeReuseHint: 再利用ヒント ---
  NodeReuseHint getReuseHint() const { return reuseHint_; }
  void setReuseHint(NodeReuseHint hint) { reuseHint_ = hint; }

  // --- reusePriority: 再利用優先度（0=即解放, 50=標準, 100=温存/最優先） ---
  int getReusePriority() const { return reusePriority_; }
  void setReusePriority(int pri) { reusePriority_ = pri; }

  // --- Phase: ライフサイクル状態 ---
  const State<Phase> &phase() const { return currentPhase_; }

protected:
  void setPhase(Phase p) { currentPhase_.set(p); }
  template <class AllocatorType, class NodeT, class GroupType>
  friend class SceneNodeController;

  NodeReuseHint reuseHint_;
  int reusePriority_; // 0-100, デフォルト50
  MutableState<Phase> currentPhase_;
};

// --- SceneNodeController: GroupごとPool設計にリファクタ ---
template <class AllocatorType, class NodeT = typename AllocatorType::NodeType, class GroupType = SceneNodeGroup>
class SceneNodeController
{
public:
  typedef NodeT NodeType;
  typedef SceneNodeReusePool<NodeT> ReusePoolType;
  typedef SceneNodePool<NodeT> PoolType;
  typedef GroupType GroupTypeAlias;

  SceneNodeController(
      AllocatorType *allocator,
      ReusePoolType *reusePool,
      SceneNodeDeallocator *deallocator,
      GroupType *group)
      : allocator_(allocator), reusePool_(reusePool), deallocator_(deallocator), group_(group) {}

  // 型安全なacquireNode: メンバ関数テンプレート
  template <typename InitInfo>
  NodeType *acquireNode(const InitInfo &info)
  {
    NodeType *node = reusePool_->pop();
    if (node)
    {
      group_->nodePool()->add(node);
      // 必要に応じてinfoをnodeに反映（サブクラスで拡張）
      return node;
    }
    node = allocator_->allocNode(info);
    group_->nodePool()->add(node);
    return node;
  }

  void releaseNode(NodeType *node)
  {
    group_->nodePool()->remove(node);
    reusePool_->push(node);
    deallocator_->scheduleDelete(node);
  }

private:
  AllocatorType *allocator_;
  ReusePoolType *reusePool_;
  SceneNodeDeallocator *deallocator_;
  GroupType *group_;
};

#endif // DECLARA_SCENENODE_HPP
