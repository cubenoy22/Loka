#include "platform/null/context/NullTextMetrics.hpp"
#include <climits>

NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized)
{
  const loka::app::SyntheticTextWidthSource source(value ? *value : loka::core::String(), style);
  const loka::app::TextLineBreaker result(source, block, state.width, style);
  if (materialized)
    *materialized = result.valid();
  return result.valid() ? MeasureNullTextLines(result, block, state.width)
                        : NullTextMeasurement(0, static_cast<short>(style.hasFontSize_ ? style.fontSize_ : 12), 1);
}

NullTextMeasurement
MeasureNullTextLines(const loka::app::TextLineBreaker &result, const loka::app::BlockStyle &block, short availableWidth)
{
  int maxWidth = 0;
  for (std::size_t i = 0; i < result.lineCount(); ++i)
  {
    const loka::app::TextLineRecord &line = result.line(i);
    int width = line.width;
    if ((!block.hasWrap_ || block.wrap_ == loka::app::TEXT_WRAP_NONE) && availableWidth > 0 && width > availableWidth
        && block.hasTruncation_)
    {
      switch (block.truncation_)
      {
      case loka::app::TEXT_TRUNCATION_NONE:
        break;
      case loka::app::TEXT_TRUNCATION_CLIP:
        width = availableWidth;
        break;
      case loka::app::TEXT_TRUNCATION_ELLIPSIS:
      {
        const int size = loka::app::SizeOf(line.metrics.ascent + line.metrics.descent).fontSize_;
        const int advance = (size + 2) / 3;
        const int capacity = availableWidth / advance;
        width = (capacity > 0 ? capacity : 1) * advance;
        break;
      }
      }
    }
    if (width > maxWidth)
      maxWidth = width;
  }
  return NullTextMeasurement(static_cast<short>(maxWidth),
                             result.height(),
                             static_cast<short>(result.lineCount() > SHRT_MAX ? SHRT_MAX : result.lineCount()));
}
