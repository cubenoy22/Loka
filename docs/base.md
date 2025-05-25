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

## AttachScope / SceneNode 自動アタッチ機能

Scene/Component 実装時の `AttachScope` 関数と自動アタッチ機能の使い方:

```cpp
void compose(SceneNodeGroup &group)
{
  // 最上位スコープ開始 - このスコープでのNode/NodeAsは自動的にgroupに追加される
  AttachScope(&group);

  // ボタンを生成して親コンテナ(group)に自動アタッチ
  SceneNodeButton *btn = NodeAs(ButtonDefinition(ButtonProps().setText("Increment")));

  // ロジックノードを生成して親コンテナ(group)に自動アタッチ
  Node(IncrementNodeDefinition(IncrementNodeProps(&count, &btn->clickEvent, &tracker)));

  // レイアウトコンテナを生成して親コンテナ(group)に自動アタッチ
  LayoutSceneNode *layout = NodeAs(LayoutSceneNodeDefinition(LayoutSceneNodeProps()));

  {
    // ネストスコープ開始 - このブロック内のNode/NodeAsは自動的にlayoutに追加される
    AttachScope(layout);

    // テキストノードを生成 (autoAttach=falseで自動アタッチを無効化)
    SceneNodeText *text = NodeAs(TextDefinition(TextProps().setText(countStr)), false);

    // 明示的に追加
    layout->addChild(text);
    layout->addChild(btn);

    // 明示的にネストスコープを閉じる
    EndScope(); // またはブロックを抜けると自動的に閉じられる
  }
}
```

- **スコープ管理方法**:
  1. 関数型スタイル: `AttachScope(container)` で開始、`EndScope()` または関数終了で終了
  2. RAII 型スタイル: `ScopedAttach scope(type, ptr)` 変数宣言でスコープ管理
- **テスト/デバッグ**:
  ```
  win32/build/native/Debug/Declara.exe --test
  ```

_注意事項: `AttachScope` 使用時は、ネストしたブロックスコープでもう一つの `AttachScope` を使う場合、
ネストブロックの終わりで明示的に `EndScope()` を呼ぶか、RAII 型 `ScopedAttach` を使用してください。_

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

## C++ 記述・移植性ルール

- **基本方針として C++98 で記述すること**
  - ただし、設計イメージの提示や試作・議論のために、より分かりやすい表現が必要な場合は C++11 以降のモダンなシンタックスを用いてもよい（その場合は明記すること）。
- **移植性が最優先**
  - できるだけ「方言」や特定コンパイラ依存の拡張を避け、多くの C++コンパイラでビルドが通ることを意識する。
- **想定コンパイラ**
  - MSVC（Visual Studio 系）
  - GCC
  - MPW（Macintosh Programmer's Workshop）
  - LLVM/Clang

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

## セグメンテーションフォルト（セグフォ）発生時の人間向けデバッグ手順

C++98/移植性重視プロジェクトでセグメンテーションフォルト（Segmentation Fault, SIGSEGV）が発生した場合、
以下の手順で原因特定・修正を行うことを推奨します。

### 1. 再現条件の特定

- どのテスト・操作でセグフォが発生するかを明確にする
- 直前のコード変更やリファクタリング範囲も記録

### 2. デバッガによる原因箇所の特定

- **gdb/lldb（Unix 系）** または **Visual Studio（Windows）** でデバッグ実行
- コマンド例（gdb）:
  ```bash
  gdb --args ./your_test_binary [args]
  run
  # セグフォ発生後
  bt   # バックトレース（コールスタック）表示
  frame N  # N番目のフレームでローカル変数・thisポインタ等を確認
  list  # 該当行のソース表示
  print var  # 変数の値を確認
  ```
- **Visual Studio** では「デバッグの開始」→「例外発生時に中断」でコールスタック・変数を確認

### 3. 典型的な原因パターン

- **delete 後のポインタ再利用（use-after-free）**
- **二重 delete（double free）**
- **未初期化ポインタの参照**
- **配列/ベクタ等の範囲外アクセス**
- **所有権・ライフサイクル設計ミス**

### 4. 修正・再発防止策

- 問題箇所の所有権・ライフサイクル設計を見直す
- スマートポインタ（ScopedPtr など）や明示的な clear()/reset() の活用
- テストごとのリソース初期化・解放を徹底し、他テストへの影響を遮断
- コメント・設計ドキュメントで所有権ルールを明示

### 5. 補助ツール

- **Valgrind（Linux）**: メモリリーク・不正アクセス検出
- **AddressSanitizer（対応コンパイラ）**: 実行時のメモリエラー検出

---

## デバッグ用ログ出力のルール

- ターミナル用のデバッグ出力（printf など）は、必ず下記のように `#ifdef TEST_BUILD` ～ `#endif` で囲むこと。
  - 本番ビルドで不要な出力やパフォーマンス劣化を防ぐため。

```cpp
#ifdef TEST_BUILD
printf("[debug] ...");
#endif
```

- このルールは StateTracker などコア層のデバッグにも必須です。
- 既存コードもこの方針に従って管理されています。

---

## git コマンド実行時のエディタ・ページャ回避ルール

- git log, git show, git diff などで vi などのエディタやページャ（less 等）が自動起動しないよう、
  必ず `--no-pager` や `--no-edit` などのオプションを付与すること。
  - 例: `git --no-pager log` / `git commit --no-edit ...`
- これにより、AI や自動化ツールがログを直接取得しやすくなり、ユーザーも余計な :q 操作をせずに済む。
- 明示的な理由がない限り、ページャ・エディタ起動を伴うコマンドは避ける。

---

## 参考

- 詳細な状態管理・依存伝播設計は `docs/architecture_state_tracker.md` を参照
- UI 設計思想・API 例は `docs/spec_declara.md` を参照

---

2025-05-05 改訂
