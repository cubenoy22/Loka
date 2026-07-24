#include "context/ToolboxEditTextContext.hpp"
#include "ToolboxScenePlatformController.hpp"
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

ToolboxEditTextContext::ToolboxEditTextContext(loka::app::EditTextNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      textRect_(),
      textX_(0),
      textY_(0),
      text_(0)
{
}

ToolboxEditTextContext::~ToolboxEditTextContext() {}

void ToolboxEditTextContext::updateData(loka::core::State<loka::core::String> *text)
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
    TEHandle te = controller->ensureEditTextControl(this, textRect_, text_, lifetimeHint());
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
    controller->recordEditHit(rect_, text_, boundary_);
  }
}

short ToolboxEditTextContext::layout(loka::app::scene::IPlatformController *controller,
                                     loka::app::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  short width = 120;
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - state.lineHeight + 2);
  rect.right = static_cast<short>(state.x + width + 3);
  rect.bottom = static_cast<short>(state.y + 6 + 2);
  Rect textRect = rect;
  textRect.left = static_cast<short>(textRect.left + 1);
  textRect.top = static_cast<short>(textRect.top + 2);
  textRect.right = static_cast<short>(textRect.right - 1);
  textRect.bottom = static_cast<short>(textRect.bottom - 1);
  updateData(node_->props.text_);
  updateRect(rect, textRect, static_cast<short>(state.x + 4), state.y);
  state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
  return width;
}

void ToolboxEditTextContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
