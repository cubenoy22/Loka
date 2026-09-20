#ifndef LOKA_TOOLBOX_ATTRIBUTED_TEXT_TABLE_HPP
#define LOKA_TOOLBOX_ATTRIBUTED_TEXT_TABLE_HPP

#include "app/layout/TextLineBreaker.hpp"
#include "context/ToolboxLayoutUtil.hpp"

/** Context-owned completed PER_RUN projection. A null break is unknown; clear
    releases every derived row together. No port transaction survives build. */
class ToolboxAttributedTextTable
{
public:
  ToolboxAttributedTextTable();
  ~ToolboxAttributedTextTable();
  void clear();
  bool build(const loka::app::AttributedString &value,
             const loka::app::BlockStyle &block,
             short width,
             const ToolboxScenePlatformController &controller);
  bool valid() const
  {
    return this->lines_ != 0;
  }
  const loka::app::TextLineBreaker &lines() const
  {
    assert(this->lines_);
    return *this->lines_;
  }
  const loka::app::AttributedString &value() const
  {
    assert(this->valid());
    return this->snapshot_;
  }
  short height() const;
  bool draw(short x,
            short top,
            const ToolboxScenePlatformController &controller,
            const loka::app::BlockStyle &block,
            short availableWidth) const;

private:
  class WidthSource;
  struct FontMeasurement
  {
    loka::app::TextLineMetrics line;
    short maxAdvance;
    FontMeasurement()
        : maxAdvance(1)
    {
    }
  };
  std::size_t rangeEnd(std::size_t start, std::size_t end, std::size_t span) const;
  loka::app::detail::TextMeasureTable<char> bytes_;
  loka::app::detail::TextMeasureTable<int> advances_;
  loka::app::detail::TextMeasureTable<loka::app::TextBreakCharacter> characters_;
  loka::app::detail::TextMeasureTable<loka::app::TextStyleSpan> spans_;
  loka::app::detail::TextMeasureTable<ToolboxTextFontDescriptor> fonts_;
  loka::app::detail::TextMeasureTable<FontMeasurement> metrics_;
  loka::app::TextLineBreaker *lines_;
  loka::app::AttributedString snapshot_;
  ToolboxAttributedTextTable(const ToolboxAttributedTextTable &);
  ToolboxAttributedTextTable &operator=(const ToolboxAttributedTextTable &);
};
#endif
