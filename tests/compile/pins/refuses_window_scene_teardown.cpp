#include "app/core/Window.hpp"

void probe(Window &window, loka::app::scene::Scene &scene)
{
  window.unmountSceneForTeardown(scene);
}
