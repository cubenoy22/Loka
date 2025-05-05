#include "Win32App.hpp"
#include "Win32Window.hpp" // Win32Window クラスの定義をインクルード
#include <windows.h>
#include <stdexcept> // for std::runtime_error
#include "core/App.hpp"

// コンストラクタ: App(nullptr) は仮。mainWindow_は削除したので初期化不要
Win32App::Win32App(HINSTANCE hInstance, int nCmdShow)
    : App(nullptr), hInstance_(hInstance), nCmdShow_(nCmdShow)
{
  // App基底クラスのwindowポインタをどう扱うかは要検討
  // 現状は nullptr のまま
}

// デストラクタ: override を追加
Win32App::~Win32App() /*override*/ // .cpp側にはoverride不要
{
  // windowsベクター内のウィンドウは App::windowClosed で削除されるか、
  // App のデストラクタで解放されるべき (App.hppに仮想デストラクタ追加済み)
}

// アプリケーション終了処理 (Appからoverride)
void Win32App::quit()
{
  PostQuitMessage(0); // Win32のメッセージループを終了させる
}

// ウィンドウが閉じたときの処理 (Appからoverride)
void Win32App::windowClosed(Window *window)
{
  // まず基底クラスの処理を呼ぶ (windowsベクターからの削除と終了判定)
  App::windowClosed(window);

  // Win32固有の後処理があればここに追加
}

void Win32App::run() /*override*/ // .cpp側にはoverride不要
{
  // 最初のウィンドウを作成
  Win32Window *mainWindow = new Win32Window(this, nullptr, "Developer Window");
  if (!mainWindow)
  {
    throw std::runtime_error("Failed to create main window object.");
  }

  // Appの管理リストに追加（明示的なキャストを追加）
  windows.push_back(static_cast<Window *>(mainWindow));
  // Appのprivateなwindowメンバーをどうするか？ (現状アクセス不可)
  // もしApp::windowを使いたいなら、protectedにするかセッターが必要

  // ウィンドウの実体を作成・表示 (visibility.set(true)経由が望ましいかも)
  mainWindow->visibility.set(true); // これで createNativeWindow が呼ばれるはず

  // mainWindow が有効か確認 (createNativeWindow内でhwnd_がセットされる)
  if (!mainWindow->hwnd())
  {
    // エラー処理: ウィンドウが作成できなかった
    // windowsリストから削除し、deleteする
    windows.pop_back();
    delete mainWindow;
    throw std::runtime_error("Failed to create native window handle.");
  }

  // Win32メッセージループ
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  // GetMessageが0を返したら (PostQuitMessageが呼ばれたら) ループを抜ける
  // run()メソッドが終了し、WinMainに戻る
}
