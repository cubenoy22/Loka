#include "platform/null/context/NullTextContext.hpp"

#include <climits>

#include "app/nodes/Text.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "core/StringBuffer.hpp"
#include "platform/StringUTF8.hpp"

namespace
{
  const int kFixedAdvance = 4;
  const int kDefaultLineHeight = 10;

  struct LineGeometry
  {
    LineGeometry()
        : lineCount(1),
          maxColumns(0)
    {
    }

    int lineCount;
    int maxColumns;
  };

  bool IsLineBreak(unsigned int value)
  {
    return value == '\n' || value == '\r';
  }

  bool IsWordSpace(unsigned int value)
  {
    return value == ' ' || value == '\t';
  }

  void FinishLine(int columns, LineGeometry &geometry)
  {
    if (columns > geometry.maxColumns)
    {
      geometry.maxColumns = columns;
    }
  }

  void SkipLineFeedAfterCarriageReturn(const loka::core::StringBuffer &text, std::size_t &index)
  {
    if (text.characterAt(index) == '\r' && index + 1 < text.length() && text.characterAt(index + 1) == '\n')
    {
      ++index;
    }
  }

  LineGeometry MeasureUnwrapped(const loka::core::StringBuffer &text)
  {
    LineGeometry geometry;
    int columns = 0;
    for (std::size_t i = 0; i < text.length(); ++i)
    {
      const unsigned int value = text.characterAt(i);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        SkipLineFeedAfterCarriageReturn(text, i);
      }
      else
      {
        ++columns;
      }
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  LineGeometry MeasureCharacterWrapped(const loka::core::StringBuffer &text, int capacity)
  {
    if (capacity <= 0)
    {
      return MeasureUnwrapped(text);
    }

    LineGeometry geometry;
    int columns = 0;
    for (std::size_t i = 0; i < text.length(); ++i)
    {
      const unsigned int value = text.characterAt(i);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        SkipLineFeedAfterCarriageReturn(text, i);
        continue;
      }
      if (columns == capacity)
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
      }
      ++columns;
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  void PlaceWord(int wordLength, int capacity, int &columns, LineGeometry &geometry)
  {
    if (wordLength <= capacity)
    {
      if (columns > 0 && columns + wordLength > capacity)
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
      }
      columns += wordLength;
      return;
    }
    if (columns > 0)
    {
      FinishLine(columns, geometry);
      ++geometry.lineCount;
      columns = 0;
    }
    while (wordLength > capacity)
    {
      FinishLine(capacity, geometry);
      ++geometry.lineCount;
      wordLength -= capacity;
    }
    columns = wordLength;
  }

  LineGeometry MeasureWordWrapped(const loka::core::StringBuffer &text, int capacity)
  {
    if (capacity <= 0)
    {
      return MeasureUnwrapped(text);
    }

    LineGeometry geometry;
    int columns = 0;
    std::size_t index = 0;
    while (index < text.length())
    {
      const unsigned int value = text.characterAt(index);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        SkipLineFeedAfterCarriageReturn(text, index);
        ++index;
        continue;
      }
      if (IsWordSpace(value))
      {
        if (columns == capacity)
        {
          FinishLine(columns, geometry);
          ++geometry.lineCount;
          columns = 0;
        }
        ++columns;
        ++index;
        continue;
      }

      int wordLength = 0;
      while (index < text.length())
      {
        const unsigned int wordValue = text.characterAt(index);
        if (IsLineBreak(wordValue) || IsWordSpace(wordValue))
        {
          break;
        }
        ++wordLength;
        ++index;
      }
      PlaceWord(wordLength, capacity, columns, geometry);
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  short ClampExtentToShort(int value)
  {
    if (value <= 0)
    {
      return 0;
    }
    if (value > SHRT_MAX)
    {
      return SHRT_MAX;
    }
    return static_cast<short>(value);
  }

  short ClampCoordinateToShort(int value)
  {
    if (value < SHRT_MIN)
    {
      return SHRT_MIN;
    }
    if (value > SHRT_MAX)
    {
      return SHRT_MAX;
    }
    return static_cast<short>(value);
  }

  int WrapCapacityForWidth(short width)
  {
    if (width <= 0)
    {
      return 0;
    }
    const int capacity = width / kFixedAdvance;
    return capacity > 0 ? capacity : 1;
  }

  loka::app::TextWrap ResolveWrap(const loka::app::TextNode *node)
  {
    if (!node || !node->props.hasAttr_ || !node->props.attr_.hasWrapValue_)
    {
      return loka::app::TEXT_WRAP_NONE;
    }
    return node->props.attr_.wrapValue_;
  }

  loka::app::TextTruncation ResolveTruncation(const loka::app::TextNode *node)
  {
    if (!node || !node->props.hasAttr_ || !node->props.attr_.hasTruncationValue_)
    {
      return loka::app::TEXT_TRUNCATION_NONE;
    }
    return node->props.attr_.truncationValue_;
  }

  /** materialized (optional) reports whether the String could be rendered at all. A
      platform String that refuses UTF-8 materialization measures as nothing, and that
      "nothing" must never become a presented value (AGENTS.md failure-degradation). */
  NullTextMeasurement MeasureText(const loka::app::TextNode *node,
                                  const loka::app::scene::LayoutState &state,
                                  const loka::core::String *rendered = 0,
                                  bool *materialized = 0)
  {
    const int lineHeight = state.lineHeight > 0 ? state.lineHeight : kDefaultLineHeight;
    if (materialized)
      *materialized = true;
    if (!node || !node->props.text_)
    {
      return NullTextMeasurement(0, ClampExtentToShort(lineHeight), 1);
    }

    const loka::core::String &value = rendered ? *rendered : node->props.text_->get();
    std::string utf8;
    loka::core::StringBuffer text(loka::core::StringEncodingUtf32);
    if (!loka::platform::CollectUtf8(value, utf8) || !text.assignFromUtf8(utf8))
    {
      if (materialized)
        *materialized = false;
      return NullTextMeasurement(0, ClampExtentToShort(lineHeight), 1);
    }
    const int capacity = WrapCapacityForWidth(state.width);
    const loka::app::TextWrap wrap = ResolveWrap(node);
    LineGeometry lines;
    switch (wrap)
    {
    case loka::app::TEXT_WRAP_NONE:
      lines = MeasureUnwrapped(text);
      break;
    case loka::app::TEXT_WRAP_WORD:
      lines = MeasureWordWrapped(text, capacity);
      break;
    case loka::app::TEXT_WRAP_CHAR:
      lines = MeasureCharacterWrapped(text, capacity);
      break;
    }

    int measuredWidth = lines.maxColumns * kFixedAdvance;
    if (wrap == loka::app::TEXT_WRAP_NONE && state.width > 0 && measuredWidth > state.width)
    {
      const loka::app::TextTruncation truncation = ResolveTruncation(node);
      if (truncation == loka::app::TEXT_TRUNCATION_CLIP)
      {
        measuredWidth = state.width;
      }
      else if (truncation == loka::app::TEXT_TRUNCATION_ELLIPSIS)
      {
        measuredWidth = capacity * kFixedAdvance;
      }
    }
    const int measuredHeight = lines.lineCount * lineHeight;
    return NullTextMeasurement(
        ClampExtentToShort(measuredWidth),
        ClampExtentToShort(measuredHeight),
        ClampExtentToShort(lines.lineCount));
  }

  bool FitsTextSeat(const loka::app::TextNode *node,
                    const loka::core::String &value,
                    const loka::core::Frame &seat,
                    const NullTextMeasurement &placed)
  {
    loka::app::scene::LayoutState measure;
    measure.width = static_cast<short>(seat.width);
    measure.lineHeight = placed.lineCount() > 0 ? placed.height() / placed.lineCount() : 0;
    bool materialized = true;
    const NullTextMeasurement output = MeasureText(node, measure, &value, &materialized);
    return materialized && output.width() <= seat.width && output.height() <= seat.height;
  }

  class NullTextNodeHandler
      : public loka::app::scene::RetainedNodeHandler<NullTextNodeHandler,
                                                     loka::app::TextNode,
                                                     NullTextContext>
  {
  public:
    static loka::app::TextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asTextNode() : 0;
    }

    static NullTextContext *create(loka::app::TextNode *text,
                                   loka::app::scene::IPlatformController *controller,
                                   const loka::app::scene::LayoutState &state)
    {
      (void)state;
      NullScenePlatformController *nullPlatform = static_cast<NullScenePlatformController *>(controller);
      (void)nullPlatform;
      return new NullTextContext(text);
    }
  };

  NullTextNodeHandler gNullTextNodeHandler;
} // namespace

NullTextMeasurement::NullTextMeasurement()
    : width_(0),
      height_(0),
      lineCount_(0)
{
}

NullTextMeasurement::NullTextMeasurement(short width, short height, short lineCount)
    : width_(width),
      height_(height),
      lineCount_(lineCount)
{
}

short NullTextMeasurement::width() const
{
  return this->width_;
}

short NullTextMeasurement::height() const
{
  return this->height_;
}

short NullTextMeasurement::lineCount() const
{
  return this->lineCount_;
}

NullTextContext::NullTextContext(loka::app::TextNode *node)
    : loka::app::scene::NativeNodeContext(),
      node_(node),
      measurement_()
{
}

NullTextContext::~NullTextContext()
{
  this->node_ = 0;
}

void NullTextContext::readLifecycleFactOnAttach()
{
  // Presentation is completed by the synchronous Null presenter.
}

short NullTextContext::layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state)
{
  bool materialized = true;
  this->measurement_ = MeasureText(this->node_, state, 0, &materialized);
  state.height = this->measurement_.height();
  this->placement_.invalidate();
  this->presented_.invalidate();
  loka::app::scene::PaintScope scope;
  // An unrenderable value has no placement: the next query refuses instead of
  // comparing against a seat that was measured from nothing.
  if (materialized && this->node_ && controller
      && static_cast<NullScenePlatformController *>(controller)->queryPaintProjectionScope(scope))
  {
    this->placement_.complete(loka::core::Frame(state.x, state.y, state.width, state.height), scope);
    this->placedStyle_ = NullTextPaintStyle(this->node_->props);
  }
  return ClampCoordinateToShort(state.y + state.height + state.spacing);
}

const NullTextMeasurement &NullTextContext::measurement() const
{
  return this->measurement_;
}

void RegisterNullTextNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullTextNodeHandler);
}

NullTextPaintStyle::NullTextPaintStyle(const loka::app::TextProps &props)
    : fontSize(0),
      weight(loka::app::TEXT_WEIGHT_NORMAL),
      wrap(loka::app::TEXT_WRAP_NONE),
      truncation(loka::app::TEXT_TRUNCATION_NONE)
{
  if (!props.hasAttr_)
    return;
  const loka::app::TextAttr &attr = props.attr_;
  fontSize = attr.fontSizeState_ ? attr.fontSizeState_->get() : (attr.hasFontSizeValue_ ? attr.fontSizeValue_ : 0);
  if (attr.hasWeightValue_)
    weight = attr.weightValue_;
  if (attr.hasWrapValue_)
    wrap = attr.wrapValue_;
  if (attr.hasTruncationValue_)
    truncation = attr.truncationValue_;
}
void NullTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact, loka::app::scene::NodeLifecycleFact next)
{
  if (next != loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->presented_.invalidate();
    this->placement_.invalidate();
  }
}
loka::app::scene::PaintAnswer NullTextContext::queryPaintDamage(const loka::app::scene::PaintQuery &query) const
{
  using namespace loka::app::scene;
  loka::core::Frame seat;
  if (query.placement != PLACEMENT_ELIGIBLE || !this->placement_.query(query.scope, seat))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->presented_.isKnown())
    return PaintAnswer::refused(PAINT_REFUSED_HISTORY_UNKNOWN);
  if (this->presented_.scope() != query.scope || !(this->placedStyle_ == NullTextPaintStyle(this->node_->props)))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  if (!this->node_->props.text_)
    return PaintAnswer::refused(PAINT_REFUSED_PROPS_UNRECONCILED);
  const loka::core::String &current = this->node_->props.text_->get();
  PaintDamage damage = {query.scope, seat.x, seat.y, 0, 0, PAINT_COVERAGE_ERASE_AND_PAINT};
  loka::core::StringCompareResult equal = current.compare(this->presented_.value(), false);
  if (equal == loka::core::StringCompareBufferRequired)
    equal = current.compare(this->presented_.value(), true);
  if (equal == loka::core::StringCompareBufferRequired)
  {
    // The current value cannot be materialized, so it cannot be measured or
    // placed; this is a placement refusal, not a stale binding.
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  }
  if (equal == loka::core::StringCompareEqual)
    return PaintAnswer::exact(damage);
  if (!FitsTextSeat(this->node_, current, seat, this->measurement_))
    return PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
  damage.width = seat.width;
  damage.height = seat.height;
  return PaintAnswer::exact(damage);
}
bool NullTextContext::commitPresented(const loka::core::String &value, const loka::app::scene::PaintScope &scope)
{
  loka::core::Frame seat;
  if (!this->placement_.query(scope, seat))
    return false;
  const NullTextPaintStyle current(this->node_->props);
  const bool restyled = !(this->placedStyle_ == current);
  if (restyled)
  {
    // A props-only apply can change the resolved style without a layout pass.
    // The widened presentation reconstructs the whole seat under the current
    // style, so adopt it here and verify coverage below; refusing forever would
    // leave the history UNKNOWN until an unrelated layout.
    this->presented_.invalidate();
    this->placedStyle_ = current;
  }
  if (restyled || !this->presented_.isKnown()
      || value.compare(this->presented_.value(), false) != loka::core::StringCompareEqual)
  {
    if (!FitsTextSeat(this->node_, value, seat, this->measurement_))
    {
      this->presented_.invalidate();
      return false;
    }
  }
  this->presented_.commit(value, scope);
  return true;
}

const void *NullTextNodeHandlerKey()
{
  return gNullTextNodeHandler.nodeTypeKey();
}
bool IsNullTextNodeHandler(const loka::app::scene::IPlatformNodeHandler *handler)
{
  return handler == &gNullTextNodeHandler;
}
