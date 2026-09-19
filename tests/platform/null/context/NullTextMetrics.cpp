#include "platform/null/context/NullTextMetrics.hpp"
#include <climits>
#include "platform/StringUTF8.hpp"

NullTextMetrics::NullTextMetrics(const loka::app::TextStyle &style, const loka::core::String *value)
    : text_(loka::core::StringEncodingUtf32),
      lineHeight_(style.hasFontSize_ ? style.fontSize_ : 12),
      materialized_(true)
{
  if (value)
  {
    std::string utf8;
    this->materialized_ = loka::platform::CollectUtf8(*value, utf8) && this->text_.assignFromUtf8(utf8);
  }
}

namespace
{
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

  int WrapCapacityForWidth(short width, int advance)
  {
    if (width <= 0)
    {
      return 0;
    }
    const int capacity = width / advance;
    return capacity > 0 ? capacity : 1;
  }

} // namespace

/** materialized (optional) reports whether the String could be rendered at all. A
    platform String that refuses UTF-8 materialization measures as nothing, and that
    "nothing" must never become a presented value (AGENTS.md failure-degradation). */
NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized)
{
  const NullTextMetrics metrics(style, value);
  const int lineHeight = metrics.lineHeight();
  const int advance = metrics.advance();
  if (materialized)
    *materialized = metrics.materialized();
  if (!metrics.materialized())
    return NullTextMeasurement(0, ClampExtentToShort(lineHeight), 1);
  const loka::core::StringBuffer &text = metrics.text();
  const int capacity = WrapCapacityForWidth(state.width, advance);
  const loka::app::TextWrap wrap = (block.hasWrap_ ? block.wrap_ : loka::app::TEXT_WRAP_NONE);
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

  int measuredWidth = lines.maxColumns * advance;
  if (wrap == loka::app::TEXT_WRAP_NONE && state.width > 0 && measuredWidth > state.width)
  {
    const loka::app::TextTruncation truncation =
        (block.hasTruncation_ ? block.truncation_ : loka::app::TEXT_TRUNCATION_NONE);
    if (truncation == loka::app::TEXT_TRUNCATION_CLIP)
    {
      measuredWidth = state.width;
    }
    else if (truncation == loka::app::TEXT_TRUNCATION_ELLIPSIS)
    {
      measuredWidth = capacity * advance;
    }
  }
  const int measuredHeight = lines.lineCount * lineHeight;
  return NullTextMeasurement(
      ClampExtentToShort(measuredWidth), ClampExtentToShort(measuredHeight), ClampExtentToShort(lines.lineCount));
}

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
