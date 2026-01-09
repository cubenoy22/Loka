#include "context/ToolboxEditTextContext.hpp"
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

ToolboxEditTextContext::ToolboxEditTextContext(declara::app::EditTextNode *node)
    : node_(node),
      rect_(),
      textRect_(),
      textX_(0),
      textY_(0),
      text_(0)
{
}

ToolboxEditTextContext::~ToolboxEditTextContext() {}

void ToolboxEditTextContext::updateData(declara::core::State<loka::core::String> *text)
{
  text_ = text;
}

void ToolboxEditTextContext::updateRect(const Rect &outerRect, const Rect &textRect, short textX, short textY)
{
  rect_ = outerRect;
  textRect_ = textRect;
  textX_ = textX;
  textY_ = textY;
}

void ToolboxEditTextContext::draw(ToolboxScenePlatformController *controller)
{
  if (!text_)
  {
    FrameRect(&rect_);
    return;
  }
  if (controller)
  {
    TEHandle te = controller->ensureEditTextControl(textRect_, text_);
    if (te)
    {
      controller->beginClip(textRect_);
      TEUpdate(&textRect_, te);
      controller->endClip();
      FrameRect(&rect_);
      return;
    }
  }
  FrameRect(&rect_);
  DrawStringAt(textX_, textY_, text_->get());
  if (controller)
  {
    controller->recordEditHit(rect_, text_);
  }
}
