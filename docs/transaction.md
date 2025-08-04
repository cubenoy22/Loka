# Declara! StateTracker 詳細仕様・実装リファレンス

このドキュメントは、Declara! の StateTracker（依存伝播・副作用管理）の詳細な仕様・API・実装例・拡張案をまとめた技術リファレンスです。
全体設計や思想・比較は docs/architecture_state_tracker.md を参照してください。

---

## 1. StateTracker 抽象クラス構造

Declara! の StateTracker は「依存伝播の管理・制御を担う抽象クラス」です。自動伝播や循環依存検出の有無・方式は実装（例：PushStateTracker）ごとに異なります。

```cpp
class StateTracker {
public:
  virtual void begin() = 0;
  virtual void set(StateBase* s, const Value& v) = 0;
  virtual void defer(std::function<void()> fn) = 0;
  virtual void markDirty(PropBase* prop) = 0;
  virtual bool end(ScenePropsBase* props) = 0;
  virtual void registerProp(PropBase* p) = 0;
  virtual ~StateTracker() {}
};
```

---

## 2. 標準実装例（PushStateTracker）

- 依存グラフを管理し、push 型自動伝播・循環依存検出・2 フェーズ副作用管理などを実装
- dirtyProps による再評価
- deferSet による再帰的評価の保留
- dryRun + commit
- copyTo + Component 通知（updateProps）

```cpp
class PushStateTracker : public StateTracker {
  // begin/end による安定評価 + 再描画伝播が行われる
};
```

---

## 3. 複数 StateTracker の選択・差し替え

- Scene ごとに異なる StateTracker（例: DirtyListStateTracker, HashStateTracker, RecordingStateTracker など）を選択可能
- 用途に応じて伝播戦略や履歴管理を柔軟に切り替えられる

---

## 4. 依存伝播・副作用管理の詳細

- 依存リストは明示的に指定（DerivedProp のコンストラクタで依存元 State/Prop を渡す）
- 依存元の値が変わると StateTracker が依存先を dirty にマークし、end() で再計算・コールバック発火
- 副作用（UI 更新・システムコール等）は defer で登録し、commit 時（end）にのみ発火
- dryRun 中は副作用を発火しない

---

## 5. RAII による自動トランザクション管理（C++例）

```cpp
#include "core/AutoTransactionGuard.hpp"
// 利用例:
// PushStateTracker tracker(...);
// {
//   AutoTransactionGuard _(tracker);
//   // 状態変更...
// } // スコープ終了時に自動でend()
```

---

## 6. サンプル

```cpp
// 通常の値変更
s_int.set(10); // トランザクションはDeclara!が自動でbegin/end

// 複数変更をまとめたい場合
tracker.begin();
s_int.set(30);
s_int.set(40);
tracker.end(nullptr);

// RAIIによる自動トランザクション管理
void onClick() {
  AutoTransactionGuard _(tracker);
  s_int.set(123);
  // ...他の状態変更...
} // スコープ終了時に自動でend()
```

---

## 7. 拡張・注意点

- 複数依存元の DerivedProp は依存元を明示的にリストで指定
- 循環依存は実行時検知（将来的に静的解析も検討）
- 動的依存や複雑な副作用管理が必要な場合は設計を拡張

---

> 全体設計・思想・他フレームワークとの比較は docs/architecture_state_tracker.md を参照してください。

## 8. 他フレームワークとの比較・Declara!の強み

- Declara!は Tracker（StateTracker, PushStateTracker 等）の設計・実装を自由に拡張できるため、ゲームやリアルタイム用途にも最適化可能です。
- Tracker ごとに依存グラフや伝播戦略、履歴管理、マルチスレッド化、独自アルゴリズム（差分伝播・バッチ更新・優先度制御など）を導入でき、OS やハードウェア特性に合わせた最適化も可能です。
- React や Solid.js も内部で複数の Tracker や Queue を使って効率化していますが、Declara!はさらに低レイヤーから設計できるため、ゲームやリアルタイム系、特殊な UI/UX にも柔軟に対応できる強みがあります。
- SwiftUI、Jetpack Compose、Flutter もリアクティブな UI 更新や一部差分描画は実装していますが、Tracker や依存グラフのカスタマイズ性、マルチスレッドや低レイヤー最適化、ゲーム用途への拡張性という点では Declara!の方が自由度・拡張性が高い設計です。

## Declara!の Tracker 設計思想は、他の UI フレームワークよりも一歩先を行く柔軟性・拡張性を持っています。

## 9. プラットフォーム連携・副作用設計

- Declara!では、OS やゲームエンジンなどプラットフォームとの連携も「副作用」の一つとして Tracker で管理できます。
- UI 更新だけでなく、システムコール・ネイティブ API・外部エンジンとのやりとりも defer で登録し、リアクティブに一元管理可能です。
- これにより、UI・ロジック・プラットフォーム連携を同じトランザクション/Tracker の枠組みで扱えるため、システム全体を非常にコンパクトかつ拡張性高く設計できます。
- ゲームやリアルタイム系でも、描画・サウンド・入力・ネットワーク等の副作用を Tracker で統合的に制御でき、最適化やカスタマイズも容易です。

Declara!の副作用設計は、プラットフォーム連携も含めてリアクティブに一元管理できる点が大きな特徴です。
