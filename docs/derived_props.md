# DerivedState 設計と多値依存の整理（2025 年 5 月時点 最新仕様）

## 概要

本ドキュメントでは、C++98 環境における `DerivedState`（合成状態）の設計思想と、多値依存・合成パターンについて整理します。

---

## 1. DerivedState の役割と設計

- **DerivedState** は「複数の State/DerivedState に依存し、EvalFn で値を自動合成・再計算する」ためのクラスです。
- 依存元 State の値が変化したとき、Tracker により自動的に再計算されます。
- C++98 互換のため、EvalFn は関数ポインタまたはファンクタで柔軟に指定できます。

---

## 依存グラフと StateTracker の役割

Declara! の StateTracker は「依存伝播の管理・制御を担う抽象クラス」です。自動伝播や循環依存検出の有無・方式は実装（例：PushStateTracker）ごとに異なります。

- DerivedState の依存関係は明示的にリストで指定
- 依存元の値が変わると、StateTracker（例：PushStateTracker）が依存先を dirty にマークし、end() で再計算・コールバック発火
- 副作用（UI 更新・システムコール等）は defer で登録し、commit 時（end）にのみ発火
- 自動伝播や循環依存検出の有無・方式は StateTracker 実装ごとに異なる

---

## 2. 多値依存・合成パターン

### 構造体合成パターン

複数の State をまとめて構造体にし、EvalFn で合成することで、柔軟なバリデーションや派生値を実現できます。

```cpp
struct UserForm {
  std::string name;
  std::string email;
  int age;
  bool agree;
};

State<std::string> name("");
State<std::string> email("");
State<int> age(0);
State<bool> agree(false);

std::vector<StateBase*> deps = { &name, &email, &age, &agree };

DerivedState<bool> isValid(
  deps,
  [&]() {
    UserForm f = { name.get(), email.get(), age.get(), agree.get() };
    return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
  }
);
```

### 派生値の多段合成

DerivedState 同士を依存させて多段合成も可能です。

```cpp
DerivedState<int> sum(
  { &a, &b },
  [&]() { return a.get() + b.get(); }
);
DerivedState<int> doubleSum(
  { &sum },
  [&]() { return sum.get() * 2; }
);
```

---

## 3. 設計方針まとめ

- **明示的な依存リスト**で循環依存や伝播経路を管理しやすい
- **EvalFn で柔軟な合成ロジック**を記述可能
- **C++98 互換**：関数ポインタ・ファンクタで柔軟に対応
- **多段合成・構造体合成もサポート**

---

2025-05-05 改訂
