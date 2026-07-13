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
      idleAccumulatedSeconds_(0.0),
      pendingWindowClosures_(),
      flushingPendingWindowClosures_(false)
{
}

App::~App()
{
  while (!pendingWindowClosures_.empty())
  {
    flushPendingWindowClosures();
  }
  delete group_;
  group_ = 0;
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
  // Platform loops call this only after the native callback/notification that
  // requested a close has unwound. This is the App clock's reclaim boundary.
  flushPendingWindowClosures();
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
  if (!window)
  {
    return;
  }
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      assert((!(*it) || (*it)->asWindow() != window) &&
             "App::windowClosed requires requestWindowClose detach first");
    }
  }
  assert(activeWindow_ != window && "A retired Window cannot remain active at reclaim");
  delete window;
}

void App::requestWindowClose(Window *window)
{
  if (!group_ || !window)
  {
    return;
  }
  for (size_t i = 0; i < pendingWindowClosures_.size(); ++i)
  {
    if (pendingWindowClosures_[i] == window)
    {
      return;
    }
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

  // Reserve before changing ownership so an allocation failure cannot leave
  // a detached Window without a queue owner.
  pendingWindowClosures_.reserve(pendingWindowClosures_.size() + 1);
  comps.erase(eraseIt);
  if (activeWindow_ == window)
  {
    this->setActiveWindow(nextActive);
  }
  pendingWindowClosures_.push_back(window);

  if (quitWhenLastWindowClosed_ && comps.empty())
  {
    this->quit();
  }
}

void App::flushPendingWindowClosures()
{
  if (flushingPendingWindowClosures_ || pendingWindowClosures_.empty())
  {
    return;
  }

  std::vector<Window *> pending;
  pending.swap(pendingWindowClosures_);
  flushingPendingWindowClosures_ = true;
  for (size_t i = 0; i < pending.size(); ++i)
  {
    this->windowClosed(pending[i]);
  }
  flushingPendingWindowClosures_ = false;
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
