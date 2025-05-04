# Declara! State/DerivedProp/Tracker 設計ガイド

## 概要

Declara! の UI フレームワークは、現代的な宣言的・リアクティブ UI 設計を C++で実現するための骨格を持っています。本ドキュメントでは、State・DerivedProp・Tracker の役割と関係性、他フレームワーク（SwiftUI/Jetpack Compose/React/Svelte/SolidJS/Angular 等）との比較、設計思想を簡潔にまとめます。

---

## 1. 主要コンポーネントの役割

| コンポーネント | 役割・特徴                                                    |
| -------------- | ------------------------------------------------------------- |
| State<T>       | 値を保持するだけのシンプルな箱。getter/setter のみ。          |
| DerivedProp    | State や他 Prop に依存し、値を自動導出・再計算する箱。        |
| Tracker        | 依存伝播・副作用管理の司令塔。Page ごとに独立して持つ         |
| Page           | State/DerivedProp/Tracker をすべて内包し、UI ロジックを一元化 |

---

## 2. 関係図

```
┌────────────┐
│   Page     │  ... 画面単位の状態・ロジックの“入れ物”
│            │
│  ┌──────┐  │
│  │State │  │
│  └──────┘  │
│  ┌───────────────┐
│  │ DerivedProp   │
│  └───────────────┘
│  ┌───────────┐
│  │ Tracker   │
│  └───────────┘
│        ...
└────────────┘
```

- **Page** … State, DerivedProp, Tracker すべてを内包する
- **Tracker** … Page ごとに独立して持つ（Page のメンバー）
- **State/DerivedProp** … Tracker を使って依存伝播・再計算

---

## 3. 依存伝播・変更検知のフローチャート

```
[ユーザー操作/ロジック]
        ↓
   State::set()
        ↓
   Tracker（依存グラフ管理）
        ↓
   ┌───────────────┐
   │ 依存先 DerivedProp  │
   └───────────────┘
        ↓
  DerivedProp::recompute()
        ↓
  BindableProp::bind() で登録されたコールバック発火
        ↓
  UI/副作用/他のロジックへ伝播
```

- State の set は UI やロジックから直接呼ばれる
- Tracker が依存グラフを管理し、State 変更時に依存先 DerivedProp を再計算
- DerivedProp は BindableProp の仕組みでコールバックを発火し、UI や副作用に伝播

---

## 4. 2 フェーズ実行（dryRun→commit）と副作用管理

Declara! の Tracker は「2 フェーズ実行」による安全な依存伝播・副作用管理を行います。

### フロー概要

```
tracker.begin()   // 伝播バッファを初期化
   ↓
State::set()      // 値を仮置き、TrackerがdirtyなPropを記録
   ↓
...（複数のsetや依存伝播が連鎖的に発生）
   ↓
tracker.end()     // dirtyなDerivedPropを再計算（dryRun→commit）
   ↓
  - dryRun: 依存グラフをたどり、値の安定点まで仮再計算
  - commit: 値を本反映、副作用（deferred）を一括発火
   ↓
副作用（UI更新・システムコール等）が安全に実行される
```

### 連鎖反応・無限ループ検知

- Tracker は dirty な Prop の再計算を「安定点（dirty=0）」まで繰り返す
- ループ回数に上限を設け、無限ループや循環依存を検知・打ち切り

### deferred による副作用管理

- Tracker::defer(fn) で副作用（UI 更新・システムコール等）を登録
- tracker.end() の commit フェーズで一括発火
- dryRun 中は副作用を発火しないため、安全な状態伝播が保証される

---

## 5. 他フレームワークとの比較

| 項目      | Declara!（C++）       | SwiftUI                   | Jetpack Compose       | Flutter                  |
| --------- | --------------------- | ------------------------- | --------------------- | ------------------------ |
| 状態管理  | State<T>              | @State, @ObservedObject   | mutableStateOf        | State<T>, ValueNotifier  |
| 派生値    | DerivedProp<T, S>     | @Binding, computed var    | derivedStateOf        | ValueListenableBuilder   |
| 伝播管理  | Tracker               | Combine/内部依存解決      | Snapshot/依存追跡     | InheritedWidget/依存追跡 |
| UI 再構築 | build(PageBuilder&)   | body: View                | Composable 関数       | Widget ツリー            |
| 所有権    | Page が全 Prop を所有 | View struct が状態を所有  | Composable が状態所有 | Widget/StatefulWidget    |
| 拡張性    | Tracker 差し替え可能  | Combine/独自 Publisher 等 | CompositionLocal 等   | Provider/InheritedModel  |
| 言語特性  | C++98/型安全/手動管理 | Swift/型安全/自動管理     | Kotlin/型安全/自動    | Dart/型安全/自動管理     |

---

### React/Svelte/SolidJS/Angular との比較

| 項目      | React (Hooks)                | Svelte         | SolidJS        | Angular (Signals) |
| --------- | ---------------------------- | -------------- | -------------- | ----------------- |
| 状態管理  | useState, useReducer         | $var, store    | createSignal   | Signal            |
| 派生値    | useMemo, derived state       | $: reactive    | createMemo     | computed          |
| 伝播管理  | React Fiber/依存追跡         | 自動依存追跡   | 自動依存追跡   | Signal Engine     |
| UI 再構築 | JSX/Virtual DOM              | テンプレート   | JSX-like       | テンプレート/Zone |
| 所有権    | 関数コンポーネントが状態所有 | コンポーネント | 関数/Signal    | Component         |
| 拡張性    | Context/カスタム Hook        | Store/Context  | Context        | DI/Zone.js        |
| 言語特性  | JS/型緩い/自動管理           | JS/型緩い/自動 | JS/型安全/自動 | TS/型安全/自動    |

---

### 【補足】Declara! の「手動管理」とは？

Declara!（C++設計）における「手動管理」とは、
依存伝播・型安全・ライフサイクル・副作用制御などを**明示的なコード・設計で制御**する方針を指します。

#### 主なポイント

- **依存伝播の明示登録**
  - DerivedProp や Tracker のコンストラクタで、依存元 State/Prop をリストで明示指定
  - Cocoa/KVO や Qt 方式に近い「依存元リスト＋通知型」を採用
  - 動的な依存追加・削除も明示的に管理
- **型安全性の担保**
  - テンプレートや明示的な型指定（State<int>など）で型安全を保証
  - 仮値（dryRun 値）も型安全に管理
- **ライフサイクル管理**
  - Page/State/Tracker の生成・破棄を明示的にコントロール
  - リスナーのクリーンアップやメモリリーク対策も手動で実装
- **副作用・伝播タイミングの制御**
  - Tracker::begin()/end()で伝播バッチング
  - Tracker::defer()で副作用（UI 更新等）を commit 時に一括発火
  - dryRun 中は副作用を発火せず、commit 境界でのみ実行
- **循環依存・無限ループ検知も明示的にガード**
  - 伝播回数上限や依存グラフの静的/動的検証を実装

##### 備考

- MobX や Jetpack Compose 等の「依存自動検出」方式も参考にしつつ、現状は**明示的な依存指定**を推奨
- RAII による自動トランザクション管理（AutoTransactionGuard）も導入済み

> 対して、SwiftUI/Jetpack Compose/SolidJS などは依存関係の自動検出や状態の自動破棄・再生成など、言語・ランタイムが多くの管理を自動化しています。

---

## 6. 設計思想とメリット

- **Page 単位で状態・依存伝播を一元管理**
- **Tracker 差し替えで伝播戦略や履歴管理も柔軟**
- **C++98 でも実現可能な最小構成**
- **副作用や伝播の粒度を明示的に制御できる**
- **宣言的 UI・依存伝播・テスト容易性・状態のライフサイクル一元化**

---

## 7. 参考：docs/transaction.md も参照

より詳細な設計意図や拡張方針は `docs/transaction.md` を参照してください。

---

2025-05-04 作成
