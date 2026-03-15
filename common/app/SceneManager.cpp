#include "SceneManager.hpp"
#include <cassert>
#include "app/scene/Scene.hpp"
#include "loka/core/util/StateUtil.hpp"

unsigned long SceneManager::SceneTransactionList::nextId_ = 1;

SceneManager::SceneManager()
    : currentScene_(0),
      pendingTransactions_(),
      tracker_(),
      window_(0)
{
  tracker_.addState(&currentScene_);
  tracker_.addState(&pendingTransactions_);
}

SceneManager::~SceneManager() {}

void SceneManager::commitTransaction(loka::app::scene::Scene *from, loka::app::scene::Scene *to)
{
  tracker_.begin();
  const SceneTransactionList &current = pendingTransactions_.getRef();
  SceneTransactionList txns = current;
  txns.push(from, to);
  pendingTransactions_.set(txns);
  tracker_.end();
  handleNextTransaction(); // 追加: 追加直後に即時遷移
}

const loka::core::State<loka::app::scene::Scene *> &SceneManager::getCurrentScene() const
{
  return currentScene_;
}

SceneManager::SceneTransactionList SceneManager::getPendingTransactions() const
{
  return pendingTransactions_.get();
}

void SceneManager::handleNextTransaction()
{
  const SceneTransactionList &txns = pendingTransactions_.getRef();
  if (txns.empty())
    return;
  loka::app::scene::Scene *oldScene = currentScene_.get();
  loka::app::scene::Scene *newScene = txns.head()->to;
  // attached の切り替えは swapScene 内で行う
  swapScene(oldScene, newScene);
  tracker_.begin();
  SceneTransactionList nextTxns = txns;
  nextTxns.popFront();
  pendingTransactions_.set(nextTxns);
  tracker_.end();
}

void SceneManager::swapScene(loka::app::scene::Scene *oldScene, loka::app::scene::Scene *newScene)
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
