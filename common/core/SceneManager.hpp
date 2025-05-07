#ifndef DECLARA_SCENEMANAGER_HPP
#define DECLARA_SCENEMANAGER_HPP

#include "Scene.hpp"
#include "State.hpp"
#include <vector>
#include <functional>

class SceneManager;
class Window;

// --- SceneTransaction: シーン遷移リクエストのカプセル化 ---
class SceneTransaction
{
public:
  // from: 遷移元Scene（多重遷移防止用）
  SceneTransaction(Scene *from, Scene *to, int priority = 0)
      : fromScene(from), nextScene(to), priority(priority) {}
  Scene *fromScene;
  Scene *nextScene;
  int priority; // 仮のフィールド
  // 必要に応じて追加情報（遷移種別・コールバック等）を拡張
};

// --- SceneManagerDelegate: C++98流の関数ポインタ型デリゲート ---
typedef bool (*SceneManagerShouldOverridePendingFn)(class SceneManager *self, SceneTransaction *pending, SceneTransaction *incoming);

class SceneManagerDelegate
{
public:
  SceneManagerShouldOverridePendingFn shouldOverridePending;
  SceneManagerDelegate() : shouldOverridePending(0) {}
};

// SceneManager固有の状態を持つコールバック
// class SceneManagerDiscardCallback : public DiscardCallback
// {
// public:
//   SceneManagerDiscardCallback(SceneManager *mgr,
//                               std::vector<SceneTransaction *> *transactions,
//                               Scene **discardRequestingScene)
//       : manager(mgr), pendingTransactions(transactions), discardRequestingScenePtr(discardRequestingScene), forceAborted(false) {}

//   void operator()(bool canDiscard) override;
//   bool forceAborted; // 強制割り込みフラグ
// private:
//   SceneManager *manager;
//   std::vector<SceneTransaction *> *pendingTransactions;
//   Scene **discardRequestingScenePtr;
// };

// --- SceneManager: シーン遷移・ライフサイクル制御の中枢 ---
class SceneManager
{
public:
  SceneManager(SceneManagerDelegate *delegate = 0);
  void commitTransaction(SceneTransaction *t);
  void handleNextTransaction(bool forceAborted = false);
  void swapScene(Scene *oldScene, Scene *newScene);
  void cancelAllTransactions();
  bool hasTransactions() const;
  const State<Scene *> *getCurrentScene() const;
  void setDelegate(SceneManagerDelegate *d);
  SceneManagerDelegate *getDelegate() const;
  void setWindow(Window *w);
  Window *getWindow() const;

protected:
  MutableState<Scene *> currentScene_;
  std::vector<SceneTransaction *> pendingTransactions; // スタックで順次実行
  SceneManagerDelegate *delegate;
  Window *window_; // PlatformContext連携用
  Scene *discardRequestingScene_ = 0;
  // SceneManagerDiscardCallback discardCallback_;
};

#endif // DECLARA_SCENEMANAGER_HPP
