#include "App.hpp"
#include "Window.hpp"
#include <algorithm>
#include <iostream>

// AppBuilder::Window メソッドの実装（Window抽象クラス問題の修正）
AppBuilder &AppBuilder::Window(const WindowOptions &opts)
{
  // PlatformContext経由でWindowを生成（context_はnonnull前提）
  AppComponent *window = context_->createWindow(opts); // Window*はAppComponent*として扱える
  components_.push_back(window);
  return *this;
}

App::App(ComponentGroup<AppComponent> *group)
    : group_(group)
{
}

App::~App()
{
  // グループの解放（ウィンドウ含む）
  delete group_;
}

void App::run()
{
  // App基底クラスの共通run処理（必要に応じて拡張）
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (AppComponent *comp : comps)
    {
      Window *win = dynamic_cast<Window *>(comp);
      if (win && win->visibility.get())
      {
        win->onRun(); // 派生Windowで初期表示を保証
      }
    }
  }
}

// App::windowClosed の実装
void App::windowClosed(Window *window)
{
  // group_のcomponentsから閉じたウィンドウを削除
  if (!group_)
    return;
  std::vector<AppComponent *> &comps = const_cast<std::vector<AppComponent *> &>(group_->getComponents());
  comps.erase(std::remove(comps.begin(), comps.end(), window), comps.end());

  // quitWhenLastWindowClosed_ が true で、ウィンドウがなくなったらアプリ終了処理を呼ぶ
  if (quitWhenLastWindowClosed_ && comps.empty())
  {
    this->quit(); // プラットフォーム固有の終了処理を呼び出す
  }
}
