#include "context/ToolboxButtonContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "loka/platform/StringUTF8.hpp"
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
}

ToolboxButtonContext::ToolboxButtonContext(loka::app::ButtonNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      label_(loka::core::String::Literal("Button")),
      emitter_(0),
      enabled_(0),
      resourceId_(0)
{
}

ToolboxButtonContext::~ToolboxButtonContext() {}

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
  if (controller && resourceId_ <= 0)
  {
    resourceId_ = controller->allocateControlId();
  }
  if (controller && resourceId_ > 0)
  {
    if (controller->ensureButtonControl(resourceId_, rect_, label_, emitter_))
    {
      return;
    }
  }
  FrameRect(&rect_);
  DrawStringAt(static_cast<short>(rect_.left + 4),
               static_cast<short>(rect_.bottom - 6),
               label_);
  if (controller)
  {
    controller->recordButtonHit(rect_, emitter_, enabled_, boundary_);
  }
}

short ToolboxButtonContext::layout(loka::core::scene::IPlatformController *controller,
                                   loka::core::scene::LayoutState &state)
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
  updateData(label, node_->props.onClick_, node_->props.enabled_, node_->props.toolboxControlId_, node_->props.controlTag_);
  updateRect(rect);
  state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
  return width;
}

void ToolboxButtonContext::render(loka::core::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
