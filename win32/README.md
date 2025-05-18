# VSCode での Win32 ビルド・実行運用ガイド

このドキュメントでは、Declara プロジェクトの Win32 版における VSCode でのビルド・実行手順と、GUI/コンソール切り替え運用についてまとめます。

## 概要

- CMake のサブシステム切り替え（GUI/Console）は、VSCode のタスクと launch.json の構成切り替えで実現します。
- tasks.json の「Config & Build」タスクを使うことで、CMake の Configure と Build を一発で実行できます。
- launch.json のデバッグ構成を切り替えることで、GUI/Console アプリの実行を簡単に切り替えられます。

---

## 1. ビルド・実行の基本フロー

1. **Ctrl+Shift+B** で「Config & Build (Native-Debug-GUI)」または「Config & Build (Native-Debug-Console)」を選択
   - 必要に応じてタスク一覧でピン留めしておくと便利です
2. ビルドが完了したら、launch.json のデバッグ構成（GUI/Console）を選択して実行

### 補足

- サブシステム（GUI/Console）を切り替えた場合は、必ず「Config & Build」タスクを再実行してください
- 同じ構成での開発中は、Build のみでインクリメンタルビルドが可能です

---

## 2. タスク構成（tasks.json）

- `Config & Build (Native-Debug-GUI)`：GUI サブシステム用の CMake 構成＋ビルド
- `Config & Build (Native-Debug-Console)`：Console サブシステム用の CMake 構成＋ビルド
- 単体の Configure/Build タスクはメニューに表示されますが、通常は Config & Build のみ使えば OK です

---

## 3. デバッグ構成（launch.json）

- `Debug Declara (WinMain)`：GUI サブシステム用の実行構成
- `Debug Declara (main)`：Console サブシステム用の実行構成
- それぞれ preLaunchTask で対応する Config & Build タスクを呼び出します

---

## 4. タスクのピン留め方法

1. `Ctrl+Shift+P` で「タスク: タスクの実行」を開く
2. 「Config & Build (Native-Debug-GUI)」や「Config & Build (Native-Debug-Console)」の右端にあるピンアイコンをクリック
3. 以降は Ctrl+Shift+B やタスク実行時に上部に表示されます

---

## 5. 注意点・Tips

- サブシステム切り替え時は必ず Config & Build タスクから再ビルドしてください（CMake のキャッシュが残るため）
- **テストやコード修正時は、必ず `cmake --build win32/build/native/Debug` でビルドし、`win32/build/native/Debug/Declara.exe` でテスト実行して動作確認してください**
  - 修正の妥当性・テスト通過を毎回確認するため、手動ビルド＆テスト実行を徹底してください
- ビルドエラー時は `cmake --build win32/build/native/Debug` で手動ビルド・エラー確認も可能
- タスクや launch.json の構成はプロジェクトの運用方針に合わせて随時見直してください

---

この運用により、Win32 アプリの GUI/Console 切り替えやビルド・デバッグが快適に行えます。

何か運用上の課題や改善案があれば、docs/ 以下に追記してください。
