#ifndef LOKA_SCENEMANAGER_HPP
#define LOKA_SCENEMANAGER_HPP

#include "app/scene/Scene.hpp"
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/dsl/CompositionList.hpp"

class Window;

class SceneManager
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

    SceneTransaction(loka::app::scene::Scene *fromScene, loka::app::scene::Scene *toScene)
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

    loka::app::scene::Scene *from;
    loka::app::scene::Scene *to;
    SceneTransaction *nextInComposition;
  };

    class SceneTransactionList
    {
    public:
    SceneTransactionList() : list_(), id_(nextId()) {}
    SceneTransactionList(const SceneTransactionList &other) : list_(), id_(other.id_) { copyFrom(other); }
    ~SceneTransactionList() { list_.clear(); }

    SceneTransactionList &operator=(const SceneTransactionList &other)
    {
      if (this == &other)
        return *this;
      list_.clear();
      copyFrom(other);
      id_ = other.id_;
      return *this;
    }

    bool operator!=(const SceneTransactionList &other) const { return id_ != other.id_; }
    bool operator==(const SceneTransactionList &other) const { return id_ == other.id_; }

    void push(loka::app::scene::Scene *from, loka::app::scene::Scene *to)
    {
      list_.appendOwned(new SceneTransaction(from, to));
      id_ = nextId();
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
        id_ = nextId();
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

    loka::dsl::CompositionList<SceneTransaction> list_;
    unsigned long id_;
    static unsigned long nextId_;
    static unsigned long nextId() { return nextId_++; }
  };

  SceneManager();
  ~SceneManager();

  // Queue and apply a scene transition immediately.
  void commitTransaction(loka::app::scene::Scene *from, loka::app::scene::Scene *to);
  // Return the currently attached scene state.
  const loka::core::State<loka::app::scene::Scene *> &getCurrentScene() const;

protected:
  // Return the pending transition queue snapshot.
  SceneTransactionList getPendingTransactions() const;
  // Consume the next queued transition.
  void handleNextTransaction();
  // Apply the actual scene swap side effects.
  void swapScene(loka::app::scene::Scene *oldScene, loka::app::scene::Scene *newScene);

public:
  void setWindow(Window *window) { window_ = window; }
  Window *window() const { return window_; }

private:
  loka::core::MutableState<loka::app::scene::Scene *> currentScene_;
  loka::core::MutableState<SceneTransactionList> pendingTransactions_;
  loka::core::PushStateTracker tracker_;
  Window *window_;
};

#endif // LOKA_SCENEMANAGER_HPP
