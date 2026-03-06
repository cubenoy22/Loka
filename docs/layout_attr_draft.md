# Layout/Attr Draft (v1)

## Goal

- NodeDSL / MenuDSL で共通に使える装飾・レイアウト指定を導入する
- 68k/Classic を考慮しつつ、将来拡張可能な最小仕様にする

## Core Direction

- API は `attributes` を中心にする
- `decorator/resolver` は内部実装として扱う
- CSS 的な広域解決は採用しない（直近コンテナ文脈を優先）

## Confirmed Rules

1. `.attr(...)` は 1 ノードにつき 1 回のみ  
  2 回目はコンパイルエラー（もしくは型レベルで不可能にする）。

2. 親コンテナ制約は「直近親」のみ適用  
  祖先コンテナの制約は孫に伝播しない。

3. 継承はデフォルト無効  
  必要な継承は `Custom*` コンテナ側で明示 opt-in 可能にする。

4. 値域不正は実装時に `debug assert`  
  例: `padding(-1)`, `opacity(1.2)`, `fontSize(0)`。

5. Dynamic/Static の更新ルール  
  - Static: bind 先で直接反映（layout/paint を必要最小で実行）  
  - Dynamic: dirty フラグでバッチ反映（layout dirty / paint dirty 分離）

6. Attr データ方針  
  - 既定 attr は小型 POD 前提  
  - 重い表現は `Extended*Attr` / `Pro*Attr` の明示 opt-in で分離

7. Resolver 層を用意  
  - v1 実装は `DefaultAttrResolver` 1つのみ  
  - 差し替え拡張ポイントは公開（将来のゲーム/動画向け）

8. Resolver の適用順序（固定）  
  `Node attr -> Container 補正 -> Platform 補正`

9. 未対応 Attr の扱い  
  - debug: assert / log  
  - release: 無視して継続

## API Sketch (v1)

```cpp
// NodeDSL
VStack()
  << Text("Title").attr(TextAttr().fontSize(14).weight(TEXT_WEIGHT_BOLD))
  << ImageView()
       .image(&imageState)
       .attr(VStackAttr().fillX().minHeight(120))
       .attr(ImageAttr().fit(IMAGE_FIT_CONTAIN)); // NG in v1: attr is once
```

v1 では `attr` 1回制約のため、実際は以下のように 1つにまとめる:

```cpp
ImageView()
  .image(&imageState)
  .attr(ImageViewAttr()
    .layout(VStackAttr().fillX().minHeight(120))
    .image(ImageAttr().fit(IMAGE_FIT_CONTAIN)));
```

## Type Safety Policy

- コンテナごとに許可 AttrType を定義する
- 不適切な AttrType はコンパイルエラーにする
- 例: `VStack` 文脈で `ZStackAttr().zIndex(...)` は不可

## Open (v1 implementation detail)

- `ImageViewAttr` の合成 API をどう定義するか
- `MenuDSL` 側の最小 attr セット（font/shortcut emphasis/disabled style）をどこまで含めるか

