#include "App.hpp"
#include "Window.hpp"
#include "PlatformContext.hpp"
#include "AppComposition.hpp"
#include "util/StateTrackerGuard.hpp"
#include "core2/scene/Scene.hpp"
#include <algorithm>

App::App(AppConfigurable *config)
    : group_(0), quitWhenLastWindowClosed_(true), config_(config)
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
    AppComposition composition(config_->getPlatformContext());
    config_->compose(composition);
    group_ = new ComponentGroup<AppComponent>(composition.build());
  }
  reflectInitialVisibilityChunks();
}

// --- visibility chunk反映をprivate関数に分離 ---
void App::reflectInitialVisibilityChunks()
{
  if (!group_)
    return;
  const std::vector<AppComponent *> &comps = group_->getComponents();
  for (size_t i = 0; i < comps.size(); ++i)
  {
    AppComponent *comp = comps[i];
    Window *win = dynamic_cast<Window *>(comp);
    if (win && win->visibilityState().get())
    {
      declara::core::StateTracker *tracker = win->getTracker();
      if (tracker)
      {
        StateTrackerGuard _(tracker);
        win->visibilityState().set(true, true);
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
