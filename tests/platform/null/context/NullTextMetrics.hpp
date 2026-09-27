#ifndef LOKA_TESTS_NULL_TEXT_METRICS_HPP
#define LOKA_TESTS_NULL_TEXT_METRICS_HPP
#include "app/layout/TextLineBreaker.hpp"
#include "app/layout/MeasurementResult.hpp"
#include "app/scene/Node.hpp"
#include <vector>

class NullTextContext;
class NullAttributedTextContext;

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

/** Null measurement table and its sole completion fact. Aggregate fallback
    geometry is readable after refusal, but never reusable. */
class NullTextMeasurement
{
public:
  NullTextMeasurement(short width = 0, short height = 0, short lineCount = 0)
      : result_(loka::core::Frame(0, 0, width, height)),
        lineCount_(lineCount)
  {
  }
  NullTextMeasurement(const loka::app::TextLineBreaker &result,
                      const loka::app::BlockStyle &block,
                      short availableWidth);
  short width() const
  {
    return static_cast<short>(this->result_.extent().width);
  }
  short height() const
  {
    return static_cast<short>(this->result_.extent().height);
  }
  short lineCount() const
  {
    return this->lineCount_;
  }

  bool reusable(short width) const
  {
    return this->result_.reusable(width);
  }
  void invalidate()
  {
    this->result_.invalidate();
  }

private:
  friend struct loka::app::testing::NullTextMeasurementAccess;
  loka::app::MeasurementResult<short, loka::core::Frame> result_;
  short lineCount_;
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
        static unsigned builds(const NullTextContext &context);
        static unsigned builds(const NullAttributedTextContext &context);
        /** Refuse the Nth following helper call; zero disables injection. */
        static void failMeasure(int count);
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

/** On refusal, out is replaced with non-reusable fallback geometry. */
bool MeasureNullText(const loka::app::TextStyle &style,
                     const loka::app::BlockStyle &block,
                     const loka::core::String *value,
                     const loka::app::scene::LayoutState &state,
                     NullTextMeasurement &out);

/** Shared Null helper refusal point, including attributed measurement. */
bool RefuseNullTextMeasurement();
#endif
