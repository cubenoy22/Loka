#include "app/core/SceneManager.hpp"

void probe(SceneManager &seat)
{
  seat.requestDetach();
  seat.requestRearm();
}
