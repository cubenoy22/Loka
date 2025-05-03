プロジェクトの仕様に関しては docs ディレクトリにある markdown を参照してください。

## mac_ppc ディレクトリにおける UI 実装方針

- **Toolbox/CFM/PPC:**
  - mac_ppc ディレクトリ配下では、Carbon ではなく Control Manager/Toolbox API（CFM/PPC 対応）を使用すること。
  - Win32 版と設計思想を揃えつつ、Mac OS Classic の文化・API 流儀に従う。
  - Carbon や Cocoa は使わず、PPC/CFM/Toolbox の互換性を最優先とする。
- **Carbon:**
  - 将来的に Carbon アプリを作成する場合は、別ディレクトリまたは明示的な区分を設けて管理すること。
  - mac_ppc ディレクトリ内では Carbon API の利用は原則禁止。

## Windows 環境でのシェル利用方針（Agent/開発者向け）

- Windows でのコマンド実行・自動化・エージェント操作時は、原則として Git Bash（または WSL/Bash）をデフォルトシェルとして利用すること。
  - rm, cp, find など Unix 系コマンドを前提とした手順・スクリプトを記述してよい。
  - VSCode でのデフォルトターミナルも「Git Bash」に設定推奨。
- PowerShell/CMD でしか動かない特殊な処理が必要な場合は、明示的にその旨を記載すること。
- エージェントによる自動コマンド実行も、可能な限り Bash 互換シェルを優先する。
