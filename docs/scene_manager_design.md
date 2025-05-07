# Declara! Scene 管理・トランザクション設計思想まとめ

## 1. 設計の背景

- Scene の切り替え・管理は、UI/アプリ/ゲームの「状態遷移」の根幹。
- 連打や非同期遷移、保存ダイアログなど、現代的な UI/UX 要件に耐える堅牢な仕組みが必要。
- C++98 互換・型安全・明示的な依存管理を重視しつつ、現代的なトランザクション/ライフサイクル制御を導入。

---

## 2. コアコンポーネントの責務

| クラス               | 役割・責務                                                                                                                            |
| -------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| SceneManager         | Scene 遷移・ライフサイクル制御の中枢。トランザクション型で順次遷移・競合判定・discardable 判定も担う                                  |
| SceneTransaction     | Scene 遷移リクエストのカプセル化。from/to で多重遷移防止も実現                                                                        |
| SceneManagerDelegate | 競合時の判定・通知用インタフェース（shouldOverridePending）                                                                           |
| Scene                | 世界そのもの。ライフサイクルフック（onCreate/onAttach/onDetach/onDestroy/isDiscardable/requestDiscard/onDiscardRequestAborted）を持つ |

---

## 3. トランザクション型遷移フロー

1. **commitTransaction**

   - 新たな SceneTransaction をスタックに積む
   - 既に pending があれば shouldOverridePending で競合判定。override なら cancelAllTransactions
   - どちらの場合も順番待ちで積む

2. **handleNextTransaction**

   - スタック先頭の Transaction を処理
   - from/to チェックで多重遷移防止
   - 現 Scene が discardable でなければ requestDiscard（C++98 流ファンクタでコールバック）
   - discardable なら swapScene で切り替え、トランザクション pop
   - 次があれば再帰的に処理

3. **swapScene**

   - old.onDetach(), currentScene.set(new), new.onAttach()

4. **requestDiscard 中のキャンセル**
   - override/cancelAllTransactions 時は onDiscardRequestAborted を呼び、コールバックも安全に破棄

---

## 4. 設計上の工夫・ポイント

- **多重遷移防止**：from/to チェックで「今の Scene からの遷移か」を厳密に判定
- **discardable 判定・保存ダイアログ**：isDiscardable/requestDiscard でユーザーの保存可否を柔軟に制御
- **C++98 流ファンクタコールバック**：ラムダが使えない環境でも状態付きコールバックを安全に実現
- **コールバックのローカルシングルトン化**：new/delete 不要、メモリリークなし
- **MutableState<T>::set()のトランザクションラップ**：StateTracker::begin()/end()で依存伝播・副作用管理を徹底

---

## 5. 典型的な利用シナリオ

- 連打や非同期遷移でも「順番待ち」で安全に処理
- 保存ダイアログ表示中にさらに遷移リクエストが来ても、onDiscardRequestAborted で緊急保存や通知が可能
- SceneManager/Window/Scene/PlatformContext が明確に責務分離され、拡張・保守も容易

---

## 6. まとめ

この Scene 管理設計は、
「現代的な UI/UX 要件」「型安全・依存管理」「C++98 互換」「拡張性・保守性」
すべてを両立するための骨格思想に基づいている。
