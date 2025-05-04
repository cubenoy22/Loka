# DerivedProp 設計と多値依存の整理

## 概要

本ドキュメントでは、C++98 環境における `DerivedProp`（導出プロパティ）の設計思想と、複雑なフォームや多値依存の扱いについて、古今東西のアプローチを踏まえて整理します。

---

## 1. DerivedProp の背景と課題

- **DerivedProp** は「ある State から導出される値」を自動的に管理・再計算する仕組み。
- 入力フォームが複雑化すると「DerivedProp2」「DerivedProp3」…のような多引数バージョンが必要になり、型パラメータやラップ関数が爆発しやすい。
- C++98 では可変長テンプレートが使えず、型安全な多引数 DerivedProp の一般化が難しい。

---

## 2. 古今東西の類似アプローチ

| 系譜      | 代表例              | 多値依存の扱い          | 備考                       |
| --------- | ------------------- | ----------------------- | -------------------------- |
| Cocoa/KVO | `NSPredicate`/KVO   | 複数プロパティを監視    | 依存元を配列で指定         |
| MobX      | `computed`          | 複数 observable を参照  | クロージャで依存を自動追跡 |
| Redux     | `reselect`/selector | 複数 state を引数で渡す | セレクタ関数で合成         |
| Svelte    | `$: derived = ...`  | 任意数の依存を参照      | 依存元をスコープで参照     |
| Qt        | `Q_PROPERTY`/signal | 複数プロパティを監視    | 明示的に connect           |

---

## 3. C++98 での現実的な選択肢

### 3.1 構造体(State/DerivedProp)パターン

- フォームの全入力値を 1 つの構造体で管理し、State/DerivedProp も 1 つに集約する方法。
- メリット: DerivedPropN の爆発を防げる。型安全・拡張性も維持。
- デメリット: 部分更新時に一手間必要（構造体のコピー・再構築）。

#### サンプル

```cpp
struct UserForm {
  std::string name;
  std::string email;
  int age;
  bool agree;
};

class UserFormPage : public Page {
public:
  UserFormPage()
    : Page(),
      formState(&transaction_, UserForm()),
      isValid(&transaction_, &formState, &UserFormPage::validate) {}

  static bool validate(const UserForm& f) {
    return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
  }

  void build(PageBuilder& b) {
    b.Text("名前:");
    // それぞれの入力で formState を部分更新するラッパーを用意
    // 例: b.TextInput([this](const std::string& v){ updateName(v); });
    b.Text("有効: " + std::string(isValid.get() ? "OK" : "NG"));
  }

  void updateName(const std::string& v) {
    UserForm f = formState.get();
    f.name = v;
    formState.set(f);
  }
  // ...email, age, agree も同様...

  State<UserForm> formState;
  DerivedProp<bool, UserForm> isValid;
};
```

---

### 3.2 ヘルパー関数・Tuple/Struct 合成

- 複数 State を `std::pair` や自作 Tuple、または構造体でまとめて 1 つの DerivedProp に渡す。
- 依存元 State が複数ある場合は、更新時にまとめて set する必要あり。

---

### 3.3 DerivedProp チェーン・合成関数

- 2 つの DerivedProp を合成して 3 つ目を作る（合成関数パターン）。
- 依存関係が複雑化しやすいが、柔軟性は高い。

---

### 3.4 Typelist ＋マクロによる DerivedPropN 自動量産案

- boost::mpl 風 Typelist を自作し、2 ～ 5 引数までマクロで自動展開する方式も検討例として存在する。
- 例：

  ```cpp
  #define DECLARA_DERIVED_PROP_ARITY(N, TEMPLATE_PARAMS, FUNC_PARAMS, ACCESS_PARAMS) \
  template <typename R, TEMPLATE_PARAMS> \
  class DerivedProp ## N : public BindableProp<R> { /* ... */ };

  DECLARA_DERIVED_PROP_ARITY(2, typename A, typename B, a_, b_)
  DECLARA_DERIVED_PROP_ARITY(3, typename A, typename B, typename C, a_, b_, c_)
  // ...
  ```

- 良い点：
  - 書くのは一度きりで済み、以降は `DerivedProp3<...>` などで呼ぶだけ。
  - Transaction との親和性も維持できる。
- 弱い点：
  - マクロ＆メタプログラミングは可読性・保守性が犠牲になりやすい。
  - C++98 だと可変長テンプレートが無いため、5 個目くらいで打ち止めになる。

---

### 3.5 Cocoa/KVO・Qt 方式（依存元リスト＋通知型）

- Cocoa/KVO や Qt（Q_PROPERTY, signal/slot）方式を参考に、依存元（監視対象）を明示的なリストで指定し、各依存元の変更時に DerivedProp へ通知する設計も有力です。
- 典型的な実装例：
  - DerivedProp 生成時に依存元 State/Prop の配列やリストを渡す
  - 各 State/Prop が値変更時に observer（DerivedProp）へ通知
  - DerivedProp は通知を受けて再計算
- サンプル：
  ```cpp
  // 依存元 State を明示的にリストアップ
  DerivedProp<bool> isValid(
    &transaction_,
    std::vector<BindablePropBase*>{ &name, &email, &age, &agree },
    &UserFormPage::validate
  );
  // State/Propが変更時にnotifyObservers()を呼ぶ
  // DerivedPropはonDependencyChanged()で再計算
  ```
- メリット：
  - 依存元が明示的で追跡・管理しやすい
  - 動的な依存追加・削除も可能
  - 要素数が増えても確実に動作しやすい
- デメリット：
  - 依存元リストの管理がやや冗長
  - 依存追加・削除時のバグに注意
- C++98 でも実装容易で、シンプルかつ堅牢な方式として推奨候補となります。

---

## 4. 選択肢まとめ

| 方法                  | メリット       | デメリット               |
| --------------------- | -------------- | ------------------------ |
| 構造体(State/Derived) | 拡張性・型安全 | 部分更新がやや冗長       |
| DerivedPropN 量産     | 柔軟・直感的   | テンプレート爆発・冗長   |
| ヘルパー関数          | 多少簡潔       | 型安全性・可読性やや低下 |

---

## 5. 推奨方針

- **C++98 では「構造体(State/DerivedProp)パターン」が現実的かつ拡張性・型安全性のバランスが良い。**
- 部分更新のラッパー関数を用意することで、UI 側の記述もシンプルに保てる。
- さらに複雑な依存や合成が必要な場合は、ヘルパー関数や合成関数パターンを併用する。

---

## 6. 補足

- 可変長テンプレートが使える C++11 以降であれば、より柔軟な DerivedPropN の自動生成も可能。
- 本設計は「型安全」「依存関係の明示」「拡張性」を重視し、冗長なテンプレート爆発を避けることを優先しています。

### 6.1 依存スタック方式（自動依存追跡）について

- MobX や Jetpack Compose、SwiftUI、solid-js などのリアクティブフレームワークでは、
  Tracker（依存スタック）を用いて「どの State/Observable に依存しているか」を自動的に検出する方式が一般的です。
- これは「計算中の DerivedProp をグローバルなスタックに push し、State::get() が呼ばれるたびに依存登録する」仕組みです。
- C++ でも、State<T>::get() 内で Tracker::active() を参照し、依存登録を自動化する実装が可能です。
- メリット：依存元を明示せず、計算ロジックをそのまま書けるため、柔軟かつ直感的な設計が可能。
- デメリット：依存グラフの動的な組み替えや、実装の複雑化、型安全性の低下（特に C++98 では）などの課題があります。
- 本リポジトリでは「依存関係の明示性」と「型安全性」を重視し、現状は明示的な依存指定を推奨していますが、
  将来的な拡張や他言語フレームワークとの比較検討の際は、この方式も参考にしてください。

---
