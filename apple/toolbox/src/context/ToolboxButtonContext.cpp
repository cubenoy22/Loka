#include "context/ToolboxButtonContext.hpp"
#include "ToolboxScenePlatformController.hpp"
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

ToolboxButtonContext::ToolboxButtonContext(declara::app::ButtonNode *node)
    : node_(node),
      rect_(),
      label_(loka::core::String::Literal("Button")),
      emitter_(0),
      enabled_(0),
      resourceId_(0)
{
}

ToolboxButtonContext::~ToolboxButtonContext() {}

void ToolboxButtonContext::updateData(const loka::core::String &label,
                                      declara::core::EmitterState *emitter,
                                      declara::core::State<bool> *enabled,
                                      short resourceId)
{
  label_ = label;
  emitter_ = emitter;
  enabled_ = enabled;
  resourceId_ = resourceId;
}

void ToolboxButtonContext::updateRect(const Rect &rect)
{
  rect_ = rect;
}

void ToolboxButtonContext::draw(ToolboxScenePlatformController *controller)
{
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
    controller->recordButtonHit(rect_, emitter_, enabled_);
  }
}
