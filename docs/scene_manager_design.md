# SceneManager2 設計ノート（2025-11）

`common/core/SceneManager2.*` を中心に、現状の Scene 遷移制御と今後強化したいポイントをまとめる。

---

## 1. 現状の構造

| モジュール                    | 役割                                                                                                                           | 主要メンバ                                                                                                                                     |
| ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| `SceneManager2`               | Window 1 枚あたり 1 つ保持。Scene の差し替えとトランザクションキューを管理。                                                   | `MutableState<Scene*> currentScene_`, `MutableState<std::vector<std::pair<Scene*, Scene*>>> pendingTransactions_`, `PushStateTracker tracker_` |
| `Window`                      | `SceneManager2` を内包し、`sceneManager()->commitTransaction(...)` を外部 API にする。                                         | `SceneManager2 sceneManager_`                                                                                                                  |
| `declara::core::scene::Scene` | SceneLifecycle を `MutableState<SceneLifecycle>` で保持。compose は Solid-mode 仕様に合わせて `NodeComposition` を受ける予定。 | `MutableState<SceneLifecycle> lifecycle_`                                                                                                      |

`SceneManager2` は旧仕様の `SceneTransaction`/`SceneManagerDelegate` を廃止し、「単純な from/to キュー + Tracker による再描画通知」に縮約している。

---

## 2. 遷移フロー

1. **commitTransaction(from, to)**

   - `pendingTransactions_` に `(from, to)` を push。
   - `PushStateTracker` で begin/end しているので、`pendingTransactions_` のバインダー（UI 側のログ等）が自動で通知される。
   - push 後に `handleNextTransaction()` を即座に呼ぶ。

2. **handleNextTransaction()**

   - `pendingTransactions_.get()` の先頭を確認。無ければ終了。
   - `swapScene(currentScene_.get(), tx.to)` を呼ぶ。
   - `pendingTransactions_` から 1 件 pop して終了。
   - 先頭を処理しただけで再帰/ループはしていないため、複数遷移を一度に消化したい場合は `commitTransaction` が連続で呼ばれる前提。

3. **swapScene(old, next)**
   - 現状は `currentScene_.set(next)` をトランザクションで包むだけ。
   - `Scene::getLifecycleState()` を書き換える処理や `onAttach/onDetach` はコメントアウトされており、今後 `SceneLifecycle` State 経由で通知する計画。

---

## 3. Window / Scene との連携

- `Window` のコンストラクタで `SceneManager2` を初期化し、`sceneManager_.getCurrentScene()` を `State<Scene*>` として公開している。
- `Window::scene()` は `sceneManager_.getCurrentScene().get()` の薄いラッパー。
- `declara_design_minutes.md` の Solid-mode 仕様では「Window::mount(CompositeNode&)」を最小スコープとして扱う計画なので、SceneManager2 は「Window 単位の Scene スロット」を維持する役割に専念させる。

---

## 4. 既知の課題

1. **ライフサイクル通知の不在**

   - `Scene::lifecycle_` (MutableState) を `SceneManager2::swapScene` から書き換えていない。
   - 旧 onAttach/onDetach を State 化することで、`Scene` 側が `bind` してライフサイクルフックを再構築できるよう整理する。

2. **discardable/requestDiscard の未実装**

   - 旧仕様で想定していた「保存ダイアログが閉じるまで遷移保留」の仕組みは現実装にはない。
   - 必要になった時点で `(Scene*, PendingAction)` の構造に拡張し、`Scene` が `EmitterState` ベースで完了通知を返す案。

3. **複数トランザクションの一括処理**
   - 今は 1 件ずつ pop するだけ。`commitTransaction` を連打すると毎回 `handleNextTransaction` が呼ばれるため、一連の遷移が長い場合に多少オーバーヘッドがある。
   - まとめて処理するなら `while (!pending.empty()) swapScene(...)` に変える or `StateTracker::defer` で次の tick に回す。

---

## 5. 今後の ToDo（優先度順）

1. `SceneManager2::swapScene` で `SceneLifecycle` State を `ON_DETACH` / `ON_ATTACH` / `ON_DESTROY` へ更新し、`Scene` 側が subscribe できるようにする。
2. `Window::mount()`（Solid-mode）実装時に `SceneManager2` を差し替えやすいよう、`Scene*` ではなく `CompositeNode`/`NodeComposition` を持てるよう API を見直す。
3. 保存ダイアログや確認ダイアログを統合するための `requestDiscard` プロトコルを `EmitterState` ベースで再定義。
4. `SceneManager2` の単体テストを `tests/SceneTests.hpp` 相当の場所に追加し、`pendingTransactions_` の挙動や Tracker 連携を自動検証する。

---

## 6. 参照

- `common/core/SceneManager2.hpp`
- `common/core/SceneManager2.cpp`
- `common/core/Window.hpp`
- `common/core2/scene/Scene.hpp`
- `docs/declara_design_minutes.md`（Solid-mode の Window/Scene 設計方針）
