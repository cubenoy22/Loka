#include "ToolboxPropsRefresh.hpp"
#include "context/ToolboxTextContext.hpp"
#include "ToolboxLayoutMetrics.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <cstring>
#include <string>

namespace
{
  class ToolboxTextNodeHandler
      : public loka::app::scene::RetainedNodeHandler<ToolboxTextNodeHandler,
                                                     loka::app::TextNode,
                                                     ToolboxTextContext>
  {
  public:
    static loka::app::TextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asTextNode() : 0;
    }

    static ToolboxTextContext *create(loka::app::TextNode *node,
                                      loka::app::scene::IPlatformController *controller,
                                      const loka::app::scene::LayoutState &state)
    {
      (void)state;
      return new ToolboxTextContext(node, static_cast<ToolboxScenePlatformController *>(controller));
    }
  };

  ToolboxTextNodeHandler gToolboxTextNodeHandler;

  bool DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return false;
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
    return true;
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

ToolboxTextContext::ToolboxTextContext(loka::app::TextNode *node, ToolboxScenePlatformController *controller)
    : ToolboxProjectedNodeContext(controller),
      node_(node),
      rect_(),
      paintRect_(),
      textX_(0),
      textY_(0),
      maxWidth_(0),
      wrapMode_(loka::app::TEXT_WRAP_NONE),
      truncationMode_(loka::app::TEXT_TRUNCATION_NONE),
      text_(0)
{
}

ToolboxTextContext::~ToolboxTextContext() {}

loka::app::scene::PaintAnswer ToolboxTextContext::queryPaintDamage(
    const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (query.placement != PLACEMENT_ELIGIBLE || query.scope != ToolboxPaintScope())
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->node_ || !this->text_ || this->node_->props.text_ != this->text_)
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  loka::core::State<loka::core::String> *live = this->liveTextState();
  PaintAnswer answer = ToolboxExactPaint(this->paintRect_, live && !live->get().equals(this->presented_.value()));
  answer.damage.coverage = PAINT_COVERAGE_ERASE_AND_PAINT;
  return answer;
}

void ToolboxTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                      loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->presented_.invalidate();
    SetRect(&this->paintRect_, 0, 0, 0, 0);
  }
  ToolboxProjectedNodeContext::onFactChanged(previous, next);
}

void ToolboxTextContext::updateData(loka::core::State<loka::core::String> *text)
{
  text_ = text;
}

void ToolboxTextContext::updateRect(const Rect &rect, short textX, short textY)
{
  rect_ = rect;
  this->presented_.invalidate();
  this->paintRect_ = rect;
  if (this->controller() && !this->controller()->intersectWithProjectionClip(rect, this->paintRect_))
    SetRect(&this->paintRect_, 0, 0, 0, 0);
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

void ToolboxTextContext::paint(bool erase)
{
  this->presented_.invalidate();
  if (!this->text_)
    return;
  ToolboxPaintClip clip(this->paintRect_);
  if (!clip.isActive())
    return;
  if (erase)
    EraseRect(&this->paintRect_);
  bool painted = false;
  if (this->maxWidth_ > 0 && this->truncationMode_ == loka::app::TEXT_TRUNCATION_ELLIPSIS)
  {
    const std::string truncated = TruncateWithEllipsis(this->text_->get(), this->maxWidth_);
    DrawUtf8At(this->textX_, this->textY_, truncated);
    // The legacy truncator cannot report conversion refusal, so it cannot
    // establish a completed value. Keep its answer conservative.
  }
  else
  {
    painted = DrawStringAt(this->textX_, this->textY_, this->text_->get());
  }
  if (painted && clip.covers(this->paintRect_))
    this->presented_.commit(this->text_->get(), ToolboxPaintScope());
}

void ToolboxTextContext::repaint()
{
  this->paint(true);
}

void ToolboxTextContext::draw(ToolboxScenePlatformController *controller)
{
  this->paint(false);
  if (controller && this->text_)
    controller->recordTextHit(this->rect_, this->textX_, this->textY_, this->text_, this->boundary_,
                              this->wrapMode_ != loka::app::TEXT_WRAP_NONE, this->visibleWidth(), this);
}

short ToolboxTextContext::layout(loka::app::scene::IPlatformController *controller,
                                 loka::app::scene::LayoutState &state)
{
  (void)controller;
  this->captureProps();
  if (!node_ || !node_->props.text_)
  {
    return 0;
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
          MeasureWrappedTextHeight(value,
                                   maxWidth_,
                                   state.lineHeight > 0 ? state.lineHeight
                                                        : ToolboxLayoutMetrics::kDefaultLineHeight,
                                   wrapChar);
    }
    width = maxWidth_;
  }
  else
  {
    maxWidth_ = 0;
  }
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - effectiveLineHeight + ToolboxLayoutMetrics::kControlAscentInset);
  rect.right = static_cast<short>(state.x + width);
  rect.bottom = static_cast<short>(state.y + ToolboxLayoutMetrics::kControlDescent);
  updateRect(rect, state.x, state.y);
  state.y = static_cast<short>(state.y + effectiveLineHeight + state.spacing);
  return width;
}

void ToolboxTextContext::render(loka::app::scene::IPlatformController *controller)
{
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  draw(toolbox);
}

bool RegisterToolboxTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  return registry.registerHandler(&gToolboxTextNodeHandler);
}

bool ToolboxTextContext::captureProps()
{
  loka::core::State<loka::core::String> *text = this->node_ ? this->node_->props.text_ : 0;
  const bool changed = ToolboxTextProjectionChanged(this->text_, text);
  this->updateData(text);
  if (!this->node_)
    return changed;
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
  return changed;
}

void ToolboxTextContext::onPropsApplied()
{
  this->presented_.invalidate();
  const bool changed = this->captureProps();
  if (changed && this->controller() && this->node_)
  {
    this->controller()->refreshContextProps(this->node_);
  }
}
