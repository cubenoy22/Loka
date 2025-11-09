# Declara! 設計メモ（最小仕様 / Solid‑mode）

> **まず動く最小**に再縮約。仮想DOMや global diff/apply はやめて、**Solid.js 型の微粒度リアクティブ**で直接 Host を更新する。

---

## 2025-11-06 — Minimal Spec (Solid‑mode)

## 2025-11-06 — Window内DSLにフォーカス（実装ロードマップ）

### Scope（今回はWindow内のみ）

- App/Windowクロスプラットフォーム統一は“後”。まずは **Window内DSL** を完成させる。
- jotai 的な粒度に備え、**Atom/State のスコープ**は Window 単位を基本に（必要ならモジュール静的で共有）。

### TODO（最小で実用的にする）

-

### Minimal Host Interfaces（擬似コード・最新版）

```cpp
typedef void (*ActionFn)(void*);
struct Action { ActionFn fn; void* ctx; }; // UI/Headless共通の実行口

// Text
struct TextHandle;
struct TextHost {
  static TextHandle* create(/* parent */);
  static void destroy(TextHandle*);
  static void setText(TextHandle*, const char*);
  static void setBounds(TextHandle*, int x, int y, int w, int h);
};

// Button（Action統一）
struct ButtonHandle;
struct ButtonProps { Action onClick; };
struct ButtonHost {
  static ButtonHandle* create(/* parent */);
  static void destroy(ButtonHandle*);
  static void setText(ButtonHandle*, const char*);
  static void setEnabled(ButtonHandle*, bool);
  static void setBounds(ButtonHandle*, int x, int y, int w, int h);
  // OS側: click -> props.onClick.fn(props.onClick.ctx)
};

// EditText
struct EditTextHandle;
struct EditTextHost {
  static EditTextHandle* create(/* parent */);
  static void destroy(EditTextHandle*);
  static void setText(EditTextHandle*, const char*);
  static void setBounds(EditTextHandle*, int x, int y, int w, int h);
  // OS側: change -> State<string>.set(newValue)（正規化はOS側）
};
```

> 旧「BoxLayoutNode（最小仕様）」と「これで“実用最小アプリ”？」は、最新の **deferred-bounds**（レイアウト）と **Action 統一**ポリシーに統合済み。必要事項はそれぞれの章を参照。



## 2025-11-06 — NodeComposition / compose（Arena & One‑shot 版）［内部実装向け］

### 方針

- **compose は初回の一度だけ実行**（One‑shot）。
- `NodeComposition` は **Arena（ヒープ）** として `std::vector` でノード/バインディングを蓄積。
- **ensure 系は不要**。`operator<<` で Arena に**追記**していくストリームモデル。

### NodeComposition（最小API：Arena）

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

- `compose(NodeComposition&)` は **pure**（I/O禁止）。Arena に**逐次 push**するだけ。
- 子は `child.compose(c.makeChild(c.current()))` のように**スコープを切り替えて push**。
- Host の生成は Arena を走査して **一括 build**（初回のみ）。以後は Binder が一点更新。

### 実装メモ

- Arena の要素は `struct Item{ Kind; Props; parent; firstChild; nextSibling; }` のような **POD 中心**で管理。
- レイアウトは **最後に push された layout 要素（last‑wins）** をスコープ単位で適用。
- `compose` 中の `set/update` は禁止（検出で defer）。
- Key/epoch/memo は **One‑shot 前提では不要**（必要になれば Parking で再導入）。

## 2025-11-06 — Idea: Scoped State in NodeComposition（useState‑like）

- **Goal**: `NodeComposition` スコープで `State<T>` を確保し、**その子ツリーだけ**を更新対象に限定。
- **Semantics**: `c.useState<Key,T>(init)` は **現在スコープ（Slot）に所有**される `State<T>` を返す。再compose時も Key で再利用。
- **Update**: `set()` は **当該スコープ配下の Binder だけ**を通知（グローバル走査なし）。
- **Lifetime**: スコープが破棄されたら自動 dispose。Host 再生成なしで Binder 解除→再接続。
- **Keys**: **Solid一発構築では未使用（Parking）**。再compose/再利用が必要になった時点で導入。
- **Threading**: メインスレ限定（現行ルールを継承）。
- **Status**: 先送り（MVP外）。実装は Parking。

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
- ✅ Solid‑mode と自然整合：Signalの局所島を作るだけ。
- ⚠️ 破棄/再接続の順序・リーク対策が必要（dispose の厳格化）。
- ⚠️ Key 設計と状態復元（再compose時）に注意。



## 2025-11-06 — Headless Components / DerivedState パイプライン（本作用の値化）

### Decision（低レイヤー隠蔽 / 高レイヤー集中）

- **HeadlessNode / LogicNode / EffectNode** も、UIと同列の **Node**。
- I/O や重い処理は **見えないノードとして compose 内に push** するだけ。
- Host やスレッド、タイマ等の **低レイヤー処理は PlatformContext 側**が最適化。
- Headless でも **onAttach** を受けられる（attachState と組み合わせる）。
- すべては最終的に **State.set()** の“値更新”に帰着（本作用）。
- **Timer や Poller も State を持てば動く**（可視/不可視は関係なし）。

### 目的

- 重い計算やAPI通信を **UIと分離**し、**値としてStateに流す**。
- コアは純粋・UIは一点更新・I/Oは境界の外で扱う。

### 最小コンセプト

- **MutableState**: 入力。
- **DerivedState**: `U f(T...)` の派生（純粋計算）。
- **State Variants**: `Debounced` / `Throttled` / `Deferred` などは **State 側のバリエーション**として提供（クラスではなく生成時の種別指定）。
- **HeadlessNode（HeadlessProcessorの役割を内包）**: 見えない“処理ノード”。I/Oや重計算を外部で実行し、**結果だけ**`MutableState` に `set()`。

### ルール

- **本作用＝値の更新**：重計算やAPIの“結果”は `State.set()` として扱う。
- **副作用は境界の外**：スレッド/HTTP/FS は Host 実装やサービス層で行い、コアは値のみ受け取る。
- **キャンセル/再入**：同一スコープで最新リクエスト以外はキャンセル（token 世代管理）。
- **スコープ**：`NodeComposition` の Slot に紐づけて **局所的に完結**（破棄で自動 dispose）。

## 2025-11-07 — Headless Lifecycle Hooks（compose前提で“機械的に書ける”）

### 4つのフェーズ

0. **Compose（One‑shot）**: Arena に push。**実行はしない**（購読/タイマ開始は不可）。
1. **Attach**: Host生成・Binder接続・購読開始。→ この瞬間に **onAttach** を実行。
2. **Stabilize（micro‑tick）**: `bindDefer` の書き込みを集約 → 安定したら **onStable** を実行（1回）。
3. **Action turn**: `onAction` キューを順次実行（次フレーム／次micro‑tickへ安全に後送り）。

### NodeComposition の最小フックAPI

```cpp
// compose内では“登録だけ”。実行はライフサイクルで行われる
void onAttach(Action a);        // attach直後に1回だけ
void afterStable(Action a);     // micro‑tick安定後に1回だけ（後送り用途もこれで代用）
```

### HeadlessNode の書き味（composeで“登録だけ”する）

```cpp
class HeadlessNode : public CompositeNode {
  HeadlessProps p;
  static void request(void* selfV);
public:
  void compose(NodeComposition& c) {
    // attachで購読を開始
    c.onAttach(Action{ &HeadlessNode::request, this });

    // 入力の変化は“安定後1回”だけ処理に流す
    c.onStable(Action{ &HeadlessNode::request, this });

    // さらに、入力State自体にも deferred購読を登録（最新値だけ処理）
    c.bindDefer(*p.in, Action{ &HeadlessNode::request, this });
  }
};
```

### 目的と効果

- **composeは純粋**を維持：pushと“実行フックの登録”しか行わない。
- **attach時点で木が完成**しているから、購読開始や初期計算が安全。
- **onStable** によって、瞬間的な連鎖更新を**1回に集約**できる。
- **onAction** は“後で一発”の雑用（ログ/遅延初期化など）に便利。

# Decision（低レイヤー隠蔽 / 高レイヤー集中）

- **HeadlessNode / LogicNode / EffectNode** も、UIと同列の **Node**。
- I/O や重い処理は **見えないノードとして compose 内に push** するだけ。
- Host やスレッド、タイマ等の **低レイヤー処理は PlatformContext 側**が最適化。
- Headless でも **onAttach** を受けられる（attachState と組み合わせる）。
- すべては最終的に **State.set()** の“値更新”に帰着（本作用）。
- **Timer や Poller も State を持てば動く**（可視/不可視は関係なし）。

### 目的

- 重い計算やAPI通信を **UIと分離**し、**値としてStateに流す**。
- コアは純粋・UIは一点更新・I/Oは境界の外で扱う。

### 最小コンセプト

- **MutableState**: 入力。
- **DerivedState**: `U f(T...)` の派生（純粋計算）。
- \*\*State Variants\*\*: `Debounced` / `Throttled` / `Deferred` などは **State 側のバリエーション**として提供（クラスではなく生成時の種別指定）。
- **HeadlessNode（HeadlessProcessorの役割を内包）**: 見えない“処理ノード”。I/Oや重計算を外部で実行し、**結果だけ**`MutableState` に `set()`。

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

- **本作用＝値の更新**：重計算やAPIの“結果”は `State.set()` として扱う。
- **副作用は境界の外**：スレッド/HTTP/FS は Host 実装やサービス層で行い、コアは値のみ受け取る。
- **キャンセル/再入**：同一スコープで最新リクエスト以外はキャンセル（token 世代管理）。
- **スコープ**：`NodeComposition` の Slot に紐づけて **局所的に完結**（破棄で自動 dispose）。

### ステータス

- MVP外（Parking）。ただし **Derived + Debounced + Headless の“型”だけ**は早期導入しても実装負債が少ない。



## 2025-11-06 — DSL案：`<<` ストリーム方式（declare撤廃 / Arena 直書き）

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

- **LayoutNode に ****\`\`**** は不要**。各ノードの `position/size` を **deferred bind** で State に流し、 伝播が\*\*一度静まった（micro‑tick安定）\*\*タイミングで最新の Bounds が揃う。
- 親は **自分の Bounds 更新を起点**に、配下の子ノードの Bounds を**計算→bind**するだけ（ループでOK）。
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

- **compose は pure**：`bindDefer` 登録と子列キャプチャのみ。I/O禁止。
- **親→子の一方向**：親Boundsの安定化で子Boundsが決まり、子は setter 経由で反映。
- **wrap/flow**：将来拡張。今ははみ出しを許容（従来方針どおり）。

### 影響 / Notes

- 既存の `BoxLayoutNode::layout(...)` は**不要**になり、この方式に置き換え。
- headless コンポーネントは影響なし（非可視・レイアウト対象外）。
- `bindDefer` は `Debounced<T>` に近いが、**フレーム内バッチ化（micro‑tick）専用**である点が異なる。



## 2025-11-06 — State Variants を PlatformContext で生成（Debounce/Throttle/Defer を統合）

### 方針 / Policy

- デバウンスやスロットル等は **State のバリエーション**で表現。個別クラス（`Debounced<T>` 等）は廃止。
- 生成は **PlatformContext** 経由：各OSの **アイドル時間/ランループ/高精度タイマ** を最適活用可能にする。
- 実装されていない種類は **enum 設定で判別**し、フォールバック（＝Plain）または TODO を返す。

### API（擬似コード）

```cpp
enum class StateKind { Plain, Deferred, Debounced, Throttled, Buffered, Sampled };

struct StateParams {
  // 任意（未使用は0）
  int deferMicroticks;   // micro‑tick 何回先でまとめて適用（frame内集約）
  int debounceMs;        // 最後だけ適用までの待機時間
  int throttleMs;        // 最大適用頻度
  int bufferSize;        // バッファリング（0=なし）
  int sampleMs;          // サンプリング間隔
};

class PlatformContext {
public:
  template<class T>
  State<T> createState(StateKind kind, const StateParams& p, const T& initial);
};
```

### 使い方イメージ

```cpp
auto& params  = ctx.createState<Params>(StateKind::Plain,     {/*...*/}, {});
auto& work    = ctx.createState<Work  >(StateKind::Debounced,  {.debounceMs=120}, {});
auto& result  = ctx.createState<Result>(StateKind::Plain,     {/*...*/}, {});

// 派生は純粋計算（core内）
Computed<Work> built = derive(params, BuildWork);
// built の変化を work へ転送（State同士のbindはコアの小さなユーティリティで）
bind_state(built, work);

// Heavy/I-O は headless で out.set()
HeadlessNode（HeadlessProcessorの役割を内包）::connect(work, result, GravityCalc);
```

### 実装メモ / Scheduling

- **Debounced**: OSタイマ/RunLoopで遅延実行。再入は最後のタスクだけ有効。
- **Throttled**: 前回適用時刻から `throttleMs` 経過でのみ適用。
- **Deferred**: **frame内の micro‑tick** 終了でまとめて適用（`bindDefer` 相当）。
- **Buffered/Sampled**: 将来用。未実装なら `StateKind` チェックでフォールバック。

### エラー/状態（TODO）

- 値の世界を壊さずに扱うため、以下のいずれかを採用：
  - `State<Expected<T>>`（`ok()/error()`）
  - `struct { T value; Status status; Error err; }` で `State<>` 化（`Status={Ok,Pending,Error,Cancelled,Timeout}`）
- 方針はMVP後に決定。現状は `Result` と `Status` を別Stateで持つのが簡潔。



## 2025-11-07 — Action / Controller パターン（C++98対応のメソッドバインド）

### ねらい

- **MVCのController**に相当する“振る舞い”を、C++98でも安全・軽量に表現。
- `bindDefer` 等の購読に **関数オブジェクト（functor）必須** → C++98でも**インターフェイス or 関数ポインタ**で賄う。

### 最小インターフェイス（ICommand）

```cpp
struct ICommand { virtual ~ICommand() {} virtual void invoke() = 0; };

struct IncCommand : ICommand {
  State<int>* s; explicit IncCommand(State<int>* st) : s(st) {}
  void invoke() { s->set(s->get()+1); }
};
```

### メソッドバインド（MemberFnThunk）

```cpp
// C++98での this+メンバ関数 バインド

template<class T>
struct MemberFnThunk : ICommand {
  typedef void (T::*Method)();
  T* self; Method m;
  MemberFnThunk(T* s, Method mm) : self(s), m(mm) {}
  void invoke() { (self->*m)(); }
};

// 例: Controller が複数の State を調整
struct Controller {
  State<int>* a; State<int>* b;
  void incBoth() { a->set(a->get()+1); b->set(b->get()+1); }
};

// compose 内
Controller ctl; ctl.a=&countA; ctl.b=&countB;
MemberFnThunk<Controller> onClick(&ctl, &Controller::incBoth);
```

### Button などへの配線（Host Props）

```cpp
struct ButtonProps { ICommand* onClick; };
// OSイベント→ props.onClick->invoke();
```

### bindDefer との組み合わせ（Controllerらしい書き味）

```cpp
// 状態の安定化後にだけ動く“本作用”コントローラ
struct ReflowController : ICommand {
  NodeComposition* c; /* 他に参照が必要ならポインタで */
  void invoke() { /* latest State を読んで必要な set() を行う */ }
};

// 親Boundsが変わったあとに1回だけ再計算する
ReflowController rc; rc.c = &c;
c.bindDefer(parentBoundsState, /* functor= */ &rc /* via adapter if needed */);
```

### 代替: 超軽量Cスタイル

```cpp
typedef void (*Fn)(void*);
struct Handler { Fn fn; void* ctx; };
static void plus1(void* p){ State<int>* s=(State<int>*)p; s->set(s->get()+1); }
Handler h = { &plus1, &count };
// ButtonProps に載せて OS 側で h.fn(h.ctx)
```

### ポイント

- **Controller = functor** として **Node/Arenaの寿命に合わせて置く**（`new/delete`散在を避ける）。
- `bindDefer` は **micro‑tick終端で1回だけ**実行 → “あれ変わったらこうする”を**安全に**まとめて発火。
- UI/Headless問わず、**すべての“本作用”は invoke() → State.set()** に還元する。



## 2025-11-07 — Composition Lifecycle（idle/composing/composed）& Node Hooks

### States（Composition側の明確化）

- **idle**: 未構築。Arena空／Host未生成。実行フック不可。
- **composing**: `compose()` 実行中。Arenaに push している最中。**I/O禁止**・購読開始禁止・setter呼び出し禁止。
- **composed**: Arena確定・Host生成・Binder接続完了。ここで **lifecycle hooks が解禁**。

### Node Hooks（overrideで“機械的に”書ける）

- `onComposed()`：`composed` 遷移直後に **micro‑tick defer で一度だけ**呼ばれる（子が揃ってから安全に初期計算）。
- `onAttach()` / `onDetach()`：Headless/Hostless含む **全ノードが任意で持てる**。`onAttach` は Host生成・購読接続完了直後、`onDetach` は購読解除・破棄直前。

### Mini API（擬似コード）

```cpp
enum class ComposeState { Idle, Composing, Composed };

class NodeComposition {
public:
  ComposeState state() const;
  // compose内から“登録だけ”する（実行はライフサイクルで）
  void onAttach(Action a);
  void onStable(Action a);
  void onAction(Action a);
};

class CompositeNode {
public:
  virtual void compose(NodeComposition& c) = 0; // pure
  virtual void onComposed() {}                  // optional（deferred once）
  virtual void onAttach() {}                    // optional（Host/Binder接続後）
  virtual void onDetach() {}                    // optional（解放前）
};
```

### Sequencing（実行順）

1. `state=idle` → `composing`（`compose`実行・フック登録のみ）
2. Arena確定 → Host生成/Bind接続 → `state=composed`
3. `onAttach()` を **親→子の順**で呼ぶ（購読開始）
4. micro‑tick安定 → `onComposed()` を **親→子**で一度だけ呼ぶ（defer実行）
5. 以降、`onAction` キューを tick 毎に処理
6. 破棄時に **子→親の逆順**で `onDetach()`

### Headless と Hooks の関係

- HeadlessNode は `onAttach()` で **購読開始／初回ジョブ投げ**、`onComposed()` で **集約後の初期再計算**、`onDetach()` で **キャンセル/解除**。
- `bindDefer(...)` は `composed` 以降にのみ発火（compose中は積むだけ）。

### 安全規約

- `composing` 中は `State.set()`・Host setter は**禁止**（検出でdefer）。
- `onComposed()`／`onAttach()` からの `State.set()` はOK（一点更新が走る）。
- Hooks内で再入/循環が起これば **次micro‑tickに後送り**（無限ループ抑止）。



## 2025-11-07 — React‑mode 解禁時の差分（Solid→Reactの可逆ブリッジ）

### モード方針

- `CompositionPolicy` を導入：`SolidOneShot`（既定） / `ReactRerender`（解禁時）。
- **サブツリー単位で切替**できる `ModeScope(policy)` を DSL に追加（既定は Solid）。

### 何が変わるか（ハイライト）

1. **compose の再入**

   - Solid: one‑shot（初回だけ push）。
   - React: **再評価可**（props/stateの変化で `compose()` 相当が再実行）。
   - 対応：`NodeComposition` に **memo/bail‑out** を復活（Key付き）。

2. **State の所有範囲**

   - Solid: `useState(slot)` はスコープ限定、**ツリーの外へ波及しない**。
   - React: コンポーネント局所の state が **再composeを誘発**。
   - 対応：`useStateReact<T>()` を追加（再composeトリガ）。Solid用 `useState<T>()` は既存のまま。

3. **Effect 语義**

   - Solid: `bindDefer/onStable` 中心（micro‑tick終端）。
   - React: `useEffect`/`useLayoutEffect` 風の **依存配列**で実行管理。
   - 対応：`EffectDeps(deps...)` を React‑mode 時だけ有効化。Solid では従来 hook を推奨。

4. **Reconciliation（差分）**

   - Solid: diff/applyなし、**setter一点更新**。
   - React: **Key付き子リスト**での VTree 差分（再生成/再利用）。
   - 対応：`Key` を NodeProps に標準化、`children` は **keyed配列**を前提に shallow‑diff。

5. **Layout**

   - Solid: **deferred‑bounds**（layout()不要）。
   - React: そのまま使える（**再compose後も** bounds計算は micro‑tick に集約）。
   - 結論：レイアウトは **方式不変**（再composeが増えても bindDefer が吸収）。

6. **HeadlessNode（Controller）**

   - Solid: onAttach/onStable + bindDefer で駆動。
   - React: そのまま **不変**。再composeしても Headless は **Action/State ベース**なので再生成不要（Keyで保持）。

7. **スケジューリング**

   - Solid: PlatformContext の StateVariant（Deferred/Debounced/Throttled）。
   - React: 追加で **batched updates**（複数 set をひとまとめ）をサブツリー単位で有効化。
   - 対応：`BatchScope{enabled=true}` を React‑mode 時に自動付与。

### API スケッチ

```cpp
enum class CompositionPolicy { SolidOneShot, ReactRerender };

struct ModeScopeProps { CompositionPolicy policy; };
NodeComposition& operator<<(const ModeScopeProps&);

// React専用の局所State（再composeをトリガ）
template<class T> ReactState<T>& useStateReact(const T& initial);

// 依存配列つきEffect（React‑modeでのみ有効）
EffectHandle EffectDeps(void (*fn)(), const DepList& deps);
```

### 推奨ルール（回帰リスクを避ける）

- **副作用はcomposeに書かない**（SolidでもReactでも同じ）。
- **Headlessにロジック集約**：UIは state を束ねるだけ。React‑modeでも Controller設計を維持。
- **Keyの安定化**：Reactサブツリーでは children に **安定Key**を必須化。
- **混在OK**：重い領域は Solid（one‑shot）、頻繁に条件分岐やリスト入替がある領域だけ React。

### マイグレーション手順（段階導入）

1. まず **Solidの既存実装**を維持。
2. Reactが欲しいサブツリーに `ModeScope{ReactRerender}` を被せる。
3. その配下で `useStateReact` / `EffectDeps` / `BatchScope` を解禁。
4. パフォーマンスが落ちる場合は **StateVariant（Debounced/Deferred）** で再compose頻度を絞る。



## 2025-11-07 — React Islands via NodeComposition Subclass（Solidの中にReact方式を局所導入）

### ねらい

- **Solid 親の中に React 的再composeを局所解禁**する“島（Island）”。
- 既存の Solid ルール（one‑shot/一点更新）を壊さずに、**サブツリーだけ**再compose可能に。

### 仕組み（要約）

- `NodeComposition` を **サブクラス化**して Island 用の振る舞いを追加：
  - `useStateReact<T>()`：この Island 内に所有され、**変化で recompose を要求**する局所 state。
  - `bindDeferReact(...)`：Island 内の変化を **micro‑tick終端に集約**し、**一度だけ recompose**。
  - **伝播ゲート**：Island 内の内部 `State<T>` は **外部への即時伝播を止める**（外部は安定後の `Computed<T>` などで受け取る）。
- Island 自身が **自分のスロット配下を再compose**（Arenaの同一スロットに**子アリーナを差し替え**）。
- Host 再利用は **Key** で担保（なければ生成し直し）。

### 擬似API

```cpp
class ReactNodeComposition : public NodeComposition {
public:
  template<class T> ReactState<T>& useStateReact(const T& init);
  void requestRecompose();                 // island内で再composeを要求
  void bindDeferReact(Action a);           // micro-tick安定で一発
  void gatePropagation(bool enabled=true); // 外側への直伝播を抑止
};

class ReactIsland : public CompositeNode {
public:
  void compose(ReactNodeComposition& c) {  // ← NodeCompositionの派生を受ける
    c.gatePropagation(true);

    // island-local states
    ReactState<int>& local = c.useStateReact<int>(0);

    // 変化を安定後にまとめて再compose
    c.bindDeferReact(Action{&ReactIsland::recomposeOnce, this});

    // ここに通常通り UI/Headless を push（内部は自由に recompose）
    c << Text(/* ... */) << Button(/* ... */) << Box(/* ... */);
  }
private:
  static void recomposeOnce(void* self);
};
```

### 実行の流れ

1. Solid 親が通常どおり build → **ReactIsland の初回 compose**（one‑shot）
2. Island 内部の `ReactState` が変化 → **bindDeferReact** が **micro‑tick終端**に `requestRecompose()` を 1 回だけ発火
3. Island は **自スロット配下だけ**を再compose（外側は不変）。Keyで Host を再利用
4. 安定後、必要な外向き値は **Computed 経由で一括反映**（伝播ゲート解除ポイント）

### 併存性

- Island の中にさらに Island をネスト可（Solid↔React↔Solid…の入れ子）。
- レイアウトは既存の **deferred‑bounds** を継続使用（再compose後も micro‑tick集約で安全）。

### 安全規約（簡易）

- Island 内の `State.set()` は **外部へ直伝播しない**（gatePropagation有効時）。
- 外部へ出したい値は **Derived/Computed** を経由し、**Island安定後**に一度だけ反映。
- 無限再composeは **micro‑tick単位の一度きり**で抑止。次回以降は新たな変化が必要。



## 2025-11-07 — P0 Decisions（Solid-only build / 当面の確定方針）

### Reuse（再利用）

- **いまは不要**。Solid.jsの **one-shot** 前提で **都度再生成**。
- `LazyColumn` などの再利用系は **React-mode/Island** 実装時に設計（Parking）。

### Bounds（境界/サイズ）

- **Host透過**を優先：macOSなら **NSView 等の既存boundsをそのまま反映**。
- レイアウトは当面 **Boxだけ**で運用。`deferred-bounds` の親→子配布ルールは現状のまま。

### Threading（スレッド）

- **考慮不要**。`State.set()` を呼ぶ箇所は **すべてメインスレッド**に限定。
- OSイベントは Host 側でメインに正規化してから `set()`（ロック/ワーカースレッドをコアに持ち込まない）。

### Lifecycle（最小）

- フェーズは **composing → composed** の2段で十分。
- 使用するフックは `** / **` を基本（`onComposed` は任意）。
- **HeadlessはRAII**：不要になったUIと同時に破棄。グローバルに走らせたい処理は **Window直下/ App直下** に配置して寿命を延ばす。

### 備考

- 上記は **Solid-only の実用最小を早く完成**させるための暫定P0。React-mode解禁時に必要な再利用/Keys/Effect依存は Island 節の指針に従って追加する。



## 2025-11-07 — SizeSpec（Absolute / MatchParent）— P0最小ルール

### 目的

- MVP段階では **サイズ指定は二択**に限定し、実装と挙動を安定化させる。

### 定義

```cpp
enum class SizeSpec { Absolute, MatchParent };

struct Size { SizeSpec spec; int value; /* Absolute時のみ有効 */ };

struct LayoutProps { Size width; Size height; };
```

### ルール

- **Absolute**: `value` をそのまま `w/h` に採用。
- **MatchParent**: 親の `Bounds.w/h` をそのまま子に配布（deferred-boundsの確定後に一括）。
- **wrap/content-measure/ratio/weight/constraint**: Parking（将来拡張）。

### 備考

- Host透過（例: NSView/NSControl）の `bounds` と整合することを前提に設計。
- BoxLayout では `gap` と方向（水平/垂直）だけを用い、子サイズは上記二択で決定する。



## 2025-11-07 — Action配線の最小形（Headlessは onComposed で bind）

### 方針

- **まずは ****\`\`**** を第一選択**（C++98最薄）。`ICommand` は必要なときだけ（複数メソッドを束ねたい等）。
- **Headless は ****\`\`**** で購読開始**するのが一番シンプル。木が完成→安定化直前なので安全。
- 購読は \`\`（micro‑tick終端で1回）を基本にして“連鎖”を1発に集約。

### 擬似コード（最小パターン）

```cpp
class MyHeadless : public CompositeNode {
  State<Params>* in;     // inject via props/ctor
  State<Result>* out;    // inject via props/ctor
  static void process(void* selfV) {
    MyHeadless* self = (MyHeadless*)selfV;
    const Params p = self->in->get();
    const Result r = HeavyOrIO(p);    // 実体はHost/サービス層で最適化
    self->out->set(r);                // 本作用：値更新
  }
public:
  void compose(NodeComposition& c) override {
    // pure：pushやhook登録だけ。購読開始はしない
    // 必要ならここで onStable に "process" を登録しておいても良い
    c.onStable(Action{ &MyHeadless::process, this }); // 任意
  }
  void onComposed() override {
    // 木が組み上がった段階で in を購読開始（最新だけ処理）
    bindDefer(*in, Action{ &MyHeadless::process, this });
  }
};
```

### ルール

- `compose()` は**登録だけ**、`onComposed()` で**購読開始**。
- **Action優先**、`ICommand` は必要時のみ。メンバ関数は小さなアダプタで `Action` に包む。
- \*\*結果は \*\*\`\` に還元。UIは Binder が一点更新で反映。

> 備考: フレーム内で多段更新が発生しても、`bindDefer` により **micro‑tick 終端で一回**だけ `process` が走るため安定。



## 2025-11-07 — DSL最終案（operator<< 撤廃 / declare・compose・scope）［内部API案］

### 結論

- `operator<<` は**撤廃**。**明示APIのみ**に統一して曖昧さを排除。
- \`\`\*\* は 1コール=1ノード\*\*（複数は不可）。
- ` で Composite を compose。` はグルーピング/Fragment用途。
- headless は `か` で等価に扱える。

> 注: この章は **内部のArenaビルダーを直接使う場合**の案として残す。外部公開APIは次章の **Spec‑first** に一本化する方向。

---

## 2025-11-07 — Spec‑first API（公開・オプション）— compose は NodeSpec を返す

### 結論 / Public API 方針

- **外部には ****\`\`**** を露出しない**。
- すべての `CompositeNode::compose()` は \`\`**（単一ノード）を返す**。
- 可視/不可視を問わず **1つのノード**を返す。複数を返したいときは \`\` を使う。
- Headless だけの構成は \`\` を返す（表示はゼロでもOK）。

### NodeSpec（最小）

```cpp
enum class NodeKind { Fragment, Box, Text, Button, EditText, Headless };

struct NodeSpec {
  NodeKind kind;
  void*    props;              // 各ノードのprops先頭（C++98互換のためvoid*）
  NodeSpec* children; int childCount; // Fragment/Box 用（可視/不可視混在OK）
};

// ヘルパ（生成系）
NodeSpec Fragment(std::initializer_list<NodeSpec> cs);
NodeSpec Box(const BoxProps& p, std::initializer_list<NodeSpec> cs);
NodeSpec Text(const TextProps& p);
NodeSpec Button(const ButtonProps& p);
NodeSpec EditText(const EditTextProps& p);
NodeSpec Headless(const HeadlessProps& p);
```

### 例（公開API側の書き味）

```cpp
class MyGreatNode : public CompositeNode {
  NodeSpec compose() override {
    auto gravity = useState<Result>({}); // Windowスコープ/ローカルなど既定の所有規約
    return Box({ .vertical=true, .gap=8 }, {
      Headless({ .params=soManyParams, .out=gravity }),
      Text({ .bind=gravity }),
      Button({ .onClick = Action{ &Self::inc, this } })
    });
  }
private:
  static void inc(void* selfV);
};
```

### Mount（Window側の責務）

- `Window::mount(NodeSpec root)` が **一度だけ**呼ばれ、内部で **Arenaに変換→Host生成→Binder接続**。
- 以後の更新は **State.set() → Binder一点更新**。`NodeSpec` の再評価は不要（Solid one‑shot）。

### Headless の扱い

- `Headless` は **表示を持たないNodeSpec** だが、**State.set() を通じた“本作用”に参加**。
- Fragment の子として返すだけでよい（Windowは可視/不可視を区別せず処理）。

### 互換 / 内部実装

- 既存の **Arena & NodeComposition** は **内部実装**として継続利用可能（`NodeSpec` からビルド）。
- 将来 React‑mode/Island を導入する際は、`NodeSpec` の **差し替え/部分再評価**を Island スコープに限定して実装。

### メリット

- 外部APIが**極小で直感的**（`compose()->NodeSpec`）。
- Solid哲学（one‑shot・一点更新）と矛盾しない。
- Headlessのみの構成（表示ゼロ）も自然に書ける。

> **注**: **Classic/PPC/68kなどコピーに厳しい環境では**、このSpec‑firstは**無効化**し、後述の **Builder公開（**\`\`**）** を採用する。

---

## 2025-11-07 — NodeSpec→Arena ビルド（内部）

### 手順（One‑shot）

1. `NodeSpec` を先行走査して **サイズ見積もり**（Arenaの予約）。
2. 前順 or 幅優先で **Item をPODとして配置**（`kind/props/parent/firstChild/nextSibling`）。
3. 1回目の走査で **Host生成**、2回目で **Binder接続**、最後に **deferred‑bounds 配布**。

### Key/ID

- Solid one‑shot では **不要**。React‑mode時に Island 内部のみ導入。

### 注意

- Propsの寿命は **Windowスコープ≥Host寿命** を満たすこと（`void* props` のC++98互換要件）。

---

## 2025-11-07 — 既存章との整合

- 「NodeComposition / compose（Arena & One‑shot 版）」と「DSL最終案」は**内部実装向け**として残置。
- 公開APIは **Spec‑first** を基本に説明する。

### 結論

- `operator<<` は**撤廃**。**明示APIのみ**に統一して曖昧さを排除。
- \`\`\*\* は 1コール=1ノード\*\*（複数は不可）。
- ` で Composite を compose。` はグルーピング/Fragment用途。
- headless は `か` で等価に扱える。

### サンプル（最小）

```cpp
class MyGreatNode : public CompositeNode {
  void compose(NodeComposition& c) override {
    auto me = c.current();
    auto& gravity = c.useState<Result>(me, {});

    c.declare(Headless{ .params=soManyParams, .out=gravity });
    c.declare(Text{ .bind=gravity });
    c.declare(Button{ /* onClick=Action{...} */ });
    c.declare(Box{ .vertical=true, .gap=8 }); // last-wins layout
  }
};
```

### 方針

- **書き味は常に“明示”**：pushのつもりが無視される、などの曖昧さを排除。
- `child(...)` は `node.compose(c.makeChild(...))` の糖衣で、**意味のねじれがない**。
- `scope{...}` は Fragment/グループとしてだけ動き、**Hostを持たない**。

### Parking（将来検討）

- `compose()` が **単一ノードを返す ****\`\`**** 形式**の糖衣（leaf向け）。
- `declare(child())` のような二段糖衣。
- Island/React-mode 時の `recompose()` 用ショートハンド。



## 2025-11-07 — PPC/68k向けのコピー回避ガイド（Spec-first最適化）

### 目的

- C++98 + 古いABI（NRVO不完全・小オブジェクトでもコピー多発）の環境で、`NodeSpec` まわりの**無駄コピーを極小化**する。

### 方針（結論）

1. **NodeSpecは極小POD**（≲16バイト）：`kind(1) + childCount(2) + props(ptr) + children(ptr)` 程度。
2. **childrenは配列“へのポインタ”****で持つ（配列本体は****アリーナ**）。深いコピーを禁止。
3. **アリーナ（SpecArena）に一発配置**：`Fragment/Box` ヘルパは**呼出時に子配列をArenaへplacement-new**し、`NodeSpec.children`はそのポインタを指す。
4. **NRVOに依存しないAPI**：`FragmentN(a,b,...)`（N=0..4）等の固定長ヘルパで**一時配列を作らない**。N超過は `FragmentFrom(Arena, span)` を使う。

### 参考API（C++98互換）

```cpp
struct NodeSpec {
  uint8_t kind; uint16_t childCount; // pack可
  const void* props;                  // props先頭
  const NodeSpec* children;           // Arena内配列
};

struct SpecArena { void* bump; size_t cap; };

inline const NodeSpec* ArenaPushChildren(SpecArena& A, const NodeSpec* src, int n);

inline NodeSpec Fragment0() { return NodeSpec{K_Fragment,0,0,0}; }
inline NodeSpec Fragment1(const NodeSpec& a, SpecArena& A) {
  const NodeSpec* ch = ArenaPushChildren(A, &a, 1);
  return NodeSpec{K_Fragment,1,0,ch};
}
inline NodeSpec Fragment2(const NodeSpec& a,const NodeSpec& b, SpecArena& A) {
  NodeSpec tmp[2] = {a,b};
  const NodeSpec* ch = ArenaPushChildren(A, tmp, 2); // ここで一回だけコピー→Arena
  return NodeSpec{K_Fragment,2,0,ch};
}
// N>4 は FragmentFrom(A, span) を用意
```

### 実装ノート

- **propsはポインタ**（所有はWindow/長寿命側）。`std::string`等は置かない（Atom/インターン化）
- **childrenはindex方式**でも可：`childrenIndex, childCount` → Arenaの先頭からのオフセット
- **compose()はアロケーション禁止**：配列確保は**必ずArena経由**。一時配列は最大N要素の自動変数のみ
- **Window::mount(root)** 直後にArenaは読み取りのみ→**破棄禁止**。Host/Binder構築が終わるまで有効

### 代替（さらに厳しい68k向け）

- `SpecWriter` モードを用意：`compose(SpecWriter& w)` に切替えると**直接Arenaにemplace**（ツリー値の返りは使わない）
- Public APIはそのまま `compose()->NodeSpec`、Classicビルドだけ `#ifdef CLASSIC` で Writer 版を使用

> 要点：**値ツリーはPODの見かけ**に留め、**実体はArenaに一回だけ配置**。戻り値や関数間受け渡しで“構造体の深いコピー”を絶対に発生させない。



## 2025-11-07 — Public API 再考：Builder（NodeComposition）を公開する案

### 結論

- **MVP/Classic互換を最優先**するなら、**NodeComposition/NodeBuilderをそのまま公開**するのが最もシンプルで安全。
- `compose(NodeBuilder&)` で **直接Arenaへ記述**（コピーゼロ／NRVO依存なし）。
- **Spec‑first（NodeSpec戻り値）****は****モダン環境向けの糖衣**として**オプション**に格下げ。

### 公開API（Builder版）

```cpp
class NodeBuilder { /* (= NodeComposition) */
  // child/declare/scope/useState/afterStable/onAttach ... （<< は無し）
};

class CompositeNode {
public:
  virtual void compose(NodeBuilder& b) = 0; // 公開・安定API（推奨）
};

class Window {
public:
  void mount(CompositeNode& root) {
    NodeBuilder b; // Arenaを内包
    root.compose(b);
    build_from_arena(b); // Host生成→Binder接続→deferred-bounds
  }
};
```

### 糖衣アダプタ（必要なら）

```cpp
// モダン環境のみ: compose() -> NodeSpec を提供
#ifdef DECLARA_ENABLE_NODESPEC
struct NodeSpec { /* ... */ };

class CompositeNodeSpec : public CompositeNode {
public:
  virtual NodeSpec compose_spec() = 0; // 糖衣
  void compose(NodeBuilder& b) final {  // 互換レイヤ
    NodeSpec s = compose_spec();
    emit_to_builder(b, s);              // NodeSpec -> Builder へ一括書込
  }
};
#endif
```

### 方針の利点

- **コピー発生ポイントが消える**（Builder直書き）。
- 1つの公開API（`compose(NodeBuilder&)`）で**全環境**をカバー。
- 必要に応じて Spec‑first を**ビルドフラグで有効化**できる（書き味を選択可能）。

### まとめ

- **公開API＝Builder**、**Spec‑first＝オプション**。これで**PPC/68k**でも安心、かつモダン開発でも書き味を犠牲にしない。



## 2025-11-07 — Dirty Flags（micro‑tick集約）— 最小設計

### 結論

- \`\`\*\* 単体は“意味を知らない”\*\*（純粋な値更新）。
- **Dirtyは Binder 登録時に意味付け**：どの `State` が変わったら **どのスコープ/処理を dirty にするか** を **購読（bind）時に宣言**する。
- 実際のフラグセットは **通知経路（Binder）で行う**。`set()`→購読先へ通知→購読側が **DirtyMask** を立てる→**micro‑tick終端**で一括処理。

### 目的

- Solid 一点更新の思想を崩さず、**高コストの再計算（例：layout/measure/reflow/logics）だけ**を **まとめて遅延**させる。

### Dirty種別（最小）

```cpp
enum class DirtyKind : uint8_t { None=0, Visual=1, Layout=2, Measure=4, Logic=8 };
struct DirtyMask { uint8_t bits; };
```

### Binder登録時に“意味”を付与

```cpp
// NodeBuilder 内の例
void bind(State<T>& s, HostSetter<T> setVisual) {
  // 値変化→即時で Host setter を叩く（Visual）
  registry.add(s, {DirtyKind::Visual}, [=](const T& v){ setVisual(v); });
}

void bindDefer(State<Bounds>& parent, Action reflow) {
  // 親Bounds変化→Layout dirty → micro‑tick終端で reflow()
  registry.add(parent, {DirtyKind::Layout}, /*no immediate set*/, reflow /*deferred*/);
}

void bindLogic(State<X>& in, Action compute) {
  // 入力変化→Logic dirty → 安定後に一度だけ重計算
  registry.add(in, {DirtyKind::Logic}, /*no immediate set*/, compute);
}
```

### 実行モデル

1. `State.set()` が走る（意味は付けない）。
2. 購読テーブルが発火し、**即時系**（Visual）のみ **その場で setter** を叩く。
3. 同時に、購読エントリが **対象スコープの DirtyMask** にビットを立てる。
4. **micro‑tick終端**で、各スコープの DirtyMask を見て順に処理：
   - `Measure` → `Layout` → `Logic` の順など **決め打ちの優先度**で一度だけ実行。
   - 実行中にさらに set が来ても **次のmicro‑tick**に後送り。
5. フレーム境界でマスクをクリア。

### 既定マッピング（当面のルール）

- **Text/String の bind** → `Visual` 即時反映。
- **Bounds/Size/ParentBounds の bindDefer** → `Layout`（reflow）。
- **Headless の in/out** → `Logic`（安定後にまとめて実行）。
- **PreferredSize の変化** → `Measure`（必要なら計測→`Layout`）

### APIメモ

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

- `State.set()` は**値だけ**。Dirty判定は **購読レイヤ**に押し込める（宣言的）。
- 既定マッピングで9割を自動化しつつ、**特殊ケースは手動で bindLogic/bindDefer を使って宣言**。
- これにより **“値の世界”と“スケジューリング”の責務分離**を維持。

