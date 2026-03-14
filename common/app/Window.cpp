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
  this->synchronizeScenePlatform();
  return changed;
}
