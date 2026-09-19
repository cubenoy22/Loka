#include "platform/null/context/NullAttributedTextContext.hpp"

#include <climits>
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace
{
  int extent(int value)
  {
    return value > SHRT_MAX ? SHRT_MAX : value;
  }

  /** Stack-local builder for one measure; only finishLine publishes a line. */
  class Lines
  {
  public:
    Lines(const loka::app::BlockStyle &block, int width)
        : block_(block),
          available_(width),
          width_(0),
          height_(0),
          maxWidth_(0),
          totalHeight_(0),
          count_(0)
    {
    }
    void append(int width, int height, bool runBoundary)
    {
      if (runBoundary && this->block_.hasWrap_ && this->block_.wrap_ != loka::app::TEXT_WRAP_NONE
          && this->available_ > 0 && this->width_ > 0 && this->width_ + width > this->available_)
        this->finishLine();
      if (this->width_ == 0 || height > this->height_)
        this->height_ = height;
      this->width_ = extent(this->width_ + width);
    }
    void finishLine()
    {
      const int height = this->height_ > 0 ? this->height_ : NullTextMetrics(loka::app::TextStyle(), 0).lineHeight();
      int width = this->width_;
      if ((!this->block_.hasWrap_ || this->block_.wrap_ == loka::app::TEXT_WRAP_NONE) && this->available_ > 0
          && width > this->available_ && this->block_.hasTruncation_)
      {
        switch (this->block_.truncation_)
        {
        case loka::app::TEXT_TRUNCATION_NONE:
          break;
        case loka::app::TEXT_TRUNCATION_CLIP:
          width = this->available_;
          break;
        case loka::app::TEXT_TRUNCATION_ELLIPSIS:
        {
          // Same deterministic cell rule as Text, using this line's largest font.
          const NullTextMetrics metrics(loka::app::SizeOf(height), 0);
          const int capacity = this->available_ / metrics.advance();
          width = (capacity > 0 ? capacity : 1) * metrics.advance();
          break;
        }
        }
      }
      if (width > this->maxWidth_)
        this->maxWidth_ = width;
      this->totalHeight_ = extent(this->totalHeight_ + height);
      this->count_ = extent(this->count_ + 1);
      this->width_ = 0;
      this->height_ = 0;
    }
    NullTextMeasurement complete()
    {
      this->finishLine();
      return NullTextMeasurement(static_cast<short>(this->maxWidth_),
                                 static_cast<short>(this->totalHeight_),
                                 static_cast<short>(this->count_));
    }

  private:
    const loka::app::BlockStyle &block_;
    int available_, width_, height_, maxWidth_, totalHeight_, count_;
  };

  NullTextMeasurement emptyMeasurement()
  {
    const NullTextMetrics metrics(loka::app::TextStyle(), 0);
    return NullTextMeasurement(0, static_cast<short>(metrics.lineHeight()), 1);
  }

  NullTextMeasurement measure(const loka::app::AttributedString &value,
                              const loka::app::BlockStyle &block,
                              const loka::app::scene::LayoutState &state,
                              bool &materialized)
  {
    materialized = value.valid();
    if (!materialized)
      return emptyMeasurement();
    Lines lines(block, state.width);
    bool afterCarriageReturn = false;
    for (std::size_t run = 0; run < value.segmentCount(); ++run)
    {
      const loka::app::AttributedString::Segment &segment = value.segment(run);
      const NullTextMetrics metrics(segment.style, &segment.text);
      if (!metrics.materialized())
      {
        materialized = false;
        return emptyMeasurement();
      }
      const loka::core::StringBuffer &text = metrics.text();
      std::size_t index = 0;
      while (index < text.length())
      {
        const unsigned int ch = text.characterAt(index);
        if (afterCarriageReturn && ch == '\n')
        {
          afterCarriageReturn = false;
          ++index;
          continue;
        }
        afterCarriageReturn = false;
        if (ch == '\n' || ch == '\r')
        {
          lines.append(0, metrics.lineHeight(), false);
          lines.finishLine();
          lines.append(0, metrics.lineHeight(), false);
          afterCarriageReturn = ch == '\r';
          ++index;
          continue;
        }
        int width = 0;
        while (index < text.length() && text.characterAt(index) != '\n' && text.characterAt(index) != '\r')
        {
          width = extent(width + metrics.advance());
          ++index;
        }
        lines.append(width, metrics.lineHeight(), true);
      }
    }
    return lines.complete();
  }

  bool fits(const loka::app::AttributedString &value, const loka::app::BlockStyle &block, const loka::core::Frame &seat)
  {
    loka::app::scene::LayoutState state;
    state.width = static_cast<short>(seat.width);
    bool materialized = false;
    const NullTextMeasurement output = measure(value, block, state, materialized);
    return materialized && output.width() <= seat.width && output.height() <= seat.height;
  }

  class NullAttributedTextNodeHandler : public loka::app::scene::RetainedNodeHandler<NullAttributedTextNodeHandler,
                                                                                     loka::app::AttributedTextNode,
                                                                                     NullAttributedTextContext>
  {
  public:
    static loka::app::AttributedTextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asAttributedTextNode() : 0;
    }
    static NullAttributedTextContext *create(loka::app::AttributedTextNode *node,
                                             loka::app::scene::IPlatformController *,
                                             const loka::app::scene::LayoutState &)
    {
      return new NullAttributedTextContext(node);
    }
  };
  NullAttributedTextNodeHandler handler;
} // namespace

NullAttributedTextContext::NullAttributedTextContext(loka::app::AttributedTextNode *node)
    : node_(node)
{
}

short NullAttributedTextContext::layout(loka::app::scene::IPlatformController *controller,
                                        loka::app::scene::LayoutState &state)
{
  bool materialized = false;
  const loka::app::AttributedString empty;
  this->measurement_ = measure(this->node_->props.text_ ? this->node_->props.text_->get() : empty,
                               this->node_->props.blockStyle_,
                               state,
                               materialized);
  state.height = this->measurement_.height();
  this->invalidatePresentation();
  loka::app::scene::PaintScope scope;
  if (materialized && controller
      && static_cast<NullScenePlatformController *>(controller)->queryPaintProjectionScope(scope))
  {
    this->placement_.complete(loka::core::Frame(state.x, state.y, state.width, state.height), scope);
    this->placedBlock_ = this->node_->props.blockStyle_;
  }
  const int bottom = state.y + state.height + state.spacing;
  return static_cast<short>(bottom < SHRT_MIN ? SHRT_MIN : extent(bottom));
}

void NullAttributedTextContext::invalidatePresentation()
{
  this->placement_.invalidate();
  this->presented_.invalidate();
}

void NullAttributedTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact,
                                              loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
    this->invalidatePresentation();
}

loka::app::scene::PaintAnswer
NullAttributedTextContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  if (!this->node_->props.text_ || !this->node_->props.text_->get().valid())
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  loka::core::Frame seat;
  if (query.placement != PLACEMENT_ELIGIBLE || !this->placement_.query(query.scope, seat)
      || !(this->placedBlock_ == this->node_->props.blockStyle_))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  if (this->presented_.scope() != query.scope)
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  const loka::app::AttributedString &current = this->node_->props.text_->get();
  PaintDamage damage = {query.scope, seat.x, seat.y, 0, 0, PAINT_COVERAGE_ERASE_AND_PAINT};
  if (current == this->presented_.value())
    return PaintAnswer::exact(damage);
  if (!fits(current, this->node_->props.blockStyle_, seat))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  damage.width = seat.width;
  damage.height = seat.height;
  return PaintAnswer::exact(damage);
}

bool NullAttributedTextContext::commitPresented(const loka::app::AttributedString &value,
                                                const loka::app::scene::PaintScope &scope)
{
  loka::core::Frame seat;
  if (!this->placement_.query(scope, seat) || !fits(value, this->node_->props.blockStyle_, seat))
  {
    this->presented_.invalidate();
    return false;
  }
  this->placedBlock_ = this->node_->props.blockStyle_;
  this->presented_.commit(value, scope);
  return true;
}

void RegisterNullAttributedTextNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&handler);
}
const void *NullAttributedTextNodeHandlerKey()
{
  return handler.nodeTypeKey();
}
bool IsNullAttributedTextNodeHandler(const loka::app::scene::IPlatformNodeHandler *candidate)
{
  return candidate == &handler;
}
