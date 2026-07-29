#include "context/ToolboxScrollBarContext.hpp"
#include "ToolboxScenePlatformController.hpp"

ToolboxScrollBarContext::ToolboxScrollBarContext(loka::app::ScrollBarNode *node,
                                                 ToolboxScenePlatformController *controller)
    : node_(node),
      boundary_(0),
      rect_(),
      resourceId_(0),
      controller_(controller)
{
}

ToolboxScrollBarContext::~ToolboxScrollBarContext()
{
  if (controller_)
  {
    // The terminal fact delivery just refreshed the hint snapshot, so the
    // retire flush decides on the freshest value -- not the last render's.
    controller_->destroyScrollBarControl(resourceId_, this->lifetimeHint());
  }
  controller_ = 0;
}

void ToolboxScrollBarContext::updateRect(const Rect &rect)
{
  rect_ = rect;
}

short ToolboxScrollBarContext::layout(loka::app::scene::IPlatformController *controller,
                                      loka::app::scene::LayoutState &state)
{
  (void)controller;
  if (!node_)
  {
    return 0;
  }
  const short thickness = static_cast<short>(loka::app::SCROLL_BAR_THICKNESS);
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - state.lineHeight + 2);
  short usedWidth = thickness;
  if (node_->props.orientation_ == loka::app::SCROLL_BAR_HORIZONTAL)
  {
    // Length comes from the layout pass, thickness never does: a scroll bar
    // that grew on its cross axis would stop looking like the system's.
    short length = state.width > 0 ? state.width : static_cast<short>(thickness * 4);
    rect.right = static_cast<short>(rect.left + length);
    rect.bottom = static_cast<short>(rect.top + thickness);
    usedWidth = length;
  }
  else
  {
    short length = state.height > 0 ? state.height : static_cast<short>(state.lineHeight * 4);
    if (length < thickness)
    {
      length = thickness;
    }
    rect.right = static_cast<short>(rect.left + thickness);
    rect.bottom = static_cast<short>(rect.top + length);
  }
  updateRect(rect);
  state.y = static_cast<short>(rect.bottom + state.spacing);
  return usedWidth;
}

void ToolboxScrollBarContext::draw(ToolboxScenePlatformController *controller)
{
  if (!node_)
  {
    return;
  }
  if (controller && resourceId_ <= 0)
  {
    // Identity rule: the auto id stays with this context for its lifetime,
    // so the controller's binding table keeps pointing at the same control
    // across recomposes.
    resourceId_ = controller->allocateControlId();
  }
  if (controller && resourceId_ > 0)
  {
    if (controller->ensureScrollBarControl(resourceId_, rect_, node_->props, lifetimeHint()))
    {
      return;
    }
  }
  // No Control Manager control: outline the band so the layout stays legible
  // instead of leaving a hole, the same fallback the button arm uses.
  FrameRect(&rect_);
}

void ToolboxScrollBarContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
