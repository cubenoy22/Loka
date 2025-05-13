# Declara! プロジェクト概要（2025 年 5 月時点 最新仕様）

## 概要

Declara! は C++98 互換・型安全・明示的依存管理を重視した、現代的な宣言的 UI フレームワークです。

- **コア設計**：State/MutableState/DerivedState/Tracker による状態管理・依存伝播
- **UI 設計**：Scene/Component/PlatformContext で構成される宣言的 UI
- **クロスプラットフォーム**：Win32, Mac OS Classic (Toolbox/CFM/PPC) など複数環境に対応

---

## エラー検出・例外方針（assert 活用・例外原則禁止）

Declara! プロジェクトでは、**例外（throw/catch）によるエラー処理は原則禁止**とし、
「設計上ありえない」バグや契約違反の検出には assert などのアサーションを積極的に活用します。

- 明らかなプログラマのバグ（nullptr 禁止・不変条件違反など）は assert で即座に検出
- 外部入力や実行時エラーには戻り値やエラーコードで対応
- 例外は使わず、C++98/組み込み/クロスプラットフォーム環境でも安全な設計を徹底
- STL/Boost/Qt 等のメジャー C++ライブラリでも「recover 不能なバグは assert」方針が一般的

### 例

```cpp
explicit AppBuilder(PlatformContext *context)
    : context_(context)
{
    assert(context_ && "AppBuilder: PlatformContext* must not be nullptr");
}
```

---

## PlatformContext 必須・AppBuilder 設計方針（エラー検出ポリシー）

Declara! では、AppBuilder の生成時に PlatformContext\*が必須です。

- AppBuilder のコンストラクタは PlatformContext\*を必ず受け取り、nullptr は許容しません。
- 実装では `assert(context_ && "AppBuilder: PlatformContext* must not be nullptr");` で明示的にチェックします。
- これは「設計上ありえない」ミス（プログラマの契約違反）をデバッグ時に即座に検出するためです。
- 例外ではなく assert を使う理由：
  - 明らかなプログラマのバグは例外ではなく assert で止めるのが C++の慣習
  - リリースビルドでは assert は無効化され、パフォーマンスへの影響もありません
  - STL や Boost、Qt など多くのメジャー C++ライブラリでも同様の方針が採用されています

### 運用・設計上のポイント

- AppBuilder を生成する際は、必ず有効な PlatformContext\*を渡してください
- PlatformContext が null の場合は assert で即座に停止し、デバッグ時に原因を特定しやすくします
- 外部入力や実行時エラーには例外や戻り値で対応し、「設計上ありえない」条件には assert を使います

---

## ディレクトリ構成（主要部分）

```
project-root/
├── win32/           ... Windows (Win32 API) 実装
├── mac_ppc/         ... Mac OS Classic (Toolbox/CFM/PPC) 実装
├── common/          ... コアロジック・UIフレームワーク共通部
├── docs/            ... ドキュメント
└── agent/           ... AI/Agent連携・開発者プロファイル
```

---

## モジュール分割方針・common ディレクトリ構成

Declara! の共通ロジックは、用途ごとに下記のように整理しています。

- **DeclaraCore** ... App や State など宣言的 UI のコア機能
- **DeclaraApp** ... デスクトップ・モバイルアプリ向け UI 部品
- **DeclaraGame** ... 2D/3D ゲーム開発向け（現状は未実装）

`common/` 配下の構成:

```
common/
  core/   ... App, State, Scene, Tracker, Window などコア機能
  app/    ... Button, PlatformContext, Text, TextInput などアプリ向け部品
  game/   ... （将来追加予定）
  その他  ... 上記に該当しない共通ファイル
```

- モジュール分割は今後さらに進める予定です。
- 詳細な設計意図や API 例は docs/spec_declara.md も参照してください。

---

## コア API 設計（State/DerivedState/MutableState/Tracker）

- **State<T>**：値の保持のみ。不変。getter のみ。
- **MutableState<T>**：State<T>を継承。set()で値を変更可能。
- **DerivedState<T>**：複数の State/DerivedState に依存し、EvalFn で値を自動合成・再計算。
- **Tracker**：依存グラフ・伝播・副作用管理の司令塔。Scene ごとに独立して持つ。

---

## 開発・ビルド環境

- **Windows (Win32 API)**：Visual Studio 2022/2025、または VSCode + MSVC
- **Mac OS Classic (Toolbox/CFM/PPC)**：MPW (Macintosh Programmer's Workshop)
- **C++98 互換**：std::function, ラムダ等は未使用。関数ポインタ・ファンクタで柔軟に対応。

---

## 命名・設計ポリシー

- 各プラットフォームの文化・命名規則を尊重
- コア層は中立的設計、UI 層は文化に寄せた設計
- 抽象化は必要最小限、実用性と親しみやすさを重視
- **C++ 実装では、メンバ変数・メンバ関数の参照時は this-> を明示すること（可読性・一貫性のため）**

---

## C++98 における RAII・リソース管理方針（ScopedPtr の利用）

Declara! では C++98 環境においても delete 漏れや例外時のリソースリークを防ぐため、
独自の RAII スマートポインタ `ScopedPtr`（`common/core/util/ScopedPtr.hpp`）を標準で利用します。

- **手動 delete は禁止**し、リソース管理は必ずスコープベースで行う（NSAutoreleasePool 的思想）
- 例外安全・RAII 徹底のため、`ScopedPtr<T>` で動的リソースを管理
- スマートポインタ未使用時は、必ず設計上の理由を明記すること
- C++11 以降の`std::unique_ptr`と同等の使い方が可能

### 例

```cpp
#include "core/util/ScopedPtr.hpp"
// ...
ScopedPtr<App> app(platformContext.getApp(...));
app->run(); // スコープ終了時に自動delete
```

- 詳細は `common/core/util/ScopedPtr.hpp` を参照
- この方針により、C++98 でも delete 漏れ・例外時のリークを防止できます

---

## 参考

- 詳細な状態管理・依存伝播設計は `docs/architecture_state_tracker.md` を参照
- UI 設計思想・API 例は `docs/spec_declara.md` を参照

---

2025-05-05 改訂
