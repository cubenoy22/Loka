# C++98 UI Component Tree System — Tree/Scene/Component Architecture

## 概要

本ドキュメントは、C++98 システムの仕様をまとめる。静的・動的 UI 双方に対応し、効率的な差分更新（diff/reconciliation）、堅牢なメモリ管理（NodePool/GC 抽象）を備える。State/Tracker 機構との統合も前提とする。

---

## 1. 設計方針

- **型安全・効率重視**: 構造体ベースの props で静的型安全性と効率を両立。
- **クラスベースのコンポーネント**: ロジックやライフサイクル管理はクラスで実装。
- **動的ツリー構造**: ポインタベースのノードツリーで動的な差分・再構築・再利用を実現。
- **静的/動的 UI 両対応**: Solid.js 的な静的最適化も、React 的な動的再構築もサポート。
- **メモリ管理抽象**: NodePool/GC 抽象で即時 delete/プール/GC など柔軟に切替可能。
- **State/Tracker 連携**: 状態管理・依存伝播と自然に統合。
- **プラットフォーム適応**: PlatformContext から NodePool/GC を所有し、各 OS/実装に最適化可能。

---

### 構造の基本責務（まとめ）

| コンポーネント     | 主な責務・役割                                                                  |
| ------------------ | ------------------------------------------------------------------------------- |
| TreeSceneComponent | Scene 構築の宣言・再構成のコア。NodePool/GC を所有。                            |
| SceneContext       | Scene 固有リソース・タイミング・描画 API 橋渡し。アニメ管理・NodeContext 取得。 |
| PlatformContext    | OS 依存の描画/タイミング/アニメ/スレッド・非同期リソース制御を統括。            |
| SceneNodeContext   | SceneNode の個別インスタンス状態（hover, anim 等）管理。最小メモリ消費両立。    |
| Window             | ユーザー最上位 UI 単位。SceneManager を保持し、Scene 切替・解放・遷移を統括。   |

> **注:** SceneContext と PlatformContext の詳細な設計思想、拡張性、プラットフォーム実装例については、[scene_and_platform_context.md](./scene_and_platform_context.md) も参照してください。

---

## 2. 主な構成要素

### 2.1 Props（構造体）

- UI ノードの属性・状態を struct で表現。
- 静的型安全・値セマンティクス。
- 例: `struct ButtonProps { std::string label; bool enabled; }`

### 2.2 Component（クラス）

- UI ロジック・イベント・ライフサイクルを担当。
- `ComponentBase` 継承、props を受け取る。
- 例: `class ButtonComponent : public ComponentBase { ... }`

### 2.3 Node（ツリー構造）

- ポインタベースの動的ノード（例: `SceneNode*`）。
- 子ノードを動的に add/remove 可能。
- 差分更新・再利用・再構築に最適。

### 2.4 NodePool / GC 抽象

- ノードの生成・破棄を一元管理。
- デフォルトは即時 delete、必要に応じてプール/GC 実装に差替え可。
- PlatformContext が所有し、各プラットフォームで最適化。

### 2.5 State/Tracker 連携

- `MutableState<T>`, `DerivedState<T>`, `PushStateTracker` などと自然に統合。
- UI の状態変化・依存伝播を効率的に管理。

### 2.6 型安全性の担保について

- 各コンポーネントの props（属性）は struct で定義し、静的型安全性を確保。
  - 例: `struct ButtonProps { std::string label; bool enabled; }`
- Component クラスは明示的に props 型を受け取るコンストラクタやメソッドを持つことで、型不一致をコンパイル時に検出。
  - 例: `ButtonComponent(const ButtonProps& props);`
- ツリー構築時も `addChild(new ButtonComponent(ButtonProps{...}))` のように型安全に記述可能。
- State/Tracker 連携も `MutableState<T>` などテンプレート型で型安全に管理。
- C++98 の struct/class/テンプレートの型システムを活用し、JavaScript/TypeScript ベースの UI フレームワークよりも props 型不一致等のバグを未然に防ぎやすい。

### 2.7 設計理由・C++98 での最適化ポイント

- 本設計は「C++98 で実現可能な範囲での React/Solid.js 的 UI ツリー」として、現代的な型安全・効率・動的性を最大限に追求。
- ポインタをノードの「ハッシュ（ID）」的に扱うことで、C++98 でもノード識別・再利用・差分更新が容易。
- C++11 以降の move semantics やスマートポインタが使えない制約下でも、明示的な所有権管理・delete・RAII で安全性を補完。
- NodePool/GC 抽象により、delete/プール/GC など運用に応じたメモリ管理方式を柔軟に選択可能。
- struct props ＋明示的なクラス設計で型安全を最大限担保し、props 型不一致等のバグを未然に防止。
- テンプレート型 State/Tracker 連携で状態管理も型安全・効率的。
- Solid.js 的な静的最適化と React 的な動的再構築の両立を目指し、静的・動的 UI の最適化戦略を状況に応じて選択可能。
- 細かな API 改善や compose 構文の直感性向上などは今後も検討余地があるが、現状の骨格は C++98 でのベストプラクティスに非常に近い。

---

#### NodePool/GC/メモリ管理仕様拡張

- NodePool から返されたノードは **TreeSceneComponent が所有**
- 再評価後、参照が消えたノードは **GCHeap に渡す**
- **GCHeap** は一定フレーム後に `delete` するか、 **NodePool に rescue** する
- **NodeReuseHint** により GC 挙動を制御：
  - `Default` (Remember 相当)
  - `Stable`（差分チェック不要）
  - `Singleton`（解放しない）
- `reusePriority` や `lastUsedFrame` により GC ヒューリスティック調整可
- `dirty` フラグを持つことで、再構築せず再描画/再評価のトリガーが可能

---

#### SceneNodeContext によるインスタンス状態分離

- SceneNode と構造共有しつつ、状態（hover, anim など）を個別に管理
- `SceneNodeContextPool` で再利用される
- `TreeSceneComponent`/`SceneContext`から `getNodeContext(SceneNode*)` のように取得
- `NodePresence` フラグ (`Attached`, `Detached`, `Purged`) によって、表示/非表示/解放の明示的制御が可能

---

#### アニメーション・状態管理の構造

- `FrameAnim`: SceneContext 管理時間に基づく単純な線形アニメーション
- `DeclaraAnimation`: より複雑な補間/再生制御に対応（未定義）
- `AnimGroupComponent`: groupId で SceneNode を束ね、SceneContext からネイティブコマンド呼び出し可能
- `PlatformContext::nativeAnimate()` によるネイティブアニメの呼び出しも SceneContext 経由で可能

---

## 3. 差分更新・再構築

- Solid.js 的な静的最適化（関数/struct で木を構築）も、React 的な動的再構築（ポインタで差分適用）も両立。
- `SceneBuilder`/`SolidTreeSceneComponent` などで compose 関数を記述。
- 再利用可能なサブツリー・ネストもサポート。

---

### 再利用と宣言の最適化

- Solid.js のような静的構造を `SolidTreeSceneComponent` でサポート
- モデル構造は Declara で宣言し、動的状態は `SceneNodeContext` で保持
- **全ての Node が Singleton である設計**も可能（完全再利用）
- 子ノード再帰最適化：ノードはすべてフラットに Pool で管理されており、親構造依存せず `isDirty()` のみで再構成可
- 再構成最小化：`isDirty`ノードを起点に親へ遡り、影響範囲（再評価スコープ）を特定して再 compose
- Tracker で State 依存を観察しつつ、意味のある変化が起きたときだけ `isDirty` をマークする 2 段階制御が可能

---

## 4. メモリ管理・所有権

- ノードの生成/破棄は NodePool/GC 経由で行う。
- デフォルトは即時 delete、将来的にプール/GC 方式も選択可。
- PlatformContext が NodePool/GC を所有し、プラットフォームごとに最適化。

---

## 5. サンプル: Compose API

```cpp
// SolidTree 形式の compose 例
static void ComposeRoot(SolidTreeSceneComponent &node) {
  node.addChild(new TextComponent("名前を入力してください"));
  node.addChild(new ButtonComponent("送信", 0, &FormScene::onSendClick));
  node.addChild(new SolidTreeSceneComponent(&FormScene::ComposeSubTree));
}
static void ComposeSubTree(SolidTreeSceneComponent &node) {
  node.addChild(new TextComponent("(サブツリー: 補足説明)"));
}
void compose(SceneBuilder &builder) {
  builder.SolidTree(&FormScene::ComposeRoot);
}
```

---

## 6. State/Tracker との統合例

- `MutableState<T>::set()` は必ず `StateTracker::begin()/end()` でラップ。
- DerivedState で複数 State 依存も struct でまとめて記述可能。

---

## 7. PlatformContext との連携

- PlatformContext が NodePool/GC を所有。
- 各 OS/実装ごとに最適なメモリ管理・Component 実装を提供。
- Scene/Component から PlatformContext 経由でリソース取得。

---

### Scene 切替とメモリ解放の最適化

- `Window` が持つ `SceneManager` によって Scene の切替と旧 Scene の後処理が統括される
- `SceneDeallocator` を通じて旧 `SceneContext` をスケジュール解放：
  - `Immediate`: すべてを同期で解放（低スペック向け）
  - `Background`: フレーム毎に解放を分割（中性能機最適）
  - `Manual`: 明示的に `update()` で解放（開発・制御用）
- Scene 切替時の快適さと確実なメモリ解放を両立するため、動作モードは `SceneManager` で制御可能
- `SceneDeallocator` は `PlatformContext` によって提供され、スレッド・非同期制御・CPU 性能プロファイルに基づき解放スケジュールを動的制御する

---

### 所有権と責務の閉じ方（C++観点）

- Scene の Node/Context の全ライフサイクルが TreeSceneComponent/SceneContext に閉じる
- 明示的な `new/delete` や外部解放が不要、完全 RAII スタイル

---

### クラス・スタイルシステム構想

- `NativeClass` 構造体に `name`, `background`, `foreground`, `stable` 等を定義
- `styleClass: MyClass{}` のように宣言可能
- `stable=true` は変更がない前提で差分評価をスキップ
- Web 出力や AI 処理において意味のある識別子となる

---

### 型安全・RAII 設計の進化ポイント（fr1nx 氏との議論より）

#### SolidTree() の関数ポインタ型テンプレート化

- `SceneBuilder::SolidTree()` をテンプレート関数化し、`void (*ComposeFn)(SolidTreeSceneComponent&)` 型を明示することで、Compose 関数のシグネチャミス（引数違い等）をコンパイル時に検出できる。
- UI ツリー宣言の型安全性がさらに高まる。C++98 でも有効。
- 実装例：

```cpp
template<void (*ComposeFn)(SolidTreeSceneComponent&)>
void SolidTree() {
    SolidTreeSceneComponent* node = new SolidTreeSceneComponent(ComposeFn);
    addNode(node); // 仮想的に追加
}
```

- 関数ポインタ型の制約で、ラムダやメンバ関数ポインタは直接渡せないが、必要ならラッパーで対応可能。

#### StateTracker の RAII 化（ScopedTracker）

- `StateTracker::begin()/end()` を RAII で自動化することで、例外や早期 return 時の事故を防げる。
- C++らしい安全なスコープ管理ができ、コードの意図も明確になる。
- 実装例：

```cpp
struct ScopedTracker {
    ScopedTracker(StateTracker& tracker) { tracker.begin(); }
    ~ScopedTracker() { tracker.end(); }
};

// 使い方
{
    ScopedTracker track(tracker);
    myState.set(newValue);
}
```

- 既存の「set()は必ず begin()/end()でラップ」ルールとも矛盾しない。

---

これらのアプローチは Declara 設計思想（型安全・事故防止・明示的構造）と親和性が高く、今後の標準化・API 設計方針として十分に検討・導入の価値がある。

---

## 8. 拡張性・今後の展望

- 静的/動的 UI の最適化戦略を状況に応じて選択可能。
- メモリ管理方式（即時 delete/プール/GC）を用途・環境に応じて切替。
- State/Tracker/Component の連携強化、型安全性・効率性の更なる向上。

---

## 9. 参考: 既存 UI フレームワークとの比較

| 項目                 | 本設計               | React        | Solid.js       |
| -------------------- | -------------------- | ------------ | -------------- |
| 型安全性             | ◎（struct/props）    | △（JS/TS）   | ◎（TS/struct） |
| 差分更新             | ◎（動的/静的両対応） | ◎            | ◎              |
| メモリ管理           | ◎（抽象化可）        | △（GC 依存） | △（GC 依存）   |
| プラットフォーム適応 | ◎（Context/抽象）    | △            | △              |

---

## 10. 備考

- 本仕様は C++98/クロスプラットフォーム/高効率・型安全志向の UI システム設計のためのもの。
- 実装例・API スタブは必要に応じて別途追加。
- 詳細は core/Scene.hpp, core/SolidTreeSceneComponent.hpp なども参照。

---

## 11. 設計仕様アップデート・拡張案

### 次の実装候補リスト

1. SceneNodeContextPool 実装
2. AnimGroupComponent 実装と groupId 発火ルール
3. NodePool の GCHeap rescue 機構追加
4. styleClass tiveClass 構造体実装（stable 対応）
5. PlatformContext::nativeAnimate() のスケルトン化
6. TreeSceneComponent→SceneContext→PlatformContext 伝達パスの確立
7. SolidTreeSceneComponent の安定利用 API 整備
8. NodePresence (`Attached`, `Detached`, `Purged`) の統合ロジック
9. dirty フラグに基づく `update()` の最小呼び出し
10. Scene 構造の差分検知最適化（if/map による動的構造切替）
11. Tracker のバッチ構成／DebouncedTracker 設計と評価遅延最適化
12. SceneDeallocator の導入と SceneManager との統合（性能別動作モード対応）＋ PlatformContext ベースの非同期解放戦略
13. RemoteStateTracker 構想（ポスト Next.js における状態同期の鍵）

---

### RemoteStateTracker 概要

- 通常の `Tracker` の begin()/end() による依存構造記録を、**ネットワーク越しに拡張**
- **状態そのものの同期ではなく、“依存構造の共有”** によって push ベースの更新が可能
- クライアント側：依存だけ送る → サーバー：必要な State だけ監視 → 差分のみ通知
- Suspense / Streaming / ServerComponent 的最適化を 1 構造で内包可能
- **「Universal Reactivity Protocol (URP)」**として、宣言的リアクティブ構造の汎用通信モデルとなり得る

---

### その他：今後の抽象化/拡張候補（議論段階）

- **InputContext**：タップ・キー入力・フォーカスなどを抽象化するイベント管理レイヤー
- **Accessibility 拡張**：SceneNode にロールやラベルなどの読み上げ属性を持たせる
- **AssetContext**：フォント、画像、音声などの共通リソース管理層
- **Scene 外部探索 API**：`Window`または`SceneManager`から Node への逆探索パスの確立
- **SceneInspector**（開発用）や `StyleAudit` 等、内部構造の可視化フック
- **RenderTargetContext**：MV 生成やレイアウトキャプチャ向けの描画ターゲット切替管理
- **アニメ・音声の同期**：時間軸統合の補助、DeclaraAnimation + SoundTrigger など

---

### 補足：Declara! の姿勢

- Declara! の構造・最適化・拡張性は可能な限り最大化されている
- ただし、すべてを内包しようとはせず、**外側に広がる設計の自由も歓迎する**
- fr1nx の思想：「Declara! は大きなボウルのようなもの。収まるものは美しく収め、収まらないものは自由に外で咲いてほしい」
- そのため、特定の用途や他思想の否定はせず、“黙って見守る”拡張の余白も残されている

---

## 12. 本スレッドでの設計決定・補足ノート（2025-05-16）

### NodePool 設計・Node サブクラス化

- NodePool は`createNode()`/`acquire()`を virtual 化し、Node のサブクラス（poolInfo や追加メタ情報付き）を返せる設計とする。
- これにより、Node ごとに poolInfo や追加情報を持たせる拡張が容易。
- NodePool の型安全性・拡張性を最大化し、将来のキャッシュ・世代管理・デバッグ用途にも柔軟に対応可能。

### props の比較・差分検知

- NodeComponentProps は都度 struct で定義し、operator==をオーバーロードする。
- これにより、props の差分検知・キャッシュ再利用・shouldUpdate 判定が型安全かつ簡単に実現できる。

### NodePresence・ライフサイクルフック

- NodePresence（Attached/Detached/Purged）や created/destroyed/recycled 等の状態を State<T>で管理。
  - ※現状の実装では `SceneNode::Phase` で presence/lifecycle の両方をカバーしており、enum・State 管理の 2 軸で十分柔軟かつ拡張性もある。
  - 必要に応じて enum/State を拡張するだけで詳細な状態遷移や OS 固有管理にも対応可能。
- SceneNodeContext（type ごと singleton）に伝えることで、OS/バックエンド連携も容易。
- created: 新規生成, destroyed: 完全解放, recycled: プールから再利用、などの 3 値で十分柔軟。
- これを State<NodePresence>や State<NodeLifecycle>で管理し、必要に応じてコールバック/フックを発火。

### スレッドモデル・NodeGroupScope

- Declara!の UI/Scene 系は原則**単一スレッドで動作**。
- マルチスレッド化が必要な場合は、Context（SceneContext/PlatformContext）や専用 Tracker で外側から安全に制御。
- Scene 単位でスレッド分離も可能な設計。
- NodeGroupScope は RAII ＋ static でスコープネスト・所属先切替を安全に実現。
- DSL で「最寄りの NodeGroup に所属」も NodeGroupScope で実現可能。

### ルート一括管理方式の State 依存

- RAII ＋グローバル方式なら、ノードごとの State 依存登録/解除は自動化できる。
- Tracker のスコープ管理で十分、明示的な責務分担は不要。

### NodeComponent 拡張・型判定

- NodeComponent の型判定は typePtr/typeId 方式（static const void\*や int）で型一致を判定。
- dynamic_cast や typeid より高速・型安全。
- キャッシュ再利用時も型一致＋ props 一致で安全に再利用可能。

### その他

- デバッグ・可視化 API（dump/inspect 等）は将来追加で十分。
- マルチスレッド・非同期は今は考慮不要、必要になったら TreeSceneContext 等で拡張。

---

> 本設計ノートは Declara! の設計思想（型安全・効率・拡張性・RAII・明示的構造）に基づき、今後の実装・運用・拡張の指針となるようまとめたものです。

---
