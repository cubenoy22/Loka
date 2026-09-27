#include "platform/null/context/NullTextMetrics.hpp"
#include <climits>
#include "app/layout/AlignedLineOffset.hpp"

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
  return NullTextMeasurement(result, block, availableWidth);
}

NullTextMeasurement::NullTextMeasurement(const loka::app::TextLineBreaker &result,
                                         const loka::app::BlockStyle &block,
                                         short availableWidth)
    : width_(0),
      height_(result.height()),
      lineCount_(static_cast<short>(result.lineCount() > SHRT_MAX ? SHRT_MAX : result.lineCount()))
{
  int y = 0;
  int maxWidth = 0;
  if (result.lineCount() > 1)
    this->additionalLines_.reserve(result.lineCount() - 1);
  for (std::size_t i = 0; i < result.lineCount(); ++i)
  {
    const loka::app::TextLineRecord &line = result.line(i);
    const int painted = loka::app::SyntheticTextLineWidth(line, block, availableWidth);
    const int x = loka::app::AlignedLineOffset(
        availableWidth, painted, block.hasAlign_ ? block.align_ : loka::app::TEXT_ALIGN_LEFT);
    const int height = line.metrics.ascent + line.metrics.descent;
    const loka::core::Frame frame(x, y, painted, height);
    if (i == 0)
      this->firstLine_ = frame;
    else
      this->additionalLines_.push_back(frame);
    if (painted > maxWidth)
      maxWidth = painted;
    y += height;
  }
  this->width_ = static_cast<short>(maxWidth);
}
