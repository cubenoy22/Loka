#ifndef LOKA_TESTS_NULL_TEXT_METRICS_HPP
#define LOKA_TESTS_NULL_TEXT_METRICS_HPP
#include "app/layout/TextLineBreaker.hpp"
#include "app/scene/Node.hpp"
#include <vector>

namespace loka
{
  namespace app
  {
    namespace testing
    {
      struct NullTextMeasurementAccess;
    }
  } // namespace app
} // namespace loka

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
  NullTextMeasurement(const loka::app::TextLineBreaker &result,
                      const loka::app::BlockStyle &block,
                      short availableWidth);
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
  friend struct loka::app::testing::NullTextMeasurementAccess;
  short width_, height_, lineCount_;
  loka::core::Frame firstLine_;
  std::vector<loka::core::Frame> additionalLines_;
};

namespace loka
{
  namespace app
  {
    namespace testing
    {
      /** Read-only facts of a completed Null projection, never recomputed by tests.
          Requires a measurement built from a valid breaker; fallback measurements
          carry aggregate counts only and have no line facts. */
      struct NullTextMeasurementAccess
      {
        static const core::Frame &line(const NullTextMeasurement &value, std::size_t index)
        {
          assert(value.firstLine_.width >= 0);
          assert(index < static_cast<std::size_t>(value.lineCount_));
          assert(index == 0 || index - 1 < value.additionalLines_.size());
          return index == 0 ? value.firstLine_ : value.additionalLines_[index - 1];
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka

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
