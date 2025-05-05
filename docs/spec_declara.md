# Declara! UI スナップショットフレームワーク仕様（2025 年 5 月時点 最新仕様）

## 1. アーキテクチャ概要

- **宣言的 UI 設計**：Scene/Component/Renderer で構成。状態（State/MutableState/DerivedState）を中心に UI を再構築。
- **状態管理**：State<T>（不変）、MutableState<T>（set 可能）、DerivedState<T>（合成・再計算）
- **依存伝播**：Tracker が依存グラフ・伝播・副作用を一元管理
- **C++98 互換**：型安全・明示的依存管理・関数ポインタ/ファンクタで柔軟に対応

---

## 2. コアコンポーネント

### App

- UIContext と Scene\*を保持
- setScene(), rerender(), run() などで UI 全体を管理

### Scene / SceneBuilder

- Scene は build(SceneBuilder&)で UI レイアウトや世界の状態を宣言
- SceneBuilder は Component\*インスタンスを所有し、renderAll()で Renderer に送信

### Component

- virtual void render(Renderer\*)を持つ抽象基底クラス
- ButtonComponent などの派生クラスは状態を持ち、描画可能な UI 部品をカプセル化

---

## 3. State & DerivedState システム

### State<T>

- 値の保持のみ。不変。getter のみ

### MutableState<T>

- State<T>を継承。set()で値を変更可能

### DerivedState<T>

- 複数の State/DerivedState に依存し、EvalFn で値を自動合成・再計算
- 依存元の値が変化したとき Tracker により自動的に再計算

### Tracker

- 依存伝播の管理・制御を担う抽象クラス。自動伝播や循環依存検出の有無・方式は実装（例：StdTracker）ごとに異なる。
- dirty な DerivedState を追跡し、2 フェーズ（dryRun→commit）で安全に伝播・副作用管理（※この挙動は StdTracker 等の実装例）。
- Tracker::defer(fn)で副作用（UI 更新等）を commit 時に一括発火

---

## 4. サンプルコード

```cpp
class FormScene : public Scene {
  MutableState<std::string> name;
  DerivedState<bool> isEnabled;
  Tracker tracker;
  // ...
  FormScene() :
    name(""),
    isEnabled({ &name }, [&]() { return name.get().length() >= 3; }),
    tracker({ &name }, { &isEnabled }) {}
  void build(SceneBuilder& b) {
    b.TextInput(&name);
    b.Button(ButtonOptions().setLabel("送信").setEnabled(&isEnabled));
  }
};
```

---

## 5. 設計思想まとめ

- **Scene 単位で状態・依存伝播・副作用を一元管理**
- **Tracker 差し替えで伝播戦略や履歴管理も柔軟**
- **C++98 でも実現可能な最小構成**
- **副作用や伝播の粒度を明示的に制御できる**
- **宣言的 UI・依存伝播・テスト容易性・状態のライフサイクル一元化**

---

2025-05-05 改訂
