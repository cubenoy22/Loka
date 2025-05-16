#ifndef DECLARA_SCENENODEGROUP_HPP
#define DECLARA_SCENENODEGROUP_HPP

#include <set>
#include "core/SceneNode.hpp"

// --- SceneNodeGroup: ノードの集合・一括管理 + NodePool所有 ---
class SceneNodeGroup
{
public:
  // NodePool型定義
  typedef SceneNodePool<SceneNode> PoolType;

  // NodePoolを外部注入 or デフォルト生成
  SceneNodeGroup(PoolType *pool = 0)
      : nodePool_(pool ? pool : new SceneNodeDefaultPool<SceneNode>()) {}
  virtual ~SceneNodeGroup()
  {
    clear();
    if (nodePool_)
      delete nodePool_;
  }

  void add(SceneNode *node)
  {
    if (node)
    {
      nodes_.insert(node);
      if (nodePool_)
        nodePool_->add(node);
    }
  }
  void remove(SceneNode *node)
  {
    nodes_.erase(node);
    if (nodePool_)
      nodePool_->remove(node);
  }
  void clear()
  {
    // NodePoolがclear()を持つ場合のみ呼ぶ
    SceneNodeDefaultPool<SceneNode> *defPool = dynamic_cast<SceneNodeDefaultPool<SceneNode> *>(nodePool_);
    if (defPool)
      defPool->clear();
    nodes_.clear();
  }
  bool contains(SceneNode *node) const
  {
    return nodes_.find(node) != nodes_.end();
  }
  size_t size() const { return nodes_.size(); }

  PoolType *nodePool() { return nodePool_; }
  const PoolType *nodePool() const { return nodePool_; }

  // イテレータ提供
  typedef std::set<SceneNode *>::iterator iterator;
  typedef std::set<SceneNode *>::const_iterator const_iterator;
  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }
  const_iterator begin() const { return nodes_.begin(); }
  const_iterator end() const { return nodes_.end(); }

protected:
  std::set<SceneNode *> nodes_;
  PoolType *nodePool_; // グループ単位のNodePool
};

// --- NodeGroupScope: RAIIで所属グループを一時切替 ---
class NodeGroupScope
{
public:
  NodeGroupScope(SceneNodeGroup *group)
  {
    prev_ = currentGroup_;
    currentGroup_ = group;
  }
  ~NodeGroupScope()
  {
    currentGroup_ = prev_;
  }
  static SceneNodeGroup *current() { return currentGroup_; }

private:
  SceneNodeGroup *prev_;
  // C++98: スレッドローカル不可。グローバルstaticで簡易実装。
  static SceneNodeGroup *currentGroup_;
};

// グローバルstatic変数の定義
SceneNodeGroup *NodeGroupScope::currentGroup_ = 0;

#endif // DECLARA_SCENENODEGROUP_HPP
