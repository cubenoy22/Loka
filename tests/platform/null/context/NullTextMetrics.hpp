#ifndef LOKA_TESTS_NULL_TEXT_METRICS_HPP
#define LOKA_TESTS_NULL_TEXT_METRICS_HPP
#include "app/style/Style.hpp"
#include "app/scene/Node.hpp"
#include "core/String.hpp"
#include "core/StringBuffer.hpp"
#include <vector>

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

/** One code point and the resolved metrics that contribute to its line. */
struct NullTextCharacter
{
  NullTextCharacter(unsigned int codePoint, int characterAdvance, int characterHeight)
      : value(codePoint),
        advance(characterAdvance),
        lineHeight(characterHeight)
  {
  }
  unsigned int value;
  int advance;
  int lineHeight;
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
  std::size_t length() const
  {
    return this->text_.length();
  }
  NullTextCharacter characterAt(std::size_t index) const
  {
    return NullTextCharacter(this->text_.characterAt(index), this->advance(), this->lineHeight());
  }
  NullTextMeasurement measure(const loka::app::BlockStyle &block, short availableWidth) const;

private:
  loka::core::StringBuffer text_;
  int lineHeight_;
  bool materialized_;
};

/** Stack-local joined code points. Segments select metrics, never break positions. */
class NullTextLayout
{
public:
  explicit NullTextLayout(int emptyLineHeight);
  bool append(const NullTextMetrics &run);
  NullTextMeasurement measure(const loka::app::BlockStyle &block, short availableWidth) const;

  std::size_t length() const
  {
    return this->characters_.size();
  }
  const NullTextCharacter &characterAt(std::size_t index) const
  {
    return this->characters_[index];
  }

private:
  std::vector<NullTextCharacter> characters_;
  int emptyLineHeight_;
};

/** Measures plain text using resolved style; no Node or borrowed State is retained. */
NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized);
#endif
