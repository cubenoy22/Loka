#include "app/layout/AlignedLineOffset.hpp"
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
    if (span >= this->spanCount())
      return false;
    out = this->table_.advances_[end] - this->table_.advances_[start];
    return true;
  }
  virtual TextLineMetrics metrics(const loka::app::TextStyle &style) const
  {
    for (std::size_t i = 0; i < this->spanCount(); ++i)
      if (this->measure_.sameFont(this->table_.fonts_[i], ToolboxTextFontDescriptor(style)))
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
  this->advances_.clear();
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
  detail::TextMeasureTable<short> positions;
  if (count == (std::numeric_limits<std::size_t>::max)()
      || !positions.allocate((count < SHRT_MAX ? count : SHRT_MAX) + 1)
      || !this->advances_.allocate(count + 1) || !this->bytes_.allocate(count) || !this->characters_.allocate(decoded.length())
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
  this->advances_[0] = 0;
  for (std::size_t i = 0; i < decoded.spanCount(); ++i)
  {
    measure.select(this->fonts_[i]);
    std::size_t previous = 0;
    while (previous < i && !measure.sameFont(this->fonts_[previous], this->fonts_[i]))
      ++previous;
    if (previous < i)
      this->metrics_[i] = this->metrics_[previous];
    else
    {
      FontInfo info;
      GetFontInfo(&info);
      this->metrics_[i].line = TextLineMetrics(info.ascent, info.descent, info.leading);
      this->metrics_[i].maxAdvance = info.widMax > 0 ? info.widMax : 1;
    }
    std::size_t start = this->characters_[this->spans_[i].start].offset;
    const std::size_t end = this->characters_[this->spans_[i].end - 1].end;
    while (start < end)
    {
      const std::size_t next = this->rangeEnd(start, end, i);
      if (next == start)
      {
        this->clear();
        return false;
      }
      // MeasureText returns signed-short positions including the final edge.
      // Native batches bound both byte count and pixels; probes use only the
      // completed wide prefix, so WORD lookahead never re-enters Font Manager.
      MeasureText(static_cast<short>(next - start), &this->bytes_[start], &positions[0]);
      const int base = this->advances_[start];
      for (std::size_t byte = 1; byte <= next - start; ++byte)
      {
        if (positions[byte] < 0 || positions[byte] > INT_MAX - base)
        {
          this->clear();
          return false;
        }
        this->advances_[start + byte] = base + positions[byte];
      }
      start = next;
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
    int dotsWidth = 0;
    if (ellipsis)
    {
      // Null defines overflow/extent, but no glyph descriptor. Follow Text's
      // measured three-dot prefix rule, with the terminal run's face for dots.
      measure.select(this->fonts_[lastSpan]);
      dotsWidth = TextWidth("...", 0, 3);
      budget -= dotsWidth;
      if (budget < 0)
        budget = 0;
    }
    std::size_t paintedEnd = line.fragmentCount
        ? this->lines_->fragment(line.firstFragment + line.fragmentCount - 1).end : 0;
    int paintedWidth = line.width;
    if (ellipsis)
    {
      paintedWidth = dotsWidth;
      for (std::size_t f = 0; f < line.fragmentCount; ++f)
      {
        const TextFragment &fragment = this->lines_->fragment(line.firstFragment + f);
        std::size_t end = fragment.end;
        if (fragment.width > budget)
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
        int advance = 0;
        if (!source.width(fragment.start, end, fragment.span, advance))
          return false;
        paintedWidth += advance;
        budget -= advance;
        paintedEnd = end;
        if (end != fragment.end)
          break;
      }
    }
    // Twin: ToolboxTextContext's plain Text painter aligns its own painted
    // lines. Both use AlignedLineOffset; this path reuses table prefix widths.
    int cursor = x + AlignedLineOffset(availableWidth, paintedWidth,
        block.hasAlign_ ? block.align_ : TEXT_ALIGN_LEFT);
    for (std::size_t f = 0; f < line.fragmentCount; ++f)
    {
      const TextFragment &fragment = this->lines_->fragment(line.firstFragment + f);
      const std::size_t end = fragment.end < paintedEnd ? fragment.end : paintedEnd;
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
