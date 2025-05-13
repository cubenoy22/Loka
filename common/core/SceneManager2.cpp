#include <vector>
#include "SceneManager2.hpp"
#include "StateUtil.hpp"
#include <cassert>

SceneManager2::SceneManager2()
    : currentScene_(0),
      pendingTransactions_(),
      tracker_(makeStateVector(&currentScene_, &pendingTransactions_, 0))
{
}

SceneManager2::~SceneManager2() {}

void SceneManager2::commitTransaction(Scene *from, Scene *to)
{
  tracker_.begin();
  std::vector<std::pair<Scene *, Scene *>> txns = pendingTransactions_.get();
  txns.push_back(std::make_pair(from, to));
  pendingTransactions_.set(txns);
  tracker_.end();
  handleNextTransaction(); // 追加: 追加直後に即時遷移
}

const State<Scene *> &SceneManager2::getCurrentScene() const
{
  return currentScene_;
}

const std::vector<std::pair<Scene *, Scene *>> &SceneManager2::getPendingTransactions() const
{
  return pendingTransactions_.get();
}

void SceneManager2::handleNextTransaction()
{
  const std::vector<std::pair<Scene *, Scene *>> &txns = pendingTransactions_.get();
  if (txns.empty())
    return;
  Scene *oldScene = currentScene_.get();
  Scene *newScene = txns.front().second;
  swapScene(oldScene, newScene);
  tracker_.begin();
  std::vector<std::pair<Scene *, Scene *>> nextTxns = pendingTransactions_.get();
  nextTxns.erase(nextTxns.begin());
  pendingTransactions_.set(nextTxns);
  tracker_.end();
}

void SceneManager2::swapScene(Scene *oldScene, Scene *newScene)
{
  if (oldScene == newScene)
    return;
  if (oldScene)
    oldScene->onDetach();
  tracker_.begin();
  currentScene_.set(newScene);
  tracker_.end();
  if (newScene)
    newScene->onAttach();
}
