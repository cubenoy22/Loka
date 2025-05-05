# Declara! 状態管理・依存伝播・Tracker 設計（2025 年 5 月時点 最新仕様）

## 概要

Declara! の UI フレームワークは、C++98 互換・型安全・明示的依存管理を重視した「宣言的・リアクティブ UI 設計」を実現します。

本ドキュメントでは、State/MutableState/DerivedState/Tracker の役割・関係性・設計思想を最新仕様に基づき整理します。

---

## 1. 主要コンポーネントの役割

| コンポーネント  | 役割・特徴                                                              |
| --------------- | ----------------------------------------------------------------------- |
| State<T>        | 値を保持するシンプルな箱。getter のみ。外部から値変更不可（不変）。     |
| MutableState<T> | State<T>を継承。set()で値を変更可能。UI やロジックから直接更新できる。  |
| DerivedState<T> | 複数の State/DerivedState に依存し、EvalFn で値を自動合成・再計算する。 |
| Tracker         | 依存グラフ・伝播・副作用管理の司令塔。Scene ごとに独立して持つ。        |
| Scene           | State/DerivedState/Tracker を内包し、UI ロジックを一元化。              |

---

## 2. 関係図

```
┌────────────┐
│   Scene     │ ... 画面単位の状態・ロジックの“入れ物”
│            │
│  ┌──────┐ │
│  │State │ │
│  └──────┘ │
│  ┌───────────────┐
│  │ DerivedState │
│  └───────────────┘
│  ┌────────┐
│  │Tracker │
│  └────────┘
│        ...
└────────────┘
```

- **Scene** … State, MutableState, DerivedState, Tracker すべてを内包
- **Tracker** … Scene ごとに独立して持つ（Scene のメンバー）
- **State/DerivedState** … Tracker を使って依存伝播・再計算

---

## 3. 依存伝播・変更検知のフローチャート

```
[ユーザー操作/ロジック]
      ↓
MutableState::set()
      ↓
Tracker（依存グラフ管理）
      ↓
┌───────────────┐
│ 依存先 DerivedState │
└───────────────┘
      ↓
DerivedState::recompute()
      ↓
StateBase::bind() で登録されたコールバック発火
      ↓
UI/副作用/他ロジックへ伝播
```

- MutableState の set() は UI やロジックから直接呼ばれる
- Tracker が依存グラフを管理し、MutableState 変更時に依存先 DerivedState を再計算
- DerivedState は StateBase::bind() で登録されたコールバックを発火し、UI や副作用に伝播

---

## 4. 2 フェーズ実行（dryRun→commit）と副作用管理

Declara! の Tracker は「2 フェーズ実行」による安全な依存伝播・副作用管理を行います。

### フロー概要

```
tracker.begin()   // 伝播バッファを初期化
   ↓
MutableState::set()      // 値を仮置きし、TrackerがdirtyなDerivedStateを記録
   ↓
...（複数のsetや依存伝播が連鎖的に発生）
   ↓
tracker.end()     // dirtyなDerivedStateを再計算（dryRun→commit）
   ↓
  - dryRun: 依存グラフをたどり、値の安定点まで仮再計算
  - commit: 値を本反映、副作用（deferred）を一括発火
   ↓
副作用（UI更新・システムコール等）が安全に実行される
```

### 連鎖反応・無限ループ検知

- Tracker は dirty な DerivedState の再計算を「安定点（dirty=0）」まで繰り返す
- ループ回数に上限を設け、無限ループや循環依存を検知・打ち切り

### deferred による副作用管理

- Tracker::defer(fn) で副作用（UI 更新・システムコール等）を登録
- tracker.end() の commit フェーズで一括発火
- dryRun 中は副作用を発火せず、安全な状態伝播を保証

---

## 5. 設計思想とメリット

- **Scene 単位で状態・依存伝播を一元管理**
- **Tracker 差し替えで伝播戦略や履歴管理も柔軟**
- **C++98 でも実現可能な最小構成**
- **副作用や伝播の粒度を明示的に制御できる**
- **宣言的 UI・依存伝播・テスト容易性・状態のライフサイクル一元化**

---

## 6. 他フレームワークとの比較

| 項目      | Declara!（C++）         | SwiftUI/Compose/React 等 |
| --------- | ----------------------- | ------------------------ |
| 状態管理  | State<T>/MutableState   | @State, useState 等      |
| 派生値    | DerivedState<T>         | computed, derived 等     |
| 伝播管理  | Tracker                 | Combine, Signal 等       |
| UI 再構築 | build(SceneBuilder&)    | body, Composable 等      |
| 所有権    | Scene が全 State を所有 | View/Component 等        |
| 拡張性    | Tracker 差し替え可能    | Context, Provider 等     |
| 言語特性  | C++98/型安全/手動管理   | Swift/Kotlin/JS 等       |

---

## 7. 設計意図まとめ

- **Scene 単位で状態・依存伝播・副作用を一元管理**
- **Tracker 差し替えで伝播戦略や履歴管理も柔軟**
- **C++98 でも実現可能な最小構成**
- **副作用や伝播の粒度を明示的に制御できる**
- **宣言的 UI・依存伝播・テスト容易性・状態のライフサイクル一元化**

---

2025-05-05 改訂
