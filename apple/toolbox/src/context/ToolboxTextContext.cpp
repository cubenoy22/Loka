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

  void DrawUtf8At(short x, short y, const std::string &utf8)
  {
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

  std::string TruncateWithEllipsis(const loka::core::String &value, short maxWidth)
  {
    if (maxWidth <= 0)
    {
      return std::string();
    }
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return std::string();
    }
    if (ToolboxMeasureTextWidth(value) <= maxWidth)
    {
      return utf8;
    }

    const short ellipsisWidth = ToolboxMeasureTextWidth(loka::core::String::Literal("..."));
    if (ellipsisWidth >= maxWidth)
    {
      return std::string("...");
    }

    std::string prefix = utf8;
    while (!prefix.empty())
    {
      std::string candidate = prefix + "...";
      if (ToolboxMeasureTextWidth(loka::core::String(candidate)) <= maxWidth)
      {
        return candidate;
      }
      prefix.erase(prefix.size() - 1);
    }
    return std::string("...");
  }
}

ToolboxTextContext::ToolboxTextContext(loka::app::TextNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      textX_(0),
      textY_(0),
      maxWidth_(0),
      wrapMode_(loka::app::TEXT_WRAP_NONE),
      truncationMode_(loka::app::TEXT_TRUNCATION_NONE),
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
  if (maxWidth_ > 0)
  {
    Rect clipRect = rect_;
    short oldX = textX_;
    short oldY = textY_;
    RgnHandle oldClip = NewRgn();
    if (oldClip)
    {
      GetClip(oldClip);
    }
    ClipRect(&clipRect);
    if (truncationMode_ == loka::app::TEXT_TRUNCATION_ELLIPSIS)
    {
      const std::string truncated = TruncateWithEllipsis(text_->get(), maxWidth_);
      DrawUtf8At(oldX, oldY, truncated);
    }
    else
    {
      DrawStringAt(oldX, oldY, text_->get());
    }
    if (oldClip)
    {
      SetClip(oldClip);
      DisposeRgn(oldClip);
    }
  }
  else
  {
    DrawStringAt(textX_, textY_, text_->get());
  }
  if (controller)
  {
    controller->recordTextHit(rect_, textX_, textY_, text_, boundary_);
  }
}

short ToolboxTextContext::layout(loka::app::scene::IPlatformController *controller,
                                 loka::app::scene::LayoutState &state)
{
  if (!node_ || !node_->props.text_)
  {
    return 0;
  }
  if (node_->props.hasAttr_)
  {
    wrapMode_ = node_->props.attr_.hasWrapValue_ ? static_cast<int>(node_->props.attr_.wrapValue_)
                                                 : static_cast<int>(loka::app::TEXT_WRAP_NONE);
    truncationMode_ = node_->props.attr_.hasTruncationValue_
                          ? static_cast<int>(node_->props.attr_.truncationValue_)
                          : static_cast<int>(loka::app::TEXT_TRUNCATION_NONE);
  }
  else
  {
    wrapMode_ = static_cast<int>(loka::app::TEXT_WRAP_NONE);
    truncationMode_ = static_cast<int>(loka::app::TEXT_TRUNCATION_NONE);
  }
  const loka::core::String &value = node_->props.text_->get();
  short measuredWidth = ToolboxMeasureTextWidth(value);
  short width = measuredWidth;
  if (state.width > 0)
  {
    maxWidth_ = state.width;
    if (width > maxWidth_)
    {
      width = maxWidth_;
    }
  }
  else
  {
    maxWidth_ = 0;
  }
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

void ToolboxTextContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
