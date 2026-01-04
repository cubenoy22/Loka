# Context & Boundary 設計

## 現状（実装済み）

### Boundary アーキテクチャ
- **BoundaryNode**: 状態境界。ローカルの `PushStateTracker` を所有し、`useState` で状態を登録。
- **GroupNode**: 非Boundary。状態は親Boundaryに委譲（`stateOwner` 経由）。
- **Scene**: ルートが非Boundaryなら `RootBoundaryWrapper` で自動ラップ。

### DSL構造
```cpp
// Boundary定義
class MyNode : public BoundaryNodeFor<MyNode> { ... };
inline BoundaryDefinition<...> MyBoundary() { return Boundary<MyNode>(); }

// Group定義（非Boundary）
class MyNode : public GroupNodeBase<MyProps> { ... };
inline NodeDefinition<...> MyGroup() { return NodeDefinition<...>(); }
```

### 状態管理
```cpp
void composeNode(NodeComposition& c) {
  // useState - 最寄りのBoundaryに登録
  auto& count = c.useState<int>(0);

  // findBoundary - 型付き親Boundary検索
  auto* ctx = c.findBoundary<HelloWorldBoundary>();
  if (ctx) ctx->messageState().set("...");
}
```

### Context API → 廃止済み
- `ContextDefinition` / `ComponentContext::provide/require` は削除。
- 共有状態は `findBoundary<T>()` で親Boundaryのインターフェースを検索。

## 設計原則

1. **登場人物を最小に**: `useState` + `findBoundary` だけ
2. **Boundary = 状態境界**: Solid.js的/React的どちらのReconciler戦略も収容可能
3. **コンパイル時エラー優先**: テンプレート/継承で誤用を静的に防ぐ
4. **1MBで動く設計**: Classic Mac OS でも余裕で動くシンプルさ

## TODO
このドキュメントの TODO は `docs/TODO.md` に集約済み。

## Notes
- findBoundary は compose の親子順に依存（親が先に compose される前提）。
- BoundaryNode は将来的に Reconciler 戦略（Solid/React型）を差し替え可能な設計。

---

## 将来検討: DSLショートハンド

現状のDSLを壊さず、追加オーバーロードで対応可能。コミュニティの反応を見て優先度を決める。

### 1. Props省略ショートハンド
```cpp
// 現状
Text(TextProps().setText("Hello"))

// 追加案
Text("Hello")
```

### 2. State Props 直接渡し
```cpp
// 現状
EditText(EditTextProps().setText(props.input))

// 追加案
EditText(props.input)  // State<string>* を直接
```

### 3. prepareNode/composeNode 統一
```cpp
// 現状: 2つのフック
virtual void prepareNode(NodeComposition& c);
virtual void composeNode(NodeComposition& c);

// 検討: 1つに統一（useState を composeNode 内で呼べる）
```

### 4. 名前空間エイリアス
```cpp
namespace loka = declara::core::scene;
class MyNode : public loka::BoundaryNodeFor<MyNode>
```

### 5. Fragment マクロ/ヘルパー
```cpp
// 現状
VStack layout;
layout << Text(...);
return c.group(layout);

// 検討: ワンライナー化
```
