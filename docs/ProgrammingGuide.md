# Loka Programming Guide

## はじめに

Loka を一言で言えば、
論理的な UI 構造と状態遷移を組み立て、
その結果を各 OS のネイティブ環境へ投影するためのシステムです。

Loka は、React、Solid.js、Flutter、SwiftUI、Jetpack Compose のような
宣言的 UI とリアクティブな状態管理の考え方を、
より厳しい制約環境でも成立するように再設計したフレームワークです。

見た目の考え方は近くても、前提はかなり異なります。
Loka では次の点を強く重視します。

- ガベージコレクションに依存しない
- 例外に依存しない
- 状態の所有者を明示する
- 更新の流れを追跡しやすくする
- 古い OS や低速な環境でも成立する構造を優先する

Loka の主眼は、C 言語で書かれたような極小・極限最適化プログラムを追うことではありません。
むしろ、現代的で大規模なプロフェッショナルアプリケーションを、
宣言的な構造を保ちながら、性能と追跡可能性を損なわずに制作できることを重視しています。

古い OS や低速な環境への対応は、そのための制約条件であって、
表現力やアプリケーション規模そのものを縮小することが目的ではありません。

その背景には、ソフトウェアは本来もっと可塑的で、追跡可能で、
組み替えやすいものであるべきだという問題意識があります。
Loka は、都度の再ビルドや巨大なブラックボックスに過度に依存せず、
構造と状態の流れを人間にも扱いやすい形で保ちたいという考え方の上にあります。

そのため、React の `useState` や Solid.js の signal、
Flutter の stateful widget、SwiftUI / Compose の `@State` に近い感覚で理解できる部分はありますが、
そのまま同じではありません。

Loka を理解するうえで大切なのは、
「UI から考える」のではなく「State から考える」ことです。
このガイドでも、その順番で説明していきます。

## このガイドの対象

このガイドは、次のような人を主な対象にしています。

- React や Solid.js の宣言的 UI に慣れている人
- Flutter / SwiftUI / Jetpack Compose の状態駆動 UI に慣れている人
- C++ で軽量な UI フレームワークを設計または利用したい人
- Loka の DSL は読めるが、設計の意図を順番に理解したい人

## 読み進め方

最初は細かい最適化やプラットフォーム差分を追わず、次の流れだけ掴んでください。

1. State がアプリケーションの事実を持つ
2. UI はその State を読む
3. イベントは State を変更する
4. 必要な場所だけが更新される

この流れが Loka の中心です。

## 1. Loka の基本思想

### UI は値の結果である

Loka では、UI を「手続き的にいじるもの」というより、
現在の State から導かれる結果として扱います。

たとえば、

- ボタンが有効か無効か
- ラベルに何が表示されるか
- 詳細パネルを表示するかどうか

といったものは、どこかの State の値として表現されるべきです。

これは React や SwiftUI と近い考え方ですが、
Loka はさらに「その State を誰が所有しているか」を重視します。

Loka の見方では、UI ライブラリの本体はネイティブ API 呼び出しそのものではありません。
本体は、論理的な UI 構造と State 遷移を組み立てることにあります。
描画やネイティブ反映は、その結果を各 OS の環境へ投影する最後の段階です。

Loka が full reactive な方向を強く取る理由のひとつは、
C++98 では現代的なクロージャーや lambda を前提にした設計ができないことです。
そのため、局所的な無名関数や hook 的な記述に依存するのではなく、
State、明示的な ownership、そして追跡可能な更新経路を中心に据えたほうが、
構造として安定しやすくなります。

同時に、Loka は C++98 でありながら、型安全性をできるだけ高い水準で保つことも重視しています。
特に DSL や Flow のように構造を組み立てる API では、
TypeTag、template 制約、明示的な API 分離などを通じて、
誤用をできるだけ早い段階で検出できるように設計しています。

### 魔法よりも追跡可能性を優先する

Loka は便利さのために大きなランタイム機構を増やしすぎません。
暗黙の挙動や自動推論を広げるよりも、
依存関係と更新経路を追えることを優先します。

これは一見すると少し手間に見えますが、
古い OS や低速 CPU、厳しいメモリ制約の上では大きな利点になります。

更新が見えやすいということは、

- どこで再構成が起きるか
- 誰が State を持っているか
- なぜ redraw が走ったのか

を調べやすいということでもあります。

### Loka は「状態駆動」だが「無制限な自動化」はしない

Loka は reactive です。
ただし、「何でも勝手に更新してくれる」方向には寄せていません。

代わりに、

- 適切な State 型を選ぶ
- 適切な所有者に置く
- 適切な Boundary で更新範囲を切る

という設計判断を明示的に行います。

この方針は、最初は少しだけ考えることが増えます。
しかし、規模が増えたときに挙動が読みやすく、
また古い環境でも成立しやすい構造になります。

## 2. まず State から考える

### なぜ State から始めるのか

Loka では、UI コンポーネントの見た目やツリー構造より先に、
「何が変化するのか」を定義するのが自然です。

たとえばカウンター画面なら、先にあるべきなのは次の事実です。

- 現在のカウント値
- ボタン押下イベント
- カウント値から導かれる表示文字列

この順番で考えると、UI はそれらを読むだけの存在になります。

React で言えば `state -> render`、
Solid.js で言えば `signal -> view`、
SwiftUI / Compose で言えば `state -> body` に近い流れです。

Loka でも同じく、最初に State を置きます。

### State は「事実」を表す

State は、その時点のアプリケーションの事実です。

たとえば次のようなものは State に向いています。

- 現在の文字列
- チェック状態
- 選択中インデックス
- 表示中かどうか
- 読み込み中かどうか
- 現在のモデル値

逆に、毎回計算すればよい見た目だけの値は、
常に独立した MutableState にする必要はありません。
元の State から導けるなら、導出値として扱うほうが自然です。

### 最小の例

以下は、ごく単純な値 State のイメージです。

```cpp
loka::core::MutableState<int> count(0);
```

この `count` は「今のカウント値」を持つ State です。
ボタンを押したら値を増やし、UI はそれを読んで表示します。

Loka では、この「State を中心に置く」構造がすべての基本になります。

## 3. Loka における主な State の種類

### `State<T>`

`State<T>` は読み取り用の値です。
「読むこと」はできますが、通常は直接変更しません。

これは「UI から見える値」という抽象の基本形です。

たとえば、

- ラベルが読む文字列
- ボタンが読む enabled 状態
- 子ノードが参照する共有値

などは `State<T>*` として渡せます。

### `MutableState<T>`

`MutableState<T>` は書き換え可能な State です。
アプリケーション内で事実が変化するなら、多くの場合これを使います。

```cpp
loka::core::MutableState<loka::core::String> title(
    loka::core::String::Literal("Hello"));
```

これは React の `useState`、Solid.js の signal、
SwiftUI / Compose の可変 state に近い位置づけです。
ただし Loka では、所有者と更新規約をより明示的に扱います。

### `EmitterState`

`EmitterState` は値ではなくイベントを表す State です。

たとえば、

- ボタンが押された
- メニューが選ばれた
- 再読み込みを要求した

といった「瞬間的な通知」を表したいときに使います。

値を保持する `MutableState<T>` と、
イベントを流す `EmitterState` を分けることで、
状態とイベントの役割が混ざりにくくなります。

### `BoundState<T>`

`BoundState<T>` は Boundary に結びついた State ハンドルです。

これは Loka らしい重要な概念です。
単なる値ではなく、

- どの owner に属するか
- どの tracker に結びつくか
- いつまで有効か

を踏まえた State と考えてください。

画面やノードのローカルな状態を持たせたいとき、
`declareStates()` や `useState()` のような流れで扱う対象がこれです。

ここで重要なのは、
`declareStates()` や `useState()` を使うと、
State が自然に「最寄りの Boundary に属する状態」として扱われることです。

これは単なる便利 API ではありません。
Loka における state ownership と lifecycle を安全に揃えるための、
かなり大切な入口です。

## 4. Boundary-first で考える

### なぜ Boundary を中心にするのか

Loka では、mutable state を「どこからでも触れるもの」として扱いません。
代わりに、Boundary を state ownership の基本単位として扱います。

この方針には理由があります。

- 誰が state を所有しているか追いやすい
- lifecycle の向きが崩れにくい
- 子が親を暗黙に安定化する設計を避けやすい
- redraw / compose / native reflection の責務を Boundary 単位で考えやすい

小規模なうちは自由な参照の方が楽に見えることがあります。
しかし規模が大きくなるほど、
「どこからでも触れる state」は dependency graph を平坦化し、
更新経路と ownership を読みづらくします。

Loka はそこを逆に絞ります。

### mutable state の正規ルート

app / DSL コードで mutable state を扱う通常ルートは、基本的に次の 2 つだけです。

1. Boundary-owned local state
2. repository-owned shared/global state

Boundary-owned local state は、通常 `declareStates()` で作ります。

```cpp
void attachNode(loka::app::scene::NodeComposition &c)
{
  c.declareStates()
      .state(count_, 0)
      .state(label_, loka::core::String::Literal("Count: 0"));
}
```

これが owner path です。

一方で、子ノードや別 Boundary が親の mutable state を直接所有したり、
raw `MutableState<T>*` をそのまま運んだりするのは、通常ルートではありません。

### `currentBoundary()` は owner-side path

`NodeComposition::currentBoundary()` は、
「いま compose している Boundary 自身」に対する owner-side access です。

これは foreign state を辿るための API ではありません。
owner に一致する `BoundState<T>` に対してだけ、意味を持つ path です。

要点は次の通りです。

- owner が自分の `BoundState<T>` を読む/更新する
- child から ancestor-owned `BoundState<T>` を直接 mutate しない
- owner-side mutable access を borrowed access と混ぜない

### `findBoundary()` は direct parent borrowed path

`findBoundary()` は、Boundary graph を自由に探索するための API ではありません。
Loka ではこれを direct parent borrowed access に絞ります。

つまり通常の意味はこうです。

- 子が親 Boundary の公開面を読む
- sibling / cousin / multi-hop traversal はしない
- `findBoundary().findBoundary()` のような連鎖は normal path では考えない

ここで大事なのは、
`findBoundary()` は mutable ownership を渡す path ではない、という点です。

child が親を読みたいなら、
親は narrow な facade か read-only state を公開します。

```cpp
class ParentFacade
{
public:
  loka::app::scene::BorrowedState<int> countState() const;
  bool canShowDetails() const;
};
```

このように、borrowed path は基本的に read-only です。

現時点では、borrowed surface を表す型として
`BorrowedState<T>` を使うのが分かりやすい方針です。

```cpp
class ParentFacade
{
public:
  loka::app::scene::BorrowedState<loka::core::String> titleState() const;
};
```

これは「read-only の borrowed input である」ことを API 上で見えやすくします。
ただし既存コードや一部 API では raw `State<T>*` が残ることもあります。
今の段階では、`BorrowedState<T>` を推奨寄りの表現と考えてください。

### 親から子へは `BoundaryProps`

親から子へ値や依存を渡す正規ルートは `BoundaryProps` です。

ここでも重要なのは、何を props に入れてよいかです。
Loka では、Boundary をまたぐ値はできるだけ次に寄せます。

- plain value
- read-only `State<T>*`
- `BorrowedState<T>`
- `Managed<T>`
- narrow facade

逆に、通常ルートでは避けるべきものがあります。

- `BoundState<T>`
- raw `MutableState<T>*`
- owner-specific mutable handle

なぜなら、それらを props 経由で運ぶと、
ownership と lifecycle の境界が曖昧になりやすいからです。

特に大事なのは、`BoundState<T>` は cross-boundary transport 用の型ではない、
という点です。

`BoundState<T>` を member として保持してよいのは、
その component / boundary 自身が `declareStates()` で作った
owner-side local state に限る、と考えるのが基本です。

つまり、

- 自分の `declareStates()` 由来の `BoundState<T>` を member に持つ: よい
- 親や別 boundary 由来の `BoundState<T>` を props/member で運ぶ: 避ける

という線引きです。

### `Managed<T>` は explicit shared access

大きい scope から小さい scope へ、
明示的に shared resource/state を配りたい場合は `Managed<T>` を使います。

ここで大切なのは、
`Managed<T>` は ownership transfer ではない、という点です。

Loka の考え方では、

- owner は大きい側にいる
- 小さい側は explicit shared access を受け取る
- child が親の lifecycle を逆向きに支えない

という重力を保ちます。

したがって `Managed<T>` は便利な抜け道ではなく、
cross-boundary sharing を explicit にするための手段です。

### 子から親を変えたいとき

子から親を変えたい場合でも、
通常は親の `BoundState<T>` を直接 `set()` しません。

代わりに、Loka では次の流れを取ります。

1. child が intent を上げる
2. parent がそれを受ける
3. parent が自分の state を更新する

つまり、

- child -> parent の読み取りは borrowed path
- child -> parent の変更要求は event / callback / facade method
- 実際の mutation は owner が行う

です。

このルールを守ると、
boundary 間通信は明示的で追跡しやすくなります。

## 5. `dangerously*` は escape hatch である

Loka には `dangerouslyUseState()` や `dangerously*` な内部面が残っています。
しかし、これは通常の app/DSL 実装で積極的に使うための API ではありません。

意味としては、

- 正規ルートでは表現しにくい
- 既存構造との接続のために一時的に必要
- 設計レビュー付きで使う例外

です。

通常コードでは、まず次を優先します。

- `declareStates()` で owner-side local state を作る
- `currentBoundary()` を owner path として使う
- `findBoundary()` は borrowed/read-only に留める
- cross-boundary sharing は `Managed<T>` か narrow facade にする

`dangerously*` を使いたくなったら、
「本当に state ownership の設計がずれていないか」を一度疑ってください。

### `DerivedState<T>`

`DerivedState<T>` は、ほかの State から計算される値です。

たとえば、

- `count` から `"Count: 3"` を作る
- 複数の条件から enabled 状態を作る
- モデルの一部を表示用形式に変換する

といったときに使えます。

すべてを `MutableState` として持つ必要はありません。
元の事実から導けるなら、導出値として扱うほうが設計は整理されます。

## 4. まず覚えるべき実践ルール

Loka を使い始める段階では、次のルールだけ先に覚えると読みやすくなります。

- 長く生きる事実を State にする
- UI は State を読む側に寄せる
- ボタン押下などの通知は `EmitterState` を使う
- 画面ローカルな状態は Boundary 側で所有する
- 導ける値は必要以上に `MutableState` へ増やしすぎない

この時点では、Boundary や Tracker の細かい内部まで理解しなくても構いません。
ただし「State には所有者がある」という感覚だけは、早い段階で持っておくべきです。

## 5. 他フレームワーク経験者向けの最初の対応づけ

ざっくりした感覚では、次の対応で考えると入りやすくなります。

- React の `state` / `useState` に近いもの: `MutableState<T>`
- Solid.js の signal に近いもの: `State<T>` と `MutableState<T>`
- SwiftUI / Compose の local state に近いもの: `BoundState<T>`
- UI event callback に近いもの: `EmitterState`
- computed value に近いもの: `DerivedState<T>`

ただし、Loka はこれらより ownership と更新境界を明示的に扱います。
その差が次の章以降の中心になります。

## 6. 最初の例: Counter

まずは、Loka の最小の流れを `Counter` で見るのが分かりやすいです。

- `BoundState<int>` でローカル state を持つ
- `EmitterState` でボタンイベントを受ける
- ハンドラで state を更新する
- UI はその state を読む

ここでは、まだ説明していない 2 つの要素が出てきます。

- `StateTrackerGuard`
- thunk パターン (`static void IncrementThunk(void *userData)`)

`StateTrackerGuard` は、State 更新を tracker の文脈で安全にまとめるための RAII ガードです。
この段階では、「State を更新するときはまず guard を作る」と理解しておけば十分です。

また、thunk パターンは C++98 の制約によるものです。
Loka では現代的な lambda や `std::function` を前提にしないため、
イベント結線では関数ポインタと `void *userData` を受ける形をよく使います。

```cpp
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"

class CounterNode : public loka::app::scene::StdCompositionNodeFor<CounterNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<CounterNode> PropsType;

  CounterNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<CounterNode>(p),
        count_(),
        countText_(),
        increment_()
  {
  }

  virtual void attachNode(loka::app::scene::NodeComposition &c)
  {
    c.declareStates()
        .state(this->count_, 0)
        .state(this->countText_, loka::core::String::Literal("Count: 0"));
    this->increment_.deferBind(&CounterNode::IncrementThunk, this);
  }

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    c.declare(loka::app::VStack()
              << loka::app::Text(this->countText_.state())
              << loka::app::Button("Increment", &this->increment_));
  }

private:
  static void IncrementThunk(void *userData)
  {
    CounterNode *self = static_cast<CounterNode *>(userData);
    if (!self)
    {
      return;
    }
    self->increment();
  }

  void increment()
  {
    loka::core::StateTrackerGuard guard(this->tracker());
    this->count_.set(this->count_.get() + 1, true);
    this->countText_.set(loka::core::String::Literal("Count: ")
                             + loka::core::String::FromInt(this->count_.get()),
                         true);
  }

  loka::app::scene::BoundState<int> count_;
  loka::app::scene::BoundState<loka::core::String> countText_;
  loka::core::EmitterState increment_;
};
```

この例で見てほしいのは、
`state -> event -> state update -> UI reflect` の流れです。
Loka では、この流れがすべての基本になります。

あわせて、

- `attachNode()` で state を作る
- `composeNode()` で UI を組み立てる
- ハンドラ内では `StateTrackerGuard` の下で state を更新する

という役割分担も見ておくと、その後の章が読みやすくなります。

## 7. State で切り替える Toggle UI

次は、条件に応じて UI を切り替える最小例です。

Loka では、まず state-driven な切り替えを考えるのが自然です。
構造を大きく組み替える前に、
`Show()` のような明示的な切り替えで表現できるかを先に考えると整理しやすくなります。

```cpp
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"

class TogglePanelNode : public loka::app::scene::StdCompositionNodeFor<TogglePanelNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<TogglePanelNode> PropsType;

  TogglePanelNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<TogglePanelNode>(p),
        detailsVisible_(),
        toggle_(),
        detailsDefinition_(loka::app::Text("Details are visible."))
  {
  }

  virtual void attachNode(loka::app::scene::NodeComposition &c)
  {
    c.declareStates().state(this->detailsVisible_, false);
    this->toggle_.deferBind(&TogglePanelNode::ToggleThunk, this);
  }

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    c.declare(loka::app::VStack()
              << loka::app::Button("Toggle Details", &this->toggle_)
              << (loka::app::Show(this->detailsVisible_.get()) << this->detailsDefinition_));
  }

private:
  static void ToggleThunk(void *userData)
  {
    TogglePanelNode *self = static_cast<TogglePanelNode *>(userData);
    if (!self)
    {
      return;
    }
    self->toggleDetails();
  }

  void toggleDetails()
  {
    loka::core::StateTrackerGuard guard(this->tracker());
    this->detailsVisible_.set(!this->detailsVisible_.get(), true);
  }

  loka::app::scene::BoundState<bool> detailsVisible_;
  loka::core::EmitterState toggle_;
  loka::app::TextDefinition detailsDefinition_;
};
```

この例では、

- 状態の事実は `detailsVisible_`
- 切り替え契機は `toggle_`
- UI の条件分岐は `Show()`

という役割分担になっています。

## 8. 同じ UI を Toolbox と macOS に投影する

Loka の cross-platform 性を理解するには、
「同じ論理 UI を別の OS に投影する」という見方が重要です。

たとえば UI を構成する Node 側は、次のようにそのまま共通で使えます。

```cpp
class HelloNode : public loka::app::scene::StdCompositionNodeFor<HelloNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<HelloNode> PropsType;

  HelloNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<HelloNode>(p)
  {
  }

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    c.declare(loka::app::VStack()
              << loka::app::Text("Hello from Loka")
              << loka::app::Button("OK"));
  }
};
```

アプリ側では、その Node を Scene / Window に載せるだけです。
論理 UI 自体は変えず、投影先だけを変えます。

```cpp
virtual void compose(loka::app::AppComposition &c)
{
  c << loka::app::WindowDef(loka::app::WindowProps()
                                .frame(40, 40, 320, 240)
                                .title("Hello")
                                .visible(true)
                                .scene(loka::app::scene::NodeDefinition<HelloProps, HelloNode>()));
}
```

このコードは、Toolbox でも macOS でも同じ論理構造として書けます。
違うのは最終的な投影先の platform/app layer です。

つまり Loka では、

- UI の論理構造はできるだけ共通に保つ
- OS ごとの差は platform/app layer に押し込む
- 利用者はまず論理 UI を組み立てる

という流れになります。

### 論理 UI が真実で、Platform は投影先である

Loka では、Scene / Boundary / State から構成される論理 UI を
single source of truth として扱います。
macOS の view、Win32 の HWND、Toolbox の control/window は、
その論理 UI を投影した結果です。

この投影は常に完全な即時同期とは限りません。
実際には、State 更新は tracker に集められ、
Scene が update request / apply analysis をまとめ、
PlatformController が必要な structure / layout / paint work を実行します。

そのため、考え方としては次のようになります。

- State / Boundary / Scene が現在の真実を持つ
- Scene は 1 回の更新波を snapshot / transaction として整理する
- PlatformController はその結果を native UI へ反映する
- native callback や dialog result は、古い論理状態に属していないか注意する

この分離は、単に将来の remote / SSR-like backend のためだけではありません。
ローカルの Cocoa / Win32 / Toolbox でも、
timer、dialog、deferred callback、native event は少し遅れて戻ってくることがあります。
Loka では、それを例外的な事故ではなく、
「host は遅延 mirror であり、truth は logical UI にある」として扱います。

アプリケーションを書く側では、
PlatformController や NativeContext に ownership 判断を押し込まず、
State と Boundary の所有関係を先に整理することが大切です。
Platform はその結果を実行する層であり、
論理 UI の事実を再決定する場所ではありません。

## 9. Flow と async without async/await

ここまでで、Loka の UI と State の基本は見えてきました。
ただし、今の Loka の強みはそれだけではありません。

もうひとつの大きな柱が、

- `Flow DSL`
- `async without async/await`
- pipeline 型の orchestration

です。

特に C++98、例外なし、coroutine なしという条件では、
非同期処理や段階的な処理フローをどう整理するかが大きな課題になります。
Loka の Flow は、そのための仕組みです。

### なぜ Flow が必要なのか

非同期処理を plain callback だけで書き始めると、
すぐに次の問題が出ます。

- 手順がコード上で見えにくい
- 成功系と失敗系が散らばる
- UI 更新と非同期結果の接続が読みづらい
- テストしにくい

Loka の Flow は、
これを「段階のある pipeline」として整理するためにあります。

つまり Flow は、
単に async を動かすための仕組みではなく、
手順・分岐・変換・副作用ポイントを見える形にするための DSL です。

Flow を作った理由は、reactive な状態変化が増えるほど、
State の更新がピンボールのように画面内を跳ね回り、
散在した hook や callback を追いかけるのが難しくなっていくからです。

Flow は、その処理フローを 1 箇所に集約し、
retry、skip、failure handling を明示できるようにすることで、
full reactive な構成であっても、従来の手続き的なプログラムに近い感覚で
追跡とデバッグを行えるようにします。

その結果として、
Loka では coroutine や async/await がなくても、
それに近い段階的な非同期プログラミングが可能になります。

### async/await がなくても、手順は書ける

Loka は async/await や coroutine に依存しません。
それでも、

- ある処理を開始する
- 完了を待つ
- 結果を変換する
- 次の処理へ渡す
- 最後に UI state を更新する

という流れは表現できます。

この意味で Flow は、
`async/await がない環境での手順の明示化` と考えると分かりやすいです。

### Flow の見方

Flow は、まず次のように捉えると入りやすくなります。

- `Step`: 意味のある処理単位
- `Flow`: それらをつないだ pipeline
- `onSuccess` / `onFailure`: 次の遷移先や終端処理

Loka では、1 ステップだけの Flow を何でも作るより、
意味のある境界で 2 段階以上に分けるほうが自然です。

たとえば、

- 読み込み開始
- デコード / 変換
- UI 反映

のように分けると、意図が見えやすくなります。

### 最小イメージ

概念的には、Flow は次のような読み方をします。

```cpp
Flow()
  | Step(STEP_OPEN, OpenFileAdapter())
  | Step(STEP_DECODE, DecodeImageAdapter())
  | Step(STEP_APPLY, ApplyToStateAdapter())
  | onSuccess(Handler::noop, FLOW_DONE)
  | onFailure(ShowErrorAdapter(), FLOW_FAILED);
```

この例で重要なのは文法そのものではなく、
処理が「段階のある pipeline」として読めることです。

### UI と Flow はどうつながるか

Loka では、UI event が直接すべての仕事を抱え込む必要はありません。
自然な流れは次のようになります。

1. ボタンやメニューが event を出す
2. event を受けて Flow を開始する
3. Flow が段階的に処理を進める
4. 最後に Main Thread へ戻って state を更新する
5. UI が結果を反映する

つまり、
`UI event -> Flow -> state update -> UI reflect`
という流れです。

この構造にすると、
UI 側は event の入口で薄く保てます。
重い処理や複数段階の処理は Flow 側へ押し出せます。

### Flow が向いているケース

Flow は特に次のような処理に向いています。

- ファイルを開いて読み込み、結果を UI に反映する
- 複数段階の検証や変換を経て state に適用する
- 成功系と失敗系を明示的に分けたい
- テストやシナリオとして手順を再利用したい
- game / multimedia 的な pipeline を記述したい

逆に、単純な 1 回の state 更新だけなら、
普通のハンドラ関数のほうが読みやすいこともあります。

### visual regression test への応用

Flow はアプリケーション処理だけでなく、
テストやシナリオ実行にも向いています。

たとえば visual regression test では、

- Window を開く
- 必要な state や event を流す
- 画面を安定状態まで進める
- bitmap / snapshot を取得する
- 基準画像と比較する

といった手順を 1 本の Flow として表現できます。

```cpp
Flow()
  | Step(STEP_OPEN_WINDOW, OpenWindowAdapter())
  | Step(STEP_DRIVE_UI, DriveScenarioAdapter())
  | Step(STEP_CAPTURE, CaptureBitmapAdapter())
  | Step(STEP_COMPARE, CompareSnapshotAdapter())
  | onSuccess(Handler::noop, TEST_PASSED)
  | onFailure(SaveFailureArtifactAdapter(), TEST_FAILED);
```

現時点では、Loka でこの運用が全面的に整っているわけではありません。
ただし考え方としてはかなり自然で、
Flow DSL の強みがそのまま visual test の記述にも活きます。

このように Loka の Flow は、
async orchestration だけでなく、
UI シナリオや visual test の記述にも応用できます。

### Loka における Flow の意味

Flow は、UI の補助機能ではありません。
Loka においては、
state-driven UI と並ぶもうひとつの大きな軸です。

UI が「論理構造を組み立てて OS に投影する」ための仕組みだとすれば、
Flow は「段階的な処理や async を構造化する」ための仕組みです。

この 2 つが揃うことで、
Loka は単なる declarative UI DSL ではなく、
UI と処理パイプラインを一貫した考え方で扱える基盤になります。

## 10. State の所有者は誰か

Loka では、State を「ただ置く」のではなく、
「誰がそれを持つのか」を明確に考えます。

これは他の宣言的 UI フレームワークにもある発想ですが、
Loka では特に重要です。
なぜなら、所有者の選び方がそのまま、

- 寿命
- 更新範囲
- 再利用性
- 安全性

に直結するからです。

### 基本原則

基本原則は単純です。

- 親全体の事実なら親が持つ
- 特定の部分だけのローカル状態ならその Boundary が持つ
- 子はできるだけ「受け取って読む側」に寄せる

そして、Boundary ローカルな状態を作るときは、
まず `declareStates()` や `useState()` を使うのが基本です。
これにより、状態の所属先が曖昧になりにくくなります。

たとえば、

- ウィンドウ全体で共有する検索文字列
- 複数の子が参照する選択中アイテム
- アプリケーション全体の設定

のようなものは、上位で持つほうが自然です。

一方で、

- その部分 UI だけが使う開閉状態
- 一時的な編集状態
- 局所的な内部フラグ

のようなものは、その Boundary に閉じ込めたほうが安全です。

### 親が持つべき State

次の条件に当てはまるなら、親が持つことをまず検討してください。

- 複数の子が同じ State を読む
- 子が入れ替わっても値を維持したい
- 画面全体の一貫性に関わる
- 子より長く生きるべき

これは React でいう state lifting に近い考え方です。
Loka でも同じく、共有される事実は上へ寄せると整理しやすくなります。

### 子の Boundary が持つべき State

次の条件なら、子の Boundary で持つほうが自然です。

- その部分だけで完結している
- 親が詳細を知らなくてよい
- 子の再利用性を高めたい
- 更新範囲を局所化したい

ただし、子が事実上「親の代わりに親の状態を持つ」構造は避けるべきです。
Loka では ownership policy として、
親が持つべきものは親が持つ、という方向を強く勧めます。

### Props で State を渡す

Loka では、親が持っている State を子へ Props 経由で渡すことがよくあります。

```cpp
struct ChildRefs
{
  loka::core::State<loka::core::String> *title_;
  loka::core::State<bool> *enabled_;
  loka::core::EmitterState *clicked_;
};
```

この形にすると、子は

- 値を読む
- イベントを発火する

ことに集中できます。

親が事実を持ち、子はそれを反映する。
これが Loka の素直な構造です。

### `declareStates()` / `useState()` を優先する理由

Loka では、好きな場所で `MutableState<T>` を作って
そのまま Boundary や子ノードへ渡すこと自体は、状況によっては可能です。
しかし、それを無秩序に行うと lifecycle mismatch を起こしやすくなります。

たとえば次のようなズレが起こります。

- State の寿命は長いのに、それを使う Boundary は短命
- Boundary は生きているのに、参照先 State が先に消える
- 誰が owner なのかがコード上で曖昧になる
- 更新経路は動いていても、設計意図が読み取れなくなる

この問題を避けるため、
Boundary ローカルな状態は `declareStates()` や `useState()` で作るのが基本です。

そうすると、

- 最寄りの Boundary に属する
- tracker との結びつきが自然に揃う
- 寿命の基準が明確になる
- 「これはローカル state だ」と読み手に伝わる

という利点があります。

特に Loka では、
「State を作れる」ことと「その State をどこに置くべきか」は別問題です。
前者の自由度より、後者の整合性のほうが重要です。

### ライフサイクル不一致を避けるための考え方

迷ったときは、次の順で判断すると整理しやすくなります。

1. この State は誰の事実か
2. その owner は子より長く生きるか
3. 子が消えても値を維持したいか
4. ローカル state として閉じたほうが自然か

この結果、

- 子だけの事実なら、その子の Boundary に置く
- 親子をまたいで共有するなら、親に置いて Props で渡す
- 寿命が曖昧なら、まず owner を決め直す

という形に寄せると崩れにくくなります。

### 避けたい構造

次のような構造は、Loka では注意が必要です。

- 親の本来の事実を、子の Boundary 内部 state として持つ
- 一時的に作った `MutableState<T>` のポインタを広く配る
- owner が不明な State を複数の Boundary で共有する
- 「たまたま今は生きている」前提で参照をつなぐ

こうした構造は、
最初は動いても、差し替えや再構成、寿命の変化が入ると壊れやすくなります。

Loka の ownership policy を単純化して言うなら、
「State は責任を持てる owner に置き、子は必要なものだけ受け取る」
です。

## 11. Boundary とは何か

Boundary は、Loka を理解するうえで最重要の概念のひとつです。

直感的には、Boundary は次のものを束ねる単位です。

- 状態の所有
- 再構成の境界
- ライフサイクル
- 観測対象の管理

React や SwiftUI のコンポーネントに近い面もありますが、
Loka の Boundary はそれより少し物理的です。
単なる見た目の部品ではなく、
「どこで更新を閉じるか」を決める構造でもあります。

### なぜ Boundary が必要なのか

もしすべての状態変化が大きな 1 つの木に対して無制限に波及すると、
制約の厳しい環境ではコストも追跡難易度もすぐに悪化します。

Boundary を持つことで、

- ローカルな状態をローカルに閉じ込める
- 再構成の範囲を切る
- 子の寿命と親の寿命を整理する

ことができます。

Loka はこの境界を明示的に置くことで、
低コストと追跡可能性を両立しようとしています。

### Boundary をどう考えるべきか

Boundary は「とりあえず何でも分割する」ためのものではありません。
むしろ次のように考えるほうが自然です。

- 独立した状態やライフサイクルが必要なら Boundary
- ただ子を並べたいだけなら Group や通常の DSL で十分
- 更新範囲を切る意味があるときに Boundary を置く

この感覚は重要です。
Boundary を増やしすぎると構造は重くなり、
逆に少なすぎると更新範囲が広がりやすくなります。

## 12. StdComposition と Boundary

Loka では、Boundary が composition と state の所有単位になります。
そして通常の DSL 実装では、`StdComposition` を前提に考えるのが基本です。

ただし Loka は、一般的な宣言的 UI ライブラリのように、
内部の composition / reuse / redraw 戦略まで単一方式に強く固定された設計ではありません。
`Boundary` ごとに別の戦略を持たせる余地があり、
Boundary 自体も入れ子にできるため、UI の区画ごとに異なる方針を適用できます。

`StdComposition` は、
一度組み立てた構造を安定的に使う方向の composition です。

より具体的には、compose を基本的に一度だけ行い、
その時点で State への bind を確立し、
以後は composition tree を組み直さずに bind 済み state の更新で反映していく、
fine-grained reactivity に近い方式です。

これは次のような場合に向いています。

- 構造がほぼ固定
- 主な変化が props や値更新に留まる
- `Show` や stream などで同一モデル内に条件分岐を保てる
- 低コストを優先したい

Loka では特に古い環境を強く意識しているため、
まず `StdComposition` を基準に考えるのはかなり自然です。

Recompose 自体が不可能なわけではありません。
別の Composition 実装を `Boundary` に載せれば、
その配下のノード全体を再構成可能にできます。

ただし現時点では、
reuse algorithm や redraw 戦略まで含めた実装コストが非常に大きく、
`StdComposition` だけでも十分な表現力とシンプルで高い性能を得られるため、
標準ではこれのみを提供しています。

これは最適化のためだけではありません。
読みやすさのためでもあります。

構造をむやみに組み替えなくてよいなら、
構造が固定されているほうが人間にも追いやすいからです。

### `Show()` は retained attach/detach として考える

`StdComposition` で条件付き表示を扱うとき、
`Show()` は「毎回 child を破棄して作り直す」仕組みではなく、
基本的には retained な child を attach / detach する仕組みとして考えます。

つまり、現在の `0.0.1` スコープでは次のように理解してください。

- `Show(false)` で child は active projection から外れる
- child の論理 identity は保持される
- `Show(true)` で同じ subtree が再 attach される
- subtree-local state は hide/show だけでは失われない

これは React 的な conditional render よりも、
SwiftUI / Compose の `if (show) { ... }` に近い感覚で使えます。
ただし Loka では、古い環境でのコストと native context の寿命をより強く意識します。

この retained attach/detach の考え方は、
`OpenFileDialog` のような native callback を持つ node でも重要です。
native callback が state を更新すると、その直後に compose / detach が進む可能性があります。
そのため、callback 側は「通知後も自分が生きている」と仮定してはいけません。

実装側の原則は次の通りです。

- native callback は通知前に必要な borrowed pointer を確保する
- state 更新や event emit の後に context-owned state を触らない
- presentation phase のような持続情報は小さい lifecycle state として持つ
- 1 パス限りの attach/compose 解釈は caller が手動で consume しない

この方針により、`Show()` / dialog / Flow が絡むケースでも、
「論理 UI の寿命」と「native callback の寿命」が混ざりにくくなります。

## 13. DSL と Composition の考え方

Loka の UI DSL は、宣言的にツリーを書くための表現です。

```cpp
c.declare(loka::app::VStack()
          << loka::app::Text("Hello")
          << loka::app::Button("OK"));
```

この見た目は React の JSX や SwiftUI の view builder、
Compose の composable 呼び出しに近い印象を与えます。
ただし、Loka の DSL は C++98 上で軽量に成立させるため、
より明示的でプリミティブです。

### DSL は「構造の宣言」であって「状態そのもの」ではない

Loka では、DSL は State を読む側です。
DSL 自体が主役ではありません。

たとえば、

- 何を表示するかは State が決める
- DSL はその State をどのノードに接続するかを書く
- イベントは State 更新へ戻る

という流れになります。

このため、DSL を見て迷ったら、
先に「この UI はどの State を読んでいるのか」を探すと理解しやすくなります。

### 局所変数よりチェインを優先する理由

Loka では、DSL-style chaining を基本にしています。
これは単なる見た目の好みではありません。

- ツリー構造がそのまま見える
- 一時オブジェクトの寿命を意識しやすい
- compose の意図が読み取りやすい

といった利点があるからです。

もちろん、読みやすさのために局所変数を使う場面はあります。
ただし原則としては、構造をそのままチェインで表すほうが Loka の流儀に合います。

### C++98 では局所変数が必ずしも読みやすさにつながらない

Loka でチェインを好む理由には、
単にツリー構造を見やすくするだけでなく、
C++98 の型記述コストを避けたいという実用上の事情もあります。

C++98 では `auto` が使えないため、
DSL の途中結果や Stream / Flow API の中間値を局所変数へ分けると、
長い型名を毎回明示しなければならない場面が出やすくなります。

特に Stream / Flow 系では、
中間表現の型が複雑になりやすく、
変数へ切り出すことでかえってノイズが増えることがあります。

そのため Loka では、
途中の型を露出させるよりも、
チェインのまま意味の流れを読める形を優先することがあります。

これは「局所変数を使ってはいけない」という意味ではありません。
実際には、次のように考えるのが自然です。

- 型を書かずに済むなら局所変数も有効
- 型ノイズが増えるならチェインのままのほうが読みやすい
- 特に C++98 では、その逆転が起こりやすい

### `.attr(...)` で属性を積む

Loka では、Node の見た目や振る舞いに関する追加情報を
`.attr(...)` で積んでいくことがあります。

```cpp
c.declare(loka::app::VStack()
          << loka::app::Text("Wrapped text")
                 .attr(loka::app::TextAttr()
                           .wrap(loka::app::TEXT_WRAP_WORD)
                           .truncation(loka::app::TEXT_TRUNCATION_NONE)));
```

`attr` は CSS のような外部スタイルシステムではなく、
宣言的な Node 定義の一部です。

また、attr は共通の巨大な 1 つの型ではなく、
`TextAttr`、`ImageViewAttr`、`MenuItemAttr` のように
view ごとに小さく分かれています。
これは古い環境でも扱いやすいサイズと明確さを保つためです。

### App / Window DSL

Loka の DSL は Scene の中だけでなく、
アプリケーション全体の構成にも使われます。

```cpp
virtual void compose(loka::app::AppComposition &c)
{
  c << loka::app::WindowDef(loka::app::WindowProps()
                                .frame(50, 50, 420, 300)
                                .title("LokaSample")
                                .visible(true)
                                .scene(loka::app::scene::NodeDefinition<MyProps, MyNode>()));
}
```

この形では、

- どの Window を持つか
- どの Scene を載せるか
- どのタイトルや表示状態にするか

を app-level の declarative composition として書けます。

### Menu DSL

メニューも同様に、Loka では declarative に構成できます。

```cpp
virtual void composeMenu(loka::app::MenuComposition &c)
{
  using namespace loka::app;

  c.declare(AppMenu()
            << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
            << MenuSeparator()
            << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));

  c.declare(Menu("File")
            << MenuItem("Open...").onClick(&this->openEvent_)
            << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
}
```

Menu DSL でも考え方は同じです。

- 論理構造を先に書く
- 必要なら event や attr を積む
- 最終的な反映は各 OS の menu system へ投影される

つまり Loka では、
Window、Scene、Menu を別々の imperative API として扱うのではなく、
どれも declarative composition の一部として捉えます。

## 14. イベントと更新

Loka では、イベントはしばしば `EmitterState` で表し、
イベントハンドラの中で State を更新します。

流れは単純です。

1. UI でイベントが起きる
2. 対応する処理が呼ばれる
3. 必要な State を変更する
4. 依存している UI が反映される

この形は React や Compose と近いですが、
Loka では更新規約がより明示的です。

### `deferBind` を基本に考える

UI まわりでは、イベント結線に `deferBind` を使うのが基本です。

これは、更新の順序や安定性を保ちやすくするためです。
対になるのは即時に処理を走らせる `bind` です。

`bind` は、その場でただちに再計算や通知を進めたいときに使います。
一方 `deferBind` は、処理を少し遅らせて現在の流れを抜けたあとに扱うため、
再入や更新順序の複雑化を避けやすくなります。

UI では、即時の反応よりも更新の安定性が重要な場面が多いため、
「すぐ再計算する必要がある特殊な場面」を除けば、
まず `deferBind` を基準に考えるとよいです。

### State 更新はトランザクションとして考える

Loka では、State 更新は単なる代入ではありません。
追跡と通知の文脈の中で扱われます。

特に `MutableState<T>::set()` は、
`StateTrackerGuard` の下で行う、という規約が重要です。

```cpp
void increment()
{
  loka::core::StateTrackerGuard guard(&this->tracker_);
  this->count_.set(this->count_.get() + 1, true);
}
```

これは少し明示的ですが、
そのぶん「いつ更新が起きたか」が読みやすくなります。

## 15. Props の役割

Props は、親から子へ渡す入力です。
Loka において Props はかなり重要です。

Props には次のようなものが入ります。

- 子が読む State
- 子が使うイベント
- 定数の設定値
- 子を構築するための参照

### Props は入力面を定義する

Props を見ると、その Node が

- 何を必要としているか
- どの程度親に依存しているか
- どこまでが外部入力か

が分かります。

この意味で Props は、単なるデータ入れではなく、
Node の public input surface です。

### 定数と State を分ける

Loka では、常に変わらない値まで何でも State に載せることは勧めません。

たとえば、

- 常に固定のラベル文字列
- 固定のメニュー項目
- 毎回変わらない設定値

は、素直に Props や Definition 側で持つほうが自然です。

本当に更新されるものだけを State にする。
この判断は、API も実行コストも整理します。

## 16. Loka で考えやすい設計パターン

### 共有される事実を親に置く

複数の子が同じ事実を読むなら、まず親に置きます。
子にコピーや独自キャッシュを持たせる前に、
本当に共有状態なのかを確認してください。

### 局所的な UI 状態は Boundary に閉じる

その部分だけの開閉状態や一時フラグは、
Boundary 側に閉じ込めたほうが追いやすくなります。

### 構造変化が必要かを先に疑う

何かを変えたいとき、
本当に Dynamic な構造変更が必要かを先に考えてください。

多くの場合、

- テキスト差し替え
- enabled 切り替え
- visible 切り替え

で足りることがあります。

### まず素直に書き、必要になってから絞る

Loka は低コストを重視しますが、
最初からすべてを細かく最適化する必要はありません。

ただし、最適化するときも原則は同じです。

- State を整理する
- ownership を整理する
- Boundary を見直す
- 再構成範囲を絞る

という順で考えると、設計が崩れにくくなります。

## 17. 他フレームワークとの比較

### React との違い

React と似ているのは、
State を中心に UI を考える点です。

大きく違うのは、

- GC 前提ではない
- 例外前提ではない
- ownership と寿命をより明示的に扱う
- 更新境界をより構造的に考える

という点です。

React よりも、実行モデルを近くで触る感覚があります。
また React が DOM 更新や副作用管理の文脈で理解されやすいのに対し、
Loka は論理 UI を構築し、その結果を各 OS のネイティブ環境へ投影するシステムとして捉えます。

### React Native との違い

React Native と Loka は、
どちらも論理的な UI をネイティブ環境へ投影するという点では近い発想を持っています。

ただし React Native は JavaScript runtime や bridge、
そして mobile-first な開発体験と強く結びついています。
一方 Loka は、より小さな runtime 前提で、
State、ownership、更新経路を明示的に扱いながら、
Classic Mac OS から modern desktop までを含む広い範囲へ
同じ composition model を投影しようとしています。

React Native が mobile-first だとすれば、
Loka は retro-first, modern-ready です。

Loka は現代的なモバイル runtime を基準にした設計ではなく、
68000 系 Macintosh の Toolbox のような制約環境を基準点にしています。
そのうえで、同じ state model と composition system を
modern macOS、Windows、ARM デバイスへ前方展開することを目指しています。

### Solid.js との違い

Solid.js と似ているのは、
State 依存を細かく意識するところです。

ただし Loka は、
より C++ 的で明示的です。
signal のような軽快さよりも、
寿命・所有・更新境界の管理を強く前に出します。

### Flutter との違い

Flutter と似ているのは、
宣言的な木を組み立てる感覚です。

一方で Loka は、
Widget システムや Dart ランタイムのような厚い土台を前提としません。
より小さく、より手触りのある構造です。

### SwiftUI / Jetpack Compose との違い

SwiftUI / Compose と似ているのは、
state-driven に body を組み立てる発想です。

違うのは、
Loka では暗黙の再実行やランタイム支援を減らし、
Boundary や State ownership をより明示的に考えることです。

また SwiftUI / Compose が各プラットフォームの宣言的 UI runtime と強く結びついているのに対し、
Loka はよりプラットフォーム中立な論理 UI を先に組み立て、
その結果を各 OS のネイティブ環境へ投影することを重視します。

そのため、少しだけ手間は増えますが、
「何が起きているか」は追いやすくなります。

## 18. Rust / React 経験者が気になりやすい ownership と共有

Rust や React に慣れている人は、
Loka の State ownership と共有について次のような疑問を持ちやすいはずです。

- State はどこまで共有してよいのか
- Boundary ローカル state を外へ渡してよいのか
- グローバル共有はどう表現するのか
- `Managed<T>` は Rust の `Arc<T>` に近いのか

ここでは、その感覚に寄せて整理します。

### まず結論

最初に結論だけ書くと、Loka では次のように考えると分かりやすくなります。

- Boundary ローカル state は、その Boundary に閉じる
- 親子共有したい事実は、親が持って Props で渡す
- Window / Scene をまたいで共有したいものは、より長寿命な owner が持つ
- 共有対象の寿命管理には `Managed<T>` が有効
- ただし `Managed<T>` が解決するのは主に共有対象の寿命であり、State ownership 全体ではない

### React 的に見ると

React の感覚に近づけると、

- 子だけの local state は、その Boundary の `declareStates()` / `useState()`
- 親子で共有する事実は、親へ持ち上げる
- 広域共有したい事実は、より上位の owner が持つ

という整理になります。

つまり Loka でも、基本は state lifting と同じです。
違うのは、Loka のほうが lifecycle と ownership をより物理的に意識することです。

React では GC やランタイムがかなり吸収してくれる部分もありますが、
Loka では C++ の生存期間と追跡可能性を自分で崩さない必要があります。

### Rust 的に見ると

Rust の感覚に近づけると、

- `Boundary` は local owner を持つ単位
- `BoundState<T>` はその owner に結びついた handle
- `Managed<T>` は共有対象の寿命を参照カウントで安定化する手段

と考えると近いです。

ただし、Loka は Rust の borrow checker を持ちません。
そのため、
「共有できるから共有する」のではなく、
「誰が owner で、どこまで共有するのが自然か」を設計で守る必要があります。

### `State<Managed<T>>` は何に向いているか

`State<Managed<T> >` は、共有対象を複数の場所から安全寄りに読むための有力な形です。

特に次のような用途に向いています。

- グローバルキャッシュを複数 Window から参照する
- 画像やモデルなどの共有リソースを複数 Scene で使う
- 長寿命なモデルを別 Window に渡す

この形の利点は、
共有対象 `T` の寿命を `Managed<T>` で吸収しつつ、
その参照自体を State として通知可能にできることです。

### ただし `Managed<T>` だけでは十分ではない

ここが重要です。

`Managed<T>` が守るのは、主に `T` 自体の寿命です。
それだけで次の問題が自動的に解決するわけではありません。

- その State は誰が owner なのか
- どの Window / Scene が更新責任を持つのか
- UI ローカルなものまで共有していないか
- State の寿命と利用者の寿命が一致しているか

たとえば、
ある Boundary ローカル state に `Managed<T>` を入れて、
それを別 Window にそのまま渡す設計は、
`T` の寿命は安定しても State ownership は不自然になりやすいです。

### 自然な共有の形

Loka で自然なのは、次のような構造です。

1. 長寿命 owner が `MutableState<Managed<T> >` を持つ
2. 各 Window / Scene はそれを `State<Managed<T> >*` として受け取る
3. UI はそれを読む
4. 更新は Main Thread で owner 側が行う

この構造だと、

- owner が明確
- 寿命の基準が明確
- 複数 Window から読める
- 共有対象 `T` の破棄タイミングも安定しやすい

という利点があります。

### 避けたい共有

次のような共有は注意が必要です。

- Boundary ローカル state をそのまま他 Window へ配る
- `Managed<T>` の中に Window 固有や Node 固有の文脈を入れる
- 一時的に作った owner 不明の State を広域共有する
- 共有モデルと UI ローカル状態を同じ器で持つ

こうした形は、
最初は動いても、破棄順や再構成が入った瞬間に読みづらく、壊れやすくなります。

### 実践ルール

迷ったときは、次のルールに寄せると安全です。

- ローカル state は `declareStates()` / `useState()` で作る
- 親子共有は `親 owner -> 子 reader` を基本にする
- Window / Scene をまたぐ共有は長寿命 owner に寄せる
- 共有対象の寿命安定化には `Managed<T>` を使う
- `BoundState<T>` は Boundary 外へ広域共有しない
- 更新は必ず Main Thread へ戻して行う

### まずは「局所」か「長寿命共有」かで考える

Loka では、State の置き場所に迷ったとき、
まず次の二択で考えると整理しやすくなります。

- その Boundary だけで完結するなら局所 state
- 複数の場所で共有したい、または lifecycle をまたいで保持したいなら長寿命 owner の shared state

この二択で、多くの基本的なユースケースはカバーできます。

逆に、寿命も ownership も曖昧な中間的 state は、
一見便利でも設計を不安定にしやすくなります。
迷ったときは、まず局所へ寄せるか、より上位の owner へ持ち上げるかを検討してください。

### 画像ビューアのようなケース

画像ビューアのようなケースでは、
最初は Window 内の Boundary ローカル state として画像の取得や表示を始めることがあります。

しかし後から、

- Window を閉じても保持しておきたい
- 次に開いたとき再利用したい
- 別 Window や別 Scene からも使いたい

といった要件が出ることがあります。

このとき、Window ローカルな state そのものを延命するのではなく、
共有価値のある画像データや decode 済みリソースだけを `Managed<T>` として
長寿命なストアへ昇格させるのが自然です。

Loka では、ライフサイクルをまたいで保持したいものは
Boundary ローカル state に留めず、
より長寿命な owner に明示的に持たせるほうが、
ownership と lifecycle の整合性を保ちやすくなります。

### 良い例: 長寿命 owner が共有 state を持つ

次のように、長寿命 owner が共有 state を持ち、
各 Window / Scene はそれを読むだけにする形は自然です。

```cpp
struct SharedImageModel
{
  loka::core::Managed<MyImage> image;
};

class AppSharedState
{
public:
  loka::core::MutableState<loka::core::Managed<SharedImageModel> > sharedImage_;
};

struct ViewerProps : public loka::app::scene::NodePropsBase<ViewerProps>
{
  typedef ViewerNode NodeType;
  typedef ViewerTypeTag TypeTag;

  loka::core::State<loka::core::Managed<SharedImageModel> > *sharedImage_;
};
```

この形なら、

- 共有 state の owner が明確
- 複数 Window から読める
- 共有対象の寿命も `Managed<T>` で安定しやすい

という利点があります。

### 良い例: ローカル state は Boundary に閉じる

一方、その Node だけが使う state は、
`declareStates()` や `useState()` で Boundary に属させるのが自然です。

```cpp
class SearchPanelNode : public loka::app::scene::StdCompositionNodeFor<SearchPanelNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<SearchPanelNode> PropsType;

  SearchPanelNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<SearchPanelNode>(p),
        query_()
  {
  }

  virtual void attachNode(loka::app::scene::NodeComposition &c)
  {
    c.declareStates().state(this->query_, loka::core::String::Literal(""));
  }

private:
  loka::app::scene::BoundState<loka::core::String> query_;
};
```

この形だと、
「この state はこの Boundary のローカル状態だ」とすぐ分かります。

### 悪い例: Boundary ローカル state を他 Window へ配る

次のように、ある Boundary のローカル state を
別 Window や別 Scene にそのまま渡すのは危険です。

```cpp
class PanelNode : public loka::app::scene::StdCompositionNodeFor<PanelNode>
{
public:
  virtual void attachNode(loka::app::scene::NodeComposition &c)
  {
    c.declareStates().state(this->localText_, loka::core::String::Literal("Hello"));
    GlobalRegistry::instance()->foreignText = this->localText_.state();
  }

private:
  loka::app::scene::BoundState<loka::core::String> localText_;
};
```

この形は、

- local state の owner が PanelNode の Boundary なのに
- 利用者がその寿命を超えて参照しやすい

という点で不自然です。

### 悪い例: owner 不明の一時 state を広く共有する

次のように、一時的に作った `MutableState<T>` を広く配るのも避けるべきです。

```cpp
void buildUi(SomeChildProps &props)
{
  loka::core::MutableState<int> tempCount(0);
  props.count_ = &tempCount;
}
```

これは典型的な lifetime bug です。
関数を抜けた時点で `tempCount` は消えるため、
受け取った側は dangling pointer を握ることになります。

## 19. まず最初に身につけたい感覚

Loka を読み書きするときは、常に次の順で考えると整理しやすくなります。

1. 何が事実として存在するか
2. その事実を誰が所有するか
3. 誰がそれを読むか
4. どこで更新範囲を切るか
5. 本当に構造変更が必要か

この順番を保てると、Loka はかなり素直に読めるようになります。

逆に、

- UI から先に考える
- とりあえず全部 MutableState にする
- ownership を曖昧にする
- Boundary を何となく増やす

という進め方をすると、設計が散らかりやすくなります。

## 20. 次に読むとよいテーマ

このガイドの次は、次のテーマに進むと理解が深まります。

- `declareStates()` と `BoundState<T>` の実践
- `Boundary` の attach / compose の流れ
- `StdComposition` と `Boundary` の使い分け
- `ObservedStateRegistrar` による観測
- `StateTrackerGuard` と更新規約
- `Props` と `Definition` の設計方針
- Scene / Projection transaction による platform 反映の考え方

## 21. Loka の責務の範囲

Loka は、論理的な UI と状態遷移を構築し、
その結果を各 OS のネイティブ環境へ投影することを主な責務とします。

このため、Loka は次のような領域を直接すべて内包することを前提としていません。

- 高度な動画編集コア
- 本格的な音声処理エンジン
- オフィススイート級の文書モデルやレイアウトコア
- ブラウザ級の DOM / document editing core

また、OS への投影を前提とする以上、
細かいテキスト編集挙動やネイティブ UI の微細な振る舞いは、
各 OS の差異や古い OS 側の制約を受けることがあります。

これは Loka の弱さというより、責務の境界です。
Loka は低レイヤーの専用コアそのものではなく、
それらの上に乗る宣言的 UI / application layer として考えるほうが自然です。

ただし、これは「できない」という意味ではありません。
Loka は C++ ベースであるため、
既存の C++ ライブラリや専用エンジンと組み合わせながら、
その上位の宣言的 UI とアプリケーション構造を構築することは十分可能です。

たとえば、

- 動画や音声の専用処理コアは別ライブラリで持つ
- 文書モデルや DOM 相当の編集コアは別層で持つ
- Loka はその状態や操作を受けて UI を構成し、OS へ投影する

という分担はかなり自然です。

## 22. モダン技術との連携

Loka はフレームワークコアとしては C++98 に留まる予定です。
これは過去の互換性に固執するためではなく、
古い Mac OS や Windows を含む幅広い環境で、
生産的な宣言的アプリケーション開発を支援するためです。

一方で、アプリケーション層やユースケース層、ドメイン層では、
現代的な C++ や Swift などの技術と自由に組み合わせることができます。
Loka はそうした外側の技術選択を妨げるものではなく、
安定した UI / application core として共存することを意図しています。

つまり、

- Loka コアは意図的に保守的
- アプリ側は必要に応じて現代的にできる
- 古い OS への対応と、外側の自由度は両立できる

という考え方です。

将来的には、同じ思想を別言語へ展開した派生系、
たとえば `Loka.rs` のような試みも面白いかもしれません。

また、Loka は 68000 系 Macintosh の Toolbox 環境のような厳しい制約下でも成立することを前提に設計されています。
そのため、iOS / iPadOS / Linux への展開は自然な方向であり、
PocketPC や PalmOS のような環境も、十分に挑戦しがいのある対象になりえます。

## まとめ

Loka は、宣言的 UI フレームワークです。
ただし、その本質は「見た目の DSL」ではなく、
State の所有と更新をどう整理するかにあります。

まず State を定義し、
その所有者を決め、
Boundary で更新範囲を切り、
DSL で UI を宣言する。

この順番で考えると、Loka の設計はかなり一貫して見えてきます。

特に React、Solid.js、Flutter、SwiftUI、Jetpack Compose に慣れている人ほど、
最初に「似ている部分」だけでなく、
「Loka は ownership と境界をより重く見る」という違いを掴むことが重要です。

それが分かると、Loka は難しいフレームワークというより、
かなり筋の通った、追跡しやすいフレームワークとして読めるようになります。
