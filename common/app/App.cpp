#include "App.hpp"
#include "Window.hpp"
#include "PlatformContext.hpp"
#include "AppComposition.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "app/scene/Scene.hpp"
#include <algorithm>

App *App::currentApp_ = 0;

App::App(AppConfigurable *config)
    : group_(0),
      quitWhenLastWindowClosed_(true),
      config_(config),
      menuBar_(0),
      activeWindow_(0),
      menuRefresh_()
{
  currentApp_ = this;
}

App::~App()
{
  if (currentApp_ == this)
  {
    currentApp_ = 0;
  }
  // グループの解放（ウィンドウ含む）
  delete group_;
  delete menuBar_;
}

static void MenuInvalidateThunk(void *userData)
{
  App *app = static_cast<App *>(userData);
  if (app)
  {
    app->requestMenuInvalidation();
  }
}

bool App::MenuRefreshThunk(void *userData)
{
  App *app = static_cast<App *>(userData);
  return app ? app->refreshDefaultMenuBar() : false;
}

void App::MenuApplyThunk(void *userData)
{
  App *app = static_cast<App *>(userData);
  if (app)
  {
    app->applyMenuBar(app->activeWindow());
  }
}

void App::run()
{
  if (!group_ && config_)
  {
    AppComposition composition(config_->getPlatformContext());
    config_->compose(composition);
    refreshDefaultMenuBar();
    group_ = new ComponentGroup<AppComponent>(composition.build());
  }
  reflectInitialVisibilityChunks();
}

bool App::wantsIdleUpdates() const
{
  return config_ ? config_->wantsIdleUpdates() : false;
}

void App::handleIdle(double elapsedSeconds)
{
  if (config_)
  {
    config_->onIdle(elapsedSeconds);
  }
}

bool App::handleKeyPress(char key)
{
  return config_ ? config_->handleKeyPress(key) : false;
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
    Window *win = comp->asWindow();
    if (win && win->visibilityState().get())
    {
      loka::core::StateTracker *tracker = win->getTracker();
      if (tracker)
      {
        loka::core::StateTrackerGuard _(tracker);
        win->visibilityState().set(true, true);
      }
    }
  }
}

void App::flushWindowInvalidations()
{
  if (!group_)
  {
    return;
  }
  const std::vector<AppComponent *> &comps = group_->getComponents();
  for (size_t i = 0; i < comps.size(); ++i)
  {
    Window *win = comps[i] ? comps[i]->asWindow() : 0;
    if (win)
    {
      if (!win->hasPendingSceneInvalidation() && !win->hasPendingScenePlatformSync())
      {
        continue;
      }
      win->flushSceneInvalidation();
    }
  }
}

// App::windowClosed の実装
void App::windowClosed(Window *window)
{
  if (!group_ || !window)
  {
    return;
  }
  std::vector<AppComponent *> &comps = const_cast<std::vector<AppComponent *> &>(group_->getComponents());
  std::vector<AppComponent *>::iterator eraseIt = comps.end();
  Window *nextActive = 0;
  for (std::vector<AppComponent *>::iterator it = comps.begin(); it != comps.end(); ++it)
  {
    Window *candidate = (*it) ? (*it)->asWindow() : 0;
    if (candidate == window)
    {
      eraseIt = it;
      continue;
    }
    if (!nextActive && candidate)
    {
      nextActive = candidate;
    }
  }
  if (eraseIt == comps.end())
  {
    return;
  }
  comps.erase(eraseIt);
  if (activeWindow_ == window)
  {
    activeWindow_ = 0;
  }
  delete window;
  if (!activeWindow_ && nextActive)
  {
    setActiveWindow(nextActive);
  }

  if (quitWhenLastWindowClosed_ && comps.empty())
  {
    this->quit();
  }
}

bool App::handleMenuCommand(int commandId, Window *window)
{
  (void)commandId;
  (void)window;
  return false;
}

void App::invalidateMenu()
{
  requestMenuInvalidation();
  flushMenuInvalidation();
}

void App::requestMenuInvalidation()
{
  menuRefresh_.request();
}

bool App::flushMenuInvalidation()
{
  return menuRefresh_.run(&MenuRefreshThunk, &MenuApplyThunk, this);
}

void App::setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar)
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

const loka::app::MenuBarDefinition *App::resolveMenuBar(Window *window)
{
  if (window && window->menuBar())
  {
    return window->menuBar();
  }
  if (!menuDiff_.valid)
  {
    refreshDefaultMenuBar();
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

bool App::refreshDefaultMenuBar()
{
  if (!config_)
  {
    return false;
  }
  loka::app::MenuBarDefinition menuBar;
  loka::app::MenuComposition menuComposition(&menuBar);
  menuComposition.setInvalidateCallback(&MenuInvalidateThunk, this);
  config_->composeMenu(menuComposition);
  menuComposition.finish();
  std::vector<size_t> dirtyMenus;
  menuComposition.takeDirtyMenuIndices(dirtyMenus);
  if (menuBar.empty())
  {
    menuDiff_.clear();
    if (menuBar_)
    {
      delete menuBar_;
      menuBar_ = 0;
      menuDiff_.valid = true;
      menuDiff_.fullRebuild = true;
      return true;
    }
    return false;
  }
  bool diffAttempted = false;
  bool diffResult = false;
  if (!menuBar_)
  {
    menuDiff_.valid = true;
    menuDiff_.fullRebuild = true;
  }
  else
  {
    diffAttempted = true;
    diffResult = loka::app::MenuCompositionDiff::Diff(*menuBar_, menuBar, menuDiff_);
    if (!diffResult)
    {
      menuDiff_.clear();
      if (dirtyMenus.empty())
      {
        return false;
      }
    }
  }
  if (!dirtyMenus.empty())
  {
    menuDiff_.valid = true;
    if (menuDiff_.fullRebuild && diffAttempted && !diffResult)
    {
      menuDiff_.fullRebuild = false;
    }
    if (!menuDiff_.fullRebuild)
    {
      for (size_t i = 0; i < dirtyMenus.size(); ++i)
      {
        bool exists = false;
        loka::dsl::CompositionCursor<loka::app::MenuCompositionDiff::ChangedIndex> it(
            menuDiff_.changedHead(), menuDiff_.changedCount());
        for (loka::app::MenuCompositionDiff::ChangedIndex *entry = it.next(); entry; entry = it.next())
        {
          if (entry->value == dirtyMenus[i])
          {
            exists = true;
            break;
          }
        }
        if (!exists)
        {
          menuDiff_.addChanged(dirtyMenus[i]);
        }
      }
    }
  }
  if (!menuBar_ || menuDiff_.valid)
  {
    if (menuBar_)
    {
      delete menuBar_;
      menuBar_ = 0;
    }
    menuBar_ = menuBar.clone();
    return true;
  }
  menuDiff_.clear();
  return false;
}

void App::clearMenuDiff()
{
  menuDiff_.clear();
}
