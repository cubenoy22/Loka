#include "app/core/Window.hpp"
#include "app/scene/Scene.hpp"

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
