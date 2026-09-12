#include "app/core/SceneManager.hpp"
#include "app/core/Window.hpp"
#include <cassert>
#include "core/util/StateTrackerGuard.hpp"

SceneManager::SceneManager()
    : currentScene_(0), desired_(0), applying_(0), tracker_(), retiredScenes_(), window_(0)
{
#ifdef TEST_BUILD
  this->lastPrepareRefusal_ = 0;
#endif
  this->tracker_.addState(&this->currentScene_);
}

SceneManager::~SceneManager()
{
  assert(!this->applying_ && "SceneManager cannot die during seat apply");
  this->window_ = 0;
  loka::app::scene::Scene *current = this->currentScene_.get();
  if (current)
  {
    current->setWindow(0);
    current->updateAttached(false);
    current->updateLifecycle(ON_DETACH);
    this->retiredScenes_.retire(current);
  }
  this->retiredScenes_.retire(this->desired_);
  while (!this->retiredScenes_.empty())
    this->retiredScenes_.drain();
}

bool SceneManager::commitTransaction(loka::app::scene::Scene *, loka::app::scene::Scene *to)
{
  if (!this->window_ || !to || this->retiredScenes_.contains(to))
    return false;
  loka::app::scene::Scene *superseded = this->desired_;
  this->desired_ = to;
  if (superseded != to && superseded != this->currentScene_.get() && superseded != this->applying_)
    this->retiredScenes_.retire(superseded);
  return true;
}

const loka::core::State<loka::app::scene::Scene *> &SceneManager::getCurrentScene() const
{
  return this->currentScene_;
}

bool SceneManager::rearmCurrentScene()
{
  loka::app::scene::Scene *current = this->currentScene_.get();
  if (!current)
    return false;
  loka::core::StateTrackerGuard guard(&this->tracker_);
  current->updateAttached(false);
  current->updateAttached(true);
  return current->composed_;
}

void SceneManager::seedScene(loka::app::scene::Scene *scene)
{
  assert(!this->currentScene_.get() && !this->desired_);
  this->commitTransaction(0, scene);
  this->applying_ = scene;
  this->installScene(scene);
  this->applying_ = 0;
}

bool SceneManager::applyReplacement()
{
#ifdef TEST_BUILD
  this->lastPrepareRefusal_ = 0;
#endif
  if (this->applying_ || !this->hasPendingReplacement())
    return false;
  loka::app::scene::Scene *next = this->desired_;
  this->applying_ = next;
  next->setWindow(this->window_);
  const bool mounted = this->window_->mountReplacementScene(next);
  if (mounted && next->mounted_)
    next->composeIfNeeded(loka::app::scene::COMPOSE_EVENT_ATTACH, false);
  // ATTACH composition can synchronously hide the Window and destroy its rail.
  // Validate the owner again before installing through the candidate's borrow.
  if (!mounted || (next->mounted_ &&
                   (!this->window_->hasLiveScenePlatform() || !next->composed_)))
  {
    // Preparation has not projected native contexts. Drop the borrowed rail
    // before this candidate can outlive the concrete Window's controller.
    next->platformController_ = 0;
    next->unmount();
    next->setWindow(0);
#ifdef TEST_BUILD
    // The desired role or retirement pool owns this identity until the next
    // admission clears the observation before any captured retirees are drained.
    this->lastPrepareRefusal_ = next;
#endif
    this->applying_ = 0;
    if (next != this->desired_)
      this->retiredScenes_.retire(next);
    return false;
  }
  this->installScene(next);
  this->applying_ = 0;
  return true;
}

void SceneManager::installScene(loka::app::scene::Scene *next)
{
  loka::app::scene::Scene *old = this->currentScene_.get();
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    if (old)
    {
      old->setWindow(0);
      old->updateAttached(false);
      old->updateLifecycle(ON_DETACH);
      old->unmount();
    }
    this->currentScene_.set(next);
    next->setWindow(this->window_);
    // Outgoing detach resets the shared controller. Attach the prepared scene,
    // project its root, then publish ON_ATTACH; applying protects reentrant adoption.
    next->updateAttached(true);
    if (next->composed_ && next->platformController_)
      next->platformController_->onChange(next->rootNode_, loka::app::scene::NODE_DIRTY_INITIAL, true);
    next->updateLifecycle(ON_ATTACH);
  }
  if (old != this->desired_)
    this->retiredScenes_.retire(old);
}
