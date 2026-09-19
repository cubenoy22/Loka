#ifndef LOKA_TESTS_NULL_TEXT_METRICS_HPP
#define LOKA_TESTS_NULL_TEXT_METRICS_HPP
#include "app/style/Style.hpp"
#include "app/scene/Node.hpp"
#include "core/String.hpp"
#include "core/StringBuffer.hpp"

/** Completed geometry from one deterministic null-platform text measure. */
class NullTextMeasurement
{
public:
  NullTextMeasurement();
  NullTextMeasurement(short width, short height, short lineCount);

  short width() const;
  short height() const;
  short lineCount() const;

private:
  short width_;
  short height_;
  short lineCount_;
};

/** One resolved run: deterministic metrics and a decoded buffer owned by this measure. */
class NullTextMetrics
{
public:
  NullTextMetrics(const loka::app::TextStyle &style, const loka::core::String *value);
  int lineHeight() const
  {
    return this->lineHeight_;
  }
  int advance() const
  {
    return (this->lineHeight_ + 2) / 3;
  }
  bool materialized() const
  {
    return this->materialized_;
  }
  const loka::core::StringBuffer &text() const
  {
    return this->text_;
  }

private:
  loka::core::StringBuffer text_;
  int lineHeight_;
  bool materialized_;
};

/** Measures plain text using resolved style; no Node or borrowed State is retained. */
NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized);
#endif
