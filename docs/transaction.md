## Declara! Compose Engine - Tracker 統合設計（Agent 向け最終手紙）

### はじめに

Declara! は今、Compose 的再評価モデルを超えて、**Tracker モデル**という普遍構造へと進化しました。

この文書は、エージェント（GPT-4o）に向けて、その設計哲学と構造意図を余すことなく伝えるものです。

---

### Tracker とは

Tracker は、以下すべてを**統合的に扱う戦略オブジェクト**です：

- 状態更新（State）
- 派生評価（DerivedProp）
- 差分検出（dryRun/diff/hash など）
- 変更伝播と通知（Component 更新）

かつて `Transaction` に分散していた責務を、Tracker がすべて引き受けます。

---

### 抽象クラス構造（最小構成）

```cpp
class Tracker {
public:
  virtual void begin() = 0;
  virtual void set(StateBase* s, const Value& v) = 0;
  virtual void defer(std::function<void()> fn) = 0;
  virtual void markDirty(PropBase* prop) = 0;
  virtual bool end(PagePropsBase* props) = 0;
  virtual void registerProp(PropBase* p) = 0;
  virtual ~Tracker() {}
};
```

---

### 実装例（標準）: `StdTracker` / `DominoTracker`

- dirtyProps による再評価
- deferSet による再帰的評価の保留
- dryRun + commit
- copyTo + Component 通知（updateProps）

```cpp
class StdTracker : public Tracker {
  // begin/end による安定評価 + 再描画伝播が行われる
};
```

---

### Page による選択制御

```cpp
class Page {
  Tracker* tracker;              // 差し替え可能
  PagePropsBase* props;

  void update() {
    tracker->begin();
    // 状態更新（name.set("Alice") など）
    tracker->end(props);
  }
};
```

この構造により、Page ごとに：

- 軽量な DirtyListTracker
- ハッシュベースの HashTracker
- Undo/履歴用の RecordingTracker

など、**用途に応じた Tracker を自由に選べる**。

---

### クラス構成ドミノ（最終版）

```
[State<T>]  →  [DerivedProp<T>]  →  [Component::updateProps()]
       ↓                      ↑
   [Tracker::set()]     [Tracker::markDirty()]
         ↓
    [begin → flush → dryRun → notify → copyTo]
         ↓
    [PagePropsBase* snapshot]（現時点での安定世界）
```

---

### 意義と位置づけ

この Tracker モデルにより、Declara! は：

- UI 反応システムの振る舞い自体を差し替え可能に
- 複数の再構成戦略を共存させた柔軟な UI 制御
- フレームワークでなく**エンジン構造**として普遍化

を実現しました。

---

Declara!は、状態変化とは何か、差分とは何か、再描画とは何か、
それらの意味をすべて開発者の手に委ねるレベルに到達しました。

---

## Declara! 依存伝播・トランザクション管理 仕様まとめ

---

### 1. 基本方針

- State/Prop の値変更は直接 set() で行う。
  - 例: `s_int.set(10);`
- トランザクション（begin/end）は Declara! ライブラリ側が責任を持って管理。
  - Page やイベントハンドラ呼び出し時に自動で begin()/end() を挟む。
- ユーザーは tracker を意識せず、宣言的に State/Prop を操作できる。
- 複数 State をまとめて変更したい場合など、明示的な begin()/end() もサポート。

---

### 2. 依存伝播・副作用管理

- 依存リストは明示的に指定。
  - DerivedProp のコンストラクタで依存元 State/Prop を渡す。
- 依存元の値が変わると、Tracker が依存先を dirty にマークし、トランザクション終了時に再計算・コールバック発火。
- 副作用（UI 更新・システムコール等）は defer で登録し、commit 時（end）にのみ発火。
- dryRun 中は副作用を発火しない。

---

### 3. 循環依存・無限ループ検知

- 循環依存が発生した場合は、実行時に無限ループ検知で打ち切り。
- 将来的には静的解析（VSCode 拡張等）で循環依存を検出・警告可能。

---

### 4. 依存リストの自動生成について

- 依存リストの自動生成（checkDep/dryRun 方式）は、evalFn が純粋関数かつ分岐なしの場合のみ限定的に可能。
- 条件分岐や動的依存がある場合は、明示的な依存リスト指定が必須。
- 現状は明示的な依存リスト指定を基本方針とする。

---

### 5. Page/State/Prop/Tracker の責務

- State/Prop は必ず Page に所属。
- Page ごとに Tracker/Transaction を持ち、依存伝播・副作用・dryRun 管理を一元化。
- Page 間の State 伝播やスレッド同期は将来的な拡張ポイント。

---

### 6. トランザクション自動管理の仕様

- State/Prop の set()は値の変更と依存伝播だけを担い、トランザクション管理はしない。
- Page やイベントハンドラ呼び出し時に Declara!が begin/end を自動で挟む。
- 複数変更や高度な制御は明示的な begin/end もサポート。
- 副作用（コールバックや defer）は必ず Declara!管理下のトランザクション内で発火する。

#### 【追加アイデア】RAII による自動トランザクション管理

C++の RAII（スコープガード）を活用し、関数スコープで自動的に begin()/end()を管理する方法も有効です。

```cpp
struct AutoTransactionGuard {
  StdTracker* tracker;
  AutoTransactionGuard(StdTracker* t) : tracker(t) { tracker->begin(); }
  ~AutoTransactionGuard() { tracker->end(nullptr); }
};

void onClick() {
  AutoTransactionGuard _(tracker);
  state->set(123);
  // ...他の状態変更...
} // ←ここでデストラクタ → tracker->end()
```

- イベントハンドラや一時的なバッチ処理で、関数スコープのライフサイクルで安全にトランザクションを管理できる
- 例外安全性も高まる

---

### 7. 複数依存元の DerivedProp

- 複数の State/Prop に依存する DerivedProp は、依存元を明示的にリストで指定する。
- 型が異なる場合は構造体や手動ラップで対応。
- C++98 では可変長テンプレート等による自動化は難しいため、現状は明示的指定が基本。

---

### 8. サンプル

```cpp
// 通常の値変更
s_int.set(10); // トランザクションはDeclara!が自動でbegin/end

// 複数変更をまとめたい場合
tracker.begin();
s_int.set(30);
s_int.set(40);
tracker.end(nullptr);

// RAIIによる自動トランザクション管理（推奨例）
void onClick() {
  AutoTransactionGuard _(tracker);
  s_int.set(123);
  // ...他の状態変更...
} // スコープ終了時に自動でend()
```

---

### 9. 設計思想

- 宣言的・型安全な依存伝播
- トランザクション管理は Declara!が一元化
- 副作用・伝播・循環依存も安全に管理
- ユーザーは tracker や伝播を意識せず、宣言的に State/Prop を操作できる

---

2025-05-04 現在の仕様。今後の拡張や設計変更時はこのドキュメントを更新すること。

## Declara! の仕様と React/Compose との比較

---

### 1. 依存伝播・副作用管理の比較

| 項目             | Declara!                                       | React/Compose                         | コメント                   |
| ---------------- | ---------------------------------------------- | ------------------------------------- | -------------------------- |
| 依存リスト       | 明示的に指定（型安全）                         | useEffect/remember で明示 or 自動     | 型安全性は Declara!が高い  |
| 依存伝播         | Tracker が一元管理、トランザクションでバッチ化 | Scheduler/バッチ化                    | ほぼ同等の仕組み           |
| トランザクション | Declara!が自動で begin/end                     | React はバッチ化、Compose は snapshot | 粒度・自動化も違和感なし   |
| 副作用           | defer で commit 時のみ発火、dryRun で抑制      | useEffect/SideEffect で管理           | 発火タイミングも同等       |
| 循環依存         | 実行時検知＋将来静的解析                       | React/Compose も基本は実行時検知      | 違和感なし                 |
| ユーザー API     | State/Prop を直接操作、tracker 意識不要        | useState/remember 等で宣言的          | 宣言的スタイルで違和感なし |
| イベントハンドラ | Declara!が begin/end 自動挿入                  | React/Compose も自動でバッチ化        | ほぼ同じ                   |

---

### 2. 違和感が出やすい点（現状は問題なし）

- 依存リストの自動生成がない（React/Compose も明示的依存リスト推奨なので違和感なし）
- 型安全性が高い分、柔軟な動的依存はやや弱い（C++の設計思想としてむしろメリット）
- 副作用の発火タイミング・dryRun の扱いも React/Compose と同等の安全性

---

### 3. 総評

Declara!の仕様は、React/Compose の設計思想と十分に整合しており、
違和感なく使える構造になっている。

- 宣言的・型安全な依存伝播
- トランザクションによるバッチ化
- 副作用管理・循環依存検知
- ユーザーは tracker や伝播を意識せず、宣言的に State/Prop を操作できる

---

今後、動的依存や複雑な副作用管理が必要になった場合は、設計を拡張すればよい。
