#include "Window.hpp"
#include "app/scene/Scene.hpp"

bool Window::flushSceneInvalidation()
{
  loka::app::scene::Scene *current = this->scene();
  if (!current)
  {
    return false;
  }
  const bool changed = current->flushInvalidation();
  if (changed || this->hasPendingScenePlatformSync())
  {
    this->synchronizeScenePlatform();
  }
  return changed;
}
