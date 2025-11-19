# core2 実装・設計進捗メモ（2025-11）

Solid-mode（`common/core2/scene`）の進行状況と課題を一本化。  
`declara_design_minutes.md` のメモと突き合わせて、どこを掘ればいいかを明確にする。

---

## 1. 現状サマリ

- `Scene`（`common/core2/scene/Scene.hpp`）は `NodeComposition` を受け取る `compose(NodeComposition)` 形式に更新済み。`MutableState<SceneLifecycle>` を内包。
- `StaticNodeManager` / `IPlatformController` の骨格は追加済み（`NodeManager.hpp`, `PlatformController.hpp`）。  
  - Solid-mode では `StaticNodeManager::mount()` が `NodeComposition::createNodeTree()` でツリーを生成し、`materialize()` を呼び出す。
- `Node`, `NodePropsBase`, `NodeDefinition`, `NodeComposition`（Arena）は `core2` にまとまり、`common/app2/Button.hpp` などの UI ノードはここを前提にしている。
- `SceneManager2` は Window 側で Scene スロットを差し替えるのみで、まだ core2 Controller とは繋いでいない。

---

## 2. 優先タスク

### 2.1 Node/Component ローカル State 管理

- `NodeContext` を Node ヒープとして拡張し、`useState<T>()`/簡易アロケータを提供する。`Node` は `NodeContext` 経由でローカル State/ネイティブハンドルを保持する。
- Compose 中の短命キャッシュは `NodeComposition` で扱うが、長寿命 State は NodeContext か Scene メンバーに閉じるルールを徹底する。
- 将来 `NativeNodeContext` を派生させ、HWND/NSView などの OS ハンドルと優先度メタデータを格納。Platform 側が優先度を見てリソース破棄できるようにする。

### 2.2 Props in/out 設計

- Props が `State<T>*` を in/out できる基本パターンを整理。  
  - 例: EditText は `State<std::string>* text`（双方向）、Button は `EmitterState* onClick`。  
- `State` を Node ローカルに閉じたいケース（Headless ノードなど）のガイドラインを docs に追記する。

### 2.3 ノードツリーの所有と取得

- `NodeComposition::createNodeTree()` は現在子ノード処理がコメントアウトされている。`INestableDefinition::getChildren()` を実装し、実際に子を複製する。
- `Controller::root()` から生成済み Node ツリーへアクセスできるようにし、Window 側が PlatformContext に渡せるようにする。

### 2.4 UI コンポーネントの compose テスト

- `Scene` サブクラスを作り、`Button`/`Box` など `common/app2` のノードを `NodeComposition` に宣言してツリー生成まで確認する。
- Win32 側で `PlatformContext::createNodeContext` が `NodeContext` のスタブしか返していないので、`NativeNodeContext`（HWND/priority/イベント購読の束ね役）を用意する。

### 2.5 再 compose / 差分

- 初期バージョンは Solid-mode（一次 compose + 直接更新）に集中。State 変化で再 compose する場合は Arena を作り直す想定。
- React 型の diff 戦略は `DynamicSceneController` に逃し、今は ToDo として残す。

---

## 3. ノード管理 / Platform 構成メモ

- `StaticNodeManager` … Solid-mode 用の実装。`Scene::compose` を呼んで `NodeComposition` を構築し、`NodeContext`/Platform に初回 materialize させる役。今後 `INodeManager` を実装した Dynamic 型に差し替え予定。  
- `IPlatformController` … Node ツリーを受け取って OS へ反映する抽象。Win32/macOS ごとに実装予定。  
- Window から見た流れ：
  1. `SceneManager2` が Scene を差し替える。
  2. ノード管理層（StaticNodeManagerなど）が `Scene::compose()` を実行し、`NodeComposition` からノードツリーを生成。
  3. 生成したルートを `IPlatformController::materialize(rootNode_)` へ渡す。

当面は Window 1 枚 = ノード管理 1 つ = Scene 1 つで十分。複数 Scene を切り替えたい場合は `SceneManager2` が NodeManager を差し替える案が有力。

---

## 4. State 所有・ライフサイクル指針

- **NodeContext State**: Node の寿命と一致。`NodeContext` に `useState`/アロケータを実装してローカル State を確保し、NodeContext 破棄で自動解放。  
- **NativeNodeContext**: OS ハンドル・GPU リソース・優先度メタデータを保持。Platform が優先度の低い Node からリソース破棄できる。  
- **Props が保持する State**: 親/Scene が所有。Node が消えても State は残り、他の Node と共有可能。  
- **EmitterState**: OS → Declara! イベントの橋渡しなので、基本的に Scene/Window の長寿命オブジェクトが所有。
- ルールは「最も長生きするコンポーネントが所有する」。これで参照切れ・use-after-free を防ぐ。

---

## 5. NodeComposition と NodeOwner（Arena）設計

- `NodeComposition` は Scene の `compose()` 実行ごとに 1 回生成され、**NodeOwner（Arena）** として `NodeDefinition` ツリーを丸ごとコピーして保持する。  
- `declare()`/`store()` に渡された `NodeDefinition` の所有権は `NodeComposition` に移る。呼び出し側はスタック上の一時オブジェクトを渡すだけで良い。  
- `NodeDefinition` のコピー（clone）は子ノードも再帰的に複製し、`NodeComposition` 側の Arena ベクタで生涯管理する。二重 delete を避けるため、`NodeDefinition` 同士で所有権を共有しない。  
- 実体 `Node` の生成は `NodeComposition::createNodeTree()` で行い、生成された `Node` の寿命は NodeManager/PlatformController が管理する。Props/Node のライフサイクルを Composition から切り離し、React 型への拡張も見据える。  
- 将来的に React/Compose 型へ拡張する場合も、`NodeComposition` を “compose の戻り値” とみなし、`NodeManager` が複数の Composition を比較して差分適用する構造にする。

---

## 6. その他メモ

- `State<T>` API（`deferBind`, `deferBindWithOld`）は Solid-mode の micro tick と親和性が高い。NodeContext から直接使えるよう、ドキュメントは `docs/architecture_state_tracker.md` に集約済み。
- `SceneNodeGroup` / `AttachScope` 系のドキュメントは削除したため、本メモが core2 設計の一次情報になる。

---

## 7. NodeContext / NativeNodeContext メモ

- `NodeContext` を **Node 専用ヒープ**として扱う。`useState<T>()` や小さなオブジェクトアロケータを提供し、ローカル state や headless ノードのバッファをここに置く。`Node` 破棄時に Context ごと解放されるため、Compose DSL に副作用を持ち込まなくて済む。
- `NativeNodeContext` は `NodeContext` を継承/内包し、HWND/NSView/HBITMAP などプラットフォーム固有ハンドルと `priority`/`memoryCostBytes`/`persistent`/`releaseRequested` を保持。Declara! 側は中身を参照せず「ネイティブ側の opaque container」として扱い、Platform 実装が必要に応じてフィールドを使う。
- Loader 系ノード（例: ImageLoader, QRLoader）は Scene から受け取った `State<Request>` と `State<Handle>` を橋渡ししつつ、非同期処理のハンドルを `NodeContext` に保存する。非表示になったら Context 破棄で自動的にキャンセル/解放できる。
- `ComponentContext`（未実装）を用意し、Scene/カスタムコンポーネントの `compose` から NodeContext へアクセスできるフックを提供する予定。DSL 自体には NodeContext を露出せず、コンポーネント抽象で橋渡しする。
