# Scene/PlatformContext 設計思想と多様な世界表現・最適化バックエンド対応

## 概要

- Scene/SceneBuilder は「アプリケーションの画面」だけでなく、ゲームや仮想空間など多様な「世界」や「状態」を抽象的に表現するための設計。
- PlatformContext は描画処理だけでなく、プラットフォーム固有のリソース管理・API 抽象化を担い、最適化されたバックエンド（例: OpenGL, Direct3D, Metal, ソフトウェアレンダラ等）への柔軟な対応を可能にする。

> **注:** Tree/Scene/Component アーキテクチャの詳細設計、NodePool、GC、差分更新などの内部実装機構については、[tree_scene_component.md](./tree_scene_component.md) も参照してください。

## 設計意図

- **抽象度の高さ**  
  Scene は「ページ」や「画面遷移」に限定せず、ゲームのレベル・仮想空間・状態遷移など、あらゆる「世界」を宣言的に構築できる。
- **バックエンド最適化**  
  PlatformContext を介して、各プラットフォームや描画 API の最適化（GPU/CPU/OS ごとの高速化）を実現。
  - 例: Windows では Direct3D、macOS では Metal、Web では WebGL など、最適なバックエンドを選択可能。
- **拡張性・再利用性**  
  UI/ゲーム/ツールなど多様な用途に対応できる柔軟な構造。
  - 新たなバックエンドやプラットフォーム追加も容易。

## 具体的な活用例

- 2D/3D ゲームの「シーン」管理
- アプリケーションの複数画面・状態遷移
- シミュレーションやエディタ等の複雑な世界表現

## SceneContext について

`SceneContext` は、各 `Scene` に紐づく「プラットフォームごとのリソース・ライフサイクル管理」を担う抽象クラスです。

### 主な役割

- **シーン単位のリソース管理**: 各 `Scene` の背後で必要となる、プラットフォーム依存のリソース（例: Win32 ウィンドウハンドル、描画コンテキスト、入力管理など）をカプセル化します。
- **ライフサイクルの分離**: `Scene` の生成・破棄に合わせて `SceneContext` も生成・破棄されるため、リソースリークやダングリング参照を防ぎやすくなります。
- **拡張ポイント**: 必要に応じて、リソース取得・解放 API などを派生クラスで拡張できます。

### 設計方針

- **所有権の明確化**: `Scene` が `SceneContext` の所有権を持ち、デストラクタで必ず `delete` します。
- **生成ルール**: `PlatformContext` の `createSceneContextForScene(Scene*)` で必ず新規インスタンスを生成し、共有リソースが必要な場合は実装側（例: Win32）で管理します。
- **抽象基底クラス**: 直接インスタンス化せず、各プラットフォームごとに派生クラス（例: `Win32SceneContext`）を実装します。

### 典型的な利用例

```cpp
class MyScene : public Scene {
  // ...
  // SceneContext* context_ は Scene 側で保持・deleteされる
};
```

- `Scene` のライフサイクルに合わせて `SceneContext` も安全に管理されます。

### 拡張例

- Win32: `Win32SceneContext` でウィンドウハンドルや GDI リソースを管理
- macOS: `MacSceneContext` で WindowRef や CoreGraphics リソースを管理

**まとめ:**
`SceneContext` は「シーンごとのプラットフォーム依存リソース管理」を担う拡張ポイントであり、所有権・生成・破棄の責任が明確な設計となっています。

## PlatformContext について

`PlatformContext` は、アプリケーション全体で共有される「プラットフォーム固有の機能・リソース管理」を担う抽象クラスです。

### 主な役割

- **OS 依存リソースの抽象化**: 各 OS やプラットフォームの機能（描画 API、入力、タイミング等）を抽象化・カプセル化します。
- **NodePool/GC の所有・管理**: UI ノードのメモリ管理を担当し、プラットフォームに最適化された NodePool/GC 実装を提供します。
- **SceneContext 生成**: `createSceneContextForScene(Scene*)` によって各 Scene に紐づく SceneContext を生成します。
- **共有リソース管理**: フォント、画像、サウンドなどの共有リソースをプラットフォーム固有の方法で最適に管理します。
- **nativeAnimate()**: プラットフォームネイティブのアニメーション機能への架け橋を提供します。

### 設計方針

- **シングルトン的利用**: アプリケーション内で通常 1 つのインスタンスとして存在します。
- **抽象基底クラス**: 直接インスタンス化せず、各プラットフォームごとに派生クラス（例: `Win32PlatformContext`）を実装します。
- **リソースの決定権**: どのリソースを共有し、どのリソースを Scene 単位で管理するかの決定権を持ちます。

### 拡張例

- Win32: `Win32PlatformContext` で Win32 API、Direct3D、GDI などのリソースを管理
- macOS: `MacPlatformContext` で AppKit、Metal、CoreAnimation などのリソースを管理

## スレッドモデル

- Declara!の UI/Scene 系は原則**単一スレッドで動作**するように設計されています。
- マルチスレッド化が必要な場合は、Context（SceneContext/PlatformContext）や専用 Tracker で外側から安全に制御します。
- Scene 単位でスレッド分離も可能な設計となっています。
- NodeGroupScope は RAII ＋ static でスコープネスト・所属先切替を安全に実現します。

## 今後の展望

- PlatformContext の拡張により、描画以外のリソース管理・入力・サウンド等も統合的に抽象化
- Scene/SceneBuilder の DSL 化やスクリプト連携による柔軟な世界構築

---

この設計により、単なるアプリケーション UI だけでなく、ゲームや仮想空間など多様な「世界」を最高のパフォーマンスで表現・実現できる基盤を目指しています。
