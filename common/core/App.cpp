#include "App.hpp"
#include "Window.hpp"
#include <algorithm> // std::remove用

App::App(Window *w) : mainWindow_(w)
{
  if (mainWindow_)
  {
    windows.push_back(mainWindow_);
  }
}

// App::windowClosed の実装
void App::windowClosed(Window *window)
{
  // windowsベクターから閉じたウィンドウを削除
  auto it = std::remove(windows.begin(), windows.end(), window);
  if (it != windows.end())
  {
    windows.erase(it, windows.end());
  }

  // quitWhenLastWindowClosed_ が true で、ウィンドウがなくなったらアプリ終了処理を呼ぶ
  if (quitWhenLastWindowClosed_ && windows.empty())
  {
    this->quit(); // プラットフォーム固有の終了処理を呼び出す
  }
}
