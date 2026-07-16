#include "context/ToolboxButtonContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "platform/StringUTF8.hpp"
#include <cstring>
#include <string>

namespace
{
  void DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    Str255 text;
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
    MoveTo(x, y);
    DrawString(text);
  }
} // namespace

ToolboxButtonContext::ToolboxButtonContext(loka::app::ButtonNode *node,
                                           ToolboxScenePlatformController *controller)
    : node_(node),
      boundary_(0),
      rect_(),
      label_(loka::core::String::Literal("Button")),
      emitter_(0),
      enabled_(0),
      resourceId_(0),
      controller_(controller)
{
}

ToolboxButtonContext::~ToolboxButtonContext()
{
  if (controller_)
  {
    controller_->destroyButtonControl(resourceId_);
  }
  controller_ = 0;
}

void ToolboxButtonContext::updateData(const loka::core::String &label,
                                      loka::core::EmitterState *emitter,
                                      loka::core::State<bool> *enabled,
                                      short resourceId,
                                      int controlTag)
{
  label_ = label;
  emitter_ = emitter;
  enabled_ = enabled;
  if (resourceId > 0)
  {
    resourceId_ = resourceId;
  }
  else if (controlTag > 0 && controlTag <= 32767)
  {
    resourceId_ = static_cast<short>(controlTag);
  }
}

void ToolboxButtonContext::updateRect(const Rect &rect)
{
  rect_ = rect;
}

void ToolboxButtonContext::draw(ToolboxScenePlatformController *controller)
{
  if (node_)
  {
    // Draw can run without a fresh layout (dirty-draw path); re-observe so
    // the binding never retires under a hint older than one pass.
    observeLifetimeHint(node_->nativeLifetimeHint());
  }
  if (controller && resourceId_ <= 0)
  {
    resourceId_ = controller->allocateControlId();
  }
  if (controller && resourceId_ > 0)
  {
    if (controller->ensureButtonControl(resourceId_, rect_, label_, emitter_, enabled_, lifetimeHint()))
    {
      return;
    }
  }
  FrameRect(&rect_);
  DrawStringAt(static_cast<short>(rect_.left + 4), static_cast<short>(rect_.bottom - 6), label_);
  if (controller)
  {
    controller->recordButtonHit(rect_, emitter_, enabled_, boundary_, this);
  }
}

short ToolboxButtonContext::layout(loka::app::scene::IPlatformController *controller,
                                   loka::app::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  loka::core::String label = loka::core::String::Literal("Button");
  if (node_->props.text_)
  {
    label = node_->props.text_->get();
  }
  short width = ToolboxMeasureTextWidth(label);
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - state.lineHeight + 2);
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(state.y + 6);
  updateData(label, node_->props.onClick_, node_->props.enabled_, 0, node_->props.controlTag_);
  observeLifetimeHint(node_->nativeLifetimeHint());
  updateRect(rect);
  state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
  return width;
}

void ToolboxButtonContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}

bool ToolboxButtonContext::handleMouseDown(const Point &point, ToolboxScenePlatformController *controller)
{
  if (!emitter_)
  {
    return false;
  }
  if (enabled_ && !enabled_->get())
  {
    return false;
  }
  if (!PtInRect(point, &rect_))
  {
    return false;
  }
  if (controller)
  {
    controller->emitHitEmitter(emitter_);
  }
  return true;
}
