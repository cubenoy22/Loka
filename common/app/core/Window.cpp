#include "app/core/Window.hpp"
#include "app/scene/Scene.hpp"

bool Window::flushSceneInvalidation()
{
  loka::app::scene::Scene *current = this->scene();
  const bool changed = current ? current->flushInvalidation() : false;
  if (changed || this->hasPendingScenePlatformSync())
  {
    this->synchronizeScenePlatform();
  }
  this->sceneManager_.reclaimRetiredScenes();
  return changed;
}
