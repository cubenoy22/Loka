#include "SceneManager2.hpp"
#include <cassert>
#include "core2/scene/Scene.hpp"
#include "util/StateUtil.hpp"

unsigned long SceneManager2::SceneTransactionList::nextId_ = 1;

SceneManager2::SceneManager2()
    : currentScene_(0),
      pendingTransactions_(),
      tracker_(),
      window_(0)
{
  tracker_.addState(&currentScene_);
  tracker_.addState(&pendingTransactions_);
}

SceneManager2::~SceneManager2() {}

void SceneManager2::commitTransaction(declara::core::scene::Scene *from, declara::core::scene::Scene *to)
{
  tracker_.begin();
  const SceneTransactionList &current = pendingTransactions_.getRef();
  SceneTransactionList txns = current;
  txns.push(from, to);
  pendingTransactions_.set(txns);
  tracker_.end();
  handleNextTransaction(); // 追加: 追加直後に即時遷移
}

const State<declara::core::scene::Scene *> &SceneManager2::getCurrentScene() const
{
  return currentScene_;
}

SceneManager2::SceneTransactionList SceneManager2::getPendingTransactions() const
{
  return pendingTransactions_.get();
}

void SceneManager2::handleNextTransaction()
{
  const SceneTransactionList &txns = pendingTransactions_.getRef();
  if (txns.empty())
    return;
  declara::core::scene::Scene *oldScene = currentScene_.get();
  declara::core::scene::Scene *newScene = txns.head()->to;
  // attached の切り替えは swapScene 内で行う
  swapScene(oldScene, newScene);
  tracker_.begin();
  SceneTransactionList nextTxns = txns;
  nextTxns.popFront();
  pendingTransactions_.set(nextTxns);
  tracker_.end();
}

void SceneManager2::swapScene(declara::core::scene::Scene *oldScene, declara::core::scene::Scene *newScene)
{
  if (oldScene == newScene)
    return;
  // if (oldScene)
  //   oldScene->onDetach(); // observableなstateで管理するためコールバック呼び出しを廃止
  tracker_.begin();
  if (oldScene)
  {
    oldScene->setWindow(0);
    oldScene->updateAttached(false);
    oldScene->updateLifecycle(ON_DETACH);
  }
  currentScene_.set(newScene);
  if (newScene)
  {
    newScene->setWindow(window_);
    newScene->updateAttached(true);
    newScene->updateLifecycle(ON_ATTACH);
  }
  tracker_.end();
  // if (newScene)
  //   newScene->onAttach(); // observableなstateで管理するためコールバック呼び出しを廃止
}
