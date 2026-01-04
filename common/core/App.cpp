#include "App.hpp"
#include "Window.hpp"
#include "PlatformContext.hpp"
#include "AppComposition.hpp"
#include "util/StateTrackerGuard.hpp"
#include "core2/scene/Scene.hpp"
#include <algorithm>

App::App(AppConfigurable *config)
    : group_(0), quitWhenLastWindowClosed_(true), config_(config), menuBar_(0), activeWindow_(0)
{
}

App::~App()
{
  // グループの解放（ウィンドウ含む）
  delete group_;
  delete menuBar_;
}

void App::run()
{
  if (!group_ && config_)
  {
    AppComposition composition(config_->getPlatformContext());
    config_->compose(composition);
    declara::app::MenuBarDefinition menuBar;
    declara::app::MenuComposition menuComposition(&menuBar);
    config_->composeMenu(menuComposition);
    if (!menuBar.empty())
    {
      setDefaultMenuBar(menuBar.clone());
    }
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

bool App::handleMenuCommand(int commandId, Window *window)
{
  (void)commandId;
  (void)window;
  return false;
}

void App::setDefaultMenuBar(const declara::app::MenuBarDefinition *menuBar)
{
  if (menuBar_)
  {
    delete menuBar_;
    menuBar_ = 0;
  }
  if (menuBar)
  {
    menuBar_ = menuBar->clone();
  }
  applyMenuBar(activeWindow_);
}

const declara::app::MenuBarDefinition *App::resolveMenuBar(Window *window) const
{
  if (window && window->menuBar())
  {
    return window->menuBar();
  }
  return menuBar_;
}

void App::setActiveWindow(Window *window)
{
  if (activeWindow_ == window)
  {
    return;
  }
  activeWindow_ = window;
  applyMenuBar(activeWindow_);
}

void App::applyMenuBar(Window *activeWindow)
{
  (void)activeWindow;
}
