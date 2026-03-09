# Flow-Based UI Test Sketch

Exploration of reusing the Loka Flow DSL for automated UI testing.

## Scope and Security Model

- Default scope is **Scene inside the app window only**.
- Global UI (menu bar, app-external UI) is not part of default capture.
- Privileged/global inspection is optional and runs only when platform capability is available.
- If global inspection is unavailable, tests must report `SKIP` (not `FAIL`).

## Concept

Flow's `Step` / `goto` / `loop` / error handling maps directly to test scenario description. On failure, `Dump()` serializes the entire flow state for debugging.

`NextTickTracker` is used as the deterministic observation boundary:
- `update request -> next tick flush -> capture/assert`
- Multiple updates in the same tick are treated as one flush unit.
- `WaitNextTick()` は sleep 置換プリミティブとして扱い、待ち時間固定 (`sleep 100ms`) を禁止する。

## Two-Layer Test Strategy

テストは2層で構成し、Loka はデータ出力に徹する。判定はすべて外部 CLI / AI が担う。

```
Layer 1: Screenshot sequence diff (視覚的デグレ検出)
Layer 2: .snap parameter assert  (数値的デグレ検出)

Loka (C++98)           外部 CLI / AI
────────────────       ─────────────────────────────
スクショ連番保存   →   sequence diff → 視覚判定
.snap 書き出し     →   diff → 数値変化検出 → PR 判定
```

### Layer 1: Screenshot Sequence

- tick ごとにスクショを連番保存
- sequence 全体を diff することで「最終状態だけでなく中間状態のデグレ」も検出できる
- 外部 AI が「デグレか仕様変更か」を判定し PR に自動フィードバック

### Layer 2: .snap Parameter Assertion

- Flow 内に `Snap()` ステップを書くと、対象ノードのパラメータ（height, x, y, width など）をファイルに書き出す
- 同時に tick 単位の時間指標（`flushTimeMs`, `recomposeTimeMs`, `layoutTimeMs` など）も書き出す
- `.snap` ファイルが存在しない場合: 新規生成して **PASS**（初回受け入れ）
- `.snap` ファイルが存在する場合: 今回の値を別ファイルに書き出し、diff は外部ツールが担う
- Loka 側は「値を書くだけ」に徹し、判定ロジックを持たない

#### Minimal `.snap` Schema (v1)

```json
{
  "schema": "loka.snap.v1",
  "test": "TextWrapRelayout",
  "step": "after-wrap",
  "tick": 12,
  "node": {
    "id": "MainText",
    "bounds": {"x": 20, "y": 40, "width": 220, "height": 64},
    "dirty": ["DIRTY_LAYOUT", "DIRTY_PROPS"]
  },
  "timing_ms": {
    "flush": 3,
    "recompose": 1,
    "layout": 2
  }
}
```

v1 ルール:
- 整数値のみ（小数なし）。
- 時間は ms 単位の tick 集計値。
- `dirty` は発生順ではなく集合（重複なし）。

## API Sketch

```cpp
// Layer 1: スクショ + Layer 2: snap を組み合わせた例
TestFlow(testState)
  | Step(WAIT_WINDOW, WaitFor(WindowVisible("Main")))
  | Step(UPDATE_TEXT, SetState(textState, "long long long..."))
  | Step(WAIT_TICK,   WaitNextTick())
  | Step(SCREENSHOT,  CaptureScreen("after-wrap"))
  | Step(SNAP,        Snap("MainText")
                        .expect(FlushTimeMs(LessThan(16)))
                        .expect(RecomposeTimeMs(LessThan(8))))
  | onFailure(Dump(testState), ABORT);
```

Privileged probe sketch:

```cpp
if (probe.canInspectGlobalUi()) {
  CaptureGlobal().expect(MenuItemExists("File/Quit"));
} else {
  Skip("global ui probe unavailable");
}
```

## Assertion Extensions (Quality / Performance)

### A. ViewDirtyFlags Assertion

見た目一致に加えて内部 dirty も検証対象にする。

```cpp
CaptureScene()
  .expect(WasDirty(DIRTY_LAYOUT))
  .expect(NotDirty(DIRTY_CHILD));
```

目的:
- 見た目は同じでも毎回 `DIRTY_CHILD` (全再構築) が走る退行を検知する。

### B. Static / Dynamic Policy Assertion

- STATIC: `SetState()` の直後に反映されることを検証。
- DYNAMIC: `WaitNextTick()` 前は未反映、後に反映されることを検証。

```cpp
Step(SET_STATIC,  SetState(staticState, 1))
  | Step(ASSERT_IMMEDIATE, CaptureScene().expect(ValueEquals("StaticText", "1")))
  | Step(SET_DYNAMIC, SetState(dynamicState, 1))
  | Step(ASSERT_NOT_YET, CaptureScene().expect(ValueEquals("DynamicText", "0")))
  | Step(WAIT_TICK, WaitNextTick())
  | Step(ASSERT_AFTER_TICK, CaptureScene().expect(ValueEquals("DynamicText", "1")));
```

### C. Dump Dirty Reason Trace

失敗時 `Dump()` に以下を含める:
- dirty 発生 Boundary
- dirty flags
- trigger になった State ID / Name
- tick ID

これにより「なぜ汚れたか」を追跡可能にする。

### D. Timing Regression Assertion

- 見た目/値だけでなく実行時間も回帰検知対象にする。
- 例: `FlushTimeMs`, `RecomposeTimeMs`, `LayoutTimeMs` を `.snap` に記録し、CI で前回との差分を判定する。
- 判定時は OS ぶれ対策として許容幅を持つ（例: `<= baseline + 2ms`）。
- 連続 N 回中 M 回超過で FAIL とする運用を推奨（例: 5 回中 3 回超過で FAIL）。

## Capture Output Policy

```
captures/
  golden/          ← 承認済みシーケンス・snap（CI artifact または git 管理）
  current/         ← 今回の run 出力
  diff/            ← 差分出力（外部ツールが生成）
```

- `current/` の per-run ディレクトリ名: `LokaTest-<locale>-<timestamp>/`
- File-safe timestamp format only (no `:` or `/`)
- Toolbox/retro targets should enforce limits:
  - max files
  - max total bytes
  - delete oldest first when limit is exceeded
- 68k/Retro68 では `CaptureScene` の既定をテキスト snap (Node tree + bounds) にする。
- bitmap screenshot は明示 opt-in のみ（メモリ/ディスク制約対策）。

## Config Hook (`LokaTest.cfg`)

When present in the executable directory, load optional test settings:

- `capture_dir`
- `max_files`
- `max_total_bytes`

CI では `capture_dir` に workspace パスを明示的に渡し、artifact として収集する。設定がない場合はデフォルト値で動作する。

## CI Integration

- CI は主実行環境。PR ごとに `current/` と `golden/` を比較して自動判定。
- PASS / FAIL / SKIP を明示的に分離して報告する。
- 有料ビジュアルリグレッションサービス不要。Loka がデータを出力し、外部 CLI / AI が判定する。
- 時間系メトリクスも `.snap` diff に含め、性能退行を PR で自動検出する。
- Runtime verification for text relayout should include:
  - short -> long text
  - long -> short text
  - resize + text update

### CI Failure Policy (v1)

- `FAIL`:
  - bounds/value の差分が許容外
  - dirty flags が期待に反する
  - timing が許容幅を継続超過
- `SKIP`:
  - 権限不足（global probe）
  - platform capability 不足
- `PASS`:
  - 上記に該当せず、比較対象との差分が許容内

## Alignment with Existing Design

### Flow Primitives Reused Directly

- `Step` for sequential actions
- `goto` / `loop` for retries and iterative scenarios
- Timeout and error handling built into the flow model
- No new control-flow concepts needed

### Dump() for Failure Diagnostics

- Serializes which Step was reached and the value of each State at the point of failure
- Flow state is already structured data, so extra instrumentation is minimal
- Reports can be assembled from a sequence of `Capture()` + `Dump()` results

### 68k Self-Testing

- Typical GUI test automation frameworks assume modern environments
- Flow-based testing runs on the same runtime as the app itself, enabling self-tests on 68k/Classic
- `.snap` format is plain text, readable by any external tool regardless of the host environment

## Status

Conceptual. Prepare now, implement in phases after core behavior stabilizes.
