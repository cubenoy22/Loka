# Layout/Attr Draft (v1)

## Goal

- NodeDSL / MenuDSL で共通に使える装飾・レイアウト指定を導入する
- 68k/Classic を考慮しつつ、将来拡張可能な最小仕様にする

## Core Direction

- API は `attributes` を中心にする
- `decorator/resolver` は内部実装として扱う
- CSS 的な広域解決は採用しない（直近コンテナ文脈を優先）
- `layout` と `attr` を分離する（`layout`: 親→子、`attr`: 自分自身）

## Layout Definition (v1 spec lock)

- `layout` は「親コンテナが子をどう配置するか」を記述する
- `attr` は「そのノード自身の見た目/振る舞い」を記述する
- `layout` は親ノード側 API で指定し、子ノード側には持たせない
- `layout` と `attr` は責務を混ぜない（同じ型に統合しない）
- 通常ポリシーはコンテナ（`VStack`/`ZStack`/`Grid` 等）が解釈する
- `Boundary` は通常ポリシーの差し替えポイントとして使う（既定実装の置換は局所化）

v1 ではレイアウト指定の実装は既存 API を継続利用し、`layout(...)` は仕様先行で定義のみ行う。

### Parent vs Child Priority (policy)

- 既定は「親制約が最終決定権」を持つ（子要求は親制約内でクランプ）
- 優先順位（同一軸の競合時）:
  1. 子の `FIXED`（明示サイズ）
  2. 親の最大/最小制約（超過は不可）
  3. 子の `INTRINSIC`（自然サイズ）
  4. 子の `FILL`（残余領域）
- 将来 `priority`（整数）を導入し、同カテゴリ競合の解決に使用する
  - ただし `priority` が高くても親制約は破らない

### Layout Override Strategy

- `attr` は宣言データのみ（`sizePolicy`, `priority` 等）
- 特殊レイアウト戦略は `layout(...).overrideOption(Functor)` 側で指定する
- 既定経路（高速・固定ルール）と、例外経路（Functor）を分離する
- `Boundary` では resolver 差し替えと同様に layout resolver 差し替えを許可する

### Layout Execution Direction (safety rule)

- レイアウト実行は親→子の一方向で固定する
- 子が親サイズを直接変更する逆流は許可しない
- `Boundary`/override でも同一パス内の逆流更新は許可しない
- 再レイアウト要求が必要な場合は次サイクルへ遅延し、同一パス再入を防止する

### v1.1 Layout Unification Plan

- v1.1 では `VStack`/`HStack`/`ZStack` 等の既存標準レイアウトを共通レイアウトフローに載せる
- 標準レイアウトは固定アルゴリズム（最適化対象）として維持する
- 任意レイアウトは `CustomLayout`（または等価ノード）で扱い、標準経路と責務分離する
- これにより自由度を確保しつつ、68k 向けの予測可能な実行コストを維持する

### Overflow Strategy (v1)

- v1 は親コンテナ側に `isClipped`（bool）のみを導入対象とする
  - `false`: はみ出し表示を許可
  - `true`: 親矩形でクリップ
- `isClipped` は `CommonAttr` で保持する（要素共通）
- 描画フェーズで clip を適用する
  - Toolbox: `ClipRect` / `SetClip`
  - Win/macOS: 各 backend の clip API へマップ
- `SCROLL` は v1 スコープ外（通常コンテナに持たせない）
- スクロールは `ScrollView` コンポーネント責務として実装する
- `LazyColumn`/`LazyRow` は `ScrollView + virtualization` 実装時に overflow 戦略を拡張する

ScrollView 仕様メモ（v1.1+）:

- `ScrollView` は特殊コンテナとして定義する
- スクロール軸方向は子へ `UNCONSTRAINED` 制約を渡す
- 反対軸は通常の親制約を維持する
- これにより `LazyColumn`/`LazyRow` 実装時の仮想化戦略と整合する

### Spacing Strategy (v1)

- `margin` は v1 では導入しない
- 余白は親コンテナ側の `padding` / `gap` で統一する
- 個別調整が必要な場合は `Spacer`（または等価ノード）で表現する
- これによりレイアウト責務を親に集約し、計算分岐と仕様複雑性を抑える

### Z-Order Strategy (v1)

- `ZStack` の既定重なり順は定義順（後ろに書いた要素が前面）
- `zIndex` は子 attr に保持し、`ZStack` 親が解釈して描画順を決定する
- 同一 `zIndex` の場合は定義順で安定ソートする
- `ZStack` 以外では `zIndex` は意味を持たない
  - v1: 無視（debug で警告/assert 推奨）
  - 将来: 可能なら静的制約へ拡張

### Hit Test Strategy (v1)

- hit test は Node レベルで扱う（`hitTest` / `handleEvent` を想定）
- v1 最小仕様:
  - `isHitTestVisible(bool)` を共通 attr として導入候補にする
  - `isHitTestVisible == false` のノードはヒット対象外
  - `isClipped == true` の親では、クリップ外座標は常にヒット対象外
- 伝播順:
  - 通常コンテナは後勝ち（後に配置された子を優先）
  - `ZStack` は前面から判定（`zIndex` + 定義順）
- `pointerEvents(NONE | ALL | SELF...)` は v1.1 以降で拡張する

### Visibility Strategy (v1)

- `GONE` は attr に持たせない。構造側（`if` / `ShowIf(cond) << Node`）で表現する
- attr 側は「表示だけ消す」用途に限定し、`HIDDEN` 相当を扱う
- 68k 最適化方針として、レイアウト時の可視判定分岐を減らすため、`GONE` は構造解決で確定させる
- 既存 `MenuItemAttr.visible(false)` はメニュー構築時のスキップとして扱い、実質的に構造的 `GONE` に相当

### Units Strategy (v1)

- v1 の単位は `px` のみを実装するが、API は `Length` 抽象を使用する
- `Length` は POD 値型として保持し、ヒープ確保や共有管理を行わない
- 推奨形:
  - `struct Length { short unit; int value; };`
  - `Length::px(int)` を提供
- 予約単位（v1 では未実装）:
  - `PT`, `PERCENT`, `EM`
  - 未実装単位は compile error か debug assert で拒否

### Px Constants (v1 helper)

- 頻出値は定数として提供する（C++98 互換）
  - `loka::util::Px::k0`, `k1`, `k2`, `k4`, `k8`, `k12`, `k16`, `k24`, `k32`
- `Px::4` のような識別子は C++ 上扱えないため `kN` 命名を採用する

### HiDPI and Scaling

- レイアウト計算は論理 px（DIP）として扱う
- 実ピクセル変換は各 platform backend が吸収する
  - Toolbox/68k: `scale = 1` 固定
  - Win/macOS: 将来 `scale != 1` を許容
- v1 は整数演算中心（68k 互換）で進め、必要なら将来 fixed-point へ拡張可能な API 形を維持する

### TLV Compatibility (v1.1+)

- `Length` を採用しても TLV 移行と矛盾しない
- TLV では `ATTR_TAG_WIDTH` 等のタグに対して payload として `{unit, value}` を格納する
- `ATTR_TAG_WIDTH_PX` / `ATTR_TAG_WIDTH_PT` のようなタグ分割は行わず、タグ爆発を回避する

### Attr Composition (v1.1+ candidate)

- v1 は「1ノード1attr」を維持する
- v1.1 以降で attr 合成を候補化する（例: `ImageViewAttr() + ZStackChildAttr()`）
- 合成結果は `AttrPack` として保持し、Resolver/Container は必要キーのみ参照する
- 同一キー競合は「右優先（last-wins）」を基本ルールとする
- `ZStack` は `AttrPack` から `zIndex` を取得し、他キーは無視できる

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
  - 既定 attr は小型 POD 前提（目安: `sizeof(default attr) <= 16-32 bytes`）
  - 長い文字列/複雑データなど重い payload は既定 attr に直接保持しない
  - 重い表現は `Extended*Attr` / `Pro*Attr` の明示 opt-in で分離

7. Attr 値カテゴリ（3種）
  - Immediate: リテラル値（`fontSize(14)`, `opacity(0.8)`, `disabled(true)`）
  - Reference: ポインタ/ID 参照（`fontName("Chicago")`, `imageHandle(asset)`）
  - Dynamic Binding: `State<T>*`（`fontSize(&fontSizeState)`, `visible(&visibleState)`）
  カテゴリは API 上で明示的に区別する。

7.1 ImageView size policy（v1 実装済み）
  - `IMAGE_VIEW_SIZE_AUTO`: 既定。`width/height` 指定優先、未指定は親幅+画像アスペクトで高さ算出
  - `IMAGE_VIEW_SIZE_FILL_PARENT`: `width/height` 未指定時は親領域を優先（高さは親高さがある場合に追従）
  - `IMAGE_VIEW_SIZE_INTRINSIC`: `width/height` 未指定時は画像の自然サイズを優先
  - `fit` は「描画の詰め方」、`sizePolicy` は「ビュー矩形の決め方」として分離する

7.2 Implicit defaults（v1）
  - attr 未指定時は要素デフォルト値を使用する
  - attr は「上書き指定のみ」を保持する（`hasX` フラグ付き）
  - 実効値は `effective = componentDefault + attrOverride` で解決する
  - デフォルト値は `Default*Attr`（または等価の resolver テーブル）で定義する
  - 初期デフォルト（v1 方針）:
    - `Text`: `sizePolicy = INTRINSIC`
    - `ImageView`: `sizePolicy = AUTO`, `fit = STRETCH`
    - `MenuItem`: `visible = true`, `disabled = false`

8. Resolver: Boundary 単位注入、関数ポインタテーブル
  - Boundary ごとに `ResolverContext` を1つ保持
  - Node/Menu は描画時に現在 Boundary の resolver を参照
  - Boundary が無い、または resolver 未指定なら `DefaultAttrResolver`（パススルー）
  - 解決単位は Attr 型単位（`resolve(attr, context)` で丸ごと受け取り）
  - v1 は `1関数 + attrTypeId` 分岐（switch-case）を標準とする
  - 局所差し替え可能、グローバル汚染なし、68k で低コスト

9. Resolver の適用順序（固定）
  `Node attr -> Container 補正 -> Platform 補正`
  順序はプラットフォーム間で安定でなければならない。

10. 未対応 Attr の扱い
  - debug: assert / log
  - release: 無視して継続

11. コンテナ-Attr 型整合チェックは `<<` 演算子側で行う
  各 Attr が scope tag を持ち、`Container::operator<<` 内で
  `IsAllowed<ContainerTag, AttrTag>` を静的チェック。

12. 同カテゴリ Attr の上書きは型単位
  プロパティ単位マージはしない。後から同型の attr を設定した場合は丸ごと置換。
  （v1 では attr 1回制約のため実質発生しない）

## v1 Scope

v1 では **要素固有 attr のみ** を対象とする。レイアウト attr はスコープ外。
既存のレイアウト系 API (`.size()` 等) は v1 では互換維持し、v1.1 以降で attr への移行計画を進める。

- 全既存要素に最小限の attr を適用（`Text`, `ImageView`, `Empty`, `MenuItem` 等）
- 要素 attr: `ImageViewAttr`, `TextAttr`, `MenuItemAttr` 等
- 子の sizing 要求（`fillX`, `minHeight` 等）は要素 attr 側に段階導入（v1.1 以降）
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

v1 推奨実装:

- `attrTypeId` を `switch` で分岐して必要な Attr だけ処理する
- 型ごとの個別関数スロット配列は v1 では採用しない（構造体肥大化を避ける）

## Common/Control Attr Layering (spec note)

- v1 では共通プロパティを `CommonAttr` として扱う想定を持つ
  - 例: `disabled`, `visible`, `opacity`
- 入力系共通プロパティは `ControlAttr` 層として拡張可能にする
  - 例: `focusable`, `tabIndex`, control-state style hooks
- 各要素 attr はこれらを内包して保持する（多重継承ではなく has-a を推奨）
  - 例: `TextAttr { CommonAttr common; ControlAttr control; ... }`
- Resolver は `CommonAttr` / `ControlAttr` を先に適用し、その後に要素固有 attr を適用する
- 未対応要素では該当共通項目を no-op 扱い（debug では警告/assert）

## Attr Types as Typed Façades

`TextAttr`, `ImageViewAttr`, `MenuItemAttr` 等は typed façade として位置づける。

役割:
1. 型安全性（コンパイル時に不正な組み合わせを検出）
2. 許可タグの名前空間（要素ごとに設定可能なプロパティを制限）
3. Builder API（`TextAttr().fontSize(14).weight(BOLD)` のチェーン記法）
4. Getter API（v1: 直接メンバアクセス、v1.1: 非 hydrate getter）

v1 では固定 POD struct がそのまま storage。
v1.1 で TLV 導入時に façade + 共有 TLV backend に分離する。

## API Sketch (v1)

```cpp
// 要素固有 attr
VStack()
  << Text("Title").attr(TextAttr().fontSize(14).weight(TEXT_WEIGHT_BOLD))
  << ImageView()
       .image(&imageState)
       .attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN))

// Static: 構築時に条件分岐
Text("hi").attr(isLarge ? TextAttr().fontSize(18) : TextAttr().fontSize(12))

// Dynamic: State bind
Text("hi").attr(TextAttr().fontSize(&fontSizeState))

// Menu: 同じ基盤
MenuItem("Open...").attr(MenuItemAttr().disabled(&isProcessing))
MenuItem("Debug...").attr(MenuItemAttr().visible(false))

// v1 ではレイアウトは既存 API を継続利用
ImageView().image(&imageState).size(0, 180);

// 将来の layout 記法（仕様先行）
VStack()
  .layout(VStackLayout().padding(8).gap(6))
  << Text("A").attr(TextAttr().weight(TEXT_WEIGHT_BOLD))
  << ImageView().image(&imageState).attr(ImageViewAttr().sizePolicy(IMAGE_VIEW_SIZE_FILL_PARENT).fit(IMAGE_FIT_CONTAIN));
```

## Type Safety Policy

- `Text`/`ImageView`/`MenuItem` は `.attr()` 呼び出し後に `*WithAttr` 型を返す（2回目 `.attr()` は型上で不可）
- v1 は要素固有 attr のみを対象にし、コンテナ依存 attr の静的チェックは v1.1 以降で導入
- 不適切な組み合わせはコンパイルエラー（または v1 では未提供 API により記述不能）

## Roadmap

| | v1 | v1.1+ |
|---|---|---|
| attr 回数 | 1回（型レベル強制） | 1回 |
| 型チェック | `*WithAttr` への型遷移で attr 1回制約 | コンテナ制約を静的チェックへ拡張 |
| 要素 attr | `ImageViewAttr`, `TextAttr` 等 | 同左 |
| 子 sizing 要求 | なし | 要素 attr 側に追加（`fillX`, `minHeight` 等） |
| `.size()` 等既存API | 互換維持 | attr へ段階移行 |
| `layout(...)` | 仕様のみ定義（実装なし） | 実装導入 |
| Menu attr | `MenuItemAttr` 等 | 同左 |
| Resolver | `DefaultAttrResolver`（パススルー） | 局所差し替え活用 |
| 合成型 | 不要 | 必要になったら検討 |

## Open (v1 implementation detail)

- `MenuDSL` 側の最小 attr セット（font/shortcut emphasis/disabled style）の具体リスト

## Internal Storage Direction (v1.1 candidate)

v1 API は維持しつつ、内部表現は以下を候補とする。

- 既定は可読性優先の TLV 方式（`enum tag + value`）
- bit レベル圧縮は行わない（保守性優先）
- `SmallBuffer`（例: 16 bytes）に優先格納
- オーバーフロー時は Arena チャンク連結で拡張
- decode/hydrate は必要な項目のみ行う

導入タイミング:

- v1.0: 固定 POD 内部表現で API/挙動を先に安定化
- v1.1: API 互換を維持したまま TLV 内部表現へ移行

重い値の扱い:

- `fontName` 等の可変長データは既定 TLV に埋め込まない
- ポインタ参照または外部テーブル ID 参照で扱う

安全ガード:

- TLV version/type id を保持
- 2-byte alignment 規約を維持
- debug で TLV 検証（範囲外/不正列の assert）

Hydrate/適用方針:

- v1.1 では「Resolver が TLV をパースしながら直接適用」を標準とする
- 毎フレームの恒久 hydrate キャッシュは持たない（メモリ優先）
- 高頻度ノードのみ将来最適化として一時キャッシュを検討

変更判定方針:

- まず TLV バイト列同一性（length + memcmp）で高速判定
- 参照系（`State<T>*` / `fontName*`）は既定でポインタ同一性比較
- 値比較が必要なタグのみ、タグ単位で明示的に比較戦略を追加

Global AttrTag Namespace（v1.1）:

- TLV 導入時にプロパティ単位のグローバル一意タグを定義
- 例: `ATTR_TAG_COMMON_VISIBLE`, `ATTR_TAG_TEXT_FONT_SIZE`, `ATTR_TAG_IMAGE_FIT`
- type id と tag id を混同しない（v1 の `AttrTypeId` は型単位、v1.1 のタグはプロパティ単位）
- Resolver は `switch(tag)` で直接ディスパッチ可能

Attr Freeze After Attach（v1.1）:

- v1 は固定 POD + 値コピーなので freeze 不要（alias 安全）
- v1.1 で TLV バッファをポインタ共有する場合、Builder → `.attr()` → `freezeAttr()` で immutable 化
- Frozen attr は copy-safe を保証

Attr Equality（v1.1）:

- v1: POD `operator==` / `memcmp` 相当で十分
- v1.1: TLV length + memcmp による高速判定
- `State<T>*` 含有時はポインタ同一性比較

Getter API 最適化（v1.1）:

- v1: 直接メンバアクセス（`attr.fontSize_`）
- v1.1: 非 hydrate getter（`hasFontSize()`, `fontSizeValueOr(fallback)`, `fontSizeState()`）
- TLV 全体をパースせず、必要なタグのみ読み取る
