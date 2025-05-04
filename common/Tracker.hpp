#ifndef DECLARA_TRACKER_HPP
#define DECLARA_TRACKER_HPP

#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "ValueHolder.hpp"
#include "Property.hpp"

// 前方宣言
class StateBase;
class PagePropsBase;

class Tracker
{
public:
  virtual void begin() = 0;
  virtual void set(StateBase *s, const void *v) = 0; // Value型は今後拡張
  virtual void defer(std::function<void()> fn) = 0;
  virtual void markDirty(PropBase *prop) = 0;
  virtual bool end(PagePropsBase *props) = 0;
  virtual void registerProp(PropBase *dependent, StateBase *dependency) = 0;
  virtual ~Tracker() {}
};

// 標準実装例（骨格のみ）
class StdTracker : public Tracker
{
public:
  void begin() override
  {
    dirtyProps.clear();
    deferred.clear();
    dryRunValues.clear();
    dryRunDirty.clear();
  }
  void set(StateBase *s, const void *v) override
  {
    // 型安全な仮状態に値をセット
    dryRunValues[s].reset(new ValueHolder<std::string>(*static_cast<const std::string *>(v)));
    markDependentsDirty(s);
  }
  void defer(std::function<void()> fn) override
  {
    deferred.push_back(fn);
  }
  void markDirty(PropBase *prop) override
  {
    // dirtyPropsに重複なく追加
    for (size_t i = 0; i < dirtyProps.size(); ++i)
    {
      if (dirtyProps[i] == prop)
        return;
    }
    dirtyProps.push_back(prop);
    dryRunDirty[prop] = true;
  }
  bool end(PagePropsBase *props) override
  {
    // dryRun: dirtyPropsを再計算・伝播し、安定点（dirty=0）を検出
    size_t maxIter = 1000; // 無限ループ防止
    while (!dirtyProps.empty() && maxIter--)
    {
      std::vector<PropBase *> current = dirtyProps;
      dirtyProps.clear();
      for (size_t i = 0; i < current.size(); ++i)
      {
        PropBase *p = current[i];
        if (dryRunDirty[p])
        {
          dryRunDirty[p] = false;
          p->recompute();
        }
      }
    }
    // 型消去: 仮値を本値に反映
    for (std::map<StateBase *, std::unique_ptr<ValueHolderBase>>::iterator it = dryRunValues.begin(); it != dryRunValues.end(); ++it)
    {
      StateBase *s = it->first;
      ValueHolderBase *vh = it->second.get();
      if (s && vh)
        s->setValue(*vh);
    }
    // deferredを実行
    for (size_t i = 0; i < deferred.size(); ++i)
    {
      deferred[i]();
    }
    deferred.clear();
    dryRunValues.clear();
    dryRunDirty.clear();
    // 仮実装: dirtyPropsが空ならtrue
    return dirtyProps.empty();
  }
  // 依存元を明示的に受け取るregisterProp
  void registerProp(PropBase *dependent, StateBase *dependency) override
  {
    dependents[dependency].push_back(dependent);
  }
  // 複数依存元を一括登録できるAPIを追加
  void registerProp(PropBase *dependent, const std::vector<StateBase *> &dependencies)
  {
    for (size_t i = 0; i < dependencies.size(); ++i)
    {
      dependents[dependencies[i]].push_back(dependent);
    }
  }
  // dryRun時の型安全な値取得API
  template <typename T>
  bool getDryRunValue(StateBase *s, T &out) const
  {
    std::map<StateBase *, std::unique_ptr<ValueHolderBase>>::const_iterator it = dryRunValues.find(s);
    if (it != dryRunValues.end())
    {
      ValueHolder<T> *vh = dynamic_cast<ValueHolder<T> *>(it->second.get());
      if (vh)
      {
        out = vh->value;
        return true;
      }
    }
    return false;
  }
  ~StdTracker() {}

private:
  std::vector<PropBase *> dirtyProps;
  std::vector<std::function<void()>> deferred;
  std::map<StateBase *, std::unique_ptr<ValueHolderBase>> dryRunValues; // 型安全な仮状態
  std::map<PropBase *, bool> dryRunDirty;                               // 仮dirtyフラグ
  std::map<StateBase *, std::vector<PropBase *>> dependents;            // 依存グラフ
  void markDependentsDirty(StateBase *s)
  {
    // Stateの依存Propをdirtyにする
    std::vector<PropBase *> &deps = dependents[s];
    for (size_t i = 0; i < deps.size(); ++i)
    {
      markDirty(deps[i]);
    }
  }
};

#endif // DECLARA_TRACKER_HPP
