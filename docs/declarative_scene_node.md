# Declara! Declarative SceneNode Architecture

## 概要

本ドキュメントは、Declara! の SceneNode/Component/State 設計思想と、C++98/68k/Win32/クロスプラットフォーム環境における型安全・効率的な UI ツリー構築・差分更新・メモリ管理戦略、そして最新の ComposeTransaction/DSL 構文拡張案をまとめたものです。

---

## 1. 設計思想の要点（2025 年 6 月最新）

- **Compose 関数は return せず、副作用で NodeComposition に UI ツリーを構築**
  - `void compose(NodeComposition& c)` 形式に統一
- **NodeComposition に DSL 文法（operator<<）と状態管理（useState）を統合**
  - `c << VStack() << ...` で UI ツリーを構築、`c.useState(...)` で状態管理
- **group の役割を明確化**
  - `group(...)`：親子関係の明示化・差分検出・キャッシュ境界・中間ノードのチェーン構築用
- **68k/低速環境でも効率的な参照渡し設計**
  - 構造体値返しを避け、すべて参照/ポインタ渡しで効率化
- **型安全・拡張性・可読性重視**
  - State/Node/Props/Definition は struct ベースで型安全に
  - NodeComposition が状態管理と UI 構築の統一インターフェース

---

## 2. 主な構成要素

- **Props/StateDefinition/NodeDefinition (struct)**：UI 属性・状態・ノード定義を型安全に表現
- **NodeComposition**：compose 内で状態管理・UI 構築を一元化
- **StatePool (IStatePool)**：State の生成・再利用・破棄を SceneNodeGroup 単位で管理
- **scoped/group/DSL 記法**：`scoped`は全体ラップ、`group`は中間ノード用、`<<`演算子で宣言的にツリー構築

---

## 3. Compose 関数の最新構文例

```cpp
void compose(NodeComposition& c) {
    const State<int> counter = c.useState("counter", 0);

    c.declare(
        VStack()
            << Text("タイトル")
            << Button("送信")
            << group(
                RadioGroup()
                    << Radio("A")
                    << Radio("B")
            )
            << Text("フッター")
    );
}
```

- `c.declare(...)` で scoped や`t <<`を省略し、最短・安全な記法に
- group(...)は中間ノードの明示化のため引き続き使用

---

## 4. 実装に必要な構文要素（2025 年設計・stream/map 対応）

```cpp
template<typename T>
struct Stream {
    const std::vector<T>& items;
    // filter/map用ファンクタを保持
    // ...
    Stream(const std::vector<T>& v) : items(v) {}
    Stream<T>& filter(const FilterFunc& f); // 実装略
    Stream<T>& map(const MapFunc& f);       // 実装略
    NodeList toNodeList();                  // Node群に変換
};

struct NodeComposition {
    NodeList root;

    template<typename T>
    T& declare(T& x) {
        root = x;
        return x;
    }

    template<typename T>
    Stream<T> stream(const std::vector<T>& items) { return Stream<T>(items); }
    // 他にも useState, operator<< など
};
```

- `NodeList`は SceneNodeGroup の代替で、ノード群のリスト型（vector<NodeDefinition\*>等）
- stream/filter/map/toNodeList で C++98 でも型安全なリスト UI 構築が可能
- group は引き続き「親子関係の明示化」「差分境界」用途で利用

---

## 5. この設計の効果・メリット

- DSL 構文の一貫性・直感性が大幅に向上
- stream/filter/map でリスト UI も簡潔・型安全に
- メモリ効率が向上し、68k/G2 など低速環境でも安全に動作
- NodeComposition が状態管理と UI 構築の統一的なインターフェースとなり、拡張もしやすい
- group()/scoped()の語義明確化で可読性・保守性も高まる
- 既存の Node/Props/Scene 構造も活かしつつ、段階的な移行・拡張が可能

---

## 6. 今後の拡張・展望

- useEffect 等の副作用管理 API の追加（C++98 では関数ポインタ/メンバー関数で対応）
- State/NodePool の最適化・差し替え
- SceneNodeContextPool, AnimGroupComponent, RemoteStateTracker 等の拡張
- マルチスレッド・非同期解放戦略の導入

---

## 7. 実装計画・段階的リファクタリング方針

- 既存の Node/Props/Scene 構造を活かしつつ、新しい Definition/State/NodeComposition 設計を段階的に導入
- 互換性を保ちながら、テスト・動作確認しつつ移行
- まずは NodeDefinition/StateDefinition/NodeComposition/StatePool の型・インターフェース案を作成
- 1 つの Scene/Component で新 API を試験導入し、動作確認

---

## 8. 実装ステップ・段階的リファクタリング方針

1. **型設計・インターフェース整理**
   - NodeDefinition/StateDefinition/Props/SceneNodeGroup/StatePool/NodeComposition の型・責務を明確化
   - 68k/Win32/現代環境での互換性を意識した設計
2. **NodeComposition 導入**
   - compose 関数の引数に NodeComposition を追加し、副作用型 UI 構築 API に移行
   - 状態管理 API（useState 等）も NodeComposition に統合
3. **DSL 記法（<<, scoped, group）の実装**
   - Node/Group/SceneNodeGroup に operator<<や scoped/group 関数を実装し、宣言的な UI ツリー構築を実現
4. **差分検出・recompose ロジックの実装**
   - Definition の hash/==による差分検出・再利用処理を実装
   - State/Node の再利用・自動 recompose のテスト
5. **既存コードとの統合・段階的移行**
   - 既存の Scene/Component を新 API に順次移行
   - 旧 API との互換層が必要なら暫定的に用意
6. **テスト・デバッグ・最適化**
   - Win32/他プラットフォームでの動作検証
   - メモリ管理・パフォーマンス・エラーハンドリングの強化

---

## 9. 差分検出・hash/==戦略

- **NodeDefinition/Props/StateDefinition には必ず operator==と hash 関数を実装**
  - 差分検出・再利用・キャッシュのために、型ごとに比較・ハッシュのロジックを明示
- **差分検出の流れ**
  1. compose/recompose 時に新旧 Definition/Props/State を hash/==で比較
  2. 差分があれば Node/State を再生成、なければ再利用
  3. 差分検出は部分木単位で行い、最小限の再構築に留める
- **hash/==の設計意図**
  - パディングや未使用領域の影響を受けず、安全・柔軟・高速な比較を実現
  - struct の拡張や将来的な型追加にも柔軟に対応
- **注意点**
  - memcmp/memcpy によるバイト比較は使わず、明示的な==/hash 実装を推奨
  - ポインタや可変長配列を含む場合は、値の意味的な等価性を意識して比較・ハッシュを設計

---

## 5. scoped 関数について

- **scoped は Compose DSL の一部として引き続き使用**
  - ノードのスコープ切り替えや、グループ化・一時的な親ノードの切り替えに利用
  - 参照渡しで効率的に動作

```cpp
template<typename T>
T& scoped(T& x) { return x; }
```

- Compose 関数内で`scoped(...)`を使うことで、ノードのスコープや親子関係を明示的に制御できます

---

## 6. t.declare(...)・scoped(...)・group(...)の使い分けと設計意図

### ✅ なぜ t.declare(...) で scoped(...) を省略できるのか？

従来：

```cpp
scoped(
    t << VStack() << ...
);
```

- これは内部的には `SceneNodeGroupDefinition root = VStack() << ...; t.root = &root;` と同義
- ただし、root がローカル変数だと関数終了時に無効化されるため、コピーや unsafe な挙動のリスクがあった

**t.declare(...) による解決：**

```cpp
t.declare(
    VStack() << ...
);
```

- declare()の中で VStack()の値を直接ヒープに保存し、そのポインタを t.root に保持
- コピーなし・安全・scoped(...)の目的をそのまま含む
- つまり、t.declare(...)は「scoped で t に渡す」行為を内包した最短構文

### ✅ scoped(...) と group(...) の違い・役割

| 関数名   | 用途                             | 実装例                                    | メモ                                   |
| -------- | -------------------------------- | ----------------------------------------- | -------------------------------------- |
| scoped() | SceneNodeGroup のルート/中間登録 | `template<typename T> T& scoped(T& node)` | 副作用なし。今は明示的に使う必要なし   |
| group()  | チェーン中の中間ノードに子追加   | `template<typename T> T& group(T& node)`  | 「子を追加する親」を明示するための補助 |

- どちらも「パススルー」だが、命名で意図を明確化
- scoped()は「ルート登録」っぽさ、group()は「中間構築」っぽさを表現

### ✅ 最終まとめ

- `t.declare(...)`は`scoped(t << ...)`を内包するので、明示的な scoped(...)は不要
- scoped(...)は今では省略可能な構文補助関数
- group(...)は RadioGroup などに<<Radio(...)を繋げたいときの補助で、読みやすさと意図の明確化が目的

---

## 7. scoped による hash 最適化ポイントについて

- **scoped でラップしたノード（またはサブツリー）は、その関数の結果として hash 値を裏で持つ設計が可能**
  - これにより、scoped でラップした単位ごとに差分検出・キャッシュ・再利用が容易になる
  - Compose DSL の記述がシンプルなまま、最適な差分更新・再利用戦略を実現できる
- **最適化ポイントとしての意義**
  - scoped でラップした箇所が「差分検出・キャッシュの境界」となり、最小限の recompose で済む
  - サブツリー単位で hash/==を持たせることで、部分的な UI 再構築やキャッシュ再利用が高速・安全に
- **設計意図**
  - scoped は単なる構文補助だけでなく、「差分検出・最適化のための明示的な境界」としても活用できる
  - Compose DSL の柔軟性とパフォーマンスを両立する重要な設計要素

---

## 8. scoped / group の設計意図と使い分け詳細

### 🧠1. << のターゲットあいまい問題

- **目的**：`<<` でネストしていく構文を安全かつ意味的に明確にする
- **問題点**：`VStack() << Text() << Button()` の中に `RadioGroup << Radio` をネストするとき、どのノードの子になるのか不明瞭になる可能性がある
- **解決**：`scoped(...)` でこのノードの中だけで `<<` を解釈するという境界を作る

### ⚙️2. 構造体コピーコストの削減

- **問題点**：C++98 では `<<` による一時オブジェクトのコピーで構造体ごとコピーが発生しがち
- **解決策**：`ComposeTransaction& t` を渡すように変更し、`t << VStack() << ...` のように参照ベースでノードを構築
- **State<T> なども `t.useState()` として、トランザクション中に管理**
- これにより、実質的にゼロコピー構築が可能に！

### 🧩3. scoped(scoped(...)) のわかりづらさ

- `scoped(...)` をネストしたとき、
  - 外側：`t.declare(scoped(...))` = 差分更新の境界
  - 内側：`RadioGroup(...) << Radio(...)` をラップする明示的グルーピング
- 見た目が紛らわしい → そこで `group(...)` を導入！

### ✅ 最終形のルール認識

| 関数名   | 役割                                         | 使うタイミング                                                 |
| -------- | -------------------------------------------- | -------------------------------------------------------------- |
| scoped() | State 差分境界 + 子リスト構築                | t.declare(...) や中間の if/else 分岐など                       |
| group()  | 子ノードを複数まとめる中間レイヤー・差分境界 | RadioGroup << group(Radio << Radio) や if/else/group(...) など |

---

### 【設計補足】group 関数の役割と scoped の吸収について

- group 関数は「構文上の境界」「親子関係の明示化」「差分検出・キャッシュ境界」の役割を担う
- 以前 scoped が担っていた「差分境界」も group に吸収し、scoped は廃止・非推奨
- これにより、DSL/API 設計がよりシンプルかつ一貫性のあるものに
- 設計指針：
  - group は「親子関係の明示化」「差分検知・寿命管理・キャッシュ再利用」のための唯一の境界関数
  - 複雑な分岐や部分的な再構築も group でカバー
  - scoped に関する記述・実装は今後削除・非推奨化

---
