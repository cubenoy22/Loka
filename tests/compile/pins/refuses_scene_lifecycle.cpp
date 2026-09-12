#include "app/scene/Scene.hpp"

void bypassSceneSeat(loka::app::scene::Scene &scene)
{
  scene.updateLifecycle(ON_DETACH);
  scene.updateLifecycle(ON_ATTACH);
}
