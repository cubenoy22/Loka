#include "context/ToolboxAttributedTextTable.hpp"
#include "platform/StringUTF8.hpp"
#include <climits>
#include <cstring>

using namespace loka::app;
namespace
{
  loka::core::LokaAllocationSite BreakSite()
  {
    return loka::core::LokaAllocationSite("ToolboxAttributedText", "Break");
  }
  short Coordinate(int value)
  {
    return static_cast<short>(value > SHRT_MAX ? SHRT_MAX : value < SHRT_MIN ? SHRT_MIN : value);
  }
} // namespace

/** Borrowed only during build's single port transaction. */
class ToolboxAttributedTextTable::WidthSource : public TextWidthSource
{
public:
  WidthSource(const ToolboxAttributedTextTable &table, const ToolboxTextMeasureScope &measure)
      : table_(table),
        measure_(measure)
  {
  }
  virtual bool valid() const
  {
    return this->table_.bytes_.valid();
  }
  virtual std::size_t length() const
  {
    return this->table_.characters_.size();
  }
  virtual const TextBreakCharacter &character(std::size_t i) const
  {
    return this->table_.characters_[i];
  }
  virtual std::size_t spanCount() const
  {
    return this->table_.spans_.size();
  }
  virtual const TextStyleSpan &span(std::size_t i) const
  {
    return this->table_.spans_[i];
  }
  virtual bool width(std::size_t start, std::size_t end, std::size_t span, int &out) const
  {
    if (start > end || end > this->table_.bytes_.size())
      return false;
    this->measure_.select(this->table_.fonts_[span]);
    out = 0;
    while (start < end)
    {
      const std::size_t next = this->table_.rangeEnd(start, end, span);
      if (next == start)
        return false;
      const short width = TextWidth(&this->table_.bytes_[start], 0, static_cast<short>(next - start));
      if (width < 0)
        return false;
      out = width > INT_MAX - out ? INT_MAX : out + width;
      start = next;
    }
    return true;
  }
  virtual TextLineMetrics metrics(const loka::app::TextStyle &style) const
  {
    for (std::size_t i = 0; i < this->spanCount(); ++i)
      if (this->span(i).style == style)
        return this->table_.metrics_[i].line;
    this->measure_.select(ToolboxTextFontDescriptor(style));
    FontInfo info;
    GetFontInfo(&info);
    return TextLineMetrics(info.ascent, info.descent, info.leading);
  }

private:
  const ToolboxAttributedTextTable &table_;
  const ToolboxTextMeasureScope &measure_;
};

ToolboxAttributedTextTable::ToolboxAttributedTextTable()
    : lines_(0)
{
}
ToolboxAttributedTextTable::~ToolboxAttributedTextTable()
{
  this->clear();
}
void ToolboxAttributedTextTable::clear()
{
  loka::core::LokaDelete(this->lines_, BreakSite());
  this->lines_ = 0;
  this->snapshot_ = AttributedString();
  this->bytes_.clear();
  this->characters_.clear();
  this->spans_.clear();
  this->fonts_.clear();
  this->metrics_.clear();
}

bool ToolboxAttributedTextTable::build(const AttributedString &value,
                                       const BlockStyle &block,
                                       short width,
                                       const ToolboxScenePlatformController &controller)
{
  this->clear();
  // Reuse the common decoder/coalescer only during construction. Its UTF-32
  // offsets are replaced below by offsets into the one joined native buffer.
  const SyntheticTextWidthSource decoded(value);
  if (!decoded.valid())
    return false;
  std::size_t count = 0;
  for (std::size_t i = 0; i < value.segmentCount(); ++i)
  {
    std::string bytes;
    if (!loka::platform::CollectUtf8(value.segment(i).text, bytes)
        || bytes.size() > (std::numeric_limits<std::size_t>::max)() - count)
      return false;
    count += bytes.size();
  }
  if (!this->bytes_.allocate(count) || !this->characters_.allocate(decoded.length())
      || !this->spans_.allocate(decoded.spanCount()) || !this->fonts_.allocate(decoded.spanCount())
      || !this->metrics_.allocate(decoded.spanCount()))
  {
    this->clear();
    return false;
  }
  std::size_t offset = 0;
  for (std::size_t i = 0; i < value.segmentCount(); ++i)
  {
    std::string bytes;
    if (!loka::platform::CollectUtf8(value.segment(i).text, bytes))
    {
      this->clear();
      return false;
    }
    if (!bytes.empty())
      std::memcpy(&this->bytes_[offset], bytes.data(), bytes.size());
    offset += bytes.size();
  }
  offset = 0;
  for (std::size_t i = 0; i < decoded.length(); ++i)
  {
    TextBreakCharacter &row = this->characters_[i];
    row = decoded.character(i);
    row.offset = offset++;
    while (offset < count && (static_cast<unsigned char>(this->bytes_[offset]) & 0xc0) == 0x80)
      ++offset;
    row.end = offset;
  }
  if (offset != count)
  {
    this->clear();
    return false;
  }
  for (std::size_t i = 0; i < decoded.spanCount(); ++i)
  {
    this->spans_[i] = decoded.span(i);
    this->fonts_[i] = ToolboxTextFontDescriptor(decoded.spanStyle(i));
  }
  ToolboxTextMeasureScope measure(controller, decoded.spanCount() ? &this->fonts_[0] : 0, decoded.spanCount());
  for (std::size_t i = 0; i < decoded.spanCount(); ++i)
  {
    std::size_t previous = 0;
    while (previous < i && this->spans_[previous].style != this->spans_[i].style)
      ++previous;
    if (previous < i)
      this->metrics_[i] = this->metrics_[previous];
    else
    {
      measure.select(this->fonts_[i]);
      FontInfo info;
      GetFontInfo(&info);
      this->metrics_[i].line = TextLineMetrics(info.ascent, info.descent, info.leading);
      this->metrics_[i].maxAdvance = info.widMax > 0 ? info.widMax : 1;
    }
  }
  const WidthSource source(*this, measure);
  this->lines_ = loka::core::LokaNew<TextLineBreaker>(BreakSite(), source, block, width);
  if (!this->lines_ || !this->lines_->valid())
  {
    this->clear();
    return false;
  }
  this->snapshot_ = value;
  return true;
}

std::size_t ToolboxAttributedTextTable::rangeEnd(std::size_t start, std::size_t end, std::size_t span) const
{
  // Both native count and native pixel result are signed shorts. WORD lookahead
  // may exceed either even though its eventual wrapped fragments are small.
  const std::size_t limit = SHRT_MAX / this->metrics_[span].maxAdvance;
  std::size_t next = end - start > limit ? start + limit : end;
  while (next > start && next < end && (static_cast<unsigned char>(this->bytes_[next]) & 0xc0) == 0x80)
    --next;
  return next;
}

bool ToolboxAttributedTextTable::draw(short x,
                                      short top,
                                      const ToolboxScenePlatformController &controller,
                                      const BlockStyle &block,
                                      short availableWidth) const
{
  if (!this->valid())
    return false;
  ToolboxTextMeasureScope measure(controller, this->fonts_.size() ? &this->fonts_[0] : 0, this->fonts_.size());
  const WidthSource source(*this, measure);
  int y = top;
  for (std::size_t i = 0; i < this->lines_->lineCount(); ++i)
  {
    const TextLineRecord &line = this->lines_->line(i);
    const bool ellipsis = (!block.hasWrap_ || block.wrap_ == TEXT_WRAP_NONE) && block.hasTruncation_
                          && block.truncation_ == TEXT_TRUNCATION_ELLIPSIS && availableWidth > 0
                          && line.width > availableWidth && line.fragmentCount > 0;
    const std::size_t lastSpan =
        line.fragmentCount ? this->lines_->fragment(line.firstFragment + line.fragmentCount - 1).span : 0;
    int budget = availableWidth;
    if (ellipsis)
    {
      // Null defines overflow/extent, but no glyph descriptor. Follow Text's
      // measured three-dot prefix rule, with the terminal run's face for dots.
      measure.select(this->fonts_[lastSpan]);
      budget -= TextWidth("...", 0, 3);
      if (budget < 0)
        budget = 0;
    }
    int cursor = x;
    for (std::size_t f = 0; f < line.fragmentCount; ++f)
    {
      const TextFragment &fragment = this->lines_->fragment(line.firstFragment + f);
      std::size_t end = fragment.end;
      if (ellipsis && fragment.width > budget)
      {
        end = fragment.start;
        std::size_t low = 0, high = this->characters_.size();
        while (low < high)
        {
          const std::size_t mid = low + (high - low) / 2;
          if (this->characters_[mid].offset < fragment.start)
            low = mid + 1;
          else
            high = mid;
        }
        for (; low < this->characters_.size() && this->characters_[low].end <= fragment.end; ++low)
        {
          int width = 0;
          if (!source.width(fragment.start, this->characters_[low].end, fragment.span, width))
            return false;
          if (width > budget)
            break;
          end = this->characters_[low].end;
        }
      }
      measure.select(this->fonts_[fragment.span]);
      std::size_t start = fragment.start;
      while (start < end)
      {
        const std::size_t next = this->rangeEnd(start, end, fragment.span);
        int width = fragment.width;
        if (next == start)
          return false;
        if ((start != fragment.start || next != fragment.end) && !source.width(start, next, fragment.span, width))
          return false;
        MoveTo(Coordinate(cursor), Coordinate(y + line.metrics.ascent));
        DrawText(&this->bytes_[start], 0, static_cast<short>(next - start));
        cursor = cursor > INT_MAX - width ? INT_MAX : cursor + width;
        if (ellipsis)
          budget -= width;
        start = next;
      }
      if (end != fragment.end)
        break;
    }
    if (ellipsis)
    {
      measure.select(this->fonts_[lastSpan]);
      MoveTo(Coordinate(cursor), Coordinate(y + line.metrics.ascent));
      DrawText("...", 0, 3);
    }
    y = Coordinate(y + line.metrics.ascent + line.metrics.descent + line.metrics.leading);
  }
  return true;
}

short ToolboxAttributedTextTable::height() const
{
  int height = 0;
  if (this->lines_)
    for (std::size_t i = 0; i < this->lines_->lineCount(); ++i)
    {
      const TextLineMetrics &metrics = this->lines_->line(i).metrics;
      height = Coordinate(height + metrics.ascent + metrics.descent + metrics.leading);
    }
  return static_cast<short>(height);
}
