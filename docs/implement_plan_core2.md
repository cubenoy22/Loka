# core2 実装・設計進捗メモ

Solid-mode（`common/core2/scene`）の進行状況と課題を一本化。

---

## 1. 現状サマリ（実装済み）

### アーキテクチャ
- **BoundaryNode**: 状態境界。ローカルの `PushStateTracker` を所有。`useState` で状態を登録。
- **GroupNode**: 非Boundary。状態は親Boundaryに委譲（`stateOwner` 経由）。
- **Scene**: `mount()`/`unmount()` でライフサイクル管理。非Boundaryルートは `RootBoundaryWrapper` で自動ラップ。
- **NodeComposition**: compose中のArena + DSL窓口（`useState`, `findBoundary`, `declare` 等）。

### DSL
```cpp
// Boundary定義
class MyNode : public BoundaryNodeFor<MyNode> { ... };
inline BoundaryDefinition<...> MyBoundary() { return Boundary<MyNode>(); }

// Group定義
class MyNode : public GroupNodeBase<MyProps> { ... };
inline NodeDefinition<...> MyGroup() { return NodeDefinition<...>(); }
```

### App層との連携
- **WindowDefinition/WindowProps**: App層のDefinition-Propsパターン（NodeDefinitionとは別基底）。
- **AppComposition**: App構成用DSL。`c.declare(WindowDef(WindowProps().scene(...).title(...).visible(...)))` でWindow宣言。

---

## 2. 完了タスク

- [x] BoundaryNode 導入、StateTracker 所有
- [x] useState 実装（NodeComposition経由、Tracker自動登録）
- [x] findBoundary<T>() 実装（親Boundary検索）
- [x] ContextDefinition/ComponentContext::provide/require 廃止
- [x] Scene: 非Boundaryルートの自動ラップ
- [x] DSL命名整理（StaticComposition* → Boundary*/Group*）
- [x] WindowDefinition/AppComposition 導入

---

## 3. 残タスク

### 3.1 Props in/out 設計

- Props が `State<T>*` を in/out できる基本パターンを整理。
  - 例: EditText は `State<std::string>* text`（双方向）、Button は `EmitterState* onClick`。

### 3.2 再 compose / 差分

- 初期バージョンは Solid-mode（一次 compose + 直接更新）に集中。
- React 型の diff 戦略は Boundary 内で閉じて実装可能な設計。

---

## 4. ノード管理 / Platform 構成メモ

- `Scene`: BoundaryNodeをルートとして保持。非Boundaryルートは `RootBoundaryWrapper` で自動ラップ。
- `IPlatformController`: Node ツリーを受け取って OS へ反映する抽象。Win32/macOS/Toolbox ごとに実装。
- Window から見た流れ：
  1. `SceneManager2` が Scene を差し替える。
  2. `Scene::mount()` が BoundaryNode をルートにツリーを生成。
  3. 生成したルートを `IPlatformController::onChange(rootNode_, flags)` へ渡す。

当面は Window 1 枚 = Scene 1 つで十分。複数 Scene を切り替えたい場合は `SceneManager2` が Scene を差し替える。

### 4.1 コンポーネント再利用パターン

- **Fragment 型**（例: `BmiCalculatorComponent`）: DSL の断片をそのまま返す軽量パターン。
- **GroupNode**: `composeNode(NodeComposition&)` を持つ非Boundary。状態は親Boundaryに委譲。
- **BoundaryNode**: 状態境界。ローカルの StateTracker を所有し、React型の再composeを内包可能。

---

## 5. State 所有・ライフサイクル指針

- **BoundaryNode State**: Boundary の寿命と一致。`useState` で確保し、Boundary 破棄で自動解放。
- **Props が保持する State**: 親/Scene が所有。Node が消えても State は残り、他の Node と共有可能。
- **EmitterState**: OS → Loka イベントの橋渡しなので、基本的に Scene/Window の長寿命オブジェクトが所有。
- ルールは「最も長生きするコンポーネントが所有する」。これで参照切れ・use-after-free を防ぐ。

---

## 6. NodeComposition と Arena 設計

- `NodeComposition` は Scene の `compose()` 実行ごとに 1 回生成され、**NodeOwner（Arena）** として `NodeDefinition` ツリーを丸ごとコピーして保持する。  
- `declare()`/`store()` に渡された `NodeDefinition` の所有権は `NodeComposition` に移る。呼び出し側はスタック上の一時オブジェクトを渡すだけで良い。  
- `NodeDefinition` のコピー（clone）は子ノードも再帰的に複製し、`NodeComposition` 側の Arena ベクタで生涯管理する。二重 delete を避けるため、`NodeDefinition` 同士で所有権を共有しない。  
- 実体 `Node` の生成は `NodeComposition::createNodeTree()` で行い、生成された `Node` の寿命は Scene/PlatformController が管理する。Props/Node のライフサイクルを Composition から切り離し、React 型への拡張も見据える。  
- 将来的に React/Compose 型へ拡張する場合も、`NodeComposition` を “compose の戻り値” とみなし、Scene が複数の Composition を比較して差分適用する構造にする。

---

## 7. その他メモ

- `State<T>` API（`deferBind`, `deferBindWithOld`）は Solid-mode の micro tick と親和性が高い。ドキュメントは `docs/architecture_state_tracker.md` に集約済み。

---

## 8. 将来検討: NativeNodeContext

- BoundaryNode の拡張として、HWND/NSView などプラットフォーム固有ハンドルを保持する `NativeNodeContext` を検討。
- `priority`/`memoryCostBytes` を保持し、Platform が優先度の低い Node からリソース破棄可能に。
- Global/Scene ヒープで共有したいリソース（Image など）は `State< Managed<ResourceRecord> >` で扱う。

---

## 9. ErrorSink / エラーハンドリング指針

- `ImageLoader` など高頻度でエラーを吐くノードは共通の `ErrorSink` を参照。
- `ErrorSink` は Window/Scene 単位のキュー型。`push(ErrorEvent)` で Producer から受信し、`tryPop()` または `EmitterState` 経由で Consumer に流す。
- `ErrorEvent` には Domain/Code/Severity/Message/Payload(Managed<void>) を含め、`State<Managed<Request>>` を使って再試行情報を渡せる。
- UI への露出は 2 方式:
  - Pull: `ErrorHost` コンポーネントが `tryPop()` で逐次処理 → ダイアログ等。
  - Push: Sink 内部の `EmitterState` で「新規エラー到着」を通知 → Toast 的な UI が `imageState` とは独立して立ち上がる。
- `PlatformContext` がデフォルトの Sink を用意し、`Scene::compose` に `ComponentContext` を通じて配布する。Loader が勝手にダイアログを開かず、全部 Sink 経由にする。Headless コンポーネントが Sink を必須依存として宣言できる仕組みも必要。
- Resumable なエラー: `ErrorEvent` の payload に `Managed<Request>`/再試行用ハンドラを埋め込み、Consumer 側で retry ボタンを提供できるようにする。Sink は `ack/errorId` 管理で多重通知を防ぐ。プラットフォームごとに非同期処理実装を揃えるため、`ResumableTask` 抽象（resume/pause/cancel/priority）を検討。ImageLoader はこのタスクを介してマルチスレッド読み込みを委譲する。
- `ErrorSinkScope`（仮）を用意し、特定の compose スコープ内でローカル Sink を生成→親 Sink にネストさせる。エラーイベントには `path`/`labels` を含め、どのコンポーネントの I/O で失敗したかを追跡しやすくする。
- Headless コンポーネントは `ErrorSink child(currentTaskContext, parentSink)` を受け取り、ローカルで発生したエラーを `child.push()` → 内部で currentTaskContext を payload に含めた上で parentSink へ forward。`ErrorEvent` の `tags` フィールドで `"image-loader"`, `"blob-loader"` など分類しておく。

## 10. ImageLoader / Image アクセス

- `Image` は `Managed<ImageRecord>` を包む値型。`Image::Empty()` を初期値にし、`State<Image>` が null を扱わなくて済むようにする。
- `ImageLoaderRequest` は URL/Data/FileHandle を含むユニオンと、`desiredSize`/`scale`/`priority`/`cachePolicy` などのヒントを持つ。`State<ImageLoaderRequest>` が更新されるたびに新しいロードタスクを生成。
- `ImageLoader`（headless component）の props 例:
  - `State<ImageLoaderRequest>* request;`
  - `State<Image>* outImage;`
  - `ErrorSink* errorSink;`
  - `ResumableTaskFactory* taskFactory;`
- フロー:
  1. request が変わると既存タスクを `cancel()` し、`outImage` に `Image::Empty()` を set。
  2. 新しい `ResumableTask` を起動し、Platform ネイティブの I/O / デコード API（GDI+, WIC, NSImage 等）で bitmap を生成。
  3. 成功したら `ImageRecord` を `Managed` でラップして `outImage->set(...)`。失敗・メモリ不足は `ErrorSink->push()` に `ErrorEvent{payload = Managed<ImageLoaderRequest>, resumableTask}` を渡す。
- `ImageInfo(Image, outInfo, ErrorSink)` と `BitmapAccess(Image, request, ErrorSink)` を用意し、NodeContext が必要に応じてメタ情報取得や CPU アクセスを要求できる。非対応フォーマットやロック不可のときは Sink へ通知。
- 変更不可 Image に対してミューテーション要求が来た場合は `ErrorSink` へエラーを投げる。Mutable 対応 Image は `BitmapAccess` が write flag を受け取り、ResumableTask 経由でバックグラウンド更新する。

## 11. BlobLoader / BlobStream 基盤

- `Blob` も `Managed<BlobRecord>` で実体を共有し、`Blob::Empty()` を初期値にする。`BlobRecord` には `MutableState<size_t> size`, `MutableState<bool> isLoading`, `MutableState<bool> isCompleted`, `MutableState<bool> isMutable`, `MutableState<float> progress` を保持。`Blob::UnknownProgress()` (= -1.0f) を `IsIndeterminate` と同義の定数として公開し、FTP/ストリーミングのように最大サイズ不明な場合のプログレスバーに使う。
- `BlobLoaderRequest` は URL/File/Memory/Resource などのソースと `expectedSize`, `priority`, `incremental` フラグを持つ。`State<BlobLoaderRequest>` を headless component に渡す。
- `BlobLoader(&request, &outBlob, &errorSink)`:
  - request 変更で旧タスクを cancel。
  - ResumableTaskFactory 経由で別スレッド I/O を開始（Win32: ReadFile/URLMon、macOS: CFReadStream/NSURLSession 等）。
  - 開始で `isLoading=true`, `isCompleted=false`, `progress=0 or Blob::UnknownProgress()` をセットし、完了で `BlobRecord` を更新し `outBlob->set(...)`。失敗は ErrorSink へ push。インクリメンタル/長さ不明の場合は常に `UnknownProgress()` を使い、そうでなければ完了時に `progress=1.0f` にする。
  - `incremental=true` の場合、チャンク到着ごとに `State<BlobChunk>` や `EmitterState<BlobChunk>` を発火。
- `BlobBuffer(&bufferState, &latestBlob, &errorSink)`:
  - `State<Blob>` を ring buffer に蓄積してストリーム再生や再試行に使えるようにする helper。`bufferState` は `State<BlobStream>` として expose し、UI ノードが subscribe できる。
  - WebRTC/WebSocket などの連続データは `BlobStream` を介して push/pull できる。
- Asset 配布:
  - Win32 では Program Files 配下や `%LOCALAPPDATA%` へアセットを配置し、`kSourceFile` で読み込む。パッケージリソースは `kSourceResource` として `FindResource/LoadResource` を内部で行う。

---

## 12. 次の検討事項（優先度高）

現在検討中の、優先度の高いタスクリスト。

- **DSLの改善: modifierの導入**
  - `Text("...").padding(5).onClick(...)` のように、メソッドチェーンでプロパティやレイアウト修飾子を指定できる、より流れるような(fluent)インターフェースを目指す。
  - これにより、`TextProps` のような中間オブジェクトの記述を削減し、宣言的なUI記述をさらに強化する。

- **DSLの改善: 動的レンダリング**
  - **リスト生成:** `map`/`filter` のようなStream APIライクな操作で、`State` が持つコレクションから動的にUIコンポーネントのリストを生成する。
  - **条件分岐:** Solid.jsの `<Show>` や `<For>` に相当する、`If(condition, ...)` や `Loop(items, ...)` のような制御フローコンポーネントを導入する。

- **論理文字列への移行**
  - 現在の `std::string` ベースから、多言語対応やプラットフォーム間の差異を吸収できる、より抽象化された文字列クラスへの移行を進める。

- **Stateベースのエラーハンドリングと分析**
  - セクション9, 10, 11で検討した `ErrorSink`, `BlobLoader` を実装し、非同期処理のエラー通知やリソースロードの失敗をリアクティブに処理する機構を完成させる。

- **実装例の拡充**
  - **画像ビューア:** `Blob` と `ImageLoader` の具体的な利用例として、画像表示コンポーネントを実装する。
  - **MyTracker:** `OpenMacTracker` のような外部アプリを参考に、編集可能なTrackerアプリをサンプルとして作成する。
