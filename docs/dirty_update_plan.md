# Dirty Update / Boundary Update Plan (Draft v5)

## Status

- Status: mixed historical plan + partially completed work
- This document is no longer a pure forward plan. Some items below are already implemented, some were superseded, and some remain future work.

Implemented or validated since this draft started:

- `StaticComposition` UPDATE propagation no longer blocks child updates
- Flow-based dirty routing regressions exist in `tests/FlowDslTests.cpp`
- mixed child-dirty scene regressions exist in `tests/Tests.cpp`
- `Scene` local-diff downgrade policy is now root-boundary-only
- `StaticVsDynamic` profiling work showed that root dynamic host boundaries materially reduce `fullRebuild -> full request` behavior in Classic

Still active / unresolved:

- startup full-draw noise in Classic Toolbox
- residual `updateEvt` / window-event redraw duplication
- broader scheduler unification and any `NextTickTracker`-style consolidation described below

Reading rule:

- treat this file as design context and a source of open questions
- verify current behavior against source/tests before implementing from this draft literally

## Goal

- macOS で `Text` の行数変化が反映されないケースを解消する
- Toolbox の再描画漏れを構造的に潰し、描画パスを一本化する
- `UpdateRgn`/invalidate の重複発行を抑え、次サイクル一括更新へ寄せる
- 各所の「その場しのぎ」な遅延更新ロジックを一掃し、設計を一本化する

## Core Design (2-layer Responsibility)

責務を2層に分け、「何が dirty か」と「いつ・どうまとめて実行するか」を分離する。
> 注: この2層は概念的な分離。実装では既存の Boundary クラス階層 (StaticComposition / DynamicComposition) と NextTickTracker に統合する。

### Layer 1: Node / NativeContext (What is dirty?)

- 責務: プロパティ変更を検知し、dirty の種別を判定して Boundary へ報告する
- 既存の `NodeDirtyFlags` をそのまま使用（新フラグ定義は不要）:
  - `NODE_DIRTY_PROPS`: 色・描画のみの変化
  - `NODE_DIRTY_LAYOUT`: サイズ・配置への影響あり
  - `NODE_DIRTY_CHILD`: 子ノード構造の変化
- **Node/NativeContext はタイマーや遅延処理を持たない**。dirty の種別判定に専念する。
- 各 Node は `declareObservedStates(...)` で **state -> dirty flags** を明示的に申告する。
  - 例: wrap あり `Text.text` は `NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT`
  - 例: `Button.enabled` は `NODE_DIRTY_PROPS`
  - 例: `Conditional.visible` は `NODE_DIRTY_CHILD`
- **Self-updating / output-only control state** は observed state に含めない。
  - 例: `EditText.text`, `PopupMenu.selectedIndex`, `OpenFileDialog.isVisible`, `OpenFileDialog.result`
  - これらは native control/context が user interaction や dialog completion で state を更新する。
  - 外部 state からの UI 反映は各 platform context の bind/apply が担当し、scene dirty source にはしない。

### Layer 2: Boundary + NextTickTracker (When & How to execute?)

この層が「実行タイミングの裁量」と「一括実行」の両方を担う。

#### Boundary (Policy)

既存の StaticComposition / DynamicComposition クラスがポリシーを体現する：

| 既存クラス | ポリシー | 動作 |
|---|---|---|
| `StaticCompositionBoundaryNodeBase` | STATIC | 即時反映。Boundary dirty は `Scene::invalidate(flags)` に流し、同一サイクルで flush/apply する。 |
| `DynamicCompositionBoundaryNodeBase` | DYNAMIC | 遅延反映。Boundary dirty は `Scene::requestInvalidate(flags)` に流し、後段の flush でまとめて apply する。 |

Boundary は Node が申告した observed states を所有し、**直近 commit で dirty になった state だけ**から dirty flags を合成する。
これは単純な boundary-wide union よりも正確で、同じ Boundary 内の unrelated state update で `NODE_DIRTY_LAYOUT` / `NODE_DIRTY_CHILD` が混入するのを防ぐ。

Scene は flush 時に `SceneCompositionDiff.fullRebuild` を確定し、platform controller へ明示的に渡す。
- `fullRebuild = true`: `NODE_DIRTY_CHILD` または `NODE_DIRTY_INITIAL`
- `fullRebuild = false`: `NODE_DIRTY_LAYOUT` / `NODE_DIRTY_PROPS` のみ

この値は platform 側で「native context を再生成してよいか」の契約として使う。
- `fullRebuild = true`: subtree/context recreate を許可
- `fullRebuild = false`: 既存 context を維持し、`relayout(...)` / props apply で済ませる

#### NextTickTracker (Batch Execution)

- 責務: `DYNAMIC` な Dirty を溜め、OS イベントループの隙間で一括実行する
- `RefreshLoop` は廃止し、Timer 用途も含めて `NextTickTracker` へ統合する。
- 実行順序: `RECOMPOSE (CHILD)` → `RELAYOUT (LAYOUT)` → `APPLY (PROPS)` → `INVALIDATE (Region Union)`
- **スコープ: Window 単位**（Loka はマルチウィンドウをサポートするため）。`App` が全 Window の NextTickTracker を一括 pump できるインターフェースを持つ。これにより OS のウィンドウ更新イベント（`WM_PAINT`、`NSView` 再描画）と同期しやすくなる。
- v1 スコープでは payload 拡張は行わず、遅延制御は `delayMs` のみを扱う（必要最小限）。

**STATIC 例外ルール（安全性）:**
- `STATIC` は即時反映を原則とする。
- ただし、同期実行で再入/破棄リスクがある更新は安全性優先で deferred を許可する。
  - 例: macOS の `MacTextContext` — text wrap relayout 中に同期 relayout を呼ぶと再入するため、NextTickTracker 経由の deferred に移行する（現行 `NSTimer(0)` の置換先）。
- deferred 例外は platform ごとに明示し、常用しない。

**delay 仕様:**
- `delayMs == 0`: 次の OS イベントループ tick での実行を**保証**する
- `delayMs > 0`: best-effort（プラットフォーム依存、例: 1000ms 指定でも OS の都合で前後する）

> 現状実装では `Scene::invalidate()` は同期 flush helper、`Scene::requestInvalidate()` は deferred helper であり、この差を Static / Dynamic policy が使い分ける。

## Cleanup & Consolidation (Removing Ad-hoc Logic)

### 1. StaticComposition の UPDATE 遮断バグ修正

- **問題**: `StaticCompositionBoundaryNodeBase::composeWithContext()` が `COMPOSE_EVENT_UPDATE` を無条件に捨てており、Static Boundary の下にある Dynamic Boundary に UPDATE が届かない。
- **修正**: UPDATE イベントを子ノードへ必ず伝播させる。Static Boundary 自身は recompose しないが、子への伝播は行う。

```
// 現状: UPDATE も DETACH 以外は全て return
if (event != COMPOSE_EVENT_ATTACH) { ...; return; }

// 修正後: UPDATE は子へ伝播、ATTACH 時のみ自身が compose
```

### 2. Scene scheduler の整理

- 理由: scene 側はすでに `NextTickTracker` を持つが、helper semantics を明確化する必要がある。
- 現状:
  - `Scene::invalidate()` は synchronous flush helper
  - `Scene::requestInvalidate()` は deferred helper
- 次段:
  - OS イベントループ統合後は DYNAMIC 側の flush 呼び出し元を platform scheduler へ寄せる
  - test helper (`FlushSceneInvalidation()`) は deterministic flush entry として維持する

### 3. 実行順序の厳格化

- `PushStateTracker` (Micro-tick: データ整合) → `NextTickTracker` (Main-tick: 描画反映) の順序を保証する。
- 同一 OS イベント内での重複描画を物理的に防ぐ。

## Design Discussion (Arena & Lifecycle)

`RECOMPOSE` 時のメモリとポインタの安全性についての検討が必要。

- **問題**: `DynamicComposition` で `RECOMPOSE` が走ると `nodeArena()->clear()` が呼ばれ、Node インスタンスが破棄される。
- **リスク**: プラットフォーム側の `NativeContext` が古い Node ポインタを保持している場合、`flush` 時にクラッシュする可能性がある。
- **採用方針**: 案 A — `RECOMPOSE` 前に必ず NativeContext を明示的に破棄（Detach）する。

## Platform Notes

### macOS

- `NSTimer(0)` による個別の遅延処理（`MacTextContext::requestRelayoutIfNeeded`）を全廃し、`NextTickTracker` に統合。
- `setNeedsDisplayInRect:` を発行する代わりに、`NextTickTracker` が算出した統合 Region を 1 回で通知。
- `NODE_DIRTY_LAYOUT` では native control/context を再生成せず、既存 context の `relayout(...)` を使う。

### Toolbox (Classic Mac)

- **Flush タイミング**: `WaitNextEvent` を呼ぶ**直前**に `NextTickTracker::flush()` を実行。
- これにより、ユーザー操作に対する体感レスポンスを最大化し、描画漏れをゼロにする。
- `STATIC` Policy を活用し、メニューのハイライトなど即時性が求められる UI の応答性を維持。

**既知の未解決課題（公開前に対処が必要）:**
- **ZStack hit test**: Toolbox は z-order を OS でサポートしないため、hit test をノードツリーの逆順走査で自前実装する必要がある。ZStack の入れ子が深い場合の正確性と性能が課題。
- **テキスト高さの 2 パス問題**: `wrap != none` の Text は、レイアウト時点では行数が確定しないため「レイアウト → 描画 → 高さ確定 → 再レイアウト要求」の 2 パスが必要。NextTickTracker の次 tick 遅延で再レイアウトを投げる経路が確立されると解決できる見込み（macOS と共通の仕組み）。

### Win32

- `InvalidateRect` の乱発による Flicker を抑制。
- `WM_SIZE` 等の OS 起点イベントも `NODE_DIRTY_LAYOUT` として Tracker 経由で処理。
- `NODE_DIRTY_LAYOUT` では native child/control を再生成せず、既存 context の `relayout(...)` を使う。

## Migration Steps

1. `StaticCompositionBoundaryNodeBase::composeWithContext()` の UPDATE 伝播修正（独立バグ修正、即着手可能）
2. `Boundary` (StaticComposition / DynamicComposition) に `NextTickTracker` との接続口を追加
3. `NativeContext` 各実装で、タイマー管理を `NextTickTracker::request()` 呼び出しに置換
4. macOS / Toolbox / Win32 のメインループへ `NextTickTracker::flush()` を組み込み
6. Flow API ベースの検証フェーズを追加し、`WaitNextTick()` を使って以下を自動検証
   - ViewDirtyFlags (`DIRTY_LAYOUT/PROPS/CHILD`) の発生妥当性
   - Static 即時 / Dynamic 次 tick のポリシー差
   - Dump への dirty reason (state trigger) 記録
   - flush/recompose/layout の tick 時間を `.snap` に記録し性能退行を検知
7. Profiling 出力を段階移行
   - `PROFILE_SECTION` / `PROFILE_FUNC` マクロは維持
   - 出力先を `profile.txt` 単体から Flow `.snap` 集約へ移行
   - 移行完了後に `profile.txt` 既定出力を廃止（必要時のみ opt-in）

### Profiling Migration Phases

1. **Phase A (Dual Output)**
   - 既存 `profile.txt` 出力を維持しつつ、同じ tick 計測を `.snap` にも書き出す。
   - 期間中は双方の値を比較し乖離がないことを確認。
2. **Phase B (Snap Primary)**
   - CI 判定は `.snap` のみを正とする。
   - `profile.txt` はローカルデバッグ用 opt-in に切り替える。
3. **Phase C (Legacy Off by Default)**
   - `profile.txt` の既定出力を停止。
   - 必要時は `LokaTest.cfg` かビルドフラグで有効化する。

## Acceptance Criteria

| 項目 | 期待動作 |
|---|---|
| STATIC Boundary 下の変化 | 同一サイクル内に反映される |
| DYNAMIC Boundary 下の変化 | `requestInvalidate()` 後の flush/tick で反映される |
| UpdateRgn / InvalidateRect | 同一 tick 内で 1 回に統合して発行される |
| macOS: Text wrap 行数変化 | テキスト変更後の次 tick で正しくリレイアウトされる |
| Toolbox: 再描画漏れ | `WaitNextEvent` 前の flush により漏れゼロ |
| 性能回帰検知 | `.snap` の時間指標差分で PR で検知できる |

## Test Plan

### 必須 100% カバレッジ経路

以下の経路はすべてのコードパスをテストで通過させること（行/分岐の % 目標は設けない）:

- **Merge 規則**: `NODE_DIRTY_CHILD | NODE_DIRTY_LAYOUT | NODE_DIRTY_PROPS` の OR 蓄積が正しく機能する
- **State-level routing**: 同じ Boundary に複数種別の observed state が同居していても、dirty になった state 由来の flags のみが通知される
- **Flush 順序**: RECOMPOSE → RELAYOUT → APPLY → INVALIDATE の順が崩れないこと
- **再入防止**: flush 中に新たな dirty が発生しても、現サイクルには混入せず次 tick に回ること
- **Rebuild contract**: `fullRebuild=false` の update では native context identity を維持し、`fullRebuild=true` のときだけ recreate を許可する

### 検証方法

- `TraceSink` を導入し、`markDirty → flushBegin → applyPaint` の順序を時系列で記録・検証。
- `flushNow()` を用いて、遅延反映が期待通りに（かつ 1 回に集約されて）実行されるかを単体テスト。
- macOS Text wrap と Toolbox redraw の再現ケースを回帰テストとして追加する。
- macOS / Win32 では `Button/EditText/PopupMenu/ImageView/Text/Cell` の relayout reuse を platform test で固定する。

### macOS Runtime Verification Checklist (Text wrap relayout)

`build-verified` だけでなく、以下は `runtime-verified` として記録する:

1. **準備**
   - `cmake -S . -B build/Testing -DTEST_BUILD=ON`
   - `cmake --build build/Testing`
   - `cmake --build --preset macos-debug`
2. **再現シナリオ**
   - 折り返し有効 (`wrap=word` か `wrap=char`) の `Text` を含む画面を表示する。
   - 初期表示時に 1 行に収まる短文を表示する。
   - 同一 `State<String>` を長文へ更新して、行数が増える入力を行う。
3. **期待結果**
   - 更新した tick では再入クラッシュしない（同期 relayout をしない）。
   - **次 tick で** Text の高さが再計算され、下位 UI の配置が崩れず追従する。
   - 同一イベント中に複数回更新しても、relayout は 1 回に集約される。
4. **回帰観点**
   - 長文 -> 短文（行数減少）でも高さが縮む。
   - Window resize 直後の text 更新でも反映漏れしない。
   - `wrap=none` の Text はこの deferred relayout 経路に入らない。

### Runtime Verification Record (macOS)

| Date | Commit | Environment | Scenario | Result | Notes |
|---|---|---|---|---|---|
| YYYY-MM-DD | `<hash>` | macOS / CPU arch | short->long text wrap | PASS/FAIL | |
| YYYY-MM-DD | `<hash>` | macOS / CPU arch | long->short text wrap | PASS/FAIL | |
| YYYY-MM-DD | `<hash>` | macOS / CPU arch | resize + text update | PASS/FAIL | |

記録ルール:
- `PASS` は再現シナリオの期待結果 3 項目を満たした場合のみ。
- `FAIL` の場合は最小再現手順とスクリーンショット/ログの保存先を Notes に残す。
