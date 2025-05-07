#include "Win32App.hpp"
#include "Win32Window.hpp" // Win32Window クラスの定義をインクルード
#include <windows.h>
#include <stdexcept> // for std::runtime_error
#include "core/App.hpp"

Win32App::Win32App(HINSTANCE hInstance, int nCmdShow)
    : App(), hInstance_(hInstance), nCmdShow_(nCmdShow)
{
  // App基底クラスのwindowポインタをどう扱うかは要検討
  // 現状は windows ベクターのみ利用
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

  // Trackerのbegin/endでState変更をトランザクション化
  if (mainWindow->getTracker())
  {
    mainWindow->getTracker()->begin();
    mainWindow->visibility.set(true); // これで createNativeWindow が呼ばれるはず
    mainWindow->getTracker()->end();
  }
  else
  {
    mainWindow->visibility.set(true);
  }

  // mainWindow が有効か確認 (createNativeWindow内でhwnd_がセットされる)
  if (!mainWindow->hwnd())
  {
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
