#include "context/ToolboxTextContext.hpp"
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

ToolboxTextContext::ToolboxTextContext(declara::app::TextNode *node)
    : node_(node),
      rect_(),
      textX_(0),
      textY_(0),
      text_(0)
{
}

ToolboxTextContext::~ToolboxTextContext() {}

void ToolboxTextContext::updateData(declara::core::State<loka::core::String> *text)
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
    controller->recordTextHit(rect_, textX_, textY_, text_);
  }
}
