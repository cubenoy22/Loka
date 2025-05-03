# Windows (Win32 API) テンプレート

初期ビルド用の空 main.c を配置しています。

---

## 🛠️ Ninja + VSCode でのビルド・実行手順（推奨）

### 1. VSCode の起動

- Windows on ARM で x64 バイナリをビルドしたい場合は、
  - スタートメニューで「x64 Native Tools for VS 2022」を検索し、起動します。
  - そのコマンドプロンプトで `code .` を実行して VSCode を開きます。
- ARM64 バイナリをビルドしたい場合は「ARM64 Native Tools」で同様に起動します。
- どちらも Ninja generator でホストアーキテクチャのバイナリが生成されます。

### 2. ビルド用ディレクトリ作成

```sh
mkdir -p win32/build/native/Debug
cd win32/build/native/Debug
```

### 3. CMake 実行（Ninja generator）

```sh
cmake -G "Ninja" ../../../
```

### 4. ビルド

```sh
cmake --build .
```

### 5. 実行

```sh
./Declara.exe
```

---

## 注意

- Visual Studio generator や MinGW Makefiles のコマンド例は不要になったため削除しています。
- CMake のプリセットや `-A` オプションではホストアーキテクチャは切り替わりません。
- どのアーキテクチャでビルドされるかは VSCode の起動元（Native Tools）で決まります。
