#include "app/RectSurface.hpp"

void rectSurfaceGround()
{
  loka::app::RectSurfaceModel model;
  loka::core::MutableState<loka::app::RectSurfaceModel> state(model);
  (void)loka::app::RectSurface(&state);
}
