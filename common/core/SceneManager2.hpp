#ifndef DECLARA_SCENEMANAGER2_HPP
#define DECLARA_SCENEMANAGER2_HPP

#include "core2/scene/Scene.hpp"
#include "State.hpp"
#include "StateTracker.hpp"
#include <vector>

class Window;

class SceneManager2
{
public:
  SceneManager2();
  ~SceneManager2();

  // トランザクション追加
  void commitTransaction(declara::core::scene::Scene *from, declara::core::scene::Scene *to);
  // 現在のシーン取得
  const State<declara::core::scene::Scene *> &getCurrentScene() const;

protected:
  // ペンディングトランザクション取得
  const std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > &getPendingTransactions() const;
  // トランザクション進行
  void handleNextTransaction();
  // 副作用: シーン切り替え
  void swapScene(declara::core::scene::Scene *oldScene, declara::core::scene::Scene *newScene);

public:
  void setWindow(Window *window) { window_ = window; }
  Window *window() const { return window_; }

private:
  MutableState<declara::core::scene::Scene *> currentScene_;
  MutableState<std::vector<std::pair<declara::core::scene::Scene *, declara::core::scene::Scene *> > > pendingTransactions_;
  declara::core::PushStateTracker tracker_;
  Window *window_;
};

#endif // DECLARA_SCENEMANAGER2_HPP
