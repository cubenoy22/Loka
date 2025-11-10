#include <vector>
#include "SceneManager2.hpp"
#include "util/StateUtil.hpp"
#include <cassert>
#include "core2/scene/Scene.hpp"

SceneManager2::SceneManager2()
    : currentScene_(0),
      pendingTransactions_(),
      tracker_(makeStateVector(&currentScene_, &pendingTransactions_, 0))
{
}

SceneManager2::~SceneManager2() {}

void SceneManager2::commitTransaction(declara::core::scene::Scene *from, declara::core::scene::Scene *to)
{
  tracker_.begin();
  std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > txns = pendingTransactions_.get();
  txns.push_back(std::make_pair(from, to));
  pendingTransactions_.set(txns);
  tracker_.end();
  handleNextTransaction(); // 追加: 追加直後に即時遷移
}

const State<declara::core::scene::Scene *> &SceneManager2::getCurrentScene() const
{
  return currentScene_;
}

const std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > &SceneManager2::getPendingTransactions() const
{
  return pendingTransactions_.get();
}

void SceneManager2::handleNextTransaction()
{
  const std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > &txns = pendingTransactions_.get();
  if (txns.empty())
    return;
  declara::core::scene::Scene *oldScene = currentScene_.get();
  declara::core::scene::Scene *newScene = txns.front().second;
  swapScene(oldScene, newScene);
  tracker_.begin();
  std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > nextTxns = pendingTransactions_.get();
  nextTxns.erase(nextTxns.begin());
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
  currentScene_.set(newScene);
  tracker_.end();
  // if (newScene)
  //   newScene->onAttach(); // observableなstateで管理するためコールバック呼び出しを廃止
}
