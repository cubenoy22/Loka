#include "context/ToolboxCellContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "loka/platform/StringUTF8.hpp"
#include <cstring>
#include <string>

namespace
{
  void BuildPascalString(const loka::core::String &value, Str255 text)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      text[0] = 0;
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
  }
}

ToolboxCellContext::ToolboxCellContext(loka::app::CellNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      text_(0)
{
}

ToolboxCellContext::~ToolboxCellContext() {}

void ToolboxCellContext::updateData(loka::core::State<loka::core::String> *text)
{
  text_ = text;
}

void ToolboxCellContext::updateRect(const Rect &rect)
{
  rect_ = rect;
}

void ToolboxCellContext::draw(ToolboxScenePlatformController *controller)
{
  Rect drawRect = rect_;
  EraseRect(&drawRect);
  FrameRect(&drawRect);
  if (controller)
  {
    controller->recordCellHit(rect_, node_ ? node_->props.onClick_ : 0, boundary_, this, text_);
  }
  if (!text_)
  {
    return;
  }
  Str255 text;
  BuildPascalString(text_->get(), text);
  if (text[0] == 0)
  {
    return;
  }
  short textWidth = StringWidth(text);
  FontInfo info;
  GetFontInfo(&info);
  short textHeight = static_cast<short>(info.ascent + info.descent);
  short rectWidth = static_cast<short>(drawRect.right - drawRect.left);
  short rectHeight = static_cast<short>(drawRect.bottom - drawRect.top);
  short textX = static_cast<short>(drawRect.left + (rectWidth - textWidth) / 2);
  short textY = static_cast<short>(drawRect.top + (rectHeight - textHeight) / 2 + info.ascent);
  MoveTo(textX, textY);
  DrawString(text);
}

short ToolboxCellContext::layout(loka::core::scene::IPlatformController *,
                                 loka::core::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  short width = state.width;
  if (width <= 0 && node_->props.text_)
  {
    width = ToolboxMeasureTextWidth(node_->props.text_->get());
  }
  short height = state.height;
  if (height <= 0)
  {
    height = static_cast<short>(state.lineHeight + 6);
  }
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y);
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(state.y + height);
  updateData(node_->props.text_);
  updateRect(rect);
  state.y = static_cast<short>(state.y + height);
  if (state.height <= 0)
  {
    state.y = static_cast<short>(state.y + state.spacing);
  }
  return width;
}

void ToolboxCellContext::render(loka::core::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
