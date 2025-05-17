# TreeScene DSL 設計・運用仕様

## 概要

本ドキュメントは、Declara UI ツリーシステム（SceneNode/SceneNodeGroup/TreeSceneComponent 等）における、
C++98 準拠・型安全・効率的な DSL（ビルダー API/ファンクタ方式）設計方針・運用ルールをまとめたものです。

---

## 1. DSL 設計の基本方針

- C++98 環境下で「型安全」「責務分離」「再利用性」「部分的最適化」を両立するため、
  - ファンクタ（関数オブジェクト）＋関数チェーン型のビルダー API を採用
  - SceneNodeGroup/TreeSceneComponent 等のグループ単位で compose/再構築を行う
- if/for 等の動的構造もファンクタ内で自然に記述可能
- Key/Props/State の明示的なバインド・比較による差分最適化を重視

---

## 1a. ノード再利用戦略（ReuseCategory/ReuseHeuristic）

- ノードの再利用・破棄方針は priority 数値ではなく「用途カテゴリ」と「ヒューリスティック戦略」で明示的に指定
- 推奨カテゴリ:
  - **Reuse_Default**: グループの設定を継承（ノード個別指定なし）
  - **Reuse_OneShot**: 一時的な UI（トースト・アニメ等）。消えたら即破棄
  - **Reuse_FrequentlyUse**: 頻繁に出入りする UI（リスト・Lazy 系）。プールで再利用
  - **Reuse_Singleton**: 常駐 UI や状態保持が重要なもの。基本は再利用しない
- ヒューリスティック例:
  - **ReuseHeuristic_Default**: グループの設定を継承
  - **ReuseHeuristic_None**: カテゴリ方針のみ
  - **ReuseHeuristic_LRU/MRU**: 直近利用順で再利用優先度を制御
  - **Custom**: 独自戦略
- SceneNode/SceneNodeGroup 生成時に両方指定可能
- ノード側で Default 指定時はグループの設定を参照、それ以外はノード個別値を優先

---

## 再利用戦略の一括指定と継承について

- SceneNodeGroup で再利用戦略（カテゴリ・ヒューリスティック）をまとめて指定できるため、
  グループ配下のノードは`Default`指定で自動的にグループ設定を継承します。
  これにより、パフォーマンスやメモリ方針の一括最適化が容易です。
  クリティカルなノードのみ個別指定すれば十分です。

---

## 2. ファンクタ型ビルダー API の利用例

```cpp
// State型
class CounterState : public StateBase {
public:
    int value;
    CounterState(int v) : value(v) {}
};

// Props基底クラス
class MyComponentProps {
public:
    std::string key;
    CounterState* state;
    MyComponentProps(const std::string& k, CounterState* s) : key(k), state(s) {}
    bool operator==(const MyComponentProps& rhs) const {
        return key == rhs.key && state == rhs.state;
    }
};

// Component（ノード）例
class MyComponent : public SceneNode {
public:
    MyComponent(const MyComponentProps& props,
                NodeReuseCategory cat = Reuse_Default,
                NodeReuseHeuristic heur = ReuseHeuristic_Default)
        : SceneNode(cat, heur), props_(props) {}
private:
    MyComponentProps props_;
};

// SceneNodeGroupがStateTrackerを持つ例（dirtyマーク不要）
class SceneNodeGroup {
public:
    // recomposeトリガーStateのみ受け取る
    SceneNodeGroup(const std::vector<StateBase*>& recomposeStates = std::vector<StateBase*>(),
                  NodeReuseCategory cat = Reuse_Default,
                  NodeReuseHeuristic heur = ReuseHeuristic_Default)
        : tracker_(), reuseCategory(cat), reuseHeuristic(heur) {
        for (size_t i = 0; i < recomposeStates.size(); ++i) {
            if (recomposeStates[i]) bindDeferRecompose(recomposeStates[i]);
        }
    }
    void bindDeferRecompose(StateBase* state) {
        // Stateの変更通知コールバックを登録（recomposeトリガ）
        state->setOnChanged(SceneNodeGroup::onRecomposeStatic, this);
        tracker_.registerState(state);
    }
    static void onRecomposeStatic(void* groupPtr) {
        // コールバックからインスタンスメソッドを呼ぶ
        static_cast<SceneNodeGroup*>(groupPtr)->recompose();
    }
    void add(SceneNode* node) { /* ...existing code... */ }
    // compose関数でファンクタを受け取って呼び出す（RAIIでbegin/end管理）
    template<typename Builder>
    void compose(Builder& builder) {
        AutoTransactionGuard guard(&tracker_); // RAIIでbegin()/end()自動管理
        builder(*this);
    }
    NodeReuseCategory reuseCategory;
    NodeReuseHeuristic reuseHeuristic;
private:
    StateTracker tracker_;
    void recompose() { /* 差分再構築処理 */ }
    // ...existing code...
};

// ファンクタ型ビルダー（State受け渡し）
struct MyGroupBuilder {
    CounterState* state;
    MyGroupBuilder(CounterState* s) : state(s) {}
    ~MyGroupBuilder() {
        // stateの後始末やバインド解除が必要ならここで
    }
    void operator()(SceneNodeGroup& group) const {
        group.bindDeferRecompose(state); // グループにStateをバインド
        group.add(new MyComponent(MyComponentProps("main", state), Reuse_Singleton));
        if (state->value > 0) {
            group.add(new MyComponent(MyComponentProps("positive", state), Reuse_FrequentlyUse, ReuseHeuristic_LRU));
        } else {
            group.add(new MyComponent(MyComponentProps("nonpositive", state), Reuse_OneShot));
        }
    }
};

// --- SceneNodeGroup: Stateバンドルstruct/テンプレート受け取り例 ---
// 任意のStateセットをstructでまとめて型安全に渡せる
struct MyStateBundle {
    CounterState* counter;
    SomeOtherState* other;
    MyStateBundle(CounterState* c, SomeOtherState* o) : counter(c), other(o) {}
};

template<typename StateBundleT>
class SceneNodeGroup {
public:
    // Stateバンドルを受け取るコンストラクタ
    SceneNodeGroup(const StateBundleT& states,
                  NodeReuseCategory cat = Reuse_Default,
                  NodeReuseHeuristic heur = ReuseHeuristic_Default)
        : reuseCategory(cat), reuseHeuristic(heur) {
        bindAllStates(states);
    }
    // ...existing code...
private:
    template<typename Bundle>
    void bindAllStates(const Bundle& bundle) {
        // ユーザーが明示的に呼ぶか、SFINAE/マクロ等で自動化も可
        // 例: bindDefer(bundle.counter); bindDefer(bundle.other); ...
    }
};

// 利用例
MyStateBundle bundle(counterStatePtr, otherStatePtr);
SceneNodeGroup<MyStateBundle> group(bundle);
// ...existing code...

// 利用例
CounterState counter(42);
SceneNodeGroup group(0, 0, Reuse_FrequentlyUse, ReuseHeuristic_LRU);
MyGroupBuilder builder(&counter);
group.compose(builder);
// builderのデストラクタでstateの後始末も可能
// groupのデストラクタでbindDefer解除も可能
```

- SceneNode/SceneNodeGroup 生成時に再利用カテゴリ・ヒューリスティックを明示的に指定
- NodePool/ReusePool 側でカテゴリ・ヒューリスティックに応じて破棄・再利用戦略を切り替え
- Props/Key/State の流れが明示的で、部分的な再構築・最適化が容易

---

## 2a. RAII スコープによる自動ノード登録（2025-05-17 仕様追加）

- **SceneNodeAttachScope** による RAII スコープ管理を導入。
  - `SceneNodeAttachScope`のスコープ内で生成された SceneNode 派生オブジェクトは、
    スコープの種類（SceneNodeGroup または LayoutSceneNode）に応じて自動的に`add`/`addChild`される。
  - これにより、`group.add()`や`layout->addChild()`の呼び忘れによるメモリリーク・所有権の曖昧さを防止。
  - スコープはネスト可能で、最も内側のスコープが優先される（スタック管理）。
- 主要な SceneNode 派生クラス（SceneNodeButton, SceneNodeText, SceneNodeTextInput 等）は、
  コンストラクタで`SceneNodeAttachScope::autoAttach(this);`を呼ぶことで自動登録が有効化されている。
- これにより、compose/ビルダー API 内でノードを生成するだけで、
  所属先グループ・レイアウトへの登録が自動化され、宣言的かつ安全な UI ツリー構築が可能。
- 旧来の`group.add()`や`layout->addChild()`による手動登録も引き続き利用可能だが、
  新規実装・推奨パターンは RAII スコープ＋自動登録方式。

---

## 3. Key/Props/State の設計・バインド指針

- Props 基底クラスに`std::string key`を持たせ、動的構造でも安定した再利用を担保
- Props/State/Key は必ず明示的にバインド・比較できるよう設計
- if/for 等で State/Key/Props を使う場合は、必ずグループ/ノードのコンストラクタで bind すること
- バインド忘れは将来的に Linter/VSCode 拡張等で検出予定（TODO）

---

## 4. グループ入れ子・部分的最適化

- SceneNodeGroup の入れ子構造で部分的な再構築・最適化が可能
- インナーグループは無名ファンクタや一時オブジェクトで十分
- グループ単位で dirty/compose 管理し、大規模ツリーの効率的な差分適用を実現

---

## 5. 今後の検討・拡張ポイント

- NodeReuseHint/reusePriority の運用・最適化（設計方針のみ整理、実装は後回し）
- Props/State/Key の比較・再利用判定の拡張（複合 Key/ネスト Props 等）
- ビルダー API/DSL のさらなる簡素化（無名ファンクタ/ヘルパー/マクロ等）
- State/Props バインド安全性・デバッグ性向上（Linter/拡張での検出）
- 将来的な拡張（複数プラットフォーム対応、仮想ツリー的差分アルゴリズム等）

---

## 本設計の特徴的な強み

- SceneNodeGroup によって「最適化スコープ」を明示的に分割できるため、
  グループ単位で recompose 範囲・差分適用範囲を柔軟に設計可能です。
- 複数の State や Props を struct/テンプレートでまとめて型安全に受け渡し、
  グループ単位で I/O State のバンドル・責務分離がしやすい構造となっています。
- これにより、大規模 UI でも状態管理・依存関係の整理が容易で、設計意図も明確に保てます。

### 仮想ツリー的な差分アルゴリズムについて

本設計では、グループ単位での recompose ＋ Key/Props/State バインドによる差分最適化を基本とし、
仮想ツリー的な細粒度 diff アルゴリズムは現時点では採用していません。
これは多くの現代 UI ライブラリ（React, Solid.js, Jetpack Compose 等）と同様の設計方針であり、
グループ粒度の設計・Key/Props/State の明示的なバインドで十分な最適化が可能です。
将来的な拡張余地としては検討可能ですが、現状は不要です。

---

## 論理ツリーと物理ツリーの一致性について

本設計は、React のように「論理ツリー＝実態ツリー」を保ちつつ、
SceneNodeGroup 単位で再描画（recompose）スコープを柔軟に設計できるのが大きな特徴です。
これにより、パフォーマンスコストや再利用戦略が人間にも AI にも直感的に把握でき、
型安全・責務分離・最適化のバランスを高次元で両立できます。

---

## 参考

- tree_scene_component.md（従来の設計方針・責務分離）
- scene_and_platform_context.md
- architecture_state_tracker.md

---

> 本ドキュメントは今後の設計・運用・レビューの共通基盤として随時アップデートします。
