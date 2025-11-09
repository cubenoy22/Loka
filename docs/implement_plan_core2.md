# core2 実装・設計進捗メモ（2025-11）

Solid-mode（`common/core2/scene`）の進行状況と課題を一本化。  
`declara_design_minutes.md` のメモと突き合わせて、どこを掘ればいいかを明確にする。

---

## 1. 現状サマリ

- `Scene`（`common/core2/scene/Scene.hpp`）は `NodeComposition` を受け取る `compose(NodeComposition)` 形式に更新済み。`MutableState<SceneLifecycle>` を内包。
- `Controller` / `StaticSceneController` / `IPlatformController` の骨格は追加済み（`Controller.hpp`, `StaticSceneController.hpp`, `PlatformController.hpp`）。  
  - ただし `StaticSceneController::run()` 内の `composition.createNodeTree()` と `platformController_->materialize()` は未着手。
- `Node`, `NodePropsBase`, `NodeDefinition`, `NodeComposition`（Arena）は `core2` にまとまり、`common/app2/Button.hpp` などの UI ノードはここを前提にしている。
- `SceneManager2` は Window 側で Scene スロットを差し替えるのみで、まだ core2 Controller とは繋いでいない。

---

## 2. 優先タスク

### 2.1 Node/Component ローカル State 管理

- `Node` 内で `MutableState` を生成し、Node の寿命に合わせて破棄する API（仮: `Node::useState<T>()`）を追加する。
- Arena 構築中の一時 State をどう保持するか（NodeComposition 内に `stateArena_` を追加するか、Node が自前 new/delete するか）を決める。

### 2.2 Props in/out 設計

- Props が `State<T>*` を in/out できる基本パターンを整理。  
  - 例: EditText は `State<std::string>* text`（双方向）、Button は `EmitterState* onClick`。  
- `State` を Node ローカルに閉じたいケース（Headless ノードなど）のガイドラインを docs に追記する。

### 2.3 ノードツリーの所有と取得

- `NodeComposition::createNodeTree()` は現在子ノード処理がコメントアウトされている。`INestableDefinition::getChildren()` を実装し、実際に子を複製する。
- `Controller::root()` から生成済み Node ツリーへアクセスできるようにし、Window 側が PlatformContext に渡せるようにする。

### 2.4 UI コンポーネントの compose テスト

- `Scene` サブクラスを作り、`Button`/`Box` など `common/app2` のノードを `NodeComposition` に宣言してツリー生成まで確認する。
- Win32 側で `PlatformContext::createNodeContext` が `NodeContext` のスタブしか返していないので、最低限の HWND ラッパーを用意する。

### 2.5 再 compose / 差分

- 初期バージョンは Solid-mode（一次 compose + 直接更新）に集中。State 変化で再 compose する場合は Arena を作り直す想定。
- React 型の diff 戦略は `DynamicSceneController` に逃し、今は ToDo として残す。

---

## 3. Controller / Platform 構成メモ

- `Controller` … Scene のライフサイクル State に bind し、`ON_ATTACH` で `compose()` を実行する基底。  
- `StaticSceneController` … Solid-mode 用。`NodeComposition` を組み、`NodeContext`/Platform に初回 materialize させる役。  
- `IPlatformController` … Node ツリーを受け取って OS へ反映する抽象。Win32/macOS ごとに実装予定。  
- Window から見た流れ：
  1. `SceneManager2` が Scene を差し替える。
  2. `SceneLifecycle` State を `ON_ATTACH` にすると Controller が compose。
  3. Controller は `IPlatformController::materialize(rootNode_)` を呼ぶ。

当面は Window 1 枚 = Controller 1 つ = Scene 1 つで十分。複数 Scene を切り替えたい場合は `SceneManager2` が Controller を差し替える案が有力。

---

## 4. State 所有・ライフサイクル指針

- **Node メンバー State**: Node の寿命と一致。`Node` デストラクタで `ScopedPtr` などを使って解放。  
- **Props が保持する State**: 親/Scene が所有。Node が消えても State は残り、他の Node と共有可能。  
- **EmitterState**: OS → Declara! イベントの橋渡しなので、基本的に Scene/Window の長寿命オブジェクトが所有。
- ルールは「最も長生きするコンポーネントが所有する」。これで参照切れ・use-after-free を防ぐ。

---

## 5. NodeComposition 所有権

- `NodeComposition` は 1 compose あたり 1 つ生成し、Arena 内に `NodeDefinition` と Props を丸ごとコピーする。  
- `declare()` で登録したルートは `root()` から取得。`createNodeTree()` で実行時 Node ツリーを new する。  
- Controller/Window 側は「現在の NodeComposition（または root Node）へのポインタ」を持つだけにし、破棄時は `delete rootNode` + `delete composition` を呼べば良い。  
- 旧 `SceneNodeGroup` は利用しない。全て `core2` の Arena ベースに寄せる。

---

## 6. その他メモ

- `State<T>` API（`deferBind`, `deferBindWithOld`）は Solid-mode の micro tick と親和性が高い。NodeContext から直接使えるよう、ドキュメントは `docs/architecture_state_tracker.md` に集約済み。
- `SceneNodeGroup` / `AttachScope` 系のドキュメントは削除したため、本メモが core2 設計の一次情報になる。
