#ifndef DECLARA_SCENENODEGROUP_HPP
#define DECLARA_SCENENODEGROUP_HPP

#include <set>
#include <vector>
#include "core/SceneNode.hpp"
#include "core/State.hpp"
#include "core/util/SceneNodeAttachScope.hpp"

// --- SceneNodeGroup: ノードの集合・一括管理 + NodePool所有 ---
class SceneNodeGroup : public SceneNode
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
                 NodeReuseHeuristic heur = SceneNode::ReuseHeuristic_Default);

  // State依存リストを受け取るコンストラクタを追加
  SceneNodeGroup(const std::vector<StateBase *> &recomposeDeps,
                 PoolType *pool = 0,
                 NodeReuseCategory cat = SceneNode::Reuse_Default,
                 NodeReuseHeuristic heur = SceneNode::ReuseHeuristic_Default);

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
      // Remove from previous parent if needed
      if (node->getParentGroup() && node->getParentGroup() != this)
      {
        node->getParentGroup()->remove(node);
      }
      // Avoid duplicate add
      nodes_.erase(node);
      nodes_.insert(node);
      if (nodePool_)
        nodePool_->add(node);
      node->setParentGroup(this);
    }
  }
  void remove(SceneNode *node)
  {
    nodes_.erase(node);
    if (nodePool_)
      nodePool_->remove(node);
    if (node && node->getParentGroup() == this)
      node->setParentGroup(nullptr);
  }
  void clear()
  {
    // NodePoolがclear()を持つ場合のみ呼ぶ
    SceneNodeDefaultPool<SceneNode> *defPool = dynamic_cast<SceneNodeDefaultPool<SceneNode> *>(nodePool_);
    if (defPool)
      defPool->clear();
    // for (SceneNode *node : nodes_)
    // {
    //   delete node;
    // }
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
  void bindDefer(StateBase *state)
  {
    if (!state)
      return;
    // すでにバインド済みなら何もしない
    for (size_t i = 0; i < boundStates_.size(); ++i)
    {
      if (boundStates_[i] == state)
        return;
    }
    boundStates_.push_back(state);
    // コールバック登録（deferBind）
    state->deferBind(&SceneNodeGroup::onRecomposeStatic, this);
  }
  // バインド済みState一覧取得
  const std::vector<StateBase *> &boundStates() const { return boundStates_; }

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
  static void onRecomposeStatic(void *userData)
  {
    SceneNodeGroup *self = static_cast<SceneNodeGroup *>(userData);
    if (self)
      self->recompose();
  }
  virtual void recompose()
  {
    // デフォルトはclearのみ。将来unbindAll()もここで呼ぶ
    clear();
    // 利用側でcompose()を呼ぶこと
  }

  std::set<SceneNode *> nodes_;
  PoolType *nodePool_; // グループ単位のNodePool
  // Stateバインド用メンバ
  StateBase *boundState_;
  NodeReuseCategory reuseCategory_;
  NodeReuseHeuristic reuseHeuristic_;
  SceneNodeGroup *parentGroup_;
  std::vector<StateBase *> boundStates_;
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

inline SceneNodeGroup &operator<<(SceneNodeGroup &group, SceneNode *node)
{
  group.add(node);
  return group;
}

#endif // DECLARA_SCENENODEGROUP_HPP
