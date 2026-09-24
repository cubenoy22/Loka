#include "app/core/Window.hpp"
#include "app/FocusPublisher.hpp"
#include "app/scene/Scene.hpp"

void Window::unmountSceneForTeardown(loka::app::scene::Scene &scene)
{
  scene.unmount();
}

loka::app::scene::Scene *Window::applySceneWork()
{
  loka::app::scene::Scene *retired = this->sceneManager_.retiredScenes_.snapshot();
  if (this->sceneManager_.applyPendingWork())
    this->synchronizeScenePlatform();
  return retired;
}

void Window::reclaimScenes(loka::app::scene::Scene *retired)
{
  this->sceneManager_.retiredScenes_.drainSnapshot(retired);
}

bool Window::flushSceneInvalidation()
{
  loka::app::scene::Scene *current = this->scene();
  const bool changed = current ? current->flushInvalidation() : false;
  if (changed || this->hasPendingScenePlatformSync())
    this->synchronizeScenePlatform();
  this->drainNativeRetirements();
  return changed;
}

void Window::reconcileFocus()
{
  loka::app::scene::Scene *current = this->scene();
  if (this->sceneManager_.applying_ || !current || current->isRunInProgress()
      || current->focus().isPublishing() || !current->attached_.get()
      || !current->rootNode_ || !this->hasLiveScenePlatform() || !current->platformController_)
    return;
  loka::app::scene::NodeContext *target = 0;
  const bool answered = current->platformController_->readNativeFocus(target);
  loka::app::detail::FocusPublisher::reconcile(current->focus(), answered, target);
}
