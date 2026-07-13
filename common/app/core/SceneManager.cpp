#include "app/core/SceneManager.hpp"
#include <cassert>
#include "app/scene/Scene.hpp"
#include "core/util/StateUtil.hpp"

unsigned long SceneManager::SceneTransactionList::nextId_ = 1;

SceneManager::SceneManager()
    : currentScene_(0),
      pendingTransactions_(),
      tracker_(),
      retiredScenes_(),
      window_(0)
{
  tracker_.addState(&currentScene_);
  tracker_.addState(&pendingTransactions_);
  LOKA_AUDIT_ALIVE_INC(SceneManager);
}

SceneManager::~SceneManager()
{
  SceneTransaction *transaction = pendingTransactions_.getRef().head();
  while (transaction)
  {
    retiredScenes_.retire(transaction->to);
    transaction = transaction->nextInComposition;
  }

  loka::app::scene::Scene *current = currentScene_.get();
  if (current)
  {
    current->setWindow(0);
    current->updateAttached(false);
    current->updateLifecycle(ON_DETACH);
    retiredScenes_.retire(current);
  }
  retiredScenes_.drain();
  LOKA_AUDIT_ALIVE_DEC(SceneManager);
}

void SceneManager::commitTransaction(loka::app::scene::Scene *from, loka::app::scene::Scene *to)
{
  tracker_.begin();
  const SceneTransactionList &current = pendingTransactions_.getRef();
  SceneTransactionList txns = current;
  txns.push(from, to);
  pendingTransactions_.set(txns);
  tracker_.end();
  handleNextTransaction(); // Apply the queued transition immediately.
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
  // swapScene owns the attached/lifecycle updates.
  swapScene(oldScene, newScene);
  tracker_.begin();
  SceneTransactionList nextTxns = txns;
  nextTxns.popFront();
  pendingTransactions_.set(nextTxns);
  tracker_.end();
  // Reclamation cannot become visible until every synchronous detach and
  // transaction notification has finished using the old Scene.
  retiredScenes_.retire(oldScene);
}

void SceneManager::swapScene(loka::app::scene::Scene *oldScene, loka::app::scene::Scene *newScene)
{
  if (oldScene == newScene)
    return;
  // Lifecycle is expressed through observable state instead of direct callbacks.
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
  // Reclamation cannot become visible until every synchronous detach and
  // transaction notification has finished using the old Scene.
  retiredScenes_.retire(oldScene);
}
