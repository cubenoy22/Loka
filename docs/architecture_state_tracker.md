# Loka State / Tracker 実装ノート（2025-11 最新）

`common/core/State.hpp` と `common/core/StateTracker.*` に実装されている State システムの整理メモ。  
`declara_design_minutes.md` の Solid-mode アプローチと揃えて、実装のどこを見れば良いか・どう使うかを一本化した。

---

## 1. 実装の骨格

| 型/モジュール | 役割 | 定義場所 |
| --- | --- | --- |
| `StateBase` | すべての State の基底。`bind`/`deferBind` と依存列挙を提供 | `common/core/State.hpp` |
| `State<T>` | 読み取り専用状態。`bind` で即時購読、`deferBind` で遅延購読 | 同上 |
| `MutableState<T>` | `State<T>` + `set(value, forceUpdate)`。`currentTracker` に dirty を通知 | 同上 |
| `DerivedState<T>` | 依存先 `std::vector<StateBase*>` と `EvalFn` で値を自動合成 | 同上 |
| `EmitterState` | `State<void>` の軽量イベント版 (`emit()` だけ呼べば OK) | 同上 |
| `StateTracker` | begin/end/defer/markDirty API を定義する抽象基底 | `common/core/StateTracker.hpp` |
| `PushStateTracker` | 依存グラフを push 型で伝播させる実装 | `common/core/StateTracker.*` |
| `AutoTransactionGuard` | RAII で `begin()`/`end()` を自動管理 | `common/core/util/AutoTransactionGuard.hpp` |

**Tracker 登録:** `PushStateTracker` はコンストラクタで受け取った State から `dependencies` を走査し、`registerDependency` によりグラフを構築 (`StateBase::getDependencyStates()` を使用)。Window など長寿命オブジェクトは `makeStateVector(...)` で監視対象を束ねる。

---

## 2. 依存伝播フロー

```
MutableState::set()
  └─ 値が変われば State<T>::notifyStateChanged()
        └─ bind/deferBind したハンドラを優先度順に発火
  └─ 現在の Tracker があれば markDirty(this)
          └─ dependents[dependency] を辿り dirty queue へ
StateTracker::end()
  └─ dirty queue を安定するまで再計算 (DerivedState::recompute)
  └─ phase=COMMIT で deferred 副作用を一括実行
```

- `deferBind` は「値が変わったタイミングだけ呼びたい」場合に利用。`bind` との優先度を `StatePriority` で制御できる。
- `deferBindWithOld` は旧値/新値をまとめて扱うラッパー。`State<void>` の場合も `EmitterState` で同じ API。
- `StateTracker::defer(fn, userData)` を直接呼べば、micro-tick(= `end()` 後) まで副作用を遅延できる。Solid-mode の「micro tick 集約」はこの仕組みで代替可能。

---

## 3. DerivedState の実践メモ

### 3.1 EvalFn パターン

```cpp
struct BMICalc : DerivedState<float>::EvalFn {
  State<float>* weight;
  State<float>* height;
  BMICalc(State<float>* w, State<float>* h) : weight(w), height(h) {}
  float operator()() {
    const float w = weight->get();
    const float h = height->get();
    return (h > 0.f) ? w / (h * h) : 0.f;
  }
};

std::vector<StateBase*> deps;
deps.push_back(&weight);
deps.push_back(&height);

DerivedState<float> bmi(deps, new BMICalc(&weight, &height));
```

- 依存は `std::vector<StateBase*>` で明示。Scene/Window 単位で `StateTracker` に巡回検知させられる。
- EvalFn は `operator()()` を持つ純粋仮想基底なので、C++98 でもメンバーをキャプチャ可能。
- `DerivedState::recompute()` が true を返すと親 Tracker が dependents をたどって再帰的に dirty 化。

### 3.2 多値合成の型まとめ

複数の State を struct にまとめて返すと props 連携が整理しやすい。

```cpp
struct UserForm { std::string name; std::string email; int age; bool agree; };

struct FormEval : DerivedState<UserForm>::EvalFn {
  State<std::string>* name;
  State<std::string>* email;
  State<int>* age;
  State<bool>* agree;
  UserForm operator()() {
    UserForm f = { name->get(), email->get(), age->get(), agree->get() };
    return f;
  }
};
```

さらに `DerivedState<bool>` を `UserForm` に依存させれば複数段の合成も自然に書ける。

---

## 4. トランザクション制御と副作用

- `PushStateTracker::begin()` は監視対象の `currentTracker` をセットし、dirty/deferred バッファを初期化。
- `end()` は dirty が無くなるまで `recompute()` を繰り返し、最後に `deferred` を FIFO で実行。`TrackerPhase` を `IDLE -> PRECOMMIT -> COMMIT -> IDLE` と更新するので、ネスト検出も容易。
- 明示的に複数の `set()` をまとめたい場合は `AutoTransactionGuard guard(window->getTracker());` を使う。
- `MutableState::set(value, /*forceUpdate=*/true)` とすると値が同じでも通知を行う。既存 UI で「タイトルを押下したら必ず再描画」などの要件に使用。

---

## 5. bind/deferBind の実務メモ

| API | 目的 | 代表的な使い方 |
| --- | --- | --- |
| `bind(cb, userData, callImmediately=true, callOnce=false, priority=0)` | 値が変わるたびに即時通知。`callImmediately` で現在値を同期。 | UI 初期化で OS ウィジェットへ即反映したいとき |
| `deferBind(cb, userData, priority)` | 変更検知タイミングのみ通知。複数変更をまとめたい場合に便利。 | NodeContext 内でレイアウトや I/O をまとめて叩く |
| `deferBindWithOld(cb)` | 旧値と新値の差分が必要な場合に利用。 | `Window::title` 変更ログなど |
| `EmitterState::emit()` | OS 起点イベント → Loka の一方向橋渡し。 | Win32Button の `BN_CLICKED` など |

Bind 解除は `unbind` / `deferUnbind` を必ず対で呼ぶ。NodeContext のデストラクタで解除すればリークを防げる。

---

## 6. デバッグとガード

- `StateTracker` と `MutableState` には `#ifdef TEST_BUILD` のログが既に仕込まれている。`cmake -DTEST_BUILD=ON` などで有効化すると依存グラフや dirty 伝播の様子が標準出力に出る。
- 循環依存は `PushStateTracker::markDirty` 内で `visiting_` セットを使って検出。検出時は stderr に警告を出すだけなので、必要なら assert に切り替えると早期に気付ける。
- DerivedState の EvalFn が割り込み可能な OS API を呼ぶ場合は `StateTracker::defer()` に逃がす。Solid-mode の micro tick を厳密にやりたいなら `MutableState` 側ではなく Tracker 側で一括管理する。

---

## 7. 今後のフォロー項目

1. **NodeContext との自動統合**  
   - `Win32PlatformContext::createNodeContext` がまだ `NodeContext` の空実装だけを返している。`StateTracker` から `NodeContext` の `defer` キューを共有できるようブリッジする。
2. **Scene/Window 境界の micro-tick**  
   - 現状は `Window` ごとに Tracker を 1 つだけ持ち、`Window::getTracker()` から直接 begin/end を呼ぶ。複数 Window が並行で走る場合のガード（phase チェックなど）を追加予定。
3. **DerivedState のライフサイクル管理**  
   - EvalFn に `new` を渡している場所（BMI プランなど）は `ScopedPtr` で包む or Scene の destructor で delete するルールをドキュメント化する。

---

## 8. 参照ファイル

- `common/core/State.hpp`
- `common/core/StateTracker.hpp`
- `common/core/StateTracker.cpp`
- `common/core/util/AutoTransactionGuard.hpp`
- `common/core/util/StateUtil.hpp` (`makeStateVector` など)
- `docs/bmi_calculator_implementation_plan.md`（具体例）

2025-11-10 改訂
