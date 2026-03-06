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
  2 回目はコンパイルエラー。`NodeDefinition<T>` → `NodeDefinition<T, AttrTag>` で型レベル強制。

2. 親コンテナ制約は「直近親」のみ適用
  祖先コンテナの制約は孫に伝播しない。

3. 継承はデフォルト無効
  必要な継承は `Custom*` コンテナ側で明示 opt-in 可能にする。

4. 値域不正は実装時に `debug assert`
  例: `padding(-1)`, `opacity(1.2)`, `fontSize(0)`。

5. Dynamic/Static の更新ルール
  - Static: 構築時に確定（三項演算子で条件分岐可能）。Dynamic bind は不要。
  - Dynamic: `State<T>*` で bind → 値変更時に再反映（dirty フラグでバッチ）
  - Dynamic attr のライフサイクルは Boundary が管理:
    Node は attr 内の `State<T>*` を検出し、最寄りの Boundary 経由で bind/unbind する。
    Boundary が dirty フラグ管理・再描画バッチを担う。
  - 各プロパティに `T` リテラルと `State<T>*` の両オーバーロードを用意

6. Attr データ方針
  - 既定 attr は小型 POD 前提
  - 重い表現は `Extended*Attr` / `Pro*Attr` の明示 opt-in で分離

7. Resolver: Boundary 単位注入、関数ポインタテーブル
  - Boundary ごとに `ResolverContext` を1つ保持
  - Node/Menu は描画時に現在 Boundary の resolver を参照
  - Boundary が無い、または resolver 未指定なら `DefaultAttrResolver`（パススルー）
  - 解決単位は Attr 型単位（`resolve(attr, context)` で丸ごと受け取り）
  - 局所差し替え可能、グローバル汚染なし、68k で低コスト

8. Resolver の適用順序（固定）
  `Node attr -> Container 補正 -> Platform 補正`

9. 未対応 Attr の扱い
  - debug: assert / log
  - release: 無視して継続

10. コンテナ-Attr 型整合チェックは `<<` 演算子側で行う
  各 Attr が scope tag を持ち、`Container::operator<<` 内で
  `IsAllowed<ContainerTag, AttrTag>` を静的チェック。

11. 同カテゴリ Attr の上書きは型単位
  プロパティ単位マージはしない。後から同型の attr を設定した場合は丸ごと置換。
  （v1 では attr 1回制約のため実質発生しない）

## v1 Scope

v1 では **要素固有 attr のみ** を対象とする。レイアウト attr はスコープ外。
既存のレイアウト系 API (`.size()` 等) は v1 では互換維持し、v1.1 以降で attr への移行計画を進める。

- 全既存要素に最小限の attr を適用（`Text`, `ImageView`, `Empty`, `MenuItem` 等）
- 要素 attr: `ImageAttr`, `TextAttr`, `MenuItemAttr` 等
- レイアウト attr (`VStackChildAttr`, `fillX`, `minHeight` 等): v1.1 以降
- 合成型 (`ImageViewAttr().layout().image()`): 不要（attr 1回 + 要素 attr のみ）
- MenuDSL: 同じ attr 基盤を共有（`disabled` 等の共通プロパティ）

## Resolver Architecture

```cpp
// 関数ポインタテーブル
typedef void (*AttrResolveFn)(void *attr, int attrTypeId, void *userData);

// Boundary が保持
struct ResolverContext {
  AttrResolveFn resolve;   // null = パススルー
  void *userData;
};

// v1 デフォルト: 何もしない
// 将来: ゲーム viewport / 設定パネル等で局所差し替え
```

## API Sketch (v1)

```cpp
// 要素固有 attr
VStack()
  << Text("Title").attr(TextAttr().fontSize(14).weight(TEXT_WEIGHT_BOLD))
  << ImageView()
       .image(&imageState)
       .attr(ImageAttr().fit(IMAGE_FIT_CONTAIN))

// Static: 構築時に条件分岐
Text("hi").attr(isLarge ? TextAttr().fontSize(18) : TextAttr().fontSize(12))

// Dynamic: State bind
Text("hi").attr(TextAttr().fontSize(&fontSizeState))

// Menu: 同じ基盤
MenuItem("Open...").attr(MenuItemAttr().disabled(&isProcessing))

// v1 ではレイアウトは既存 API を継続利用
ImageView().image(&imageState).size(0, 180);
```

## Type Safety Policy

- `NodeDefinition<T>` に `.attr()` すると `NodeDefinition<T, AttrTag>` を返す
- `<<` 演算子で `IsAllowed<ContainerTag, AttrTag>` を静的チェック
- 不適切な組み合わせはコンパイルエラー
- 例: `VStack` 文脈で `ZStackChildAttr` は不可（v1.1 以降で適用）

## Roadmap

| | v1 | v1.1+ |
|---|---|---|
| attr 回数 | 1回（型レベル強制） | 1回 |
| 型チェック | `NodeDefinition<T, AttrTag>` + `<<` で static | 同左 |
| 要素 attr | `ImageAttr`, `TextAttr` 等 | 同左 |
| レイアウト attr | なし | `VStackChildAttr` 等を追加 |
| `.size()` 等既存API | 互換維持 | attr へ段階移行 |
| Menu attr | `MenuItemAttr` 等 | 同左 |
| Resolver | `DefaultAttrResolver`（パススルー） | 局所差し替え活用 |
| 合成型 | 不要 | 必要になったら検討 |

## Open (v1 implementation detail)

- Resolver 関数ポインタテーブルの粒度: 1関数で `attrTypeId` 分岐 vs Attr 型ごとに個別スロット
- `MenuDSL` 側の最小 attr セット（font/shortcut emphasis/disabled style）の具体リスト
