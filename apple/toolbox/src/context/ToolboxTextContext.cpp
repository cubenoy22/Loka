#include "context/ToolboxTextContext.hpp"
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
      // Remove one UTF-8 code point from the tail.
      std::size_t pos = prefix.size();
      if (pos == 0)
      {
        break;
      }
      do
      {
        --pos;
      } while (pos > 0 && (static_cast<unsigned char>(prefix[pos]) & 0xC0u) == 0x80u);
      prefix.erase(pos);
    }
    return std::string("...");
  }

  short MeasureWrappedTextHeight(const loka::core::String &value, short maxWidth, short lineHeight, bool charWrap)
  {
    if (maxWidth <= 0 || lineHeight <= 0)
    {
      return lineHeight;
    }
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8) || utf8.empty())
    {
      return lineHeight;
    }

    int lines = 1;
    std::string current;
    std::string currentWord;
    std::size_t i = 0;
    while (i < utf8.size())
    {
      std::size_t cpStart = i;
      ++i;
      while (i < utf8.size() && (static_cast<unsigned char>(utf8[i]) & 0xC0u) == 0x80u)
      {
        ++i;
      }
      const std::string cp = utf8.substr(cpStart, i - cpStart);
      const bool isSpace = (cp.size() == 1 && (cp[0] == ' ' || cp[0] == '\t'));
      const bool isBreak = (cp.size() == 1 && (cp[0] == '\n' || cp[0] == '\r'));
      if (isBreak)
      {
        ++lines;
        current.clear();
        currentWord.clear();
        continue;
      }

      if (charWrap || isSpace)
      {
        std::string next = current + cp;
        if (ToolboxMeasureTextWidth(loka::core::String(next)) > maxWidth && !current.empty())
        {
          ++lines;
          current = cp;
        }
        else
        {
          current = next;
        }
        if (isSpace)
        {
          currentWord.clear();
        }
        else
        {
          currentWord = cp;
        }
        continue;
      }

      // word wrap path: keep token together, but force-break long tokens
      std::string nextWord = currentWord + cp;
      std::string candidate = current + cp;
      if (ToolboxMeasureTextWidth(loka::core::String(candidate)) > maxWidth && !current.empty())
      {
        ++lines;
        current = cp;
        currentWord = cp;
      }
      else
      {
        current = candidate;
        currentWord = nextWord;
      }
    }

    int total = lines * lineHeight;
    if (total < lineHeight)
    {
      total = lineHeight;
    }
    return static_cast<short>(total);
  }
} // namespace

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

short ToolboxTextContext::visibleWidth() const
{
  if (!text_)
  {
    return 0;
  }
  short width = ToolboxMeasureTextWidth(text_->get());
  const short maxWidth = static_cast<short>(rect_.right - rect_.left);
  if (maxWidth > 0 && width > maxWidth)
  {
    width = maxWidth;
  }
  return width;
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
    if (oldClip != 0)
    {
      GetClip(oldClip);
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
      SetClip(oldClip);
      DisposeRgn(oldClip);
    }
    else
    {
      // Low-memory fallback: draw without changing clip state.
      DrawStringAt(oldX, oldY, text_->get());
    }
  }
  else
  {
    DrawStringAt(textX_, textY_, text_->get());
  }
  if (controller)
  {
    controller->recordTextHit(
        rect_, textX_, textY_, text_, boundary_, wrapMode_ != loka::app::TEXT_WRAP_NONE, visibleWidth());
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
    wrapMode_ = node_->props.attr_.hasWrapValue_ ? node_->props.attr_.wrapValue_ : loka::app::TEXT_WRAP_NONE;
    truncationMode_ =
        node_->props.attr_.hasTruncationValue_ ? node_->props.attr_.truncationValue_ : loka::app::TEXT_TRUNCATION_NONE;
  }
  else
  {
    wrapMode_ = loka::app::TEXT_WRAP_NONE;
    truncationMode_ = loka::app::TEXT_TRUNCATION_NONE;
  }
  const loka::core::String &value = node_->props.text_->get();
  short measuredWidth = ToolboxMeasureTextWidth(value);
  short width = measuredWidth;
  const bool wrapWord = (wrapMode_ == loka::app::TEXT_WRAP_WORD);
  const bool wrapChar = (wrapMode_ == loka::app::TEXT_WRAP_CHAR);
  short effectiveLineHeight = state.lineHeight;
  if (state.width > 0)
  {
    maxWidth_ = state.width;
    if (wrapWord || wrapChar)
    {
      effectiveLineHeight =
          MeasureWrappedTextHeight(value, maxWidth_, state.lineHeight > 0 ? state.lineHeight : 12, wrapChar);
    }
    width = maxWidth_;
  }
  else
  {
    maxWidth_ = 0;
  }
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - effectiveLineHeight + 2);
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(rect.top + effectiveLineHeight + 4);
  updateData(node_->props.text_);
  updateRect(rect, state.x, state.y);
  state.y = static_cast<short>(state.y + effectiveLineHeight + state.spacing);
  return width;
}

void ToolboxTextContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}
