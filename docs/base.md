# Declara! プロジェクト概要（2025 年 5 月時点 最新仕様）

## 概要

Declara! は C++98 互換・型安全・明示的依存管理を重視した、現代的な宣言的 UI フレームワークです。

- **コア設計**：State/MutableState/DerivedState/Tracker による状態管理・依存伝播
- **UI 設計**：Scene/Component/Renderer で構成される宣言的 UI
- **クロスプラットフォーム**：Win32, Mac OS Classic (Toolbox/CFM/PPC) など複数環境に対応

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
  app/    ... Button, Renderer, Text, TextInput などアプリ向け部品
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

## 参考

- 詳細な状態管理・依存伝播設計は `docs/architecture_state_tracker.md` を参照
- UI 設計思想・API 例は `docs/spec_declara.md` を参照

---

2025-05-05 改訂
