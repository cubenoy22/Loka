#ifndef LOKA_TESTS_NULL_TEXT_METRICS_HPP
#define LOKA_TESTS_NULL_TEXT_METRICS_HPP
#include "app/layout/TextLineBreaker.hpp"
#include "app/scene/Node.hpp"

/** Completed geometry from one deterministic null-platform text measure. */
class NullTextMeasurement
{
public:
  NullTextMeasurement(short width = 0, short height = 0, short lineCount = 0)
      : width_(width),
        height_(height),
        lineCount_(lineCount)
  {
  }
  short width() const
  {
    return this->width_;
  }
  short height() const
  {
    return this->height_;
  }
  short lineCount() const
  {
    return this->lineCount_;
  }

private:
  short width_, height_, lineCount_;
};

/** Null's synthetic truncation applies to completed common line geometry. */
NullTextMeasurement MeasureNullTextLines(const loka::app::TextLineBreaker &result,
                                         const loka::app::BlockStyle &block,
                                         short availableWidth);

NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized);
#endif
