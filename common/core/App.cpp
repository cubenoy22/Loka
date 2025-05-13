#include "App.hpp"
#include "Window.hpp"
#include <algorithm>
#include <iostream>
#include "util/AutoTransactionGuard.hpp"

// AppBuilder::Window メソッドの実装（Window抽象クラス問題の修正）
AppBuilder &AppBuilder::Window(Scene *initialScene, const WindowOptions &opts)
{
  AppComponent *window = context_->createWindow(initialScene, opts);
  components_.push_back(window);
  return *this;
}

App::App(AppConfigurable *config)
    : group_(0), config_(config)
{
}

App::~App()
{
  // グループの解放（ウィンドウ含む）
  delete group_;
}

void App::run()
{
  if (!group_ && config_)
  {
    AppBuilder builder(config_->getPlatformContext());
    config_->configure(builder);
    group_ = new ComponentGroup<AppComponent>(builder.build());
  }
  reflectInitialVisibilityChunks();
}

// --- visibility chunk反映をprivate関数に分離 ---
void App::reflectInitialVisibilityChunks()
{
  if (!group_)
    return;
  const std::vector<AppComponent *> &comps = group_->getComponents();
  for (auto *comp : comps)
  {
    Window *win = dynamic_cast<Window *>(comp);
    if (win && win->visibility.get())
    {
      StateTracker *tracker = win->getTracker();
      if (tracker)
      {
        AutoTransactionGuard _(tracker);
        win->visibility.set(true, true);
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
