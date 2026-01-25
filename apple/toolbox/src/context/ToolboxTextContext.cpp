#include "context/ToolboxTextContext.hpp"
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

ToolboxTextContext::ToolboxTextContext(loka::app::TextNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      textX_(0),
      textY_(0),
      text_(0)
{
}

ToolboxTextContext::~ToolboxTextContext() {}

void ToolboxTextContext::updateData(loka::core::State<loka::core::String> *text)
{
  text_ = text;
}

void ToolboxTextContext::updateRect(const Rect &rect, short textX, short textY)
{
  rect_ = rect;
  textX_ = textX;
  textY_ = textY;
}

void ToolboxTextContext::draw(ToolboxScenePlatformController *controller)
{
  if (!text_)
  {
    return;
  }
  DrawStringAt(textX_, textY_, text_->get());
  if (controller)
  {
    controller->recordTextHit(rect_, textX_, textY_, text_, boundary_);
  }
}

short ToolboxTextContext::layout(loka::core::scene::IPlatformController *controller,
                                 loka::core::scene::LayoutState &state)
{
  if (!node_ || !node_->props.text_)
  {
    return 0;
  }
  const loka::core::String &value = node_->props.text_->get();
  short width = ToolboxMeasureTextWidth(value);
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - state.lineHeight + 2);
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(state.y + 6);
  updateData(node_->props.text_);
  updateRect(rect, state.x, state.y);
  state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
  return width;
}

void ToolboxTextContext::render(loka::core::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
