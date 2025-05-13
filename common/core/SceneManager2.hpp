#ifndef DECLARA_SCENEMANAGER2_HPP
#define DECLARA_SCENEMANAGER2_HPP

#include "Scene.hpp"
#include "State.hpp"
#include "StateTracker.hpp"
#include <vector>

class SceneManager2
{
public:
  SceneManager2();
  ~SceneManager2();

  // トランザクション追加
  void commitTransaction(Scene *from, Scene *to);
  // 現在のシーン取得
  const State<Scene *> &getCurrentScene() const;

protected:
  // ペンディングトランザクション取得
  const std::vector<std::pair<Scene *, Scene *>> &getPendingTransactions() const;
  // トランザクション進行
  void handleNextTransaction();
  // 副作用: シーン切り替え
  void swapScene(Scene *oldScene, Scene *newScene);

private:
  MutableState<Scene *> currentScene_;
  MutableState<std::vector<std::pair<Scene *, Scene *>>> pendingTransactions_;
  PushStateTracker tracker_;
};

#endif // DECLARA_SCENEMANAGER2_HPP
