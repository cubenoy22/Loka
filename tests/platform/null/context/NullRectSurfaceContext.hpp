#ifndef LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"

class NullScenePlatformController;

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
} // namespace loka

class NullRectSurfaceContext : public loka::app::scene::NativeNodeContext
{
public:
  NullRectSurfaceContext(loka::app::RectSurfaceNode *node, NullScenePlatformController *controller);
  virtual ~NullRectSurfaceContext();

  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  loka::app::RectSurfaceNode *node_;
  NullScenePlatformController *controller_;
};

void RegisterNullRectSurfaceNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP
