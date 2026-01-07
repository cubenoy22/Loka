#ifndef LOKA_SCENEMANAGER2_HPP
#define LOKA_SCENEMANAGER2_HPP

#include "core2/scene/Scene.hpp"
#include "State.hpp"
#include "StateTracker.hpp"
#include "loka/dsl/CompositionList.hpp"

class Window;

class SceneManager2
{
public:
  struct SceneTransaction
  {
    SceneTransaction()
        : from(0),
          to(0),
          nextInComposition(0)
    {
    }

    SceneTransaction(declara::core::scene::Scene *fromScene, declara::core::scene::Scene *toScene)
        : from(fromScene),
          to(toScene),
          nextInComposition(0)
    {
    }

    SceneTransaction(const SceneTransaction &other)
        : from(other.from),
          to(other.to),
          nextInComposition(0)
    {
    }

    SceneTransaction &operator=(const SceneTransaction &other)
    {
      if (this == &other)
        return *this;
      from = other.from;
      to = other.to;
      nextInComposition = 0;
      return *this;
    }

    SceneTransaction *clone() const { return new SceneTransaction(*this); }

    declara::core::scene::Scene *from;
    declara::core::scene::Scene *to;
    SceneTransaction *nextInComposition;
  };

  class SceneTransactionList
  {
  public:
    SceneTransactionList() : list_() {}
    SceneTransactionList(const SceneTransactionList &other) : list_() { copyFrom(other); }
    ~SceneTransactionList() { list_.clear(); }

    SceneTransactionList &operator=(const SceneTransactionList &other)
    {
      if (this == &other)
        return *this;
      list_.clear();
      copyFrom(other);
      return *this;
    }

    bool operator!=(const SceneTransactionList &other) const { return !equals(other); }
    bool operator==(const SceneTransactionList &other) const { return equals(other); }

    void push(declara::core::scene::Scene *from, declara::core::scene::Scene *to)
    {
      list_.appendOwned(new SceneTransaction(from, to));
    }

    bool empty() const { return list_.count() == 0; }
    size_t size() const { return list_.count(); }
    SceneTransaction *head() const { return list_.head(); }

    void popFront()
    {
      SceneTransaction *item = list_.head();
      if (!item)
        return;
      if (list_.remove(item))
      {
        delete item;
      }
    }

  private:
    void copyFrom(const SceneTransactionList &other)
    {
      SceneTransaction *cur = other.list_.head();
      while (cur)
      {
        list_.appendClone(*cur);
        cur = cur->nextInComposition;
      }
    }

    bool equals(const SceneTransactionList &other) const
    {
      if (list_.count() != other.list_.count())
        return false;
      loka::dsl::CompositionCursor<SceneTransaction> left(list_.head(), list_.count());
      loka::dsl::CompositionCursor<SceneTransaction> right(other.list_.head(), other.list_.count());
      for (SceneTransaction *l = left.next(), *r = right.next(); l && r; l = left.next(), r = right.next())
      {
        if (l->from != r->from || l->to != r->to)
          return false;
      }
      return true;
    }

    loka::dsl::CompositionList<SceneTransaction> list_;
  };

  SceneManager2();
  ~SceneManager2();

  // トランザクション追加
  void commitTransaction(declara::core::scene::Scene *from, declara::core::scene::Scene *to);
  // 現在のシーン取得
  const State<declara::core::scene::Scene *> &getCurrentScene() const;

protected:
  // ペンディングトランザクション取得
  SceneTransactionList getPendingTransactions() const;
  // トランザクション進行
  void handleNextTransaction();
  // 副作用: シーン切り替え
  void swapScene(declara::core::scene::Scene *oldScene, declara::core::scene::Scene *newScene);

public:
  void setWindow(Window *window) { window_ = window; }
  Window *window() const { return window_; }

private:
  MutableState<declara::core::scene::Scene *> currentScene_;
  MutableState<SceneTransactionList> pendingTransactions_;
  declara::core::PushStateTracker tracker_;
  Window *window_;
};

#endif // LOKA_SCENEMANAGER2_HPP
