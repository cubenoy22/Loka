# attachNode / composeNode 責務分離メモ

## 背景

現在の Loka では、`attachNode()` と `composeNode()` の両方が
`NodeComposition` に近い文脈を扱えます。

そのため、現状の実装上は `composeNode()` の中で `declareStates()` を呼べてしまいます。
実際、ケースによってはそれでも動作します。

しかしこれは、長期的に安定した API とは言いにくいです。

理由は単純で、Loka では Boundary によって recompose の有無や頻度が異なるためです。
その結果、

- state の生成・登録
- lifecycle 上の初期化
- declarative UI の構築

が同じ場所に混ざりやすくなります。

暗黙の運用ルールとしては、

- `attachNode()` で state を生成・登録する
- `composeNode()` では state を読んで UI を組み立てる

のが正しい方向ですが、今は API 上それを十分に表現できていません。

## 問題

### 1. `composeNode()` に state 宣言を書けてしまう

`composeNode()` は declarative UI を組み立てる場所であるべきです。
しかし、`declareStates()` が見えていると、
state lifecycle が compose の実行条件に引きずられます。

これは特に、recompose の有無が Boundary ごとに異なる Loka では危険です。

### 2. 責務が型で分離されていない

本来は次のように分かれているべきです。

- `attachNode()`: lifecycle / state registration / setup
- `composeNode()`: declarative composition

しかし、同じような文脈を触れてしまうため、
ルールがドキュメント頼りになります。

### 3. 暗黙ルールが増える

現状は、

- `composeNode()` に `declareStates()` を書いても動く場合がある
- でも将来的な recompose を考えると書くべきではない

という状態です。

これは「使えるが使うべきではない API」が露出している状態であり、
利用者にも実装者にも分かりにくいです。

## 目標

API の責務を次のように分離したいです。

- `attachNode()` では state の生成・登録や bind/setup を行う
- `composeNode()` では declarative UI の構築だけを行う
- `composeNode()` からは `declareStates()` を見えなくする
- 可能なら compile-time に近い形で誤用を防ぐ

## 方向性

## 案 A: `composeNode()` に渡す型を細くする

もっとも自然な案は、
`composeNode()` の引数型を `NodeComposition` そのものではなく、
declarative 操作だけに絞った型へ分離することです。

仮称:

- `DeclarativeNodeComposition`

この型が持つもの:

- `declare()`
- `Show(...)`
- `conditional()`
- `group()`
- declarative tree 構築に必要な最小限の helper

この型が持たないもの:

- `declareStates()`
- state registration 系 API
- attach/setup 専用の lifecycle helper

イメージ:

```cpp
virtual void attachNode(AttachContext &ctx);
virtual void composeNode(DeclarativeNodeComposition &c);
```

この形にすると、
`composeNode()` で state を生やす余地を型で消せます。

## 案 B: `attachNode()` 側の文脈を明示的に分離する

もうひとつの案は、
`attachNode()` に渡す引数を専用コンテキストへ分けることです。

仮称:

- `AttachContext`

この型が持つもの:

- `declareStates()`
- `findBoundary()`
- bind/setup に関わる helper
- tracker/lifecycle 系アクセス

この案では、
`attachNode()` は state/lifecycle の場、
`composeNode()` は declarative compose の場、
という役割がより明確になります。

## 案 C: 現行 API のまま制約だけ強める

最小変更としては、

- `composeNode()` 内での `declareStates()` を debug assert で禁止する
- docs / examples を全面的に `attachNode()` ベースへ寄せる

という運用強化もあります。

ただしこれは、
型で責務を分ける案より弱いです。
Loka の方針としては、可能なら API で表現したほうがよいです。

## 推奨

基本方針としては、

1. `composeNode()` 側の型を declarative 専用に細くする
2. `attachNode()` 側に state / lifecycle 文脈を寄せる

の組み合わせが最もきれいです。

つまり理想形は、

```cpp
virtual void attachNode(AttachContext &ctx);
virtual void composeNode(DeclarativeNodeComposition &c);
```

です。

## 想定される利点

- `composeNode()` の責務が明確になる
- `declareStates()` の誤用を API レベルで防げる
- docs 上の暗黙ルールを減らせる
- recompose の有無に依存しない state lifecycle 設計になる
- examples / guide の説明もシンプルになる

## 想定される影響

- 既存 Node 実装のシグネチャ変更
- `composeNode()` 内で attach/lifecycle 系 helper を使っている箇所の整理
- `NodeComposition` に集約されていた API の再分類
- tests / examples / docs の追従

## 移行案

### 最小移行

- まず docs と examples を `attachNode()` での `declareStates()` に統一する
- `composeNode()` での `declareStates()` に debug warning / assert を入れる

### 中間移行

- `NodeComposition` を内部的に分割しつつ、互換 API を一時的に残す
- `composeNode()` から見える API を段階的に削る

### 理想移行

- `AttachContext`
- `DeclarativeNodeComposition`

へ完全に分離し、`composeNode()` では declarative API のみ許可する

## ドキュメント方針

この分離が入れば、Programming Guide では次のように簡潔に書けます。

- `attachNode()` は state の生成・登録と setup の場
- `composeNode()` は declarative UI を組み立てる場

つまり、
`composeNode()` に `declareStates()` を書いてよいかどうか、
という注意書き自体を減らせます。

## メモ

- Loka では Boundary によって recompose 特性が異なる
- だからこそ state lifecycle を compose 実行タイミングに結びつけるべきではない
- この変更は単なる style ではなく、API 責務を正しく表現するための分離
