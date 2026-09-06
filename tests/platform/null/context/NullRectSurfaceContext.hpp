#ifndef LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP
#define LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/RectSurface.hpp"
#include "platform/null/context/NullPaintPlacement.hpp"

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

  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;
  using loka::app::scene::NativeNodeContext::commitPresented;
  bool commitPresented(const loka::app::RectSurfaceModel &value,
                       bool clearBackground,
                       const loka::app::scene::PaintScope &scope);
  void invalidatePaintHistory()
  {
    this->presented_.invalidate();
  }
  /** Called before every fallible projection: a placement is a derived cache and is
      re-established only by a successful layout in that pass (AGENTS.md
      failure-degradation). A refused projection therefore leaves no stale seat. */
  void invalidatePresentation()
  {
    this->presented_.invalidate();
    this->placement_.invalidate();
  }

private:
  loka::app::scene::PaintFact<loka::app::RectSurfaceModel> presented_;
  loka::app::scene::PaintFact<bool> presentedClearBackground_;
  NullPaintPlacement placement_;
  loka::app::RectSurfaceNode *node_;
  NullScenePlatformController *controller_;
};

void RegisterNullRectSurfaceNodeHandler(NullScenePlatformController &controller);

#endif // LOKA_TESTS_PLATFORM_NULL_RECT_SURFACE_CONTEXT_HPP
