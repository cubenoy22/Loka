#include "app/core/App.hpp"
#include "app/core/Window.hpp"
#include "app/PlatformContext.hpp"
#include "app/core/AppComposition.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/scene/Scene.hpp"
#include <algorithm>

App::App(AppConfigurable *config)
    : group_(0),
      quitWhenLastWindowClosed_(true),
      config_(config),
      menuController_(config, &App::ApplyMenuBarThunk, this),
      activeWindow_(0),
      idleAccumulatedSeconds_(0.0)
{
}

App::~App()
{
  delete group_;
}

void App::ApplyMenuBarThunk(void *userData, Window *activeWindow)
{
  App *app = static_cast<App *>(userData);
  if (app)
  {
    app->applyMenuBar(activeWindow);
  }
}

void App::run()
{
  if (!group_ && config_)
  {
    AppComposition composition(config_->getPlatformContext());
    config_->compose(composition);
    menuController_.refreshDefaultMenuBar();
    group_ = new AppComponentGroup(composition.build());
  }
  reflectInitialVisibilityChunks();
}

loka::app::IdlePolicy App::idlePolicy() const
{
  if (activeWindow_)
  {
    return activeWindow_->idlePolicy();
  }
  return config_ ? config_->idlePolicy() : loka::app::IdlePolicy::none();
}

bool App::consumeIdle(double elapsedSeconds, double &dispatchElapsedSeconds)
{
  const loka::app::IdlePolicy policy = this->idlePolicy();
  if (policy.mode == loka::app::IDLE_MODE_NONE)
  {
    idleAccumulatedSeconds_ = 0.0;
    return false;
  }
  if (policy.mode == loka::app::IDLE_MODE_EVERY_TICK)
  {
    idleAccumulatedSeconds_ = 0.0;
    dispatchElapsedSeconds = elapsedSeconds;
    return true;
  }
  idleAccumulatedSeconds_ += elapsedSeconds;
  if (policy.intervalSeconds <= 0.0 || idleAccumulatedSeconds_ >= policy.intervalSeconds)
  {
    dispatchElapsedSeconds = idleAccumulatedSeconds_;
    idleAccumulatedSeconds_ = 0.0;
    return true;
  }
  return false;
}

void App::handleIdle(double elapsedSeconds)
{
  if (activeWindow_ && activeWindow_->handleIdle(elapsedSeconds))
  {
    return;
  }
  if (config_)
  {
    config_->onIdle(elapsedSeconds);
  }
}

bool App::handleKeyPress(char key)
{
  if (activeWindow_ && activeWindow_->handleKeyPress(key))
  {
    return true;
  }
  return config_ ? config_->handleKeyPress(key) : false;
}

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
  menuController_.invalidate(activeWindow_);
}

void App::requestMenuInvalidation()
{
  menuController_.requestInvalidation();
}

bool App::flushMenuInvalidation()
{
  return menuController_.flushInvalidation(activeWindow_);
}

void App::setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar)
{
  menuController_.setDefaultMenuBar(menuBar, activeWindow_);
}

const loka::app::MenuBarDefinition *App::resolveMenuBar(Window *window)
{
  return menuController_.resolveMenuBar(window);
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
  return menuController_.refreshDefaultMenuBar();
}

void App::clearMenuDiff()
{
  menuController_.clearDiff();
}
