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
  int ClampExtent(int value)
  {
    return value > SHRT_MAX ? SHRT_MAX : value;
  }
  bool IsLineBreak(unsigned int value)
  {
    return value == '\n' || value == '\r';
  }
  bool IsWordSpace(unsigned int value)
  {
    return value == ' ' || value == '\t';
  }

  /** Owns only this measure's line totals; no segment identity enters layout. */
  class Lines
  {
  public:
    Lines(int emptyLineHeight, const loka::app::BlockStyle &block, int availableWidth)
        : emptyHeight_(emptyLineHeight),
          block_(block),
          available_(availableWidth),
          width_(0),
          height_(0),
          maxWidth_(0),
          totalHeight_(0),
          count_(0)
    {
    }
    int width() const
    {
      return this->width_;
    }
    void append(int advance, int lineHeight)
    {
      this->width_ = ClampExtent(this->width_ + advance);
      if (lineHeight > this->height_)
        this->height_ = lineHeight;
    }
    void lineBreak(int lineHeight)
    {
      this->append(0, lineHeight);
      this->finishLine();
      this->emptyHeight_ = lineHeight;
    }
    void finishLine()
    {
      const int height = this->height_ > 0 ? this->height_ : this->emptyHeight_;
      int width = this->width_;
      if ((!this->block_.hasWrap_ || this->block_.wrap_ == loka::app::TEXT_WRAP_NONE) && this->available_ > 0
          && width > this->available_ && this->block_.hasTruncation_)
      {
        switch (this->block_.truncation_)
        {
        case loka::app::TEXT_TRUNCATION_NONE:
          break;
        case loka::app::TEXT_TRUNCATION_CLIP:
          width = this->available_;
          break;
        case loka::app::TEXT_TRUNCATION_ELLIPSIS:
        {
          const NullTextMetrics metrics(loka::app::SizeOf(height), 0);
          const int capacity = this->available_ / metrics.advance();
          width = (capacity > 0 ? capacity : 1) * metrics.advance();
          break;
        }
        }
      }
      if (width > this->maxWidth_)
        this->maxWidth_ = width;
      this->totalHeight_ = ClampExtent(this->totalHeight_ + height);
      this->count_ = ClampExtent(this->count_ + 1);
      this->width_ = 0;
      this->height_ = 0;
    }
    NullTextMeasurement complete()
    {
      this->finishLine();
      return NullTextMeasurement(static_cast<short>(this->maxWidth_),
                                 static_cast<short>(this->totalHeight_),
                                 static_cast<short>(this->count_));
    }

  private:
    int emptyHeight_;
    const loka::app::BlockStyle &block_;
    int available_, width_, height_, maxWidth_, totalHeight_, count_;
  };
} // namespace

NullTextLayout::NullTextLayout(int emptyLineHeight)
    : emptyLineHeight_(emptyLineHeight)
{
}

bool NullTextLayout::append(const NullTextMetrics &run)
{
  if (!run.materialized())
    return false;
  for (std::size_t index = 0; index < run.length(); ++index)
    this->characters_.push_back(run.characterAt(index));
  return true;
}

namespace
{
  /** Both inputs expose the same character facts. Plain text borrows its decoded
      buffer; attributed text owns joined rows. Neither exposes segment boundaries. */
  template <typename Characters>
  NullTextMeasurement MeasureCharacters(const Characters &characters,
                                        int emptyLineHeight,
                                        const loka::app::BlockStyle &block,
                                        short availableWidth)
  {
    const loka::app::TextWrap wrap = block.hasWrap_ && availableWidth > 0 ? block.wrap_ : loka::app::TEXT_WRAP_NONE;
    Lines lines(emptyLineHeight, block, availableWidth);
    std::size_t index = 0;
    while (index < characters.length())
    {
      const NullTextCharacter &character = characters.characterAt(index);
      if (IsLineBreak(character.value))
      {
        int breakHeight = character.lineHeight;
        if (character.value == '\r' && index + 1 < characters.length()
            && characters.characterAt(index + 1).value == '\n')
        {
          ++index;
          if (characters.characterAt(index).lineHeight > breakHeight)
            breakHeight = characters.characterAt(index).lineHeight;
        }
        lines.lineBreak(breakHeight);
        ++index;
        continue;
      }
      std::size_t end = index + 1;
      if (wrap == loka::app::TEXT_WRAP_WORD && !IsWordSpace(character.value))
      {
        int wordWidth = character.advance;
        while (end < characters.length() && !IsWordSpace(characters.characterAt(end).value)
               && !IsLineBreak(characters.characterAt(end).value))
        {
          wordWidth = ClampExtent(wordWidth + characters.characterAt(end).advance);
          ++end;
        }
        if (lines.width() > 0 && lines.width() + wordWidth > availableWidth)
          lines.finishLine();
      }
      // A word too wide for an empty line follows Text's forced-character rule.
      // Spaces and CHAR wrapping use the same per-code-point capacity check.
      for (; index < end; ++index)
      {
        const NullTextCharacter &next = characters.characterAt(index);
        if (wrap != loka::app::TEXT_WRAP_NONE && lines.width() > 0 && lines.width() + next.advance > availableWidth)
          lines.finishLine();
        lines.append(next.advance, next.lineHeight);
      }
    }
    return lines.complete();
  }
} // namespace

NullTextMeasurement NullTextLayout::measure(const loka::app::BlockStyle &block, short availableWidth) const
{
  return MeasureCharacters(*this, this->emptyLineHeight_, block, availableWidth);
}

NullTextMeasurement NullTextMetrics::measure(const loka::app::BlockStyle &block, short availableWidth) const
{
  if (!this->materialized())
    return NullTextMeasurement(0, static_cast<short>(this->lineHeight()), 1);
  return MeasureCharacters(*this, this->lineHeight(), block, availableWidth);
}

NullTextMeasurement MeasureNullText(const loka::app::TextStyle &style,
                                    const loka::app::BlockStyle &block,
                                    const loka::core::String *value,
                                    const loka::app::scene::LayoutState &state,
                                    bool *materialized)
{
  const NullTextMetrics metrics(style, value);
  if (materialized)
    *materialized = metrics.materialized();
  return metrics.measure(block, state.width);
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
