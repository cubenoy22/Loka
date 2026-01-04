# Loka 設計メモ（最小仕様 / Solid‑mode）

> **まず動く最小**に再縮約。仮想 DOM や global diff/apply はやめて、**Solid.js 型の微粒度リアクティブ**で直接 Host を更新する。

---

## 2025-11-09 — 実装状況（Implementation Status）

### ✅ 完成済み（Production Ready）

#### 1. **State システム（`common/core/State.hpp`）**

```cpp
// 基底クラス: StateBase
// - bind/unbind/deferBind/deferUnbind API を提供
// - 優先度付きハンドラ管理（priority: -1=defer, 0=normal, 100=high）
class StateBase {
  virtual void bind(OnChangeFn cb, void* userData,
                   bool callImmediately=true, bool callOnce=false, int priority=0);
  virtual void unbind(OnChangeFn cb, void* userData);
  virtual void deferBind(OnChangeFn cb, void* userData, int priority=0);
  virtual void deferUnbind(OnChangeFn cb, void* userData);
};

// 読み取り専用State: State<T>
// - get() のみ公開、set()はprotected
// - bind時に即座に呼ばれる（callImmediately=true）
template<typename T> class State : public StateBase;

// 書き込み可能State: MutableState<T>
// - set(value, forceUpdate=false) を公開
// - StateTracker統合（markDirty）済み
template<typename T> class MutableState : public State<T>;

// 派生State: DerivedState<T>
// - 依存Stateの変化で自動再計算（recompute()）
// - EvalFn仮想基底クラスで計算式をカプセル化（C++98互換）
template<typename T> class DerivedState : public State<T>;

// イベント専用State: EmitterState（= State<void>）
// - emit() で購読者全員に通知
// - ボタンクリック等のOS起点イベントに使用
class EmitterState : public State<void>;
```

**実装の特徴:**

- ✅ **優先度付きハンドラ**: `bind(..., priority)` で実行順制御
- ✅ **deferBind**: 即時実行せず、通知時のみコールバック発火（設計書の`bindDefer`相当）
- ✅ **deferBindWithOld**: 旧値・新値を受け取るコールバック対応
- ✅ **EmitterState**: 値を持たないイベント伝播専用 State（UI イベントに最適）

#### 2. **StateTracker（`common/core/StateTracker.hpp`）**

```cpp
// トランザクション管理・依存伝播エンジン
class PushStateTracker : public StateTracker {
  void begin();                    // トランザクション開始
  void markDirty(StateBase* s);    // State変化をマーク
  void defer(Fn, userData);        // 副作用を遅延実行キューに積む
  bool end();                      // dirty伝播→再計算→deferred実行
  void registerDependency(...);    // 依存グラフ構築（DerivedState用）
  TrackerPhase phase() const;      // IDLE/PRECOMMIT/COMMIT
};
```

**実装の特徴:**

- ✅ **依存グラフ管理**: DerivedState の依存関係を自動追跡
- ✅ **循環依存検出**: `visiting_` セットで再帰チェック
- ✅ **トランザクション境界**: `begin()`～`end()` で変化を一括処理
- ✅ **deferred 実行**: micro-tick 終端相当の動作（`end()`で一括発火）

**設計書との対応:**

- `StateTracker::end()` が **micro-tick 終端** に相当
- `defer()` が **bindDefer/onStable** の実装基盤
- `markDirty()` → 依存伝播 → `recompute()` の流れが確立済み

#### 3. **NodeComposition（`common/core2/scene/NodeComposition.hpp`）**

```cpp
struct NodeComposition {
  // Arena: compose中に作られた全Definitionのコピーを所有
  std::vector<NodeDefinitionBase*> arena_;
  NodeDefinitionBase* root_;

  // アリーナにコピー作成（one-shot構築）
  template<typename T> T* copyToArena(const T& def);

  // ルートノード宣言
  template<typename T> T& declare(const T& def);

  // ツリー生成
  Node* createNodeTree() const;

  // DSL糖衣（conditional, stream, map, filter）
  ConditionalDefinition conditional(State<bool>& cond, T& node);
  StreamView<It> stream(It begin, It end);
};
```

**実装の特徴:**

- ✅ **Arena ベース**: 一時オブジェクトをヒープにコピーして所有
- ✅ **One-shot 構築**: `declare()` → `copyToArena()` → Arena 管理
- ✅ **DSL 統合**: conditional/stream/map/filter 対応

**設計書との対応:**

- `copyToArena()` が設計書の **Arena push** に該当
- `declare()` が設計書の **単一ノード宣言** に該当
- NRVO 不要・コピー回避は `copyToArena()` の **明示的ヒープ管理** で達成

#### 4. **Managed<T>（`common/core/Managed.hpp`）**

```cpp
template<typename T>
class Managed {
public:
  typedef void (*ReleaserFn)(T*, void*);

  static Managed<T> Wrap(T* value,
                         ReleaserFn releaser = &Managed::DefaultDelete,
                         void* userData = 0);
  bool isValid() const;
  int useCount() const;
  T* get() const;
  void reset(); // refcount-- / 0でreleaser
};
```

**実装の特徴:**

- ✅ **Intrusive ref-count**: コピー/代入で retain/release。`State<Managed<ImageHandle> >` がそのまま書ける。
- ✅ **カスタム解放**: `ReleaserFn` で ImageCache/Platform が優先度・LRU に応じた破棄トリガを受け取れる。
- ✅ **値比較対応**: `==`/`!=` 実装済みなので `State<T>` の比較にもフィット。C++98 専用でも扱いやすい。

**利用シナリオ:**

- Image/QR ローダーが `Managed<ImageRecord>` を返し、`State` に乗せた参照カウントで使用中リソースを保護。
- NodeContext が `Managed<NativeNodeContext>` を保持し、`reset()` でネイティブリソースを即座に返却。

### ⚠️ 部分実装（Partial / Needs Integration）

#### 4. **Node/NodeDefinition（`common/core2/scene/Node.hpp`）**

```cpp
class Node {
  NodeContext* context;
  MutableState<NodeDirtyFlags> dirty;  // PROPS/CHILD/LAYOUT/MYSELF
  virtual void compose() {}
};

// Definition = Props + Factory を保持するラッパー
template<class PropsT, class NodeT>
struct NodeDefinition : public NodeDefinitionBase {
  PropsType props;
  Node* create() const { return new NodeT(props); }
};
```

**現状:**

- ✅ Dirty 管理の基盤あり（`DirtyType` enum）
- ⚠️ Host 生成・Binder 接続のロジックは **未確認**
- ⚠️ `compose()` → Arena → Host 生成の**フルパイプライン**が不明

### ❌ 未実装（Not Yet / Design Only）

#### 5. **Platform-specific NodeContext 実装**

```cpp
// Node/Props/NodeDefinition の基盤は完成（common/core2/scene/Node.hpp）
// プラットフォーム別のNodeContext実装が必要:
// - Win32ButtonContext (HWND wrapper)
// - NSButtonContext (NSButton wrapper)
// - など
```

#### 6. **Layout（deferred-bounds）**

```cpp
// 設計書のBoxLayout実装は未確認
// NodeにMutableState<DirtyType>があるので、LAYOUT dirtyの基盤はある
```

#### 7. **Window::mount() Entry Point**

```cpp
// Window.hppは存在するが、mount(CompositeNode&)の実装は未確認
```

### 📝 micro-tick スケジューリングの位置づけ

**現在の実装:**

- `StateTracker::end()` が **トランザクション終端** = **micro-tick 終端** に相当
- `deferBind()` のハンドラは `notifyStateChanged()` で即座に呼ばれる（厳密な micro-tick 集約ではない）

**設計書の意図:**

- フレーム内で複数回の `State.set()` が起きても、**一度だけ**再計算・レイアウト実行
- `bindDefer` は **micro-tick 終端で集約**して発火

**統合の方針:**

- `StateTracker` の `defer()` を活用すれば、**既存実装で十分**
- `deferBind` のハンドラを **即座に呼ばず**、`StateTracker::defer()` に積む改修を検討
- または、現状の `deferBind` を「準即時（同期的だが低優先度）」と割り切る

**結論:**

- ✅ **今すぐ micro-tick 厳密化は不要**（StateTracker のトランザクション境界で実用上問題なし）
- ⚠️ 将来的に「フレーム内で複数トランザクション」が発生する場合、**PlatformContext 側でフレーム境界管理**を追加

---

## 2025-11-06 — Minimal Spec (Solid‑mode)

## 2025-11-06 — Window 内 DSL にフォーカス（実装ロードマップ）

### Scope（今回は Window 内のみ）

- App/Window クロスプラットフォーム統一は“後”。まずは **Window 内 DSL** を完成させる。
- jotai 的な粒度に備え、**Atom/State のスコープ**は Window 単位を基本に（必要ならモジュール静的で共有）。

### TODO（最小で実用的にする）

このセクションの TODO は `docs/TODO.md` に集約済み。

---

## ErrorSink 設計メモ（Image/Loader 系リソース向け）

- 背景:
  - ImageLoader / ImageProvider が URL・Data など複数 State を束ねると、読み込み失敗・メモリ枯渇・Cancellation など**複数のエラー**がほぼ同時に発生し得る。
  - `State<Error>` だと上書きで潰れるし、`State<std::vector<Error> >` は Scene DSL から扱いづらい。
  - Android 的な「トースト」「ダイアログ」など、**順次処理** or **リアルタイム通知**をモードごとに切り替えられるメカニズムが必要。

- コンセプト: `ErrorSink`
  - Window/Scene 単位で 1 つ以上用意できる**イベントキュー**。
  - Producer（ImageLoader/QRLoader/PlatformController 等）は `ErrorSink::push(const ErrorEvent &evt)` を呼ぶだけでよい。
  - `ErrorSinkScope`（仮）を導入し、特定の compose/ノードサブツリーにローカルな Sink を差し込めるようにする。これにより「どの ImageLoader が失敗したか」などを容易にトレースでき、エラー時の UI も局所化できる。
  - Headless コンポーネントは `ErrorSink child(&currentTaskContext, parentSink)` のようにローカル Sink を生成し、その中で発生したエラーは `currentTaskContext`（`State<Request>` や `ResumableTask`）を payload に含めたうえで親へ forward/flush する。処理中のタスク情報が ErrorEvent に必ず載るので、ユーザーにも「どのダウンロードで失敗したか」が提示できる。
  - Consumer は 2 パターンをサポート:
    1. **Pull 型** … `bool tryPop(ErrorEvent *out)` で順次処理。UI コンポーネントが1つずつダイアログを出したいときに使う。
    2. **Push/通知型** … `EmitterState` や `State<ErrorEvent>` と連動させ、トーストのように即時通知。Sink 内部で `EmitterState` を保持し、新規 push ごとに emit。
  - `ErrorEvent` には以下を含める:
    - `ErrorDomain`（例: kErrorDomainImage, kErrorDomainIO）
    - `ErrorCode`（enum or int）
    - `std::string message`（ユーザー向け or デバッグ向け）
    - `Managed<void>` payload（失敗したリソースハンドルや再試行用 Request を格納）
    - `Severity` / `RecoveryHint` など（Toast/ダイアログ振り分け用）

- 所有と露出:
  - `PlatformContext` がデフォルトの Sink を提供（`PlatformContext::errorSink()`）。
  - Scene / Headless コンポーネントは `NodeComposition` 経由で `ErrorSink*` を受け取る。要求された Sink を渡さないと compose できないルールにすれば、エラーを握りつぶせなくなる。
  - Sink 自体は `Managed<ErrorSink>` で共有しても良いが、基本は Singleton/Window 固定でもOK。

- Resumable / retry:
  - `ErrorEvent` の payload として `Managed<Request>` などを入れておき、Consumer が `retry()` できるようにする。
  - Sink 側は `markResolved(eventId)` などを持ち、再実行後に同じエラーを重複表示しない仕組みを追加予定。
  - OS / Platform ごとの非同期処理を共通化するために `ResumableTask`（pause/resume/cancel を備えた抽象）を用意し、ImageLoader のような Headless コンポーネントがタスク実行をプラットフォームに委譲できるようにする。マルチスレッド IO や Priority をここで吸収する。

- UI への統合例:
  1. `ErrorHost`（仮）コンポーネントが `ErrorSink*` を受け取り `deferBind` でイベントを購読 → `StateQueue` に積んで独自 UI を表示。
  2. Platform 固有（Win32 のトースト）に直接繋ぐ場合は `Win32PlatformController` が Sink を監視し、`MessageBox` などを開く。
  3. `withErrorSinkScope(ErrorSink* parent, ErrorSink* local, composeFn)` のようなAPIで局所的に Sink を差し替え、エラーパスのタグ付けや親Sinkとのネストを可能にする。エラーイベントには `path`/`labels` を含め、ダイアログで「BMIカードの画像読み込みに失敗」のように絞って表示できるようにする。
  4. `ErrorEvent` へ `std::vector<std::string> tags` を含め、Headless コンポーネントは `"bmi-card.image-loader"` のようなタグを付与。親 Sink が必要に応じてタグにマッチするものだけを再掲示したり、デバッグログへ振り分けたりできる。

- TODO:
  - このセクションの TODO は `docs/TODO.md` に集約済み。

---

## ImageLoader / Image アクセス設計

- `Image` 型:
  - `struct Image { Managed<ImageRecord> handle; }` のように `Managed<T>` を内包した軽量値。`Image::Empty()` シングルトンを用意し、`State<Image>` が常に有効値を持つようにする。
  - `ImageRecord` には `NativeImageHandle`（NSImage/HBITMAP 等）、`memoryCost`, `size`, `mutableToken` などを格納。Global/Scene キャッシュで共有し、refcount 0 で破棄。
- `ImageLoaderRequest`:
  - `enum SourceType { URL, DATA, FILE_HANDLE, PLATFORM_NATIVE };`
  - `struct ImageLoaderRequest { SourceType type; std::string url; Managed<BinaryData> bytes; Size desiredSize; float scale; int priority; bool allowDiskCache; }`
  - `State<ImageLoaderRequest>` をNodeが受け、変更時に ResumableTask で新規ロードを実行。
- `ImageLoader` / `ImageProvider`:
  - Headless component。Props: `State<ImageLoaderRequest>* request; State<Image>* outImage; ErrorSink* errorSink; ResumableTaskFactory* taskFactory;`
  - `compose()` で `request` を bind → 変更時に `Image::Empty()` を outImage へ set → `taskFactory->create(request)` で非同期ロード開始。
  - 成功: decode 結果を `Managed<ImageRecord>` にラップして `outImage->set(newImage)`。
  - 失敗: `ErrorSink` へ `ErrorEvent{domain=kImage, code=kDecodeFailed, payload=Managed<Request> }` を push。`ResumableTask` を payload に含めれば「Retry」ボタンで `resume()` が呼べる。
  - キャンセル/再リクエスト時は `ResumableTask::cancel()` で OS 側 I/O を止める。
- `ImageInfo` / `BitmapAccess`:
  - `bool ImageInfo(const Image& image, ImageInfoResult* out, ErrorSink* sink)` … `out` には width/height/scale/format/alpha を格納。情報がキャッシュされていなければ `ResumableTask` で取得し、失敗時に sink へ通知。
  - `bool BitmapAccess(const Image& image, BitmapAccessRequest* req, ErrorSink* sink)` … CPU メモリへマップし、`req->callback(BitmapView view)` でピクセルへアクセス。書き込みを許可すると `Mutable` モードとなり、終了後に Platform へアップロード。非対応フォーマットの場合は sink へエラーを送る。
  - いずれも宣言的 API として DSL に露出させず、NodeContext/Platform 層が必要時に呼ぶ想定。
- 再サイジング/ミューテーション:
  - `ImageInfo` の設定値（`desiredSize` 等）が変更されたら `ImageLoader` が新しい `ImageRecord` を生成して `State<Image>` を更新。既存 Image の Mutable 編集が必要な場合は `ResumableTask` によってバックグラウンドで再アップロードし、結果を同じ `Image` ハンドルへ反映。失敗時は `ErrorSink` 経由で通知。
- DSL への統合:
  - UI ノード（`ImageNode` 等）は `State<Image>*` を受け取るだけ。`Image::Empty()` の場合はプレースホルダを描画。
  - `ImageDialog`, `ImagePreview` など複数のコンポーネントが同じ `State<Image>` を共有しても refcount で安全に管理される。

---

## BlobLoader / BlobStream / Streaming バッファリング

- `Blob` 型:
  - `struct Blob { Managed<BlobRecord> handle; }` として Image と同じく `Managed` で実体を共有。`Blob::Empty()` を用意し、`State<Blob>` が null を扱わない。
  - `BlobRecord` には `MutableState<size_t> size`, `MutableState<bool> isLoading`, `MutableState<bool> isCompleted`, `MutableState<bool> isMutable`, `MutableState<float> progress` を揃える。`Blob::UnknownProgress()` (= -1.0f) を `IsIndeterminate` と同じ意味の定数にし、FTP や規模不明のストリーミングで indeterminate progress bar を表示する際に使える。`BlobInfo(&blob, &info)` を後日用意して Scene からメタ情報を取得可能にする。
- `BlobLoaderRequest`:
  - `enum BlobSource { kSourceUrl, kSourceFile, kSourceMemory, kSourceStream };`
  - `struct BlobLoaderRequest { BlobSource source; std::string url; std::string filePath; Managed<BinaryData> memory; size_t expectedSize; int priority; bool incremental; }`
  - `State<BlobLoaderRequest>` を bind し、変更時に I/O タスクを更新。
- `BlobLoader`:
  - Props: `State<BlobLoaderRequest>* request; State<Blob>* outBlob; ErrorSink* errorSink; ResumableTaskFactory* taskFactory;`
  - 振る舞い:
    1. 新しい request でタスクを作成し、Platform 固有の I/O API（Win32ならReadFile/URLMon、macOSならCFReadStream 等）を別スレッドで扱う。
    2. ロード開始時に `isLoading=true`, `isCompleted=false`, `progress=0 or Blob::UnknownProgress()` を設定し、完了/失敗で `ErrorSink` に通知。成功時は `BlobRecord` を更新して `outBlob->set(newBlob)`、`progress=1`（長さ不明 or incremental なら `UnknownProgress()`）をセット。
    3. `incremental` が true の場合は `onChunk` 的なイベントを `State<BlobChunk>` や `EmitterState<BlobChunk>` で配信し、ストリーム再生（WebRTC/動画/音声）に備える。
- `BlobBuffer` / `BlobStream`:
  - `BlobBuffer(&bufferState, &latestBlob, &errorSink)` のような headless component を用意し、`State<Blob>` を ring buffer や double buffer に流し込む。
  - `State<BlobStreamRequest>` を受けとり、`latestBlob` が更新されるたびに `bufferState` へ追加。`bufferState` は `State<BlobStream>`（内部に queue を持つ操作オブジェクト）として expose。
  - WebRTC などのリアルタイム用途では `BlobStream` 上で `State<BlobFrame>` を発行し続ける。`BlobStream` 自体が `EmitterState` を内包して新着データを通知するイメージ。
- 典型的な使い方:
  1. `State<BlobLoaderRequest> request; State<Blob> blob;`
  2. `BlobLoader(&request, &blob, &errorSink);`
  3. `BlobBuffer(&buffer, &blob, &errorSink);`
  4. UI ノード/コンポーネントは `buffer` から `State<BlobChunk>` を subscribe して audio/video 再生 or ファイル一覧などを更新。
- Win32 での配布:
  - アプリに同梱するデータは `Program Files\\MyApp\\assets\\` のような固定パスか、`%LOCALAPPDATA%` へ展開したキャッシュを参照。BlobLoaderRequest に `kSourceFile` + `filePath` を渡せば、Win32 側で `CreateFileW + ReadFile`、macOS なら `NSFileHandle` などを使って同じ API で扱える。
  - パッケージ埋め込みリソース（リソースDLLやRCファイル）を読みたい場合は `kSourceResource` を追加し、Win32 の `FindResource/LoadResource` を内部で行う。

---

## Graphics / Multimedia モジュール構想

- **パッケージ層（案）**
  - `declara.graphics.cpu` … QuickDraw / GDI / CoreGraphics など CPU ラスタライズ系を集約し、BitmapAccess/Blob と連携。
  - `declara.graphics.gpu` … OpenGL / OpenGL ES / Direct3D / Vulkan / Metal を束ね、ImageRecord の GPU ハンドルを管理。
  - `declara.ui.platform` … NSView / NSImage / HWND などの OS 固有 UI を PlatformContext で隠蔽。Loka API からは見えない。
  - `declara.multimedia` … ゲーム、動画、MV（OpenGL + MIDI 再生など）を統合。リアルタイム/オフライン両対応でカスタムアニメーションカーブやタイムラインを扱う。
- **Image の架け橋**
  - `Image` は CPU/GPU いずれのハンドルも内包できる。QR コードなど CPU 生成のビットマップも `ImageRecord` に格納して GPU テクスチャへアップロード可能。
- **NativeStyle / NativeAnimation**
  - key-value 形式でスタイル/アニメーションを指定し、PlatformContext が理解できるキーだけ適用。未対応キーは無視。
  - `platform` を指定すれば CoreAnimation 固有のオプションや `declara.multimedia` 独自カーブを渡せる。Web/SSG 向けには CSS 相当への変換を想定。
- **隠蔽ポリシー**
  - すべての OS 固有実装は PlatformContext に閉じ込める。Loka コアやアプリは抽象 API だけを参照。

### 既存の Node/Props パターン（実装済み）

**設計書の「Host Interfaces」は不要。** 既存の `Node + Props + NodeContext` パターンで完結する。

```cpp
// 例: ButtonNode の実装パターン（common/app/Button.hpp）

// 1. Props: State参照を保持
struct ButtonProps : public NodePropsBase<ButtonProps> {
  State<std::string>* text;
  State<bool>* enabled;
  EmitterState* onClick;  // ← OSイベントはemit()で伝播

  ButtonProps& setText(State<std::string>* t) { text = t; return *this; }
  ButtonProps& setOnClick(EmitterState* e) { onClick = e; return *this; }
};

// 2. Node: Props参照を保持、NodeContextで実際のHost管理
class ButtonNode : public Node {
public:
  const ButtonProps& props;
  ButtonNode(const ButtonProps& p) : props(p) {}
  // NodeContext* context; // ← OS固有のウィジェット/ビューを保持
};

// 3. Definition: Arena用のファクトリ
struct ButtonDefinition : public NodeDefinition<ButtonProps, ButtonNode> {
  // create() で new ButtonNode(props) を返す
};
typedef ButtonDefinition Button;  // DSL糖衣
```

**NodeContext の責務:**

- OS 固有のウィジェット（NSView, HWND 等）を保持
- Props の State に bind して、変化を OS ウィジェットに反映
- OS イベントを EmitterState::emit() で Loka 側に伝播

---

## 2025-11-10 — NodeContext/NativeContext 設計アップデート

- **NodeContext は Node のヒープ**として再定義。`NodeContext::useState<T>()` やシンプルな allocator を追加し、コンポーネント内のローカル state・タイマ・非同期ハンドルをここに閉じ込める。`Node` 破棄時に Context ごと掃除できるため、Compose DSL に副作用が流れ込まない。
- **NativeNodeContext** は NodeContext を継承 or 内包し、HWND/NSView/GPU リソースと `priority`/`memoryCost` を保持。PlatformController は `priority` を見てメモリが逼迫したときに低優先度ハンドルから破棄できる。ImageLoader/QRLoader もここにハンドルや pending request を置く。
- Loka コアは NativeNodeContext のフィールド（`priority`, `memoryCostBytes`, `persistent`, `releaseRequested` など）を参照せず、あくまで opaque な箱として扱う。プラットフォーム実装（Win32/macOS）が必要に応じて優先度・コスト情報を読んでリソースを管理する。
- **ComponentContext（仮）**: Scene/カスタムコンポーネントの `compose` から NodeContext にアクセスさせる橋渡しを検討中。DSL (`NodeComposition`) はあくまで Definition 構築専用にし、ランタイム state への入口は ComponentContext→NodeContext の経路だけに限定する。
- **State 所有ルール**を更新：短命 state は NodeContext、長命/共有 state は Scene/親 props、グローバルキャッシュは PlatformContext が持つ。優先度や解放戦略は NativeNodeContext + Platform 側で制御。

**ルール:**

- **Props は State へのポインタだけ**を持つ（値のコピーなし）
- **Node は Props の const 参照**を保持（不変）
- **NodeContext が State を購読**し、OS ウィジェットを更新
- **OS → Loka** は `EmitterState::emit()` で一方向伝播

**例: OS 側の実装（擬似コード）**

```cpp
// Win32ButtonContext（NodeContextの実装）
class Win32ButtonContext : public NodeContext {
  HWND hwnd;
  const ButtonProps& props;

  Win32ButtonContext(const ButtonProps& p) : props(p) {
    hwnd = CreateWindow("BUTTON", ...);

    // Props の State を購読（一点更新）
    if (props.text) {
      props.text->bind(&Win32ButtonContext::updateText, this);
    }
    if (props.enabled) {
      props.enabled->bind(&Win32ButtonContext::updateEnabled, this);
    }
  }

  static void updateText(void* ctx) {
    Win32ButtonContext* self = (Win32ButtonContext*)ctx;
    SetWindowText(self->hwnd, self->props.text->get().c_str());
  }

  // OSイベント→Loka
  void onOSClick() {
    if (props.onClick) {
      props.onClick->emit();  // ← これだけ！
    }
  }
};
```

**メリット:**

- ✅ Host Interface 層が不要（Node + NodeContext で完結）
- ✅ Props は State ポインタだけ（軽量・コピーなし）
- ✅ OS 固有の実装は NodeContext 継承で隠蔽
- **Node は Props の const 参照**を保持（不変）
- **NodeContext が State を購読**し、OS ウィジェットを更新
- **OS → Loka** は `EmitterState::emit()` で一方向伝播

**例: OS 側の実装（擬似コード）**

```cpp
// Win32ButtonContext（NodeContextの実装）
class Win32ButtonContext : public NodeContext {
  HWND hwnd;
  const ButtonProps& props;

  Win32ButtonContext(const ButtonProps& p) : props(p) {
    hwnd = CreateWindow("BUTTON", ...);

    // Props の State を購読（一点更新）
    if (props.text) {
      props.text->bind(&Win32ButtonContext::updateText, this);
    }
    if (props.enabled) {
      props.enabled->bind(&Win32ButtonContext::updateEnabled, this);
    }
  }

  static void updateText(void* ctx) {
    Win32ButtonContext* self = (Win32ButtonContext*)ctx;
    SetWindowText(self->hwnd, self->props.text->get().c_str());
  }

  // OSイベント→Loka
  void onOSClick() {
    if (props.onClick) {
      props.onClick->emit();  // ← これだけ！
    }
  }
};
```

**メリット:**

- ✅ Host Interface 層が不要（Node + NodeContext で完結）
- ✅ Props は State ポインタだけ（軽量・コピーなし）
- ✅ OS 固有の実装は NodeContext 継承で隠蔽
- ✅ Arena パターンと完全に整合

> 旧「BoxLayoutNode（最小仕様）」は、**deferred-bounds**（レイアウト）ポリシーに統合済み。

## 2025-11-06 — NodeComposition / compose（Arena & One‑shot 版）［内部実装向け］

### 方針

- **compose は初回の一度だけ実行**（One‑shot）。
- `NodeComposition` は **Arena（ヒープ）** として `std::vector` でノード/バインディングを蓄積。
- **引数で NodeComposition を受け取る方式**を採用（戻り値方式は不採用）

### 設計判断：なぜ戻り値ではなく引数方式か

**検討した方式 A: 戻り値（不採用）**

```cpp
class CompositeNode {
  NodeDefinition compose() {  // ← 戻り値で返す
    return Button().setText("OK");
  }
};
```

**問題点:**

- NodeDefinition のコピーコストが大きい（Props + 子配列）
- 68k/PPC 等の古い ABI では NRVO が不完全で、コピーが多発
- 一時オブジェクトの管理が複雑

**採用した方式 B: 引数（現行）**

```cpp
class CompositeNode {
  void compose(NodeComposition& c) {  // ← Arena を引数で受け取る
    c.declare(Button().setText("OK"));
  }
};
```

**メリット:**

- ✅ コピーなし：Arena に直接 push
- ✅ C++98/古い ABI でも安定動作
- ✅ メモリ配置が明確（Arena 一発管理）
- ✅ NRVO に依存しない

### NodeComposition（最小 API：Arena）

```cpp
struct Slot { uint32_t idx; }; // Arena 内インデックス（スコープ開始点など）

class NodeComposition {
public:
  // スコープ（親子の視点切替）
  NodeComposition makeChild(Slot parent_start);
  Slot current() const; // 現在スコープの開始 Slot

  // 状態（スコープ所有）
  template<class T>
  State<T>& useState(Slot owner, const T& initial);

  // --- DSL（operator<< 撤廃） ---
  // 1) 単一ノードを “宣言” して Arena に追加（1 call = 1 Node）
  NodeId declare(const Text&);
  NodeId declare(const Button&);
  NodeId declare(const EditText&);
  NodeId declare(const Headless&); // Timer/Processor 等（非可視）
  NodeId declare(const Box&);      // layout 指定（last-wins）

  // 2) 子ノード（Composite）を明示的に compose する糖衣
  template<class Child>
  void child(Child& node) { node.compose(makeChild(current())); }

  // 3) スコープだけを作る（Fragment/Scope）。headless だけを束ねる等に使う
  template<class Fn>
  void scope(Fn body) { auto sub = makeChild(current()); body(sub); }

  // 条件分岐（糖衣）
  template<class Fn>
  void conditional(bool pred, Fn body) { if (pred) scope(body); }
};
```

### compose 契約

- `compose(NodeComposition&)` は **pure**（I/O 禁止）。Arena に**逐次 push**するだけ。
- 子は `child.compose(c.makeChild(c.current()))` のように**スコープを切り替えて push**。
- Host の生成は Arena を走査して **一括 build**（初回のみ）。以後は Binder が一点更新。

### 実装メモ

- Arena の要素は `struct Item{ Kind; Props; parent; firstChild; nextSibling; }` のような **POD 中心**で管理。
- レイアウトは **最後に push された layout 要素（last‑wins）** をスコープ単位で適用。
- `compose` 中の `set/update` は禁止（検出で defer）。
- Key/epoch/memo は **One‑shot 前提では不要**（必要になれば Parking で再導入）。

## 2025-11-06 — Idea: Scoped State in NodeComposition（useState‑like）

- **Goal**: `NodeComposition` スコープで `State<T>` を確保し、**その子ツリーだけ**を更新対象に限定。
- **Semantics**: `c.useState<Key,T>(init)` は **現在スコープ（Slot）に所有**される `State<T>` を返す。再 compose 時も Key で再利用。
- **Update**: `set()` は **当該スコープ配下の Binder だけ**を通知（グローバル走査なし）。
- **Lifetime**: スコープが破棄されたら自動 dispose。Host 再生成なしで Binder 解除 → 再接続。
- **Keys**: **Solid 一発構築では未使用（Parking）**。再 compose/再利用が必要になった時点で導入。
- **Threading**: メインスレ限定（現行ルールを継承）。
- **Status**: 先送り（MVP 外）。実装は Parking。

### Micro API（案）

```cpp
// in NodeComposition
template<class T>
State<T>& useState(const Slot& owner, Key key, const T& initial);

// usage in compose
void Child::compose(NodeComposition& c){
  Slot me = c.ensureBoxLayout(c.root(), true, 4, Key{0xB0});
  State<float>& weight = c.useState<float>(me, Key{0xW1}, 60.0f);
  State<float>& height = c.useState<float>(me, Key{0xH1}, 170.0f);
  // bind -> EditText, Computed BMI -> Text
}
```

### Pros / Cons（要点）

- ✅ React より**賢い局所更新**：所有スコープで伝播が閉じる。
- ✅ Solid‑mode と自然整合：Signal の局所島を作るだけ。
- ⚠️ 破棄/再接続の順序・リーク対策が必要（dispose の厳格化）。
- ⚠️ Key 設計と状態復元（再 compose 時）に注意。

## 2025-11-06 — Headless Components / DerivedState パイプライン（本作用の値化）

### Decision（低レイヤー隠蔽 / 高レイヤー集中）

- **HeadlessNode / LogicNode / EffectNode** も、UI と同列の **Node**。
- I/O や重い処理は **見えないノードとして compose 内に push** するだけ。
- Host やスレッド、タイマ等の **低レイヤー処理は PlatformContext 側**が最適化。
- Headless でも **onAttach** を受けられる（attachState と組み合わせる）。
- すべては最終的に **State.set()** の“値更新”に帰着（本作用）。
- **Timer や Poller も State を持てば動く**（可視/不可視は関係なし）。

### 目的

- 重い計算や API 通信を **UI と分離**し、**値として State に流す**。
- コアは純粋・UI は一点更新・I/O は境界の外で扱う。

### 最小コンセプト

- **MutableState**: 入力。
- **DerivedState**: `U f(T...)` の派生（純粋計算）。
- **State Variants**: `Debounced` / `Throttled` / `Deferred` などは **State 側のバリエーション**として提供（クラスではなく生成時の種別指定）。
- **HeadlessNode（HeadlessProcessor の役割を内包）**: 見えない“処理ノード”。I/O や重計算を外部で実行し、**結果だけ**`MutableState` に `set()`。

### ルール

- **本作用＝値の更新**：重計算や API の“結果”は `State.set()` として扱う。
- **副作用は境界の外**：スレッド/HTTP/FS は Host 実装やサービス層で行い、コアは値のみ受け取る。
- **キャンセル/再入**：同一スコープで最新リクエスト以外はキャンセル（token 世代管理）。
- **スコープ**：`NodeComposition` の Slot に紐づけて **局所的に完結**（破棄で自動 dispose）。

## 2025-11-07 — Headless Lifecycle Hooks（compose 前提で“機械的に書ける”）

### 4 つのフェーズ

0. **Compose（One‑shot）**: Arena に push。**実行はしない**（購読/タイマ開始は不可）。
1. **Attach**: Host 生成・Binder 接続・購読開始。→ この瞬間に **onAttach** を実行。
2. **Stabilize（micro‑tick）**: `bindDefer` の書き込みを集約 → 安定したら **onStable** を実行（1 回）。

# Decision（低レイヤー隠蔽 / 高レイヤー集中）

- **HeadlessNode / LogicNode / EffectNode** も、UI と同列の **Node**。
- I/O や重い処理は **見えないノードとして compose 内に push** するだけ。
- Host やスレッド、タイマ等の **低レイヤー処理は PlatformContext 側**が最適化。
- Headless でも **onAttach** を受けられる（attachState と組み合わせる）。
- すべては最終的に **State.set()** の“値更新”に帰着（本作用）。
- **Timer や Poller も State を持てば動く**（可視/不可視は関係なし）。

### 目的

- 重い計算や API 通信を **UI と分離**し、**値として State に流す**。
- コアは純粋・UI は一点更新・I/O は境界の外で扱う。

### 最小コンセプト

- **MutableState**: 入力。
- **DerivedState**: `U f(T...)` の派生（純粋計算）。
- \*\*State Variants\*\*: `Debounced` / `Throttled` / `Deferred` などは **State 側のバリエーション**として提供（クラスではなく生成時の種別指定）。
- **HeadlessNode（HeadlessProcessor の役割を内包）**: 見えない“処理ノード”。I/O や重計算を外部で実行し、**結果だけ**`MutableState` に `set()`。

### API スケッチ（擬似コード）

```cpp
// Derived（純粋）
template<class U, class F, class... Ss /* states */>
class DerivedState { /* subscribes to Ss..., computes U via F */ };

// Debounce/Throttle（値の間引き）
template<class T> class Debounced { /* subscribe to S, emit last after N ms */ };

template<class T> class Throttled { /* emit at most 1 per N ms */ };

// Headless（UIなしの処理）
class HeadlessNode（HeadlessProcessorの役割を内包） {
public:
  template<class InState, class OutState, class Fn>
  static void connect(InState& in, OutState& out, Fn fn /* heavy or I/O */);
  // 実体はOS/プラットフォーム側で並列/キャンセル/再試行など最適化
};
```

### GravityCalc 例（デバウンス）

```cpp
State<Params> params;
auto derived = DerivedState<Work, BuildWork>(params);    // 軽い加工
auto deb = Debounced<Work>(derived, /*ms=*/120);         // 間引き
State<Result> result;
HeadlessNode（HeadlessProcessorの役割を内包）::connect(deb, result, GravityCalc /* heavy */);
// UI は result を Text/Graph に bind するだけ
```

### ルール

- **本作用＝値の更新**：重計算や API の“結果”は `State.set()` として扱う。
- **副作用は境界の外**：スレッド/HTTP/FS は Host 実装やサービス層で行い、コアは値のみ受け取る。
- **キャンセル/再入**：同一スコープで最新リクエスト以外はキャンセル（token 世代管理）。
- **スコープ**：`NodeComposition` の Slot に紐づけて **局所的に完結**（破棄で自動 dispose）。

### ステータス

- MVP 外（Parking）。ただし **Derived + Debounced + Headless の“型”だけ**は早期導入しても実装負債が少ない。

## 2025-11-06 — DSL 案：`<<` ストリーム方式（declare 撤廃 / Arena 直書き）

### 結論

- `declare(...)` と `ensure*` は廃止。\`\`\*\* に一本化\*\*し、Arena に直接 push。
- **headless / visual / layout** を同列に扱い、**最後の layout** を適用（last‑wins）。
- `conditional` で条件付き push（If/Switch は後回し）。

### 使用像（擬似コード）

```cpp
class MyGreatNode : public CompositeNode {
  void compose(NodeComposition& c) {
    auto me = c.current();
    auto& gravity = c.useState<Result>(me, {});

    c << Headless(GravityCalc{ .params = soManyParams, .out = gravity })
      << Text(Label{ .bind = gravity })
      << Button(Ok{})
      << Box(Vertical{ .gap = 8 }); // スコープ内の最終レイアウトを適用
  }
};
```

### 実装ノート

- `operator<<` は **Arena 末尾に Item を追加**。必要なら `parent` を `current()` のスコープで設定。
- 初回起動時に Arena を一次走査：Host を create → Binder 接続 → layout を適用。
- 更新は `State.set()` → 対応 Binder の setter 呼び出しのみ（Host 再生成なし）。
- 将来、複数レイアウトやネスト指定が必要になったら `c.layout(Box(...))` で明示化可。

## 2025-11-06 — Layout を `layout()` なしで実現（deferred-bounds 方式）

### 結論 / Decision

- **LayoutNode に \*\***\`\`\***\* は不要**。各ノードの `position/size` を **deferred bind** で State に流し、 伝播が\*\*一度静まった（micro‑tick 安定）\*\*タイミングで最新の Bounds が揃う。
- 親は **自分の Bounds 更新を起点**に、配下の子ノードの Bounds を**計算 →bind**するだけ（ループで OK）。
- `NodeComposition` のスコープ内に子がまとまっているので、**子列を走査**して Bounds を配布すれば完結。

### 仕組み / Mechanics

- **bindDefer(bounds)**：`State<Bounds>` への書き込みを **micro‑tick の末尾に集約**（バッチ化）。
- **stabilization barrier**：フレーム内の更新が止まったら 1 回だけ **onStable** を発火（必要なら）。
- **一点更新**：各子は `bind(bounds)` で Host の `setBounds` に直結。Host 再生成は起きない。

### 擬似コード（BoxLayout の再設計）

```cpp
struct Bounds { int x,y,w,h; };

class BoxLayoutNode : public CompositeNode {
  bool horizontal; int gap;
  std::vector<NodeId> children; // このスコープの visual 子Id
public:
  void compose(NodeComposition& c) {
    // 子の列をスコープから取得して保持（Arena側で列挙可）
    children = c.enumerateVisualChildren(/*of:this-scope*/);

    // 親のboundsが変わったら子のboundsを再計算
    c.bindDefer(c.boundsOf(this), [&](const Bounds& parent){
      int x = parent.x, y = parent.y;
      for (auto id : children) {
        auto sz = c.preferredSize(id); // 固定 or State から
        Bounds b = { x, y, sz.w, sz.h };
        c.bindBounds(id, b); // -> Host.setBounds(...)
        if (horizontal) x += sz.w + gap; else y += sz.h + gap;
      }
    });
  }
};
```

### ルール / Rules

- **compose は pure**：`bindDefer` 登録と子列キャプチャのみ。I/O 禁止。
- **親 → 子の一方向**：親 Bounds の安定化で子 Bounds が決まり、子は setter 経由で反映。
- **wrap/flow**：将来拡張。今ははみ出しを許容（従来方針どおり）。

### 影響 / Notes

- 既存の `BoxLayoutNode::layout(...)` は**不要**になり、この方式に置き換え。
- headless コンポーネントは影響なし（非可視・レイアウト対象外）。

## 将来拡張：State Variants（Debounce/Throttle 等）

MVP では`deferBind()`で基本的な遅延実行を実現。将来的に以下の State バリエーションを PlatformContext 経由で提供可能：

- **Debounced**: 最後の変化のみ反映（タイマーベース）
- **Throttled**: 一定期間に 1 回だけ適用
- **Buffered**: 複数の値をバッファリング
- **Sampled**: 一定間隔でサンプリング

実装時は各 OS のタイマー/ランループ機構を活用。

```cpp
// Solid（既定）: one-shot compose
class SolidNode : public CompositeNode {
  void compose(NodeComposition& c) {
    // 初回のみ実行、State変化は bind で一点更新
  }
};

// React Island（将来）: 局所的な再compose
class ReactIsland : public CompositeNode {
  void compose(ReactNodeComposition& c) {
    // ReactState変化で再composeトリガー
    // Key付きで子を再利用
  }
};
```

### 実装時の要件

- **Key**: 子ノードの識別・再利用に使用
- **Reconciliation**: Key 付き差分アルゴリズム
- **局所化**: Island 内部の変化は外部に即座伝播させない
- **micro-tick 統合**: 既存の deferBind メカニズムと整合

> 詳細は実際に必要になった時点で設計。現状は Solid 実装に集中。

### 既定マッピング（当面のルール）

- **Text/String の bind** → `Visual` 即時反映。
- **Bounds/Size/ParentBounds の bindDefer** → `Layout`（reflow）。
- **Headless の in/out** → `Logic`（安定後にまとめて実行）。
- **PreferredSize の変化** → `Measure`（必要なら計測 →`Layout`）

### API メモ

```cpp
struct Subscription {
  StateBase* src;
  DirtyMask mask;        // 変化で立てるビット
  Action immediate;      // その場で実行（省略可）
  Action deferred;       // micro‑tick終端で一度だけ
  Slot scope;            // dirty を集計するスコープ
};
```

### 方針

- `State.set()` は**値だけ**。Dirty 判定は **購読レイヤ**に押し込める（宣言的）。
- 既定マッピングで 9 割を自動化しつつ、**特殊ケースは手動で bindLogic/bindDefer を使って宣言**。
- これにより **"値の世界"と"スケジューリング"の責務分離**を維持。

---

## 2025-11-09 — BMI計算アプリ実装計画
