# core2 実装・設計進捗メモ（2025-11）

Solid-mode（`common/core2/scene`）の進行状況と課題を一本化。  
`declara_design_minutes.md` のメモと突き合わせて、どこを掘ればいいかを明確にする。

---

## 1. 現状サマリ

- `Scene`（`common/core2/scene/Scene.hpp`）は `NodeComposition` を受け取る `compose(NodeComposition)` 形式に更新済み。`MutableState<SceneLifecycle>` を内包。
- `StaticNodeManager` / `IPlatformController` の骨格は追加済み（`NodeManager.hpp`, `PlatformController.hpp`）。  
  - Solid-mode では `StaticNodeManager::mount()` が `NodeComposition::createNodeTree()` でツリーを生成し、`materialize()` を呼び出す。
- `Node`, `NodePropsBase`, `NodeDefinition`, `NodeComposition`（Arena）は `core2` にまとまり、`common/app/Button.hpp` などの UI ノードはここを前提にしている。
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

- `Scene` サブクラスを作り、`Button`/`Box` など `common/app` のノードを `NodeComposition` に宣言してツリー生成まで確認する。
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
- Global/Scene ヒープで共有したいリソース（Image など）は `State< Managed<ResourceRecord> >` で扱う。`Managed<T>` が参照カウンタ/カスタム releaser を提供するので、State の set/unset だけで retain/release を保証できる。
- Loader 系ノード（例: ImageLoader, QRLoader）は Scene から受け取った `State<Request>` と `State<Handle>` を橋渡ししつつ、非同期処理のハンドルを `NodeContext` に保存する。非表示になったら Context 破棄で自動的にキャンセル/解放できる。
- `ComponentContext`（未実装）を用意し、Scene/カスタムコンポーネントの `compose` から NodeContext へアクセスできるフックを提供する予定。DSL 自体には NodeContext を露出せず、コンポーネント抽象で橋渡しする。

## 8. ErrorSink / エラーハンドリング指針

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

## 9. ImageLoader / Image アクセス

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

## 10. BlobLoader / BlobStream 基盤

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
