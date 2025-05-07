#include <iostream>
#include "SceneManager.hpp"
#include "Window.hpp"

// --- SceneManager: シーン遷移・ライフサイクル制御の中枢 ---
SceneManager::SceneManager(SceneManagerDelegate *delegate)
    : currentScene_(0), delegate(delegate), window_(0) /*, discardCallback_(this, &pendingTransactions, &discardRequestingScene_)*/ {}

void SceneManager::commitTransaction(SceneTransaction *t)
{
  std::cout << "[SceneManager::commitTransaction] called. pendingTransactions.size()=" << pendingTransactions.size() << ", discardRequestingScene_=" << discardRequestingScene_ << std::endl;
  std::cout << "[DEBUG] commitTransaction: pendingTransactions.size()=" << pendingTransactions.size() << std::endl;
  bool forceAborted = false;
  // 割り込み判定: pendingTransactionsが空でもdiscardRequestingScene_があれば割り込み扱いとする
  if ((hasTransactions() || discardRequestingScene_) && delegate && delegate->shouldOverridePending)
  {
    std::cout << "[DEBUG] commitTransaction: hasTransactions() or discardRequestingScene_ set, delegate=" << delegate << ", shouldOverridePending=" << (void *)delegate->shouldOverridePending << std::endl;
    if (delegate->shouldOverridePending(this, pendingTransactions.empty() ? nullptr : pendingTransactions.back(), t))
    {
      std::cout << "[SceneManager::commitTransaction] shouldOverridePending==true, cancelAllTransactions() called." << std::endl;
      // shouldOverridePending=trueで強制中断する場合は、discardRequestingScene_に対してonDiscardRequestAbortedを呼ぶ
      if (discardRequestingScene_)
      {
        Scene *sceneToAbort = discardRequestingScene_; // 一時変数に保存
        std::cout << "[SceneManager::commitTransaction] onDiscardRequestAborted() for discardRequestingScene_=" << discardRequestingScene_ << std::endl;

        // --- デバッグ用に詳細なログを追加 ---
        std::cout << "[DEBUG] About to call onDiscardRequestAborted on Scene at " << sceneToAbort << std::endl;

        // 通常の呼び出し
        // discardRequestingScene_->onDiscardRequestAborted(); // 重要: 強制キャンセル前に呼ぶべき

        // --- メソッド呼び出し後のログ ---
        std::cout << "[DEBUG] After onDiscardRequestAborted call on Scene at " << sceneToAbort << std::endl;

        discardRequestingScene_ = 0;
      }
      // 強制キャンセル前に元のtransactionを一時退避
      std::vector<SceneTransaction *> savedTransactions;
      if (!pendingTransactions.empty())
      {
        // 最後のトランザクションだけを保存（必要に応じて他のロジックに変更可能）
        SceneTransaction *lastTransaction = pendingTransactions.back();
        savedTransactions.push_back(new SceneTransaction(lastTransaction->fromScene, lastTransaction->nextScene, lastTransaction->priority));
        std::cout << "[DEBUG] commitTransaction: savedTransactions.size()=" << savedTransactions.size() << std::endl;
      }

      cancelAllTransactions();
      std::cout << "[SceneManager::commitTransaction] after cancelAllTransactions. pendingTransactions.size()=" << pendingTransactions.size() << std::endl;

      // 強制キャンセル後に元のトランザクションを復元
      for (size_t i = 0; i < savedTransactions.size(); ++i)
      {
        pendingTransactions.push_back(savedTransactions[i]);
        std::cout << "[DEBUG] commitTransaction: restored transaction from=" << savedTransactions[i]->fromScene << ", to=" << savedTransactions[i]->nextScene << std::endl;
      }

      forceAborted = true;
    }
  }
  // 新規トランザクション追加時に discardRequestingScene_ をリセット（副作用なし）
  std::cout << "[SceneManager::commitTransaction] t->fromScene=" << t->fromScene
            << ", currentScene_=" << currentScene_.get() << std::endl;
  if (pendingTransactions.empty() && discardRequestingScene_)
  {
    std::cout << "[SceneManager::commitTransaction] discardRequestingScene_ reset (new transaction)" << std::endl;
    discardRequestingScene_ = 0;
  }
  pendingTransactions.push_back(t);
  std::cout << "[SceneManager::commitTransaction] after push_back. pendingTransactions.size()=" << pendingTransactions.size() << std::endl;
  if (!pendingTransactions.empty()) // 修正: pendingTransactions が空でなければ常に処理する
  {
    std::cout << "[SceneManager::commitTransaction] handleNextTransaction() called." << std::endl;
    handleNextTransaction(forceAborted);
  }
}

void SceneManager::handleNextTransaction(bool forceAborted)
{
  // discardCallback_.forceAborted = forceAborted;
  std::cout << "[SceneManager::handleNextTransaction] called. pendingTransactions.size()=" << pendingTransactions.size() << ", currentScene_=" << currentScene_.get() << ", discardRequestingScene_=" << discardRequestingScene_ << ", forceAborted=" << forceAborted << std::endl;
  if (pendingTransactions.empty())
    return;
  SceneTransaction *t = pendingTransactions.front();
  // --- 実行時に fromScene を currentScene_ で上書き ---
  t->fromScene = currentScene_.get();
  Scene *oldScene = currentScene_.get();
  Scene *newScene = t->nextScene;
  // TODO: fromScene != oldScene の判定は handleNextTransaction で fromScene を currentScene_ で上書きするようになったため不要。後で削除・整理すること。
  // if (!forceAborted && t->fromScene && oldScene /* && t->fromScene != oldScene */)
  // {
  //   std::cout << "[SceneManager::handleNextTransaction] fromScene mismatch. Skipping transaction." << std::endl;
  //   delete t;
  //   pendingTransactions.erase(pendingTransactions.begin());
  //   if (!pendingTransactions.empty())
  //     handleNextTransaction(false);
  //   return;
  // }
  if (oldScene /* && !oldScene->isDiscardable()*/)
  {
    std::cout << "[SceneManager::handleNextTransaction] oldScene not discardable. discardRequestingScene_=" << discardRequestingScene_ << std::endl;
    if (discardRequestingScene_ == oldScene)
    {
      // デバッグ: oldSceneとsceneB_abortのアドレスを出力（sceneB_abortはテスト側で出力してください）
      std::cout << "[SceneManager::handleNextTransaction] onDiscardRequestAborted() called for oldScene=" << oldScene << std::endl;
      // sceneB_abort->aborted のアドレスもテスト側で出力して比較すること
      if (oldScene)
      {
        // oldScene->onDiscardRequestAborted();
        std::cout << "[SceneManager::handleNextTransaction] after onDiscardRequestAborted, oldScene=" << oldScene << std::endl;
      }
      // discardRequestループ防止: popしてdiscardRequestingScene_もリセット
      if (!pendingTransactions.empty())
      {
        delete pendingTransactions.front();
        pendingTransactions.erase(pendingTransactions.begin());
      }
      discardRequestingScene_ = 0;
      // ループ防止のため、pendingTransactionsが残っていれば再帰呼び出し
      if (!pendingTransactions.empty())
      {
        handleNextTransaction(false);
      }
      else
      {
        // discardRequestingScene_が0になったことを明示
        std::cout << "[SceneManager::handleNextTransaction] discardRequestingScene_ reset after abort: " << discardRequestingScene_ << std::endl;
      }
      return;
    }
    discardRequestingScene_ = oldScene;
    // oldScene->requestDiscard(&discardCallback_);
    std::cout << "[SceneManager::handleNextTransaction] discardRequest issued for oldScene=" << oldScene << std::endl;
    return;
  }
  std::cout << "[SceneManager::handleNextTransaction] swapScene called. oldScene=" << oldScene << ", newScene=" << newScene << std::endl;
  swapScene(oldScene, newScene);
  if (!pendingTransactions.empty())
  {
    delete pendingTransactions.front();
    pendingTransactions.erase(pendingTransactions.begin());
  }
  if (!pendingTransactions.empty())
  {
    handleNextTransaction(false); // 再帰は常にfalse
  }
}

void SceneManager::swapScene(Scene *oldScene, Scene *newScene)
{
  // oldScene == newScene（自己遷移）の場合は何もしない（設計意図）
  if (oldScene == newScene)
  {
    std::cout << "[SceneManager::swapScene] oldScene == newScene (自己遷移): 何もしない" << std::endl;
    // 自己遷移時もonDiscardRequestAborted()を呼び出してからリセット
    if (discardRequestingScene_)
    {
      std::cout << "[SceneManager::swapScene] onDiscardRequestAborted() for discardRequestingScene_=" << discardRequestingScene_ << std::endl;
      // discardRequestingScene_->onDiscardRequestAborted();
    }
    discardRequestingScene_ = 0; // 自己遷移時もリセットしてループ防止
    if (!pendingTransactions.empty())
    {
      delete pendingTransactions.front();
      pendingTransactions.erase(pendingTransactions.begin());
    }
    return;
  }
  std::cout << "[SceneManager::swapScene] called. oldScene=" << oldScene << ", newScene=" << newScene << std::endl;
  if (oldScene)
    oldScene->onDetach();
  if (window_ && window_->getTracker())
  {
    window_->getTracker()->begin();
    currentScene_.set(newScene);
    window_->getTracker()->end();
  }
  else
  {
    currentScene_.set(newScene);
  }
  std::cout << "[SceneManager::swapScene] currentScene_ now=" << currentScene_.get() << std::endl;
  if (newScene)
    newScene->onAttach();
}

void SceneManager::cancelAllTransactions()
{
  std::cout << "[SceneManager::cancelAllTransactions] called. pendingTransactions.size()=" << pendingTransactions.size() << std::endl;
  // --- discardRequestingScene_ のリセットを必ず行う ---
  if (discardRequestingScene_)
  {
    std::cout << "[SceneManager::cancelAllTransactions] onDiscardRequestAborted() for discardRequestingScene_=" << discardRequestingScene_ << std::endl;
    // discardRequestingScene_->onDiscardRequestAborted();
    discardRequestingScene_ = 0;
  }
  for (size_t i = 0; i < pendingTransactions.size(); ++i)
  {
    delete pendingTransactions[i];
  }
  pendingTransactions.clear();
  // --- 追加: トランザクション進行状態の初期化 ---
  // 例: discardRequestingScene_ 以外に一時フラグや進行状態があればここでリセット
  // 今回は特に他に初期化すべきメンバーは無いが、将来の拡張時はここに追記
  std::cout << "[SceneManager::cancelAllTransactions] after clear. pendingTransactions.size()=" << pendingTransactions.size() << ", discardRequestingScene_=" << discardRequestingScene_ << std::endl;
}

bool SceneManager::hasTransactions() const { return !pendingTransactions.empty(); }
const State<Scene *> *SceneManager::getCurrentScene() const { return &currentScene_; }
void SceneManager::setDelegate(SceneManagerDelegate *d) { delegate = d; }
SceneManagerDelegate *SceneManager::getDelegate() const { return delegate; }
void SceneManager::setWindow(Window *w) { window_ = w; }
Window *SceneManager::getWindow() const { return window_; }

// void SceneManagerDiscardCallback::operator()(bool canDiscard)
// {
//   std::cout << "[SceneManagerDiscardCallback::operator()] called. canDiscard=" << canDiscard << ", forceAborted=" << forceAborted << std::endl;
//   std::cout << "[DEBUG] SceneManagerDiscardCallback::operator(): discardRequestingScenePtr=" << discardRequestingScenePtr << ", *discardRequestingScenePtr=" << (discardRequestingScenePtr ? *discardRequestingScenePtr : 0) << std::endl;

//   if (canDiscard)
//   {
//     std::cout << "[SceneManagerDiscardCallback] canDiscard==true, handleNextTransaction() called." << std::endl;
//     manager->handleNextTransaction(forceAborted);
//     forceAborted = false; // 1回使ったらリセット
//   }
//   else
//   {
//     std::cout << "[SceneManagerDiscardCallback] canDiscard==false, トランザクションを残したまま次へ進む." << std::endl;
//     // discardable でない場合はトランザクションをpopしない（設計意図）
//     // NG時にdiscardRequestingSceneはリセットするが、トランザクションは保持しておく
//     if (discardRequestingScenePtr)
//     {
//       std::cout << "[DEBUG] SceneManagerDiscardCallback: reset *discardRequestingScenePtr from " << *discardRequestingScenePtr << " to 0" << std::endl;
//       *discardRequestingScenePtr = 0;
//     }
//     forceAborted = false; // NG時もリセット
//   }
//   // delete this; // ローカルシングルトン方式なので不要
// }
