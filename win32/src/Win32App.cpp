#include "Win32App.hpp"
#include "Win32Window.hpp" // Win32Window クラスの定義をインクルード
#include <windows.h>
#include <stdexcept> // for std::runtime_error
#include "core/App.hpp"

Win32App::Win32App(AppBuilder &builder, HINSTANCE hInstance, int nCmdShow)
    : App(composeComponents(builder)), hInstance_(hInstance), nCmdShow_(nCmdShow)
{
  // App基底クラスのwindowポインタをどう扱うかは要検討
  // 現状は group_ のみ利用
}

// デストラクタ: override を追加
Win32App::~Win32App()
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

void Win32App::run()
{
  App::run();

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
