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
      flushingWindowWork_(false)
{
}

App::~App()
{
  for (size_t i = 0; i < this->pendingWindowClosures_.size(); ++i)
    this->pendingWindowClosures_[i]->dialogResults().close();
  if (this->group_)
  {
    const std::vector<AppComponent *> &components = this->group_->getComponents();
    for (size_t i = 0; i < components.size(); ++i)
    {
      Window *window = components[i] ? components[i]->asWindow() : 0;
      if (window)
        window->dialogResults().close();
    }
  }
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
  projectInitialVisibilityChunks();
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

void App::projectInitialVisibilityChunks()
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

bool App::hasPendingWindowAdmission() const
{
  if (!this->pendingWindowClosures_.empty())
    return true;
  if (!this->group_)
    return false;
  const std::vector<AppComponent *> &components = this->group_->getComponents();
  for (size_t i = 0; i < components.size(); ++i)
  {
    Window *window = components[i] ? components[i]->asWindow() : 0;
    if (window && (!window->scene() || !window->scene()->isRunInProgress()) &&
        window->dialogResults().hasRunnableWork())
      return true;
  }
  return false;
}

/** Borrowed admission rows remain owned by the App group or close queue. */
struct App::AdmittedWindow
{
  explicit AdmittedWindow(Window *value)
      : window(value), scenes(0), dialogRetirements(value->captureDialogRetirements()) {}
  Window *window;
  loka::app::scene::Scene *scenes;
  Window::DialogRetirements dialogRetirements;
};

bool App::isWindowClosePending(Window *window) const
{
  return std::find(this->pendingWindowClosures_.begin(), this->pendingWindowClosures_.end(), window) !=
         this->pendingWindowClosures_.end();
}

void App::flushWindowInvalidations()
{
  // The App clock admits seat requests; native command callbacks only request work.
  // The close drain also uses this guard, so its callbacks cannot enter admission.
  if (this->flushingWindowWork_)
    return;
  this->flushPendingWindowClosures();
  if (!this->group_)
    return;

  this->flushingWindowWork_ = true;
  std::vector<AdmittedWindow> admitted;
  const std::vector<AppComponent *> &comps = this->group_->getComponents();
  for (size_t i = 0; i < comps.size(); ++i)
  {
    Window *win = comps[i] ? comps[i]->asWindow() : 0;
    // Direct Scene::invalidate() can enter a run outside this App guard.
    // Exclude the entire row so neither replacement nor reclaim touches it.
    if (win && win->scene() && win->scene()->isRunInProgress())
      continue;
    if (win && (win->hasPendingNativeVisibility() ||
                win->hasPendingSceneInvalidation() || win->hasPendingScenePlatformSync() ||
                win->dialogResults().hasRunnableWork()))
    {
      if (admitted.empty())
        admitted.reserve(comps.size());
      admitted.push_back(AdmittedWindow(win));
    }
  }
  // Snapshot our rows before callbacks can remove a Window from the group.
  // All seats apply before any Scene run: adoption from X's run waits even for Y.
  for (size_t i = 0; i < admitted.size(); ++i)
  {
    // An earlier row's callback may have delivered a terminal close for this
    // snapshot row. The App owns both lists; never recreate a close-queued rail.
    if (this->isWindowClosePending(admitted[i].window))
    {
      admitted[i].window = 0;
      continue;
    }
    admitted[i].window->applyNativeVisibility();
    if (this->isWindowClosePending(admitted[i].window))
      continue;
    admitted[i].window->deliverDialogResults();
    if (this->isWindowClosePending(admitted[i].window))
      continue;
    admitted[i].scenes = admitted[i].window->applySceneWork();
  }
  for (size_t i = 0; i < admitted.size(); ++i)
  {
    if (!admitted[i].window ||
        this->isWindowClosePending(admitted[i].window))
      continue;
    admitted[i].window->flushSceneInvalidation();
    if (this->isWindowClosePending(admitted[i].window))
      continue;
    admitted[i].window->reclaimScenes(admitted[i].scenes);
    admitted[i].window->reclaimDialogResults(admitted[i].dialogRetirements);
  }
  this->flushingWindowWork_ = false;
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
  window->dialogResults().close();
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

  const std::vector<AppComponent *> &comps = group_->getComponents();
  bool found = false;
  Window *nextActive = 0;
  for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
  {
    Window *candidate = (*it) ? (*it)->asWindow() : 0;
    if (candidate == window)
    {
      found = true;
      continue;
    }
    if (!nextActive && candidate)
    {
      nextActive = candidate;
    }
  }
  if (!found)
  {
    return;
  }

  // Reserve before changing ownership so an allocation failure cannot leave
  // a detached Window without a queue owner.
  pendingWindowClosures_.reserve(pendingWindowClosures_.size() + 1);
  window->dialogResults().close();
  if (!group_->remove(window))
  {
    return;
  }
  if (activeWindow_ == window)
  {
    this->setActiveWindow(nextActive);
  }
  pendingWindowClosures_.push_back(window);

  if (quitWhenLastWindowClosed_ && group_->getComponents().empty())
  {
    this->quit();
  }
}

void App::flushPendingWindowClosures()
{
  if (flushingWindowWork_ || pendingWindowClosures_.empty())
  {
    return;
  }

  std::vector<Window *> pending;
  pending.swap(pendingWindowClosures_);
  flushingWindowWork_ = true;
  for (size_t i = 0; i < pending.size(); ++i)
  {
    this->windowClosed(pending[i]);
  }
  flushingWindowWork_ = false;
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
