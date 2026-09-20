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
  const loka::core::Frame extent = loka::app::SyntheticTextExtent(result, block, availableWidth);
  return NullTextMeasurement(static_cast<short>(extent.width),
                             static_cast<short>(extent.height),
                             static_cast<short>(result.lineCount() > SHRT_MAX ? SHRT_MAX : result.lineCount()));
}
