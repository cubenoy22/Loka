#include "platform/null/context/NullRectSurfaceContext.hpp"

#include "app/RectSurface.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace
{
  class NullRectSurfaceNodeHandler : public loka::app::scene::RetainedNodeHandler<NullRectSurfaceNodeHandler,
                                                                                  loka::app::RectSurfaceNode,
                                                                                  NullRectSurfaceContext>
  {
  public:
    static loka::app::RectSurfaceNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asRectSurfaceNode() : 0;
    }

    static NullRectSurfaceContext *create(loka::app::RectSurfaceNode *surface,
                                          loka::app::scene::IPlatformController *controller,
                                          const loka::app::scene::LayoutState &state)
    {
      (void)controller;
      (void)state;
      return new NullRectSurfaceContext(surface);
    }
  };

  NullRectSurfaceNodeHandler gNullRectSurfaceNodeHandler;
} // namespace

NullRectSurfaceContext::NullRectSurfaceContext(loka::app::RectSurfaceNode *node)
    : loka::app::scene::NativeNodeContext(),
      node_(node)
{
}

NullRectSurfaceContext::~NullRectSurfaceContext()
{
  this->node_ = 0;
}

void NullRectSurfaceContext::readLifecycleFactOnAttach()
{
  // The null rail has no surface presentation; layout is its only operation.
}

short NullRectSurfaceContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  if (!this->node_)
  {
    return state.y;
  }
  const short height = this->node_->props.height_ > 0 ? this->node_->props.height_ : state.height;
  return static_cast<short>(state.y + height + state.spacing);
}

void RegisterNullRectSurfaceNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullRectSurfaceNodeHandler);
}
