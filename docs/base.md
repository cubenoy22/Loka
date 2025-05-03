# プロジェクト概要

本プロジェクトは、C 言語により
**Windows (Win32 API)** および **Mac OS Classic (Macintosh Toolbox with C)** 向けに、
**ビルド可能な最小テンプレート構成**を作成することを目的とする。

---

## 開発環境

| ターゲット                           | 言語   | 開発環境                                                        | 備考                                                          |
| :----------------------------------- | :----- | :-------------------------------------------------------------- | :------------------------------------------------------------ |
| Windows (Win32 API)                  | C 言語 | Visual Studio 2022/2025 推奨<br>または VSCode + MSVC 構成も可能 | 空の Win32 プロジェクト作成、または tasks.json 経由ビルド設定 |
| Mac OS Classic (Toolbox / CFM / PPC) | C 言語 | MPW (Macintosh Programmer's Workshop)                           | PPC ターゲット、InterfaceLib リンク必須                       |

---

## ディレクトリ構成（初期）

```
project-root/
├── win32/                     ← Windows版（Win32 API使用）
│   ├── src/
│   │   └── main.c
│   ├── build/
│   └── README.md
├── mac_ppc/                   ← Mac OS Classic Toolbox版（PPC/CFM）
│   ├── src/
│   │   └── main.c
│   ├── build/
│   └── README.md
├── common/                    ← 将来的に共通で使えるコード置き場（初期は空でも可）
├── docs/                      ← ドキュメント、設定ファイル置き場
│   └── settings.md
├── agent/                     ← Agent用の設定や人格定義、補助ドキュメント置き場
│   └── （任意の設定ファイル群）
└── README.md                  ← プロジェクト全体の概要
```

※ `.vscode/settings.json` にて適切なエージェント関連パスやビルドタスクを指定済み。

---

## 基本要件

- 各ターゲットごとにビルド可能な`main.c`を配置する
- "Developer"という文字列をウィンドウ上に表示できる最小限のコードとする
- 再描画イベント（`WM_PAINT` / `updateEvt`）に対応し、復元後も文字が表示される
- すべて C 言語で記述する（C++は使用しない）
- 具体的なアプリ仕様（機能追加）はこの時点では行わない

---

## ビルド・実行方法（概略）

### Windows 版（Win32）

1. Visual Studio で「空の Win32 プロジェクト」を作成する
2. `win32/src/main.c`をプロジェクトに追加する
3. ビルド設定を「Windows アプリケーション」に変更する
4. ビルド・実行してウィンドウを確認する

※ VSCode を使用する場合は、Visual Studio Build Tools（MSVC）をインストールし、
tasks.json を用意してビルドタスクを設定することでも可能。

### Mac PPC 版（Classic Toolbox）

1. MPW 上で Makefile または手動で以下を実行
2. `C main.c.o`
3. `Link main.o.o -o MyAppName -d InterfaceLib`
4. 実行ファイルを起動する

---

## ドキュメント・管理

- `docs/settings.md` にプロジェクト管理情報を記載
- `agent/` にエージェント用設定、人格ファイルなどをまとめる
- 各ターゲットごとのビルド手順や留意点も将来的にまとめる

---

## C++/共通コードにおける API 設計・命名規則

- C++/共通コードでは、C++98 互換性を意識しつつも、現代的・表現的な API 設計を優先します。

---

## 備考

- コードは可読性・メンテナンス性を意識して記述する
- プラットフォーム間共通処理は、`common/`ディレクトリに集約する方針とする
- 後から具体的な機能要件（アプリ仕様）は別途定義する
