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
  // Node再利用カテゴリ・ヒューリスティック
  typedef SceneNode::NodeReuseCategory NodeReuseCategory;
  typedef SceneNode::NodeReuseHeuristic NodeReuseHeuristic;

  // NodePoolを外部注入 or デフォルト生成 + Stateバインド + 再利用戦略指定
  SceneNodeGroup(PoolType *pool = 0,
                 NodeReuseCategory cat = SceneNode::Reuse_Default,
                 NodeReuseHeuristic heur = SceneNode::ReuseHeuristic_Default)
      : nodePool_(pool ? pool : new SceneNodeDefaultPool<SceneNode>()),
        boundState_(0), reuseCategory_(cat), reuseHeuristic_(heur), parentGroup_(0) {}
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

  // StateバインドAPI（defer/observe用）
  void bindDefer(StateBase *state) { boundState_ = state; }
  StateBase *boundState() const { return boundState_; }

  // Node再利用カテゴリ・ヒューリスティックのgetter/setter
  NodeReuseCategory getReuseCategory() const
  {
    if (reuseCategory_ == SceneNode::Reuse_FollowParent && parentGroup_)
      return parentGroup_->getReuseCategory();
    return reuseCategory_;
  }
  void setReuseCategory(NodeReuseCategory cat) { reuseCategory_ = cat; }
  NodeReuseHeuristic getReuseHeuristic() const
  {
    if (reuseHeuristic_ == SceneNode::ReuseHeuristic_FollowParent && parentGroup_)
      return parentGroup_->getReuseHeuristic();
    return reuseHeuristic_;
  }
  void setReuseHeuristic(NodeReuseHeuristic heur) { reuseHeuristic_ = heur; }

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
  // Stateバインド用メンバ
  StateBase *boundState_;
  NodeReuseCategory reuseCategory_;
  NodeReuseHeuristic reuseHeuristic_;
  SceneNodeGroup *parentGroup_;
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

#endif // DECLARA_SCENENODEGROUP_HPP
