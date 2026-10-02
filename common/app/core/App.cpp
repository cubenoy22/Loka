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
  // Shutdown destroys every owner; discard borrowed tail snapshots first.
  this->pendingReclaim_.clear();
  for (size_t i = 0; i < this->pendingWindowClosures_.size(); ++i)
    this->pendingWindowClosures_[i]->closeDialogResults();
  if (this->group_)
  {
    const std::vector<AppComponent *> &components = this->group_->getComponents();
    for (size_t i = 0; i < components.size(); ++i)
    {
      Window *window = components[i] ? components[i]->asWindow() : 0;
      if (window)
        window->closeDialogResults();
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
        loka::core::StandaloneTransactionGuard _(tracker);
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
    if (this->windowHasAdmissionWork(window))
      return true;
  }
  return false;
}

bool App::windowHasAdmissionWork(Window *window) const
{
  if (!window || this->isWindowClosePending(window)
      || (window->scene() && window->scene()->isBusy()))
    return false;
  loka::app::DialogResultDelivery *delivery = window->dialogResultDelivery();
  return window->hasPendingNativeVisibility() || window->hasPendingSceneInvalidation()
         || window->hasPendingScenePlatformSync() || (delivery && delivery->hasRunnableWork());
}

App::AdmittedWindow::AdmittedWindow(
    Window *value, loka::app::DialogResultDelivery *results,
    loka::app::DialogResultDelivery::Retirement *retirements)
    : window(value), scenes(0), delivery(results), dialogRetirements(retirements)
{
}

void App::AdmissionBatch::begin(const std::vector<Window *> &pending)
{
  if (this->isOpen())
    return;
  this->closes = pending;
  this->phase_ = OPEN;
}

void App::AdmissionBatch::remember(const AdmittedWindow &row)
{
  for (size_t i = 0; i < this->rows.size(); ++i)
  {
    if (this->rows[i].window == row.window)
    {
      this->rows[i] = row;
      return;
    }
  }
  this->rows.push_back(row);
}

void App::AdmissionBatch::clear()
{
  this->rows.clear();
  this->closes.clear();
  this->phase_ = BETWEEN_TAILS;
}

bool App::isWindowClosePending(Window *window) const
{
  return std::find(this->pendingWindowClosures_.begin(), this->pendingWindowClosures_.end(), window)
         != this->pendingWindowClosures_.end();
}

void App::flushWindowInvalidations()
{
  this->admitAndApplyWindows();
  this->reclaimWindows();
}

void App::admitAndApplyWindows()
{
  if (this->flushingWindowWork_)
    return;
  this->pendingReclaim_.begin(this->pendingWindowClosures_);
  if (!this->group_)
    return;

  this->flushingWindowWork_ = true;
  std::vector<AdmittedWindow> admitted;
  const std::vector<AppComponent *> &comps = this->group_->getComponents();
  for (size_t i = 0; i < comps.size(); ++i)
  {
    Window *win = comps[i] ? comps[i]->asWindow() : 0;
    if (this->windowHasAdmissionWork(win))
    {
      loka::app::DialogResultDelivery *delivery = win->dialogResultDelivery();
      if (admitted.empty())
        admitted.reserve(comps.size());
      admitted.push_back(AdmittedWindow(win, delivery, delivery ? delivery->retirementSnapshot() : 0));
    }
  }
  // Snapshot our rows before callbacks can remove a Window from the group.
  // All seats apply before any Scene run: adoption from X's run waits even for Y.
  for (std::vector<AdmittedWindow>::iterator it = admitted.begin(); it != admitted.end(); ++it)
  {
    // An earlier row's callback may have delivered a terminal close for this
    // snapshot row. The App owns both lists; never recreate a close-queued rail.
    Window *window = it->window;
    if (this->isWindowClosePending(window))
      continue;
    window->applyNativeVisibility();
    if (this->isWindowClosePending(window))
      continue;
    if (it->delivery)
    {
      it->delivery->deliver();
      if (this->isWindowClosePending(window))
        continue;
    }
    it->scenes = window->applySceneWork();
  }
  for (std::vector<AdmittedWindow>::iterator it = admitted.begin(); it != admitted.end(); ++it)
  {
    Window *window = it->window;
    if (this->isWindowClosePending(window))
      continue;
    window->flushSceneInvalidation();
  }
  for (size_t i = 0; i < admitted.size(); ++i)
    this->pendingReclaim_.remember(admitted[i]);
  // Detach is observable work and belongs before the turn closes. Reclaim
  // keeps the Window alive until then. Mirror drainWindowClosures' busy skip.
  const size_t closeCount = this->pendingWindowClosures_.size();
  for (size_t i = 0; i < closeCount; ++i)
  {
    Window *window = this->pendingWindowClosures_[i];
    loka::app::scene::Scene *scene = window->scene();
    if (scene && !scene->isBusy())
      window->retireSceneForClose();
  }
  this->flushingWindowWork_ = false;
}

void App::reclaimWindows()
{
  if (this->flushingWindowWork_ || !this->pendingReclaim_.isOpen())
    return;
  this->flushingWindowWork_ = true;
  // Remove borrowed identities before the close drain can delete their owners.
  std::vector<AdmittedWindow> &rows = this->pendingReclaim_.rows;
  for (size_t i = 0; i < rows.size(); ++i)
    if (this->isWindowClosePending(rows[i].window))
      rows[i].window = 0;
  this->drainWindowClosures(this->pendingReclaim_.closes);
  for (size_t i = 0; i < rows.size(); ++i)
  {
    AdmittedWindow &row = rows[i];
    Window *window = row.window;
    if (!window || this->isWindowClosePending(window)
        || (window->scene() && window->scene()->isBusy()))
      continue;
    window->reclaimScenes(row.scenes);
    if (row.delivery)
      row.delivery->reclaim(row.dialogRetirements);
  }
  this->pendingReclaim_.clear();
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
      assert((!(*it) || (*it)->asWindow() != window) && "App::windowClosed requires requestWindowClose detach first");
    }
  }
  assert(activeWindow_ != window && "A retired Window cannot remain active at reclaim");
  window->closeDialogResults();
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
  window->closeDialogResults();
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
  if (this->flushingWindowWork_ || this->pendingReclaim_.isOpen()
      || this->pendingWindowClosures_.empty())
    return;
  const std::vector<Window *> pending(this->pendingWindowClosures_);
  this->flushingWindowWork_ = true;
  this->drainWindowClosures(pending);
  this->flushingWindowWork_ = false;
}

void App::drainWindowClosures(const std::vector<Window *> &pending)
{
  for (size_t i = 0; i < pending.size(); ++i)
  {
    Window *window = pending[i];
    if (window->scene() && window->scene()->isBusy())
      continue;
    this->pendingWindowClosures_.erase(
        std::find(this->pendingWindowClosures_.begin(), this->pendingWindowClosures_.end(), window));
    this->windowClosed(window);
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

void App::reconcileFocus()
{
  if (this->flushingWindowWork_ || !this->group_)
    return;
  /** Each completion walks live App-owned windows (w), never foreign rows.
      Repeated scans visit O(w^2) group entries; linear visited lookups cost
      O(w^3) pointer comparisons in the worst case. Local storage is O(1)
      through eight windows and O(w) beyond that; only the overflow allocates.
      Visited pointers are identities only and are never dereferenced. If a
      new window reuses a visited address, the next completion picks it up. */
  class VisitedWindows
  {
  public:
    VisitedWindows() : inlineCount_(0) {}
    bool contains(Window *window) const
    {
      return std::find(this->inline_, this->inline_ + this->inlineCount_, window)
                 != this->inline_ + this->inlineCount_
             || std::find(this->spill_.begin(), this->spill_.end(), window) != this->spill_.end();
    }
    void add(Window *window)
    {
      if (this->inlineCount_ < INLINE_CAPACITY)
        this->inline_[this->inlineCount_++] = window;
      else
        this->spill_.push_back(window);
    }

  private:
    enum { INLINE_CAPACITY = 8 };
    Window *inline_[INLINE_CAPACITY];
    size_t inlineCount_;
    std::vector<Window *> spill_;
  };
  VisitedWindows visited;
  for (;;)
  {
    Window *next = 0;
    const std::vector<AppComponent *> &components = this->group_->getComponents();
    for (size_t i = 0; i < components.size(); ++i)
    {
      Window *window = components[i] ? components[i]->asWindow() : 0;
      if (window && !this->isWindowClosePending(window)
          && !visited.contains(window))
      {
        next = window;
        break;
      }
    }
    if (!next)
      return;
    visited.add(next);
    next->reconcileFocus();
  }
}
