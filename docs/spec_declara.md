# UI スナップショットフレームワーク仕様（ベクトル要約）

## 1. アーキテクチャ概要

- **スナップショットベースの UI フレームワーク**で、リアクティブなプロパティシステムを持つ。
- **C++98 互換**環境（例: MPW）をターゲット。
- **Jetpack Compose や SwiftUI に近い宣言的 UI 設計**を志向。
  - 状態（State/Prop）を中心に UI ツリーを再構築し、差分更新やスナップショット的な再描画を行う。
  - React の仮想 DOM よりも、Compose/SwiftUI の「状態 →UI 再構築」思想に近い。
- **差分アルゴリズムを使わず、静的な宣言的 UI を実現**
  - UI ツリー全体をスナップショット的に再構築し、軽量な再描画を実現。
  - 大きく UI 構造が変わる場合は Page 単位で切り替える設計とし、現役ライブラリ同等の機能をレガシー環境でも高速・軽量に提供。
- 参考技術スタック：
  - Jetpack Compose（Android）
  - SwiftUI（Apple）
  - Elm Architecture
  - Flutter
  - React（Fiber 以降）

## 2. コアコンポーネント

### App

- `UIContext`と`Page*`を保持。
- 主な責務：
  - `setPage()`：現在のページを切り替える。
  - `rerender()`：UI をクリアし、新しいコンポーネントツリーを構築・描画。
  - `run()`：イベントループに入る。

### Page / PageBuilder

- `Page`は`build(PageBuilder&)`でレイアウトを定義。
- `PageBuilder`は`Component*`インスタンスを所有。
- `renderAll()`で全コンポーネントを`Renderer`に送信。

### Component

- `virtual void render(Renderer*)`を持つ抽象基底クラス。
- 派生コンポーネント（例: `ButtonComponent`）は状態を持ち、描画可能な UI 部品をカプセル化。

## 3. State & Prop システム

### State<T>

- 可変値を保持。
- `set()`で依存する Prop を dirty にする。

### StaticProp<T>

- 定数値（例: `TRUE`, `FALSE`）。
- 無条件の有効化などに利用。

### DerivedProp<T, S>

- `State<S>`から導出され、`EvalFn`で計算。
- 値変化時に`bind()`コールバックを自動呼び出し。

### Transaction

- dirty な Prop を追跡し、再計算をトリガー。
- `commit()`で伝播を解決。
- **連鎖的な伝播が続いた場合でも、無限ループを検知して安全に停止**。
- **安定化（全ての Prop が確定）を待ってから自動反映するため、軽量かつ安定した UI 更新が可能**。

## 4. UI 構成

### Renderer

- プラットフォーム固有の描画インターフェース（ここでは未実装）。
- 例：
  - `createButton(label, enabled, onClick)`
  - `clearUI()`, `setButtonEnabled(...)`

### ButtonComponent（計画）

- label, PropBase\*（enabled）, onClick ハンドラを所有。
- `dynamic_cast`で有効状態を解決。

## 5. MPW 互換性に関する考慮

| 機能           | 状態       | 備考                                  |
| -------------- | ---------- | ------------------------------------- |
| `dynamic_cast` | 対応       | RTTI が必要                           |
| `std::vector`  | 慎重に利用 | メモリ使用に注意                      |
| `std::string`  | 慎重に利用 | 必要に応じて C 文字列にフォールバック |
| ラムダ式       | 非対応     | 関数ポインタで代替                    |

## 6. 今後の展望

- `TextComponent`, `TextInputComponent`の追加
- `bind()`→`rerender()`の仕組み実装
- `ComponentID`による差分更新の検討
- `Page::serialize()`によるシリアライズ対応
- レイアウトメタデータ（サイズ・アラインメント等）の統合

## 7. 例：コードフロー（要約）

```cpp
App app;
app.setRenderer(new MyRenderer());
app.setPage(new FormPage());
app.run();
```

---

## 補足: 抽象クラス構造と Prop バインド

### 設計思想の位置づけ

- 本フレームワークは、**Jetpack Compose や SwiftUI のような宣言的 UI 設計**を C++98 環境で再現することを目指している。
- 状態（State/Prop）を中心に UI ツリーを再構築し、差分更新やスナップショット的な再描画を行う。
- **差分アルゴリズムを使わず、静的な宣言的 UI を実現**し、Page 単位の切り替えで大規模な UI 変更にも対応。
- **Transaction による伝播は無限ループ検知・安定化待ちの仕組みで安全かつ軽量に実現**。
- React の仮想 DOM よりも、Compose/SwiftUI の「状態 →UI 再構築」思想に近い。
- 参考技術スタック：
  - Jetpack Compose（Android）
  - SwiftUI（Apple）
  - Elm Architecture
  - Flutter
  - React（Fiber 以降）

### Renderable / Page の抽象クラス

- `Renderable` … UI 部品の基底。`render(Renderer*)`, `updateProps()` を持つ。
- `Page` … 画面単位の基底。`build(PageBuilder&)`, `serialize()` を持つ。

### ButtonComponent の enabled バインド

- enabled 状態は`PropBase*`で外部バインド可能。
- `DerivedProp`を使うと、値変化時に`setEnabledProp()`で UI へ即時反映。
- バインドは`DerivedProp::bind()`経由で、PageBuilder の静的メソッドをコールバックとして利用。

### UIContext, Renderer 連携

- PageBuilder は UIContext, Renderer を保持し、UI 部品生成や状態反映を仲介。
- UI 操作は Renderer 経由で実行。

---

## 設計思想：状態（Prop）の所有権と UI 構造の一致

本フレームワークでは、Page（画面単位の UI クラス）が State/DerivedProp などの Prop（状態オブジェクト）をメンバ変数として直接所有する設計を採用しています。これは React の useState/useReducer に近い思想であり、以下の利点があります。

- **状態の所有権が UI 単位で明確になる**
  - 各 Page が自分の状態（State/DerivedProp）を直接持つことで、状態のライフサイクルと UI のライフサイクルが一致し、delete 責任や参照切れの心配がなくなる
- **UI 部品（Component）は Prop の参照のみを持つ**
  - UI 部品は状態の所有権を持たず、Page が管理する Prop を参照するだけなので、責務分離が明確
- **宣言的 UI 設計・再現性・テスト性が高まる**
  - 状態の初期化・管理・破棄が Page 単位で完結し、UI の再構築やテストが容易

この設計は C++98 でも安全かつシンプルに実現でき、現代的な宣言的 UI フレームワークの思想をレガシー環境でも再現可能にしています。

例：

```cpp
class FormPage : public Page {
  State<std::string> name;
  DerivedProp<bool, std::string> isEnabled;
  // ...
  void build(PageBuilder& b) {
    b.TextInput(&name);
    b.Button(ButtonOptions().setLabel("送信").setEnabled(&isEnabled));
  }
};
```

---

## 8. UI 再構築・差分更新に関する設計方針

本フレームワークの現状設計では、`App::rerender`は「`setPage()`で Page インスタンスが切り替わった時のみ」呼ばれます。
一度 Page がセットされて UI が構築された後は、状態（Prop/State/DerivedProp）の変化による UI の再構築・再レンダリングは行いません。
UI は静的なまま保持され、差分更新や動的な UI 再構築は今後の拡張テーマとしています。

この設計により、Page 単位での明快な責務分離と、シンプルな UI ライフサイクル管理を実現しています。
